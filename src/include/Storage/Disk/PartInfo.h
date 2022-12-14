/****
 * PartInfo.h
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
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

#pragma once

#include <Storage/Partition.h>
#include <Data/Uuid.h>
#include "Error.h"

namespace Storage::Disk
{
/*
 * While not a native feature of file systems, operating systems should also aim to align partitions correctly,
 * which avoids excessive read-modify-write cycles.
 * A typical practice for personal computers is to have each partition aligned to start at a 1 MiB (= 1,048,576 bytes) mark,
 * which covers all common SSD page and block size scenarios, as it is divisible by all commonly used sizes
 * - 1 MiB, 512 KiB, 128 KiB, 4 KiB, and 512 B.
 */
constexpr uint32_t PARTITION_ALIGN{0x100000U};

/**
 * @brief Identifies exact disk volume type
 */
enum class SysType : uint8_t {
	unknown, ///< Partition type not recognised
	fat12,
	fat16,
	fat32,
	exfat,
};

using SysTypes = BitSet<uint8_t, SysType>;

static constexpr SysTypes fatTypes = SysType::fat12 | SysType::fat16 | SysType::fat32 | SysType::exfat;

/**
 * @brief MBR partition system type indicator values
 * @see https://en.wikipedia.org/wiki/Partition_type#List_of_partition_IDs
 */
enum SysIndicator {
	SI_FAT12 = 0x01,
	SI_FAT16 = 0x04,  ///< FAT16 with fewer than 65536 sectors
	SI_FAT16B = 0x06, ///< FAT16B with 65536 or more sectors
	SI_IFS = 0x07,
	SI_EXFAT = 0x07,
	SI_FAT32X = 0x0c, ///< FAT32 with LBA
};

inline SysType getSysTypeFromIndicator(SysIndicator si)
{
	switch(si) {
	case SI_FAT12:
		return SysType::fat12;
	case SI_FAT16:
	case SI_FAT16B:
		return SysType::fat16;
	case SI_FAT32X:
		return SysType::fat32;
	case SI_EXFAT:
		return SysType::exfat;
	default:
		return SysType::unknown;
	}
}

/**
 * @brief Adds information specific to MBR/GPT disk partitions
 */
struct DiskPart {
	Uuid typeGuid;		   ///< GPT type GUID
	Uuid uniqueGuid;	   ///< GPT partition unique GUID
	SysType systype{};	 ///< Identifies volume filing system type
	SysIndicator sysind{}; ///< Partition sys value

	/**
	 * @brief Print full contents of this structure
	 */
	size_t printTo(Print& p) const;
};

/**
 * @brief In-memory partition information
 *
 * A disk Storage::Partition refers to this instance.
 */
struct PartInfo : public Partition::Info, public DiskPart {
	using OwnedList = OwnedLinkedObjectListTemplate<PartInfo>;

	template <typename... Args> PartInfo(Args... args) : Partition::Info(args...)
	{
	}

	/**
	 * @brief Obtain additional disk information
	 *
	 * Accessed via `Partition::diskpart()` method
	 */
	const Disk::DiskPart* diskpart() const override
	{
		return this;
	}

	/**
	 * @brief Print important fields only
	 */
	size_t printTo(Print& p) const override;
};

/**
 * @brief Common type for MBR/GPT partition table
 */
class BasePartitionTable : public PartInfo::OwnedList
{
};

/**
 * @brief Validate partition table entries
 * @param firstAvailableBlock First block number which may be allocated to a partition
 * @param totalAvailableBlocks Number of blocks available for partition allocation
 * @param blockSize Size of a block
 * @retval Error
 *
 * For each partition:
 *
 * - If size <= 100 then the actual size is calculated as a percentage and updated
 * - If offset = 0 then a suitable location is found and the offset updated
 *
 * On success, partition entries are ordered by position.
 */
Error validate(BasePartitionTable& table, storage_size_t firstAvailableBlock, storage_size_t totalAvailableBlocks,
			   uint32_t blockSize);

} // namespace Storage::Disk

String toString(Storage::Disk::SysType type);
