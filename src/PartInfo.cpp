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
#include <debug_progmem.h>

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

Error validate(BasePartitionTable& table, storage_size_t firstAvailableBlock, storage_size_t totalAvailableBlocks,
			   uint32_t blockSize)
{
	if(firstAvailableBlock == 0 || blockSize == 0) {
		return Error::BadParam;
	}

	auto is_aligned = [&](storage_size_t value, uint32_t alignment) { return value % alignment == 0; };
	auto align = [&](storage_size_t value, uint32_t alignment) { return value - value % alignment; };
	auto align_up = [&](storage_size_t value, uint32_t alignment) { return align(value + alignment - 1, alignment); };

	const auto blockAlign = PARTITION_ALIGN / blockSize;

	const uint64_t minOffset = firstAvailableBlock * blockSize;
	const uint64_t maxOffset = uint64_t(firstAvailableBlock + totalAvailableBlocks) * blockSize - 1;

	uint64_t totalBlocks{0};
	for(auto& part : table) {
		if(part.size <= 100) {
			auto blocks = align_up(uint64_t(totalAvailableBlocks) * part.size / 100U, blockAlign);
			if(totalBlocks + blocks > totalAvailableBlocks) {
				// Clip to available space
				blocks = totalAvailableBlocks - totalBlocks;
				if(blocks == 0) {
					debug_e("[DISK] No room for '%s', out of space", part.name.c_str());
					return Error::NoSpace;
				}
			}
			part.size = blocks * blockSize;
			totalBlocks += blocks;
		} else {
			if(!is_aligned(part.offset, PARTITION_ALIGN)) {
				debug_e("[DISK] Partition '%s' mis-aligned", part.name.c_str());
				return Error::MisAligned;
			}
			totalBlocks += align_up(part.size, PARTITION_ALIGN);
		}
		if(part.offset != 0 && (part.offset < minOffset || part.offset > maxOffset)) {
			debug_e("[DISK] Partition '%s' offset outside valid range (%llu <= %llu <= %llu", part.name.c_str(),
					uint64_t(part.offset), minOffset, maxOffset);
			return Error::OutOfRange;
		}
	}

	if(totalBlocks > totalAvailableBlocks) {
		debug_e("[DISK] Partition table exceeds available space");
		return Error::NoSpace;
	}

	// Create temporary list of references which can be sorted
	Vector<Partition::Info*> list;
	list.ensureCapacity(table.count());
	for(auto& p : table) {
		list.addElement(&p);
	}

	auto sortList = [&]() { list.sort([](auto& p1, auto& p2) -> int { return p1->offset > p2->offset ? 1 : 0; }); };
	sortList();

	// Assign undefined offsets
	while(list[0]->offset == 0) {
		auto pz = list[0];
		storage_size_t endOffset = minOffset;
		for(unsigned j = 1; j < list.count(); ++j) {
			auto pr = list[j];
			if(pr->offset == 0) {
				continue;
			}
			endOffset = align_up(pr->offset + pr->size, PARTITION_ALIGN);

			if(j + 1 < list.count()) {
				auto avail = list[j + 1]->offset - endOffset;
				if(avail >= pz->size) {
					break;
				}
			} else {
				auto avail = maxOffset + 1 - endOffset;
				if(avail >= pz->size) {
					break;
				}
				debug_e("[DISK] No room for '%s'", pz->name.c_str());
				return Error::NoSpace;
			}
		}
		pz->offset = endOffset;
		sortList();
	}

	auto lastPart = list.lastElement();
	uint64_t endOffset = lastPart->offset + lastPart->size - 1;

	assert(endOffset <= maxOffset);

	debug_i("Unused space: %llu bytes (%llu blocks)", maxOffset - endOffset, (maxOffset - endOffset) / blockSize);

	return Error::Success;
}

} // namespace Storage::Disk
