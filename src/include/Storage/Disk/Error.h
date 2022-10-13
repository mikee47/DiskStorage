/****
 * Error.h
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

#pragma once

#include <WString.h>

namespace Storage::Disk
{
#define DISK_ERRORCODE_MAP(XX)                                                                                         \
	XX(Success, "Success")                                                                                             \
	XX(BadParam, "Invalid parameter(s)")                                                                               \
	XX(MisAligned, "Partition is mis-aligned")                                                                         \
	XX(OutOfRange, "Partition offset out of valid range")                                                              \
	XX(NoSpace, "No room for partition(s)")                                                                            \
	XX(NoMem, "Memory allocation failed")                                                                              \
	XX(ReadFailure, "Media read failed")                                                                               \
	XX(WriteFailure, "Media write failed")                                                                             \
	XX(EraseFailure, "Media erase failed")

enum class Error {
#define XX(tag, ...) tag,
	DISK_ERRORCODE_MAP(XX)
#undef XX
};

inline bool operator!(Error err)
{
	return err == Error::Success;
}

} // namespace Storage::Disk

String toString(Storage::Disk::Error err);
