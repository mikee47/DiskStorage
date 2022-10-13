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
ErrorCode formatDisk(Device& device, GPT::PartitionTable& table, const Uuid& diskGuid)
{
	if(partitions.isEmpty()) {
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
	const auto partAlignSectors = GPT_ALIGN >> sectorSizeShift; // Partition alignment for GPT [sector]
	const auto numPartitionTableSectors =
		GPT_ITEMS * sizeof(gpt_entry_t) >> sectorSizeShift; // Size of partition table [sector]
	const uint64_t backupPartitionTableSector = driveSectors - numPartitionTableSectors - 1;
	uint64_t nextAllocatableSector = 2 + numPartitionTableSectors;
	const uint64_t sz_pool = backupPartitionTableSector - nextAllocatableSector; // Size of allocatable area
	uint32_t bcc = 0;															 // Cumulative partition entry checksum
	uint64_t sz_part = 1;
	unsigned partitionIndex = 0; // partition table index
	unsigned partitionCount = 0; // Number of partitions created
	auto entries = workBuffer.as<gpt_entry_t[]>();
	const auto entriesPerSector = sectorSize / sizeof(gpt_entry_t);
	auto part = partitions.begin();
	for(; partitionIndex < GPT_ITEMS; ++partitionIndex) {
		auto i = partitionIndex % entriesPerSector;
		if(i == 0) {
			workBuffer.clear();
		}

		// Is the size table not terminated?
		if(part) {
			// Align partition start
			nextAllocatableSector = align_up(nextAllocatableSector, partAlignSectors);
			// Is the size in percentage?
			if(part->size <= 100) {
				sz_part = sz_pool * part->size / 100U;
				// Align partition end
				sz_part = align_up(sz_part, partAlignSectors);
			} else {
				sz_part = part->size >> sectorSizeShift;
			}

			// Clip the size at end of the pool
			if(nextAllocatableSector + sz_part > backupPartitionTableSector) {
				if(nextAllocatableSector < backupPartitionTableSector) {
					sz_part = backupPartitionTableSector - nextAllocatableSector;
				} else {
					// No room for any more partitions
					// TODO: How to report this to the user? Return partition count?
					part->size = sz_part = 0;
					part = nullptr;
				}
			}
		}

		auto& entry = entries[i];

		// Add a partition?
		if(part) {
			part->offset = nextAllocatableSector << sectorSizeShift;
			part->size = sz_part << sectorSizeShift;

			auto dp = part->diskpart();
			entry.partition_type_guid = dp && dp->typeGuid ? dp->typeGuid : GPT::PARTITION_BASIC_DATA_GUID;
			if(dp && dp->uniqueGuid) {
				entry.unique_partition_guid = dp->uniqueGuid;
			} else {
				entry.unique_partition_guid.generate();
			}
			entry.starting_lba = nextAllocatableSector;
			entry.ending_lba = nextAllocatableSector + sz_part - 1;
			nextAllocatableSector += sz_part;

			auto namePtr = part->name.c_str();
			for(unsigned j = 0; j < ARRAY_SIZE(entry.partition_name) && *namePtr != '\0'; ++j, ++namePtr) {
				entry.partition_name[j] = *namePtr;
			}

			++partitionCount;
			if(++part == partitions.end()) {
				part = nullptr;
			}
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
		.header_size = sizeof(gpt_header_t),
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
	header.header_crc32 = crc32(&header, sizeof(header));
	if(!writeSectors(header.my_lba, &header, 1)) {
		return Error::WriteFailure;
	}

	/* Create secondary GPT header */
	std::swap(header.my_lba, header.alternate_lba);
	header.partition_entry_lba = backupPartitionTableSector;
	header.header_crc32 = 0;
	header.header_crc32 = crc32(&header, sizeof(header));
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
	if(!writeSectors(0, &mbr, 1)) {
		return Error::WriteFailure;
	}

	auto& pt = static_cast<CustomDevice&>(device).partitions();
	pt.clear();
	while(!partitions.isEmpty()) {
		pt.add(partitions.pop());
	}

	return partitionCount;
}

} // namespace Storage::Disk
