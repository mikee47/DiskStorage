#pragma once

#include "PartInfo.h"
#include "Error.h"

namespace Storage::Disk::MBR
{
/**
 * @brief Specification for creating a partition using the MBR scheme
 */
struct PartitionSpec {
	/**
	 * @brief Size of volume in bytes, or as percentage of device size
	 * Volume size will be rounded down to the appropriate alignment for the partitioning scheme.
	 */
	storage_size_t size;
	/**
	 * @brief Partition identifier
	 */
	SysIndicator sysIndicator;
};

/**
 * @brief Re-partition a device with the given set of partitions using the MBR scheme
 * @param device
 * @param spec List of partition specifications
 * @param numSpecs Number of partitions to create
 * @retval ErrorCode On success, number of partitions created
 * @note All existing partition information is destroyed
 *
 * Returned number of partitions may be fewer than requested if there was insufficient space.
 */
ErrorCode createPartition(Device& device, PartitionSpec* spec, size_t partitionCount);

/**
 * @brief Create a single MBR partition
 * @note All existing partition information is destroyed
 */
inline ErrorCode createPartition(Device& device, PartitionSpec& spec)
{
	return createPartition(device, &spec, 1);
}

} // namespace Storage::Disk::MBR
