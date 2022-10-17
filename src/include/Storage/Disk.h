#pragma once

#include "Disk/MBR.h"
#include "Disk/GPT.h"

#ifdef ARCH_HOST
#include <Storage/Disk/HostFileDevice.h>
#endif

namespace Storage::Disk
{
bool scanPartitions(Device& device);

} // namespace Storage::Disk
