/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/SmingHub/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * HostFileDevice.h
 *
 ****/

#pragma once

#include "BlockDevice.h"

namespace Storage::Disk
{
/**
 * @brief Create custom storage device using backing file
 */
class HostFileDevice : public BlockDevice
{
public:
	/**
	 * @brief Construct a file device with custom size
	 * @param name Name of device
	 * @param filename Path to file
	 * @param size Size of device in bytes
	 */
	HostFileDevice(const String& name, const String& filename, storage_size_t size);

	/**
	 * @brief Construct a device using existing file
	 * @param name Name of device
	 * @param filename Path to file
	 *
	 * Device will match size of existing file
	 */
	HostFileDevice(const String& name, const String& filename);

	~HostFileDevice();

	String getName() const override
	{
		return name.c_str();
	}

	Type getType() const override
	{
		return Type::file;
	}

protected:
	bool raw_sector_read(storage_size_t address, void* dst, size_t size) override;
	bool raw_sector_write(storage_size_t address, const void* src, size_t size) override;
	bool raw_sector_erase_range(storage_size_t address, size_t size) override;
	bool raw_sync() override
	{
		return true;
	}

private:
	CString name;
	int file{-1};
};

} // namespace Storage::Disk
