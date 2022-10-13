/****
 * MBR.h
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
namespace MBR
{
class PartitionTable : public BasePartitionTable
{
public:
	/**
	 * @brief Add a new MBR partition definition
	 * @param sysType Intended content for this partition (or 'unknown')
	 * @param SysIndicator Appropriate system code SI_xxx
	 * @param offset Start offset, or 0 to have position calculated
	 * @param size Size of partition (in bytes), or percentage (0-100) of total partitionable disk space
	 * @retval bool true on success
	 * @note MBR does not have partition name field; this will appear as 'mbr1', 'mbr2', etc.
	 */
	bool add(SysType sysType, SysIndicator sysIndicator, storage_size_t offset, storage_size_t size)
	{
		auto part =
			new PartInfo(nullptr, fatTypes[sysType] ? Partition::SubType::Data::fat : Partition::SubType::Data::any,
						 offset, size, 0);
		if(part == nullptr) {
			return false;
		}
		part->systype = sysType;
		part->sysind = sysIndicator;
		return BasePartitionTable::add(part);
	}
};

} // namespace MBR

/**
 * @brief Partition a device using the MBR scheme
 * @param device
 * @param table Partitions to create
 * @retval Error
 */
Error formatDisk(Device& device, MBR::PartitionTable& table);

} // namespace Storage::Disk
