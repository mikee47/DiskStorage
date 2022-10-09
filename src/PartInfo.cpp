/****
 * PartInfo.cpp
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

#include "include/Storage/Disk/PartInfo.h"
#include "include/Storage/Disk/GPT.h"

String toString(Storage::Disk::SysType type)
{
	using Type = Storage::Disk::SysType;
	switch(type) {
	case Type::unknown:
		return F("unknown");
	case Type::fat12:
		return F("fat12");
	case Type::fat16:
		return F("fat16");
	case Type::fat32:
		return F("fat32");
	case Type::exfat:
		return F("exfat");
	}

	return nullptr;
}

namespace
{
template <typename T, typename... Args> size_t tprintln(Print& p, String tag, const T& value, Args... args)
{
	size_t n{0};
	n += p.print(tag.padRight(20));
	n += p.print(": ");
	n += p.println(value, args...);
	return n;
}

} // namespace

namespace Storage::Disk
{
size_t DiskPart::printTo(Print& p) const
{
	size_t n{0};

#define TPRINTLN(tag, value, ...) n += tprintln(p, F("  " tag), value, ##__VA_ARGS__)

	TPRINTLN("Sys Type", systype);
	if(typeGuid || uniqueGuid) {
		String typeName = GPT::getTypeName(typeGuid);
		if(typeName) {
			TPRINTLN("EFI Type", typeName);
		}
		TPRINTLN("EFI Type GUID", typeGuid);
		TPRINTLN("EFI Unique GUID", uniqueGuid);
	}
	if(sysind) {
		TPRINTLN("Sys Indicator", String(sysind, HEX, 2));
	}

	return n;
}

size_t PartInfo::printTo(Print& p) const
{
	size_t n = Partition::Info::printTo(p);

	n += p.print(_F(", SysType "));
	n += p.print(systype);
	if(typeGuid) {
		n += p.print(_F(", EFI type "));
		String s = GPT::getTypeName(typeGuid);
		n += p.print(s ?: typeGuid);
	}
	if(uniqueGuid) {
		n += p.print(_F(", id "));
		n += p.print(uniqueGuid);
	}
	return n;
}

} // namespace Storage::Disk
