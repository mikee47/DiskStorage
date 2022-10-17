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
#include "BlockDevice.h"

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

// Base GUID type for regular Sming partition types; node[0..1] indicate type and subtype
struct SmingTypeGuid : public Uuid {
	static constexpr Uuid PROGMEM baseguid{0x3cd54234, 0xcb54, 0x4ed5, 0xbc, 0x8b, 0x00, 0x00, 0x7d, 0x42, 0x84, 0x70};

	SmingTypeGuid() : Uuid(baseguid)
	{
	}

	SmingTypeGuid(Partition::FullType fulltype) : SmingTypeGuid()
	{
		node[0] = uint8_t(fulltype.type);
		node[1] = fulltype.subtype;
	}

	static Partition::FullType match(const Uuid& guid)
	{
		auto tmp = guid;
		Partition::FullType ft{Partition::Type(tmp.node[0]), tmp.node[1]};
		tmp.node[0] = 0;
		tmp.node[1] = 0;
		if(baseguid != tmp) {
			ft = Partition::FullType{};
		}
		return ft;
	}
};

#define XX(name, ...) extern const Uuid name##_GUID;
EFI_PARTITION_TYPE_GUID_MAP(XX)
#undef XX

class PartitionTable : public BasePartitionTable
{
public:
	/**
	 * @brief Add a new standard GPT partition definition
	 * @param name Partition name
	 * @param sysType Intended content for this partition
	 * @param offset Start offset, or 0 to have position calculated
	 * @param size Size of partition (in bytes), or percentage (0-100) of total partitionable disk space
	 * @param uniqueGuid Unique partition identifier (optional: will be generated if not provided)
	 * @param typeGuid Partition type GUID (default is BASIC_DATA)
	 * @param flags
	 * @retval bool true on success
	 */
	bool add(const String& name, SysType sysType, storage_size_t offset, storage_size_t size,
			 const Uuid& uniqueGuid = {}, const Uuid& typeGuid = {}, Partition::Flags flags = 0)
	{
		auto part = new PartInfo(
			name, fatTypes[sysType] ? Partition::SubType::Data::fat : Partition::SubType::Data::any, offset, size, 0);
		if(part == nullptr) {
			return false;
		}
		part->systype = sysType;
		part->typeGuid = typeGuid ?: GPT::PARTITION_BASIC_DATA_GUID;
		if(uniqueGuid) {
			part->uniqueGuid = uniqueGuid;
		} else {
			part->uniqueGuid.generate();
		}
		return BasePartitionTable::add(part);
	}

	/**
	 * @brief Add a new GPT partition for a regular Sming filing system
	 * @param name Partition name
	 * @param type Sming partition type/subtype
	 * @param offset Start offset, or 0 to have position calculated
	 * @param size Size of partition (in bytes), or percentage (0-100) of total partitionable disk space
	 * @param uniqueGuid Unique partition identifier (optional: will be generated if not provided)
	 * @param flags
	 * @retval bool true on success
	 * @note These partitions use a custom type GUID and won't be recognised by external software.
	 */
	bool add(const String& name, Partition::FullType type, storage_size_t offset, storage_size_t size,
			 const Uuid& uniqueGuid = {}, Partition::Flags flags = 0)
	{
		auto part = new PartInfo(name, type, offset, size, 0);
		if(part == nullptr) {
			return false;
		}

		if(type == Partition::SubType::Data::fat) {
			part->typeGuid = GPT::PARTITION_BASIC_DATA_GUID;
		} else {
			part->typeGuid = SmingTypeGuid(type);
		}
		if(uniqueGuid) {
			part->uniqueGuid = uniqueGuid;
		} else {
			part->uniqueGuid.generate();
		}
		return BasePartitionTable::add(part);
	}
};

/**
 * @brief Get string for known GPT type GUIDs
 */
String getTypeName(const Uuid& typeGuid);

} // namespace GPT

/**
 * @brief Partition a device using the GPT scheme
 * @param device
 * @param table Partitions to create
 * @retval Error
 */
Error formatDisk(BlockDevice& device, GPT::PartitionTable& table, const Uuid& diskGuid = {});

} // namespace Storage::Disk
