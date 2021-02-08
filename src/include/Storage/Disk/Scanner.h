#pragma once

#include <Storage/Device.h>
#include "PartInfo.h"
#include "SectorBuffer.h"

namespace Storage
{
namespace Disk
{
struct gpt_mbr_record_t;

class Scanner
{
public:
	Scanner(Device& device);
	~Scanner();

	std::unique_ptr<PartInfo> next();

private:
	enum class State {
		idle,
		MBR, ///< Master Boot Record
		GPT, ///< GUID Partition Table
		error,
		done,
	};

	unsigned scanMbrEntries(uint32_t baseLba);

	Device& device;
	SectorBuffer buffer;
	SectorBuffer entryBuffer; // GPT
	State state{};
	uint64_t sector{0};								// GPT
	std::unique_ptr<gpt_mbr_record_t[]> mbrEntries; // MBR
	uint16_t numPartitionEntries{0};
	uint16_t partitionIndex{0};
	uint16_t sectorSize{0};
	uint8_t sectorSizeShift{0};
};

bool scanPartitions(Device& device);

} // namespace Disk
} // namespace Storage
