COMPONENT_DEPENDS	:= Storage
COMPONENT_INCDIRS	:= src/include
COMPONENT_SRCDIRS	:= src src/Arch/$(SMING_ARCH)
COMPONENT_DOXYGEN_INPUT := src/include

COMPONENT_VARS			+= DISK_MAX_SECTOR_SIZE
DISK_MAX_SECTOR_SIZE	?= 512
GLOBAL_CFLAGS			+= -DDISK_MAX_SECTOR_SIZE=$(DISK_MAX_SECTOR_SIZE)

COMPONENT_RELINK_VARS += ENABLE_BLOCK_DEVICE_STATS
ifeq ($(ENABLE_BLOCK_DEVICE_STATS),1)
COMPONENT_CPPFLAGS += -DENABLE_BLOCK_DEVICE_STATS
endif
