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
		std::fill_n(get(), mSize, 0);
	}

private:
	size_t mSectorCount{0};
	size_t mSize{0};
};

} // namespace Disk
} // namespace Storage
