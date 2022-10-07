#include "include/Storage/Disk.h"
#include "include/Storage/Disk/Scanner.h"
#include <Storage/CustomDevice.h>

namespace Storage::Disk
{
bool scanPartitions(Device& device)
{
	auto& dev = static_cast<CustomDevice&>(device);
	dev.partitions().clear();

	Scanner scanner(device);
	std::unique_ptr<PartInfo> part;
	while((part = scanner.next())) {
		if(part->name.length() == 0 && part->uniqueGuid) {
			part->name = part->uniqueGuid;
		}
		dev.partitions().add(part.release());
	}

	return bool(scanner);
}

} // namespace Storage::Disk
