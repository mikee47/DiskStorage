/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * HostFileDevice.cpp
 *
 ****/

#include <hostlib/hostlib.h>
#include <Storage/Disk/HostFileDevice.h>
#include <debug_progmem.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef __WIN32
#include <winioctl.h>
#ifndef FSCTL_SET_ZERO_DATA
#define FSCTL_SET_ZERO_DATA CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 50, METHOD_BUFFERED, FILE_WRITE_DATA)
#endif
#endif

namespace
{
#ifdef __WIN32
HANDLE getHandle(int file)
{
	return HANDLE(_get_osfhandle(file));
}
#endif

void setSparse([[maybe_unused]] int file)
{
#ifdef __WIN32
	if(!DeviceIoControl(getHandle(file), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, nullptr, nullptr)) {
		debug_w("[HFD] SetSparse: FAIL, %u", GetLastError());
	}
#else
	// Linux creates sparse files by default
#endif
}

bool zeroSparse(int file, uint64_t offset, uint64_t len)
{
#ifdef __WIN32
	uint64_t data[]{offset, offset + len};
	return DeviceIoControl(getHandle(file), FSCTL_SET_ZERO_DATA, data, sizeof(data), nullptr, 0, nullptr, nullptr);
#elif defined(__APPLE__)
	if(::lseek(file, offset, SEEK_SET) != offset) {
		return false;
	}
	uint8_t buffer[4096]{};
	while(len != 0) {
		size_t toWrite = std::min(size_t(len), sizeof(buffer));
		int written = ::write(file, buffer, toWrite);
		if(written != toWrite) {
			return false;
		}
		len -= written;
	}
	return true;
#else
	return fallocate64(file, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, offset, len) == 0;
#endif
}

bool truncateFile(int file, uint64_t size)
{
#ifdef __WIN32
	::lseek64(file, size, SEEK_SET);
	return SetEndOfFile(getHandle(file));
#elif defined(__APPLE__)
	return ::ftruncate(file, size) == 0;
#else
	return ::ftruncate64(file, size) == 0;
#endif
}

int64_t seekFile(int file, int64_t offset, int whence = SEEK_SET)
{
#ifdef __APPLE__
	return ::lseek(file, offset, whence);
#else
	return ::lseek64(file, offset, whence);
#endif
}

} // namespace

namespace Storage::Disk
{
HostFileDevice::HostFileDevice(const String& name, const String& filename, storage_size_t size) : name(name)
{
	file = ::open(filename.c_str(), O_CREAT | O_BINARY | O_RDWR, 0644);
	if(file < 0) {
		return;
	}
	setSparse(file);
	size -= size % getBlockSize();
	sectorCount = size >> sectorSizeShift;

	allocateBuffers(4);

	if(truncateFile(file, getSize())) {
		return;
	}

	debug_e("[HFD] Failed to create file '%s', size %llu", name.c_str(), uint64_t(getSize()));

	::close(file);
	file = -1;
	sectorCount = 0;
	::unlink(filename.c_str());
}

HostFileDevice::HostFileDevice(const String& name, const String& filename) : name(name)
{
	file = ::open(filename.c_str(), O_BINARY | O_RDWR);
	if(file < 0) {
		return;
	}
	auto filesize = seekFile(file, 0, SEEK_END);
#ifndef ENABLE_STORAGE_SIZE64
	if(Storage::isSize64(filesize)) {
		debug_e("[HFD] Failed to open '%s', too big %llu, require ENABLE_STORAGE_SIZE64=1", name.c_str(), filesize);
		::close(file);
		file = -1;
		return;
	}
#endif
	setSparse(file);
	filesize -= filesize % getBlockSize();
	sectorCount = filesize >> sectorSizeShift;

	allocateBuffers(4);
}

HostFileDevice::~HostFileDevice()
{
	if(file >= 0) {
		::close(file);
	}
}

bool HostFileDevice::raw_sector_read(storage_size_t address, void* dst, size_t size)
{
	auto offset = uint64_t(address) << sectorSizeShift;
	auto res = seekFile(file, offset);
	if(uint64_t(res) != offset) {
		return false;
	}

	size <<= sectorSizeShift;
	auto count = ::read(file, dst, size);
	return size_t(count) == size;
}

bool HostFileDevice::raw_sector_write(storage_size_t address, const void* src, size_t size)
{
	auto offset = uint64_t(address) << sectorSizeShift;
	auto res = seekFile(file, offset);
	if(uint64_t(res) != offset) {
		return false;
	}

	size <<= sectorSizeShift;
	auto count = ::write(file, src, size);
	return size_t(count) == size;
}

bool HostFileDevice::raw_sector_erase_range(storage_size_t address, size_t size)
{
	auto offset = uint64_t(address) << sectorSizeShift;
	auto len = uint64_t(size) << sectorSizeShift;
	return zeroSparse(file, offset, len);
}

} // namespace Storage::Disk
