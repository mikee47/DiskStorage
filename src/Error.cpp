#include <Storage/Disk/Error.h>

namespace Storage::Disk::Error
{
String toString(ErrorCode err)
{
	err = (err > 0) ? 0 : -err;
	switch(err) {
#define XX(tag, ...)                                                                                                   \
	case tag:                                                                                                          \
		return F(#tag);
		DISK_ERRORCODE_MAP(XX)
#undef XX
	default:
		return F("Error #") + err;
	}
}

} // namespace Storage::Disk::Error
