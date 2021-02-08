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
