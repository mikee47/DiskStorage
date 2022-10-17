#include "include/Storage/Disk.h"
#include "include/Storage/Disk/Scanner.h"
#include <Storage/Device.h>

namespace Storage::Disk
{
bool scanPartitions(Device& device)
{
	auto& pt = device.editablePartitions();
	pt.clear();

	Scanner scanner(device);
	std::unique_ptr<PartInfo> part;
	while((part = scanner.next())) {
		if(part->name.length() == 0 && part->uniqueGuid) {
			part->name = part->uniqueGuid;
		}
		pt.add(part.release());
	}

	return bool(scanner);
}

} // namespace Storage::Disk
