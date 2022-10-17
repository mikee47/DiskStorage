/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * Buffer.h
 *
 ****/
#pragma once

#include <Storage/Device.h>
#include <map>

namespace Storage::Disk
{
struct Buffer : public std::unique_ptr<uint8_t[]> {
	static constexpr uint32_t invalid{uint32_t(-1)};
	uint32_t sector{invalid};
	bool dirty{false};

	void invalidate()
	{
		sector = invalid;
		dirty = false;
	}
};

class BufferList
{
public:
	BufferList(uint16_t sectorSize, size_t count) : mSize(1 << getSizeBits(count))
	{
		list.reset(new Buffer[mSize]);
		if(!list) {
			mSize = 0;
		}
		for(unsigned i = 0; i < mSize; ++i) {
			list[i].reset(new uint8_t[sectorSize]);
		}
	}

	Buffer& get(storage_size_t sector)
	{
		return list[sector & (mSize - 1)];
	}

	Buffer* begin() const
	{
		return &list[0];
	}

	Buffer* end() const
	{
		return &list[mSize];
	}

	size_t size() const
	{
		return mSize;
	}

private:
	std::unique_ptr<Buffer[]> list;
	size_t mSize;
};

} // namespace Storage::Disk
