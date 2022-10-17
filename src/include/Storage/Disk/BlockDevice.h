/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * BlockkDevice.h
 *
 ****/
#pragma once

#include <Storage/Device.h>
#include "Buffer.h"
#include <map>

namespace Storage::Disk
{
/**
 * @brief Base class for sector-addressable (block) devices
 *
 * Inherited classes must set the `sectorCount` value, and implement the four `raw_xxx` methods.
 *
 * This class supports byte-level access using internal buffering, which if required must
 * be enabled by the application via the `allocateBuffers` method.
 * Without buffering, read/writes must always be sector-aligned.
 * Rrase must always be sector-aligned.
 *
 * For power-loss resiliency it is important to call `sync()` at appropriate times.
 * Filing system implementations should do this after closing a file, for example.
 * Applications should consider this if leaving files open for extended periods, and explicitly
 * call 'flush' on the filing system or 'sync' on the partition or device if necessary.
 */
class BlockDevice : public Device
{
public:
	bool read(storage_size_t address, void* dst, size_t size) override;
	bool write(storage_size_t address, const void* src, size_t size) override;
	bool erase_range(storage_size_t address, storage_size_t size) override;

	size_t getBlockSize() const override
	{
		return sectorSize;
	}

	storage_size_t getSize() const override
	{
		return sectorCount << sectorSizeShift;
	}

	storage_size_t getSectorCount() const override
	{
		return sectorCount;
	}

	bool sync() override;

	/**
	 * @brief Set number of sector buffers to use
	 * @param numBuffers Number of buffers to allocate 1,2,4,8,etc. Pass 0 to deallocate/disable buffering.
	 * @retval bool false on memory allocation error, or if failed to flush existing buffers to disk
	 *
	 * Required to support byte-level read/write operations on block devices.
	 * Buffering can improve performance, with diminishing returns above around 4 sectors.
	 */
	bool allocateBuffers(unsigned numBuffers);

	struct Stat {
		enum Function { read, write, erase };
		struct Func {
			uint32_t count[2]{}; // Hit, Miss

			uint32_t totalCount() const
			{
				return count[0] + count[1];
			}

			size_t printTo(Print& p) const;
		};
		Func func[3];					  ///< Read, Write, Erase
		std::map<uint32_t, Func> sectors; ///< By sector

		void update(Function fn, uint32_t sector, uint32_t cacheSector);
		size_t printTo(Print& p) const;
	};
	Stat stat;

protected:
	virtual bool raw_sector_read(storage_size_t address, void* dst, size_t size) = 0;
	virtual bool raw_sector_write(storage_size_t address, const void* src, size_t size) = 0;
	virtual bool raw_sector_erase_range(storage_size_t address, size_t size) = 0;
	virtual bool raw_sync() = 0;

	bool flushBuffer(Buffer& buf);
	bool flushBuffers();

	std::unique_ptr<BufferList> buffers;
	uint64_t sectorCount{0};
	uint16_t sectorSize{defaultSectorSize};
	uint8_t sectorSizeShift{getSizeBits(defaultSectorSize)};
};

} // namespace Storage::Disk
