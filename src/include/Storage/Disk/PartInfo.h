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

namespace Storage
{
namespace Disk
{
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

	DiskPart()
	{
	}

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
	template <typename... Args> PartInfo(Args... args) : Partition::Info(args...), DiskPart()
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

} // namespace Disk
} // namespace Storage

String toString(Storage::Disk::SysType type);
