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
#include <Storage/Device.h>
#include <Storage/Disk/SectorBuffer.h>
#include <Storage/Disk/diskdefs.h>

// Definitions from FileSystem
namespace Storage::Disk
{
ErrorCode formatDisk(Device& device, const MBR::PartitionSpec* partitionSpec, size_t partitionCount)
{
	if(partitionSpec == nullptr || partitionCount == 0 || partitionCount > 4) {
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

	/* Get working buffer */
	SectorBuffer workBuffer(sectorSize, 1);
	if(!workBuffer) {
		return Error::NoMem;
	}

	uint32_t numDeviceSectors = device.getSectorCount();
	// Determine drive CHS without any consideration of the drive geometry
	constexpr uint8_t sectorsPerTrack = N_SEC_TRACK;
	uint8_t numHeads;
	for(numHeads = 8; numHeads != 0 && numDeviceSectors / (numHeads * sectorsPerTrack) > 1024; numHeads *= 2) {
	}
	if(numHeads == 0) {
		// Number of heads needs to be < 256
		numHeads = 255;
	}

	workBuffer.clear();
	auto& mbr = workBuffer.as<legacy_mbr_t>();

	unsigned partIndex;
	uint32_t sect = sectorsPerTrack;
	for(partIndex = 0; partIndex < partitionCount && sect < numDeviceSectors; ++partIndex, ++partitionSpec) {
		uint32_t numPartSectors;
		if(partitionSpec->size <= 100) {
			// Size as percentage
			numPartSectors = uint64_t(partitionSpec->size) * (numDeviceSectors - sectorsPerTrack) / 100U;
		} else {
			numPartSectors = partitionSpec->size >> sectorSizeShift;
		}
		if(sect + numPartSectors > numDeviceSectors || sect + numPartSectors < sect) {
			// Clip at drive size
			numPartSectors = numDeviceSectors - sect;
		}
		if(numPartSectors == 0) {
			// End of table or no sector to allocate
			break;
		}

		struct CHS {
			uint8_t head;
			uint8_t sector;
			uint8_t track;
		};

		auto calc_CHS = [&](uint32_t sect) -> CHS {
			unsigned tracks = sect / sectorsPerTrack;
			uint8_t sec = 1 + (sect % sectorsPerTrack);
			unsigned cyl = tracks / numHeads;
			uint8_t head = tracks % numHeads;
			sec |= (cyl >> 2) & 0xC0;
			return CHS{head, sec, uint8_t(cyl)};
		};

		auto start = calc_CHS(sect);
		auto end = calc_CHS(sect + numPartSectors - 1);

		mbr.partition_record[partIndex] = gpt_mbr_record_t{
			.start_head = start.head,
			.start_sector = start.sector,
			.start_track = start.track,
			.os_type = partitionSpec->sysIndicator,
			.end_head = end.head,
			.end_sector = end.sector,
			.end_track = end.track,
			.starting_lba = sect,
			.size_in_lba = numPartSectors,
		};

		sect += numPartSectors;
	}

	mbr.signature = MSDOS_MBR_SIGNATURE;
	return device.write(0, &mbr, sectorSize) ? partIndex : Error::WriteFailure;
}

} // namespace Storage::Disk
