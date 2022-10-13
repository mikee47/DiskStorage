/****
 * SectorBuffer.h
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

#include <cstdint>
#include <memory>

namespace Storage
{
namespace Disk
{
/**
 * @brief Buffer for working with disk sectors
 */
class SectorBuffer : public std::unique_ptr<uint8_t[]>
{
public:
	SectorBuffer()
	{
	}

	SectorBuffer(size_t sectorSize, size_t sectorCount) : mSectorCount(sectorCount), mSize(sectorSize * sectorCount)
	{
		reset(new uint8_t[mSize]);
	}

	template <typename T> T& as()
	{
		return *reinterpret_cast<T*>(get());
	}

	template <typename T> const T& as() const
	{
		return *reinterpret_cast<const T*>(get());
	}

	size_t size() const
	{
		return mSize;
	}

	uint32_t sectors() const
	{
		return mSectorCount;
	}

	void clear()
	{
		fill(0);
	}

	void fill(uint8_t value)
	{
		std::fill_n(get(), mSize, value);
	}

	bool operator==(const SectorBuffer& other) const
	{
		return *this && other && mSize == other.mSize && memcmp(get(), other.get(), mSize) == 0;
	}

private:
	size_t mSectorCount{0};
	size_t mSize{0};
};

} // namespace Disk
} // namespace Storage
