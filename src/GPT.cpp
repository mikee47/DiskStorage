#include <Storage/Disk/GPT.h>
#include <Storage/Device.h>
#include <Storage/Disk/SectorBuffer.h>
#include <Storage/Disk/diskdefs.h>

extern "C" {
int os_get_random(void* buf, size_t len);
}

// Definitions from FileSystem
namespace Storage ::Disk::GPT
{
/* Create partitions in GPT format */
ErrorCode createPartition(Device& device, const PartitionSpec* partitionSpec, size_t numSpecs)
{
	if(partitionSpec == nullptr || numSpecs == 0) {
		return Error::BadParam;
	}

	uint16_t sectorSize;
#if DISK_MAX_SECTOR_SIZE != DISK_MIN_SECTOR_SIZE
	sectorSize = device.getSectorSize();
	if(sectorSize > DISK_MAX_SECTOR_SIZE || sectorSize < DISK_MIN_SECTOR_SIZE || !isLog2(sectorSize)) {
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

	auto writeSectors = [&](storage_size_t sector, const void* buff, size_t count) -> bool {
		return device.write(sector << sectorSizeShift, buff, count << sectorSizeShift);
	};

	auto driveSectors = device.getSectorCount();
	auto partAlignSectors = GPT_ALIGN >> sectorSizeShift; // Partition alignment for GPT [sector]
	auto numPartitionTableSectors =
		GPT_ITEMS * sizeof(gpt_entry_t) >> sectorSizeShift; // Size of partition table [sector]
	uint64_t backupPartitionTableSector = driveSectors - numPartitionTableSectors - 1;
	uint64_t nextAllocatableSector = 2 + numPartitionTableSectors;
	uint64_t sz_pool = backupPartitionTableSector - nextAllocatableSector; // Size of allocatable area
	uint32_t bcc = 0;													   // Cumulative partition entry checksum
	uint64_t sz_part = 1;
	unsigned partitionIndex = 0; // partition table index
	unsigned partitionCount = 0; // Number of partitions created
	auto entries = workBuffer.as<gpt_entry_t[]>();
	auto entriesPerSector = sectorSize / sizeof(gpt_entry_t);
	for(; partitionIndex < GPT_ITEMS; ++partitionIndex) {
		auto i = partitionIndex % entriesPerSector;
		if(i == 0) {
			workBuffer.clear();
		}

		if(partitionIndex >= numSpecs) {
			partitionSpec = nullptr;
		}

		// Is the size table not terminated?
		if(partitionSpec != nullptr) {
			// Align partition start
			nextAllocatableSector = align_up(nextAllocatableSector, partAlignSectors);
			// Is the size in percentage?
			if(partitionSpec->size <= 100) {
				sz_part = sz_pool * partitionSpec->size / 100U;
				// Align partition end
				sz_part = align_up(sz_part, partAlignSectors);
			} else {
				sz_part = partitionSpec->size >> sectorSizeShift;
			}
			// Clip the size at end of the pool
			if(nextAllocatableSector + sz_part > backupPartitionTableSector) {
				if(nextAllocatableSector < backupPartitionTableSector) {
					sz_part = backupPartitionTableSector - nextAllocatableSector;
				} else {
					// No room for any more partitions
					// TODO: How to report this to the user? Return partition count?
					sz_part = 0;
					partitionSpec = nullptr;
				}
			}
		}

		// Add a partition?
		if(partitionSpec != nullptr) {
			auto& entry = entries[i];
			entry.partition_type_guid = PARTITION_BASIC_DATA_GUID;
			if(partitionSpec->uniqueGuid) {
				entry.unique_partition_guid = partitionSpec->uniqueGuid;
			} else {
				os_get_random(&entry.unique_partition_guid, sizeof(efi_guid_t));
			}
			entry.starting_lba = nextAllocatableSector;
			entry.ending_lba = nextAllocatableSector + sz_part - 1;
			nextAllocatableSector += sz_part;

			auto namePtr = partitionSpec->name.c_str();
			while(i < ARRAY_SIZE(entry.partition_name) && *namePtr != '\0') {
				entry.partition_name[i++] = *namePtr++;
			}

			++partitionCount;
		}

		// Write the buffer if it is filled up
		if(partitionIndex + 1 == entriesPerSector) {
			// Update cumulative partition entry checksum
			bcc = crc32(bcc, entries, sectorSize);
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
	os_get_random(&header.disk_guid, sizeof(efi_guid_t));
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
			.end_head = 0xfe,
			.end_sector = 0xff,
			.end_track = 0,
			.starting_lba = 1,
			.size_in_lba = 0xffffffff,
		}},
		.signature = MSDOS_MBR_SIGNATURE,
	};
	if(!writeSectors(0, &mbr, 1)) {
		return Error::WriteFailure;
	}

	return partitionCount;
}

} // namespace Storage::Disk::GPT
