/****
 * MBR.cpp
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
 * Based on http://elm-chan.org/fsw/ff/00index_e.html but heavily reworked
 *
 * This file is part of the DiskStorage Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#include <Storage/Disk/MBR.h>
#include <Storage/CustomDevice.h>
#include <Storage/Disk/SectorBuffer.h>
#include <Storage/Disk/diskdefs.h>

// Definitions from FileSystem
namespace Storage::Disk
{
Error formatDisk(Device& device, MBR::PartitionTable& table)
{
	if(table.isEmpty() || table.count() > 4) {
		return Error::BadParam;
	}

	uint16_t sectorSize;
#if FF_MAX_SS != FF_MIN_SS
	sectorSize = device.getSectorSize();
	if(sectorSize > FF_MAX_SS || sectorSize < FF_MIN_SS || !isLog2(sectorSize)) {
		return Error::BadParam;
	}
#else
	sectorSize = DISK_MAX_SECTOR_SIZE;
#endif
	uint8_t sectorSizeShift = getSizeBits(sectorSize);

	const uint32_t numDeviceSectors = device.getSectorCount();
	const uint32_t firstAllocatableSector = PARTITION_ALIGN >> sectorSizeShift;
	const uint32_t allocatableSectors = numDeviceSectors - firstAllocatableSector;
	auto err = validate(table, firstAllocatableSector, allocatableSectors, sectorSize);
	if(!!err) {
		return err;
	}

	/* Get working buffer */
	SectorBuffer workBuffer(sectorSize, 1);
	if(!workBuffer) {
		return Error::NoMem;
	}

	// Determine drive CHS without any consideration of the drive geometry
	uint8_t numHeads;
	for(numHeads = 8; numHeads != 0 && numDeviceSectors / (numHeads * N_SEC_TRACK) > 1024; numHeads *= 2) {
	}
	if(numHeads == 0) {
		// Number of heads needs to be < 256
		numHeads = 255;
	}

	workBuffer.clear();
	auto& mbr = workBuffer.as<legacy_mbr_t>();

	unsigned partIndex{0};
	for(auto& part : table) {
		struct CHS {
			uint8_t head;
			uint8_t sector;
			uint8_t track;
		};

		auto calc_CHS = [&](uint32_t sect) -> CHS {
			unsigned tracks = sect / N_SEC_TRACK;
			uint8_t sec = 1 + (sect % N_SEC_TRACK);
			unsigned cyl = tracks / numHeads;
			uint8_t head = tracks % numHeads;
			sec |= (cyl >> 2) & 0xC0;
			return CHS{head, sec, uint8_t(cyl)};
		};

		uint32_t sect = part.offset >> sectorSizeShift;
		uint32_t numsect = part.size >> sectorSizeShift;
		auto start = calc_CHS(sect);
		auto end = calc_CHS(sect + numsect - 1);

		auto dp = part.diskpart();
		mbr.partition_record[partIndex] = gpt_mbr_record_t{
			.start_head = start.head,
			.start_sector = start.sector,
			.start_track = start.track,
			.os_type = dp ? dp->sysind : SI_IFS,
			.end_head = end.head,
			.end_sector = end.sector,
			.end_track = end.track,
			.starting_lba = sect,
			.size_in_lba = numsect,
		};

		++partIndex;
	}

	mbr.signature = MSDOS_MBR_SIGNATURE;
	if(!device.write(0, &mbr, sectorSize) || !device.sync()) {
		return Error::WriteFailure;
	}

	auto& pt = static_cast<CustomDevice&>(device).partitions();
	pt.clear();
	while(!table.isEmpty()) {
		pt.add(table.pop());
	}

	return Error::Success;
}

} // namespace Storage::Disk
