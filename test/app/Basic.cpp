#include <Storage/Disk.h>
#include <Storage/Debug.h>
#include <SmingTest.h>

#define DIV_KB 1024ULL
#define DIV_MB (DIV_KB * DIV_KB)
#define DIV_GB (DIV_KB * DIV_MB)

using namespace Storage;
using namespace Disk;

class BasicTest : public TestGroup
{
public:
	BasicTest() : TestGroup(_F("Basic"))
	{
	}

	void execute() override
	{
		DEFINE_FSTR_LOCAL(MBR_DEVICE_FILENAME, "out/test-mbr.img")
		DEFINE_FSTR_LOCAL(GPT_DEVICE_FILENAME, "out/test-gpt.img")

		TEST_CASE("Create MBR")
		{
			MBR::PartitionTable partitions;
			partitions.add(SysType::fat16, SI_FAT16B, 0, 50);
			partitions.add(SysType::fat16, SI_FAT16B, 0, 10);
			partitions.add(SysType::fat16, SI_FAT16B, 0, 35);
			partitions.add(SysType::fat12, SI_FAT12, 0, 5);

			auto dev = createDevice(MBR_DEVICE_FILENAME, 100 * DIV_MB);
			auto err = Disk::formatDisk(*dev, partitions);
			Serial << "formatDisk: " << err << endl;
			for(auto& part : partitions) {
				Serial << part << endl;
			}
			checkPartitions(*dev, 4);
			delete dev;
		}

		TEST_CASE("Open MBR")
		{
			auto dev = openDevice(MBR_DEVICE_FILENAME);
			checkPartitions(*dev, 4);
			delete dev;
		}

		TEST_CASE("Create GPT")
		{
			// {61d3ce8a-d7c9-400b-8f00-6fdab7d52765}
			static constexpr Uuid PROGMEM myDiskGuid{0x61d3ce8a, 0xd7c9, 0x400b, 0x8f, 0x00, 0x6f,
													 0xda,		 0xb7,   0xd5,   0x27, 0x65};
			// {203a9900-f29c-49f0-bfbc-c64ed331e3dc}
			static constexpr Uuid PROGMEM myTypeGuid{0x203a9900, 0xf29c, 0x49f0, 0xbf, 0xbc, 0xc6,
													 0x4e,		 0xd3,   0x31,   0xe3, 0xdc};
			// {23672da9-d8ae-43fa-8776-5c2929d88901}
			static constexpr Uuid PROGMEM part1guid{0x23672da9, 0xd8ae, 0x43fa, 0x87, 0x76, 0x5c,
													0x29,		0x29,   0xd8,   0x89, 0x01};
			// {13b4becf-d095-41df-b41c-321f184be598}
			static constexpr Uuid PROGMEM part2guid{0x13b4becf, 0xd095, 0x41df, 0xb4, 0x1c, 0x32,
													0x1f,		0x18,   0x4b,   0xe5, 0x98};
			// {40a78f58-5977-41d1-97fd-cf542c9a1a4c}
			static constexpr Uuid PROGMEM part3guid{0x40a78f58, 0x5977, 0x41d1, 0x97, 0xfd, 0xcf,
													0x54,		0x2c,   0x9a,   0x1a, 0x4c};
			// {21da42c0-0bfb-4a53-b85d-05eb3f361805}
			static constexpr Uuid PROGMEM part4guid{0x21da42c0, 0x0bfb, 0x4a53, 0xb8, 0x5d, 0x05,
													0xeb,		0x3f,   0x36,   0x18, 0x05};
			// {3cd54234-cb54-4ed5-bc8b-55fc7d428470}
			static constexpr Uuid PROGMEM part5guid{0x3cd54234, 0xcb54, 0x4ed5, 0xbc, 0x8b, 0x55,
													0xfc,		0x7d,   0x42,   0x84, 0x70};

			GPT::PartitionTable partitions;
			partitions.add("My FAT partition", SysType::exfat, 0, 50, part1guid);
			partitions.add("My other partition", SysType::fat32, 0, 10, part2guid);
			partitions.add("yet another one", SysType::unknown, 0, 20, part3guid);
			partitions.add("last basic partition", SysType::fat16, 0, 18, part4guid);
			partitions.add("custom partition type", SysType::unknown, 0, 2, part5guid, myTypeGuid);
			// partitions.add(new Partition::Info{"custom partition type", Partition::SubType::Data::littlefs, 0, 2});

			auto dev = createDevice(GPT_DEVICE_FILENAME, 100 * DIV_MB);
			auto err = Disk::formatDisk(*dev, partitions, myDiskGuid);
			Serial << "formatDisk: " << err << endl;
			for(auto& p : partitions) {
				Serial << p << endl;
			}
			Debug::listPartitions(Serial, *dev);
			checkPartitions(*dev, 5);

			// Will be checking generated image file later, so clear BPB for each partition
			for(auto part : dev->partitions()) {
				uint8_t buffer[512]{};
				CHECK(part.write(0, buffer, sizeof(buffer)));
			}

			delete dev;
		}

		TEST_CASE("Open GPT")
		{
			auto dev = openDevice(GPT_DEVICE_FILENAME);
			checkPartitions(*dev, 5);
			delete dev;
		}

		TEST_CASE("Buffering")
		{
			auto dev = openDevice(GPT_DEVICE_FILENAME);
			REQUIRE(Disk::scanPartitions(*dev));
			auto part = *dev->partitions().begin();
			constexpr size_t bufSize{32};
			constexpr uint32_t offset{12345};
			uint8_t buf1[bufSize];
			uint8_t buf2[bufSize]{};
			os_get_random(buf1, bufSize);
			dev->allocateBuffers(0);
			REQUIRE(part.write(offset, buf1, bufSize) == false);
			REQUIRE(part.read(offset, buf2, bufSize) == false);
			dev->allocateBuffers(1);
			REQUIRE(part.write(offset, buf1, bufSize));
			REQUIRE(part.read(offset, buf2, bufSize));
			REQUIRE(memcmp(buf1, buf2, bufSize) == 0);
			dev->sync();
			delete dev;

			dev = openDevice(GPT_DEVICE_FILENAME);
			REQUIRE(Disk::scanPartitions(*dev));
			part = *dev->partitions().begin();

			memset(buf2, 0, bufSize);
			REQUIRE(part.read(offset, buf2, bufSize));
			REQUIRE(memcmp(buf1, buf2, bufSize) == 0);

			delete dev;
		}
	}

	void checkPartitions(Device& dev, unsigned expectedPartitionCount)
	{
		REQUIRE(Disk::scanPartitions(dev));
		size_t partCount{0};
		for(auto part : dev.partitions()) {
			Serial << part << endl << *part.diskpart() << endl;
			CHECK(part.diskpart() != nullptr);
			++partCount;
		}
		REQUIRE_EQ(partCount, expectedPartitionCount);
	}

	BlockDevice* createDevice(const String& filename, storage_size_t size)
	{
#ifdef ARCH_HOST
		auto dev = new HostFileDevice("test", filename, size);
#else
		BlockDevice* dev{};
#endif
		if(dev == nullptr || dev->getSize() == 0) {
			Serial << _F("Failed to create '") << filename << ": " << endl;
			TEST_ASSERT(false);
		}

		Serial << "Created \"" << filename << "\", " << dev->getSize() << " bytes." << endl;

		registerDevice(dev);

		return dev;
	}

	BlockDevice* openDevice(const String& filename)
	{
#ifdef ARCH_HOST
		auto dev = new HostFileDevice("test", filename);
#else
		BlockDevice* dev{};
#endif
		if(dev == nullptr || dev->getSize() == 0) {
			Serial << _F("Failed to open '") << filename << ": " << endl;
			TEST_ASSERT(false);
		}

		Serial << "Opened \"" << filename << "\", " << dev->getSize() << " bytes." << endl;

		registerDevice(dev);

		return dev;
	}
};

void REGISTER_TEST(basic)
{
	registerGroup<BasicTest>();
}
