/****
 * GPT.cpp
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

#include <Storage/Disk/GPT.h>
#include <Storage/CustomDevice.h>
#include <Storage/Disk/SectorBuffer.h>
#include <Storage/Disk/diskdefs.h>
#include <FlashString/Array.hpp>

// Definitions from FileSystem
namespace Storage::Disk
{
namespace GPT
{
#define XX(name, ...) const Uuid name##_GUID PROGMEM{__VA_ARGS__};
EFI_PARTITION_TYPE_GUID_MAP(XX)
#undef XX

String getTypeName(const Uuid& typeGuid)
{
	struct Entry {
		const Uuid* guid;
		const FlashString* name;
	};
#define XX(name, ...) DEFINE_FSTR_LOCAL(FS_##name, #name)
	EFI_PARTITION_TYPE_GUID_MAP(XX)
#undef XX
#define XX(name, ...) {&name##_GUID, &FS_##name},
	DEFINE_FSTR_ARRAY_LOCAL(list, Entry, EFI_PARTITION_TYPE_GUID_MAP(XX))
#undef XX

	for(auto e : list) {
		if(*e.guid == typeGuid) {
			return *e.name;
		}
	}

	return nullptr;
}

} // namespace GPT

/* Create partitions in GPT format */
Error formatDisk(Device& device, GPT::PartitionTable& table, const Uuid& diskGuid)
{
	if(table.isEmpty()) {
		return Error::BadParam;
	}

#if DISK_MAX_SECTOR_SIZE != DISK_MIN_SECTOR_SIZE
	const uint16_t sectorSize = device.getSectorSize();
	if(sectorSize > DISK_MAX_SECTOR_SIZE || sectorSize < DISK_MIN_SECTOR_SIZE || !isLog2(sectorSize)) {
		return Error::BadParam;
	}
#else
	const uint16_t sectorSize = DISK_MAX_SECTOR_SIZE;
#endif
	const uint8_t sectorSizeShift = getSizeBits(sectorSize);

	/* Get working buffer */
	SectorBuffer workBuffer(sectorSize, 1);
	if(!workBuffer) {
		return Error::NoMem;
	}

	auto writeSectors = [&](storage_size_t sector, const void* buff, size_t count) -> bool {
		return device.write(sector << sectorSizeShift, buff, count << sectorSizeShift);
	};

	const auto driveSectors = device.getSectorCount();
	const auto partAlignSectors = PARTITION_ALIGN >> sectorSizeShift; // Partition alignment for GPT [sector]
	const auto numPartitionTableSectors =
		GPT_ITEMS * sizeof(gpt_entry_t) >> sectorSizeShift; // Size of partition table [sector]
	const uint64_t backupPartitionTableSector = driveSectors - numPartitionTableSectors - 1;

	const uint64_t firstAllocatableSector = align_up(2 + numPartitionTableSectors, partAlignSectors);
	const uint64_t allocatableSectors = backupPartitionTableSector - firstAllocatableSector;
	auto err = validate(table, firstAllocatableSector, allocatableSectors, sectorSize);
	if(!!err) {
		return err;
	}

	uint32_t bcc = 0;			 // Cumulative partition entry checksum
	unsigned partitionIndex = 0; // partition table index
	auto entries = workBuffer.as<gpt_entry_t[]>();
	const auto entriesPerSector = sectorSize / sizeof(gpt_entry_t);
	auto part = table.begin();
	for(; partitionIndex < GPT_ITEMS; ++partitionIndex) {
		auto i = partitionIndex % entriesPerSector;
		if(i == 0) {
			workBuffer.clear();
		}

		auto& entry = entries[i];

		// Add a partition?
		if(part != table.end()) {
			auto dp = part->diskpart();
			assert(dp != nullptr);
			entry = gpt_entry_t{
				.partition_type_guid = dp->typeGuid,
				.unique_partition_guid = dp->uniqueGuid,
				.starting_lba = part->offset >> sectorSizeShift,
				.ending_lba = ((uint64_t(part->offset) + part->size) >> sectorSizeShift) - 1,
			};

			auto namePtr = part->name.c_str();
			for(unsigned j = 0; j < ARRAY_SIZE(entry.partition_name) && *namePtr != '\0'; ++j, ++namePtr) {
				entry.partition_name[j] = *namePtr;
			}

			++part;
		}

		// Update cumulative partition entry checksum
		bcc = crc32(bcc, &entry, sizeof(entry));

		// Write the buffer if it is filled up
		if(i + 1 == entriesPerSector) {
			// Write to primary table
			auto entryRelativeSector = partitionIndex / entriesPerSector;
			if(!writeSectors(2 + entryRelativeSector, entries, 1)) {
				return Error::WriteFailure;
			}
			// Write to secondary table
			if(!writeSectors(backupPartitionTableSector + entryRelativeSector, entries, 1)) {
				return Error::WriteFailure;
			}
		}
	}

	/* Create primary GPT header */
	auto& header = workBuffer.as<gpt_header_t>();
	header = gpt_header_t{
		.signature = GPT_HEADER_SIGNATURE,
		.revision = GPT_HEADER_REVISION_V1,
		.header_size = GPT_HEADER_SIZE,
		.my_lba = 1,
		.alternate_lba = driveSectors - 1,
		.first_usable_lba = 2 + numPartitionTableSectors,
		.last_usable_lba = backupPartitionTableSector - 1,
		.partition_entry_lba = 2,
		.num_partition_entries = GPT_ITEMS,
		.sizeof_partition_entry = sizeof(gpt_entry_t),
		.partition_entry_array_crc32 = bcc,
	};
	if(diskGuid) {
		header.disk_guid = diskGuid;
	} else {
		header.disk_guid.generate();
	}
	header.header_crc32 = crc32(&header, GPT_HEADER_SIZE);
	if(!writeSectors(header.my_lba, &header, 1)) {
		return Error::WriteFailure;
	}

	/* Create secondary GPT header */
	std::swap(header.my_lba, header.alternate_lba);
	header.partition_entry_lba = backupPartitionTableSector;
	header.header_crc32 = 0;
	header.header_crc32 = crc32(&header, GPT_HEADER_SIZE);
	if(!writeSectors(header.my_lba, &header, 1)) {
		return Error::WriteFailure;
	}

	/* Create protective MBR */
	auto& mbr = workBuffer.as<legacy_mbr_t>();
	mbr = legacy_mbr_t{
		.partition_record = {{
			.boot_indicator = 0,
			.start_head = 0,
			.start_sector = 2,
			.start_track = 0,
			.os_type = EFI_PMBR_OSTYPE_EFI_GPT,
			.end_head = 0xff,
			.end_sector = 0xff,
			.end_track = 0xff,
			.starting_lba = 1,
			.size_in_lba = uint32_t(std::min(storage_size_t(0xffffffff), driveSectors - 1)),
		}},
		.signature = MSDOS_MBR_SIGNATURE,
	};
	if(!writeSectors(0, &mbr, 1) || !device.sync()) {
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
