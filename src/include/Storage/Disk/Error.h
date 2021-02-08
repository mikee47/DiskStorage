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
