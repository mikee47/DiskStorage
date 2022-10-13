/****
 * Scanner.cpp
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
 * 
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

#include <Storage/Disk/Scanner.h>
#include <Storage/CustomDevice.h>
#include <debug_progmem.h>
#include <Storage/Disk/diskdefs.h>

namespace Storage::Disk
{
namespace
{
#define MAX_FAT12 0xFF5 // Max FAT12 clusters (differs from specs, but right for real DOS/Windows behavior)

#define READ_SECTORS(buff, sector, count) device.read(sector << sectorSizeShift, buff, count << sectorSizeShift)
#define WRITE_SECTORS(buff, sector, count) device.write(sector << sectorSizeShift, buff, count << sectorSizeShift)

String getLabel(const char* s, unsigned length)
{
	while(length > 0 && s[length - 1] == 0x20) {
		--length;
	}
	return String(s, length);
}

// Convert unicode to OEM string
String unicode_to_oem(const uint16_t* str, size_t length)
{
	char buf[length];
	unsigned i;
	for(i = 0; i < length; ++i) {
		auto c = *str++;
		if(c == 0) {
			break;
		}
		buf[i] = c;
	}

	return String(buf, i);
}

bool verifyGptHeader(gpt_header_t& gpt)
{
	/* Check signature, version (1.0) and length (92) */
	if(gpt.signature != GPT_HEADER_SIGNATURE) {
		return false;
	}
	if(gpt.revision != GPT_HEADER_REVISION_V1) {
		return false;
	}
	if(gpt.header_size < sizeof(gpt_header_t)) {
		return false;
	}

	uint32_t crc32_saved = gpt.header_crc32;
	gpt.header_crc32 = 0;
	uint32_t bcc = crc32(&gpt, gpt.header_size);
	gpt.header_crc32 = crc32_saved;
	if(bcc != crc32_saved) {
		debug_e("[GPT] bcc 0x%08x, ~bcc 0x%08x, crc32 0x%08x", bcc, ~bcc, crc32_saved);
		return false;
	}
	if(gpt.sizeof_partition_entry != sizeof(gpt_entry_t)) {
		return false;
	}
	if(gpt.num_partition_entries > 128) {
		return false;
	}

	return true;
}

PartInfo* identify(Device& device, const SectorBuffer& buffer, storage_size_t offset)
{
	auto& fat = buffer.as<const FAT::fat_boot_sector_t>();
	auto& exfat = buffer.as<const EXFAT::boot_sector_t>();

	if(exfat.signature == MSDOS_MBR_SIGNATURE && exfat.fs_type == FSTYPE_EXFAT) {
		auto part =
			new PartInfo(nullptr, Partition::SubType::Data::fat, offset, exfat.vol_length << exfat.sect_size_bits, 0);
		part->systype = SysType::exfat;
		debug_d("[DD] Found ExFAT @ 0x%llx", offset);
		return part;
	}

	// Valid JumpBoot code? (short jump, near jump or near call)
	auto b = fat.jmp_boot[0];
	if(b == 0xEB || b == 0xE9 || b == 0xE8) {
		if(fat.signature == MSDOS_MBR_SIGNATURE && fat.fat32.fs_type == FSTYPE_FAT32) {
			auto part = new PartInfo(getLabel(fat.fat32.vol_label, MSDOS_NAME), Partition::SubType::Data::fat, offset,
									 (fat.sectors ?: fat.total_sect) * fat.sector_size, 0);
			part->systype = SysType::fat32;
			debug_d("[DD] Found FAT32 @ 0x%luu", offset);
			return part;
		}

		// FAT volumes formatted with early MS-DOS lack signature/fs_type
		auto w = fat.sector_size;
		b = fat.sec_per_clus;
		if((w & (w - 1)) == 0 && w >= 512 && w <= 4096			// Properness of sector size (512-4096 and 2^n)
		   && b != 0 && (b & (b - 1)) == 0						// Properness of cluster size (2^n)
		   && fat.reserved != 0									// Properness of reserved sectors (MNBZ)
		   && fat.num_fats - 1 <= 1								// Properness of FATs (1 or 2)
		   && fat.dir_entries != 0								// Properness of root dir entries (MNBZ)
		   && (fat.sectors >= 128 || fat.total_sect >= 0x10000) // Properness of volume sectors (>=128)
		   && fat.fat_length != 0) {							// Properness of FAT size (MNBZ)
			auto part = new PartInfo(getLabel(fat.fat16.vol_label, MSDOS_NAME), Partition::SubType::Data::fat, offset,
									 (fat.sectors ?: fat.total_sect) * fat.sector_size, 0);
			auto numClusters = part->size / (fat.sector_size * fat.sec_per_clus);
			part->systype = (numClusters <= MAX_FAT12) ? SysType::fat12 : SysType::fat16;
			debug_d("[DD] Found FAT @ 0x%luu", offset);
			return part;
		}
	}

	return nullptr;
}

} // namespace

Scanner::Scanner(Device& device) : device(device)
{
}

Scanner::~Scanner()
{
}

unsigned Scanner::scanMbrEntries(uint32_t baseLba)
{
	auto& mbr = buffer.as<legacy_mbr_t>();
	unsigned n{0};
	for(unsigned i = 0; i < ARRAY_SIZE(mbr.partition_record); ++i) {
		auto& rec = mbr.partition_record[i];
		if(rec.starting_lba == 0 || rec.size_in_lba == 0) {
			continue;
		}
		mbrEntries[n] = rec;
		mbrEntries[n].starting_lba += baseLba;
		++n;
	}
	return n;
}

std::unique_ptr<PartInfo> Scanner::next()
{
	if(state == State::idle) {
#if DISK_MAX_SECTOR_SIZE != DISK_MIN_SECTOR_SIZE
		sectorSize = device.getSectorSize();
		if(sectorSize > DISK_MAX_SECTOR_SIZE || sectorSize < DISK_MIN_SECTOR_SIZE || !isLog2(sectorSize)) {
			state = State::error;
			return nullptr;
		}
#else
		sectorSize = DISK_MAX_SECTOR_SIZE;
#endif
		sectorSizeShift = getSizeBits(sectorSize);
		buffer = SectorBuffer(sectorSize, 1);
		if(!buffer) {
			return nullptr;
		}

		// Load sector 0 and check it
		if(!READ_SECTORS(buffer.get(), 0, 1)) {
			state = State::error;
			return nullptr;
		}

		auto part = identify(device, buffer, 0);
		if(part) {
			state = State::done;
			return std::unique_ptr<PartInfo>(part);
		}

		/* Sector 0 is not an FAT VBR or forced partition number wants a partition */

		// GPT protective MBR?
		auto& mbr = buffer.as<legacy_mbr_t>();

		if(mbr.signature != MSDOS_MBR_SIGNATURE) {
			return nullptr;
		}

		if(mbr.partition_record[0].os_type == EFI_PMBR_OSTYPE_EFI_GPT) {
			// Load GPT header sector
			if(!READ_SECTORS(buffer.get(), GPT_PRIMARY_PARTITION_TABLE_LBA, 1)) {
				debug_e("[DD] GPT header read failed");
				state = State::error;
				return nullptr;
			}
			auto& gpt = buffer.as<gpt_header_t>();
			if(!verifyGptHeader(gpt)) {
				debug_e("[DD] GPT invalid");
				state = State::error;
				return nullptr;
			}

			// Scan partition table
			numPartitionEntries = gpt.num_partition_entries;
			sector = gpt.partition_entry_lba;
			partitionIndex = 0;
			entryBuffer = SectorBuffer(sectorSize, 1);
			if(!entryBuffer) {
				state = State::error;
				return nullptr;
			}
			state = State::GPT;
		} else {
			mbrEntries.reset(new gpt_mbr_record_t[4]);
			numPartitionEntries = scanMbrEntries(0);
			state = State::MBR;
		}
	}

	while(partitionIndex < numPartitionEntries) {
		if(state == State::MBR) {
			auto& entry = mbrEntries[partitionIndex++];
			if(!READ_SECTORS(buffer.get(), entry.starting_lba, 1)) {
				continue;
			}
			if(entry.os_type == OSTYPE_EXTENDED) {
				numPartitionEntries = scanMbrEntries(entry.starting_lba);
				partitionIndex = 0;
				continue;
			}
			++mbrPartID;
			storage_size_t offset = entry.starting_lba << sectorSizeShift;
			auto part = identify(device, buffer, offset);
			if(part == nullptr) {
				part = new PartInfo{};
				part->offset = offset;
				part->size = entry.size_in_lba << sectorSizeShift;
			}
			part->name = "mbr" + String(mbrPartID);
			part->sysind = SysIndicator(entry.os_type);
			part->systype = getSysTypeFromIndicator(part->sysind);
			if(part->type == Partition::Type::invalid && fatTypes[part->systype]) {
				part->type = Partition::Type::data;
				part->subtype = uint8_t(Partition::SubType::Data::fat);
			}
			return std::unique_ptr<PartInfo>(part);
		}

		if(state == State::GPT) {
			auto entriesPerSector = sectorSize / sizeof(gpt_entry_t);
			if(partitionIndex % entriesPerSector == 0) {
				if(!READ_SECTORS(buffer.get(), sector++, 1)) {
					state = State::error;
					return nullptr;
				}
			}

			auto entries = buffer.as<gpt_entry_t[]>();
			auto& entry = entries[partitionIndex % entriesPerSector];
			++partitionIndex;
			if(!entry.partition_type_guid) {
				continue;
			}

			if(!READ_SECTORS(entryBuffer.get(), entry.starting_lba, 1)) {
				continue;
			}

			storage_size_t offset = entry.starting_lba << sectorSizeShift;
			auto part = identify(device, entryBuffer, offset);
			if(part == nullptr) {
				part = new PartInfo{};
				part->offset = offset;
				part->size = (1 + entry.ending_lba - entry.starting_lba) << sectorSizeShift;
			}
			part->typeGuid = entry.partition_type_guid;
			part->uniqueGuid = entry.unique_partition_guid;
			part->name = unicode_to_oem(entry.partition_name, ARRAY_SIZE(entry.partition_name));
			return std::unique_ptr<PartInfo>(part);
		}

		assert(false);
		state = State::error;
		return nullptr;
	}

	state = State::done;
	return nullptr;
}

} // namespace Storage::Disk
