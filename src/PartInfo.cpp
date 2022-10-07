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

#include <Storage/Disk/PartInfo.h>
#include <FlashString/Array.hpp>
#include <Storage/Disk/linux/efi.h>

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
String getTypeName(const Uuid& typeGuid)
{
	struct Entry {
		const efi_guid_t* guid;
		const FlashString* name;
	};
#define XX(name, ...) DEFINE_FSTR_LOCAL(FS_##name, #name)
	EFI_PARTITION_TYPE_GUID_MAP(XX)
#undef XX
#define XX(name, ...) {&name##_GUID, &FS_##name},
	DEFINE_FSTR_ARRAY_LOCAL(list, Entry, EFI_PARTITION_TYPE_GUID_MAP(XX))
#undef XX

	for(auto e : list) {
		if(*e.guid == typeGuid) {
			return *e.name;
		}
	}

	return nullptr;
}

size_t DiskPart::printTo(Print& p) const
{
	size_t n{0};

#define TPRINTLN(tag, value, ...) n += tprintln(p, F("  " tag), value, ##__VA_ARGS__)

	TPRINTLN("Sys Type", systype);
	if(typeGuid || uniqueGuid) {
		String typeName = getTypeName(typeGuid);
		if(typeName) {
			TPRINTLN("EFI Type", typeName);
		}
		TPRINTLN("EFI Type GUID", typeGuid);
		TPRINTLN("EFI Unique GUID", uniqueGuid);
	}
	if(sysind) {
		TPRINTLN("Sys Indicator", String(sysind, HEX, 2));
	}
	if(sectorSize || clusterSize) {
		TPRINTLN("Sector Size", sectorSize);
		TPRINTLN("Cluster Size", clusterSize);
	}

	return n;
}

size_t PartInfo::printTo(Print& p) const
{
	size_t n = Partition::Info::printTo(p);

	n += p.print(_F(", SysType "));
	n += p.print(systype);
	if(typeGuid) {
		n += p.print(", EFI type ");
		n += p.print(getTypeName(typeGuid));
	}
	if(uniqueGuid) {
		n += p.print(", id ");
		n += p.print(uniqueGuid);
	}
	return n;
}

} // namespace Storage::Disk
