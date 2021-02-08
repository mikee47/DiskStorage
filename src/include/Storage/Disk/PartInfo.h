#pragma once

#include <Storage/Partition.h>
#include <Data/Uuid.h>

namespace Storage
{
namespace Disk
{
/**
 * @brief Identifies exact disk volume type
 */
enum class SysType : uint8_t {
	unknown, ///< Partition type not recognised
	fat12,
	fat16,
	fat32,
	exfat,
};

using SysTypes = BitSet<uint8_t, SysType>;

static constexpr SysTypes fatTypes = SysType::fat12 | SysType::fat16 | SysType::fat32
#ifdef ENABLE_EXFAT
									 | SysType::exfat
#endif
	;

/**
 * @brief MBR partition system type indicator values
 * @see https://en.wikipedia.org/wiki/Partition_type#List_of_partition_IDs
 */
enum SysIndicator {
	SI_EXFAT = 0x07,
	SI_FAT32X = 0x0c, ///< FAT32 with LBA
	SI_FAT16B = 0x06, ///< FAT16B with 65536 or more sectors
	SI_FAT16 = 0x04,  ///< FAT16 with fewer than 65536 sectors
	SI_FAT12 = 0x01,
};

inline SysType getSysTypeFromIndicator(SysIndicator si)
{
	switch(si) {
	case SI_EXFAT:
		return SysType::exfat;
	case SI_FAT32X:
		return SysType::fat32;
	case SI_FAT16:
	case SI_FAT16B:
		return SysType::fat16;
	case SI_FAT12:
		return SysType::fat12;
	default:
		return SysType::unknown;
	}
}

/**
 * @brief Adds information specific to MBR/GPT disk partitions
 */
struct DiskPart {
	Uuid typeGuid;			///< GPT type GUID
	Uuid uniqueGuid;		///< GPT partition unique GUID
	uint32_t clusterSize{}; ///< Cluster size (bytes)
	uint16_t sectorSize{};  ///< Sector size (bytes)
	SysType systype{};		///< Identifies volume filing system type
	SysIndicator sysind{};  ///< Partition sys value

	size_t printTo(Print& p) const;
};

struct PartInfo : public Partition::Info, public DiskPart {
	template <typename... Args> PartInfo(Args... args) : Partition::Info(args...), DiskPart{}
	{
	}

	const Disk::DiskPart* diskpart() const override
	{
		return this;
	}
};

String getTypeName(const Uuid& typeGuid);

} // namespace Disk
} // namespace Storage

String toString(Storage::Disk::SysType type);
