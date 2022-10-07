#pragma once

#include "Disk/MBR.h"
#include "Disk/GPT.h"

namespace Storage::Disk
{
bool scanPartitions(Device& device);

} // namespace Storage::Disk
