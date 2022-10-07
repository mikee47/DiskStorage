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

namespace Storage
{
namespace Disk
{
using ErrorCode = int;

namespace Error
{
#define DISK_ERRORCODE_MAP(XX)                                                                                         \
	XX(Success, "Success")                                                                                             \
	XX(BadParam, "Invalid parameter(s)")                                                                               \
	XX(NoMem, "Memory allocation failed")                                                                              \
	XX(ReadFailure, "Media read failed")                                                                               \
	XX(WriteFailure, "Media write failed")                                                                             \
	XX(EraseFailure, "Media erase failed")

enum class Value {
#define XX(tag, ...) tag,
	DISK_ERRORCODE_MAP(XX)
#undef XX
};

enum {
#define XX(tag, ...) tag = -int(Value::tag),
	DISK_ERRORCODE_MAP(XX)
#undef XX
};

String toString(ErrorCode err);

} // namespace Error
} // namespace Disk
} // namespace Storage
