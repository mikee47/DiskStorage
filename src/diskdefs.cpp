/****
 * diskdefs.cpp
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

#include <Storage/Disk/diskdefs.h>
#include <debug_progmem.h>

namespace Storage
{
namespace Disk
{
uint32_t crc32_byte(uint32_t crc, uint8_t d)
{
	crc ^= d;
	for(unsigned i = 0; i < 8; ++i) {
		uint32_t mask = -(crc & 1);
		crc = (crc >> 1) ^ (0xEDB88320 & mask);
	}
	return crc;
}

uint32_t crc32(uint32_t bcc, const void* data, size_t length)
{
	bcc = ~bcc;
	auto ptr = static_cast<const uint8_t*>(data);
	while(length-- != 0) {
		bcc = crc32_byte(bcc, *ptr++);
	}
	return ~bcc;
}

} // namespace Disk
} // namespace Storage
