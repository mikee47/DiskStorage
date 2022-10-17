/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * BlockDevice.cpp
 *
 ****/

#include "include/Storage/Disk/BlockDevice.h"
#include <debug_progmem.h>

namespace Storage::Disk
{
#define CHECK_ALIGN(func)                                                                                              \
	if((address & (sectorSize - 1)) != 0 || (size & (sectorSize - 1)) != 0) {                                          \
		debug_e("[SD] " func " misaligned %llx, %llx", uint64_t(address), uint64_t(size));                             \
		return false;                                                                                                  \
	}

void BlockDevice::Stat::update(Function fn, uint32_t sector, uint32_t cacheSector)
{
#ifdef ENABLE_BLOCK_DEVICE_STATS
	unsigned i = (sector == cacheSector) ? 0 : 1;
#ifdef ARCH_HOST
	++sectors[sector].count[i];
#endif
	++func[fn].count[i];
#endif
}

size_t BlockDevice::Stat::Func::printTo(Print& p) const
{
	size_t n{0};
#ifdef ENABLE_BLOCK_DEVICE_STATS
	n += p.print(_F("hit "));
	n += p.print(count[0], DEC, 5, ' ');
	n += p.print(_F(", miss "));
	n += p.print(count[1], DEC, 5, ' ');
#endif
	return n;
}

size_t BlockDevice::Stat::printTo(Print& p) const
{
	size_t n{0};
#ifdef ENABLE_BLOCK_DEVICE_STATS
	n += p.print(_F("  Read "));
	n += p.println(func[0]);
	n += p.print(_F("  Write "));
	n += p.println(func[1]);
	n += p.print(_F("  Erase "));
	n += p.println(func[2]);

#ifdef ARCH_HOST
	Vector<decltype(sectors)::value_type> items;
	items.ensureCapacity(sectors.size());
	for(auto e : sectors) {
		if(e.second.totalCount() >= 10) {
			items.add(e);
		}
	}
	items.sort([](const auto& e1, const auto& e2) -> int { return e2.second.count[0] - e1.second.count[0]; });
	for(auto e : items) {
		n += p.print("  ");
		n += p.print(e.first, DEC, 8, ' ');
		n += p.print(": ");
		n += p.println(e.second);
	}
#endif
#else
	n += p.print(_F("  (stats disabled)"));
#endif
	return n;
}

bool BlockDevice::read(storage_size_t address, void* dst, size_t size)
{
	if(!buffers) {
		CHECK_ALIGN("read")
		return raw_sector_read(address >> sectorSizeShift, dst, size >> sectorSizeShift);
	}

	auto sector = address >> sectorSizeShift;
	uint32_t offset = address & (sectorSize - 1);
	auto dstptr = static_cast<uint8_t*>(dst);

	while(size != 0) {
		size_t chunkSize = std::min(size, size_t(sectorSize - offset));
		auto& buf = buffers->get(sector);
		stat.update(stat.read, sector, buf.sector);
		if(buf.sector != sector) {
			if(!flushBuffer(buf)) {
				return false;
			}
			buf.invalidate();
			if(!raw_sector_read(sector, buf.get(), 1)) {
				return false;
			}
			buf.sector = sector;
		}

		memcpy(dstptr, &buf[offset], chunkSize);

		dstptr += chunkSize;
		size -= chunkSize;
		++sector;
		offset = 0;
	}

	return true;
}

bool BlockDevice::write(storage_size_t address, const void* src, size_t size)
{
	if(!buffers) {
		CHECK_ALIGN("write")
		return raw_sector_write(address >> sectorSizeShift, src, size >> sectorSizeShift);
		}

	auto sector = address >> sectorSizeShift;
	uint32_t offset = address & (sectorSize - 1);
	auto srcptr = static_cast<const uint8_t*>(src);

	while(size != 0) {
		size_t chunkSize = std::min(size, size_t(sectorSize - offset));
		auto& buf = buffers->get(sector);
		stat.update(stat.write, sector, buf.sector);
		if(buf.sector != sector) {
			if(!flushBuffer(buf)) {
				return false;
			}
			if(offset != 0 || chunkSize != sectorSize) {
				buf.invalidate();
				if(!raw_sector_read(sector, buf.get(), 1)) {
					return false;
				}
			}
			buf.sector = sector;
		}

		memcpy(&buf[offset], srcptr, chunkSize);
		buf.dirty = true;

		srcptr += chunkSize;
		size -= chunkSize;
		++sector;
		offset = 0;
	}

	return true;
}

/*
 * Block devices erase state is 0 (not FF)
 */
bool BlockDevice::erase_range(storage_size_t address, storage_size_t size)
{
	CHECK_ALIGN("erase")

	address >>= sectorSizeShift;
	size >>= sectorSizeShift;

	if(!raw_sector_erase_range(address, size)) {
		return false;
	}

	if(!buffers) {
		return true;
	}

	for(; size != 0; --size, ++address) {
		auto& buf = buffers->get(address);
		stat.update(stat.erase, address, buf.sector);
		if(buf.sector == address) {
			memset(buf.get(), 0, sectorSize);
			buf.dirty = false;
		}
	}

	return true;
}

bool BlockDevice::allocateBuffers(unsigned numBuffers)
{
	if(!flushBuffers()) {
		return false;
	}
	buffers.reset();
	if(numBuffers == 0) {
		return true;
	}
	buffers.reset(new Disk::BufferList(sectorSize, numBuffers));
	return buffers && buffers->size() == numBuffers;
}

bool BlockDevice::flushBuffer(Buffer& buf)
{
	if(!buf.dirty) {
		return true;
	}
	if(!raw_sector_write(buf.sector, buf.get(), 1)) {
		return false;
	}
	buf.dirty = false;
	return true;
}

bool BlockDevice::flushBuffers()
{
	bool res{true};

	if(buffers) {
		for(auto& buf : *buffers) {
			res &= flushBuffer(buf);
		}
	}

	return res;
}

bool BlockDevice::sync()
{
	return flushBuffers() && raw_sync();
}

} // namespace Storage::Disk