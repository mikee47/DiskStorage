/****
 * Scanner.h
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

#include <Storage/Device.h>
#include "PartInfo.h"
#include "SectorBuffer.h"

namespace Storage
{
namespace Disk
{
struct gpt_mbr_record_t;

/**
 * @brief Class to iterate through disk partition tables
 *
 * Supports MBR and GPT partitioning schemes.
 */
class Scanner
{
public:
	Scanner(Device& device);
	~Scanner();

	/**
	 * @brief Obtains the next partition entry (if any)
	 */
	std::unique_ptr<PartInfo> next();

	explicit operator bool() const
	{
		return state != State::error;
	}

private:
	enum class State {
		idle,
		MBR, ///< Master Boot Record
		GPT, ///< GUID Partition Table
		error,
		done,
	};

	unsigned scanMbrEntries(uint32_t baseLba);

	Device& device;
	SectorBuffer buffer;
	SectorBuffer entryBuffer; // GPT
	State state{};
	uint64_t sector{0};								// GPT
	std::unique_ptr<gpt_mbr_record_t[]> mbrEntries; // MBR
	uint16_t numPartitionEntries{0};
	uint16_t partitionIndex{0};
	uint16_t mbrPartID{0};
	uint16_t sectorSize{0};
	uint8_t sectorSizeShift{0};
};

} // namespace Disk
} // namespace Storage
