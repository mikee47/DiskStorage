/****
 * GPT.h
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

#include "PartInfo.h"
#include "Error.h"

namespace Storage::Disk
{
namespace GPT
{
#define EFI_PARTITION_TYPE_GUID_MAP(XX)                                                                                \
	XX(PARTITION_SYSTEM, 0xC12A7328, 0xF81F, 0x11d2, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B)                   \
	XX(LEGACY_MBR_PARTITION, 0x024DEE41, 0x33E7, 0x11d3, 0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F)               \
	XX(PARTITION_MSFT_RESERVED, 0xE3C9E316, 0x0B5C, 0x4DB8, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE)            \
	XX(PARTITION_BASIC_DATA, 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)               \
	XX(PARTITION_LINUX_RAID, 0xa19d880f, 0x05fc, 0x4d3b, 0xa0, 0x06, 0x74, 0x3f, 0x0f, 0x84, 0x91, 0x1e)               \
	XX(PARTITION_LINUX_SWAP, 0x0657fd6d, 0xa4ab, 0x43c4, 0x84, 0xe5, 0x09, 0x33, 0xc8, 0x4b, 0x4f, 0x4f)               \
	XX(PARTITION_LINUX_LVM, 0xe6d6d379, 0xf507, 0x44c2, 0xa2, 0x3c, 0x23, 0x8f, 0x2a, 0x3d, 0xf9, 0x28)                \
	XX(PARTITION_LINUX_DATA, 0x0fc63daf, 0x8483, 0x4772, 0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4)

#define XX(name, ...) extern const Uuid name##_GUID;
EFI_PARTITION_TYPE_GUID_MAP(XX)
#undef XX

/**
 * @brief Get string for known GPT type GUIDs
 */
String getTypeName(const Uuid& typeGuid);

/**
 * @brief Specification for creating a partition using the GPT scheme
 */
struct PartitionSpec {
	/**
	 * @brief Size of volume in bytes, or as percentage of device size
	 * Volume size will be rounded down to the appropriate alignment for the partitioning scheme.
	 */
	storage_size_t size;
	/**
	 * @brief Partition name. Optional.
	 */
	String name;
	/**
	 * @brief Partition type GUID. Defaults to BASIC.
	 */
	Uuid typeGuid;
	/**
	 * @brief Random unique GUID to use
	 * 
	 * If null (default) then GUID will be generated automatically.
	 */
	Uuid uniqueGuid;
};

} // namespace GPT

/**
 * @brief Re-partition a device with the given set of GPT BASIC partitions
 * @param device
 * @param spec List of partition specifications
 * @param numSpecs Number of partitions to create
 * @retval ErrorCode On success, number of partitions created
 * @note All existing partition information is destroyed
 *
 * Returned number of partitions may be fewer than requested if there was insufficient space.
 */
ErrorCode createPartition(Device& device, const GPT::PartitionSpec* spec, size_t numSpecs, const Uuid& diskGuid = {});

/**
 * @brief Create a single GPT BASIC partition
 * @note All existing partition information is destroyed
 */
inline ErrorCode createPartition(Device& device, const GPT::PartitionSpec& spec, const Uuid& diskGuid = {})
{
	return createPartition(device, &spec, 1, diskGuid);
}

} // namespace Storage::Disk
