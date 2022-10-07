/* SPDX-License-Identifier: GPL-2.0-or-later */
/************************************************************
 * EFI GUID Partition Table
 * Per Intel EFI Specification v1.02
 * http://developer.intel.com/technology/efi/efi.htm
 *
 * By Matt Domsch <Matt_Domsch@dell.com>  Fri Sep 22 22:15:56 CDT 2000  
 *   Copyright 2000,2001 Dell Inc.
 ************************************************************/

#ifndef FS_PART_EFI_H_INCLUDED
#define FS_PART_EFI_H_INCLUDED

#define MSDOS_MBR_SIGNATURE 0xaa55
#define EFI_PMBR_OSTYPE_EFI 0xEF
#define EFI_PMBR_OSTYPE_EFI_GPT 0xEE

#define GPT_MBR_PROTECTIVE 1
#define GPT_MBR_HYBRID 2

#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL
#define GPT_HEADER_REVISION_V1 0x00010000
#define GPT_PRIMARY_PARTITION_TABLE_LBA 1

using efi_guid_t = Uuid;

struct gpt_header_t {
	uint64_t signature;
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_crc32;
	uint32_t reserved1;
	uint64_t my_lba;
	uint64_t alternate_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	efi_guid_t disk_guid;
	uint64_t partition_entry_lba;
	uint32_t num_partition_entries;
	uint32_t sizeof_partition_entry;
	uint32_t partition_entry_array_crc32;

	/* The rest of the logical block is reserved by UEFI and must be zero.
	 * EFI standard handles this by:
	 *
	 * uint8_t		reserved2[ BlockSize - 92 ];
	 */
};

struct gpt_entry_attributes_t {
	uint64_t required_to_function : 1;
	uint64_t reserved : 47;
	uint64_t type_guid_specific : 16;
};

struct gpt_entry_t {
	efi_guid_t partition_type_guid;
	efi_guid_t unique_partition_guid;
	uint64_t starting_lba;
	uint64_t ending_lba;
	gpt_entry_attributes_t attributes;
	uint16_t partition_name[72 / sizeof(uint16_t)];
};

struct __attribute__((packed)) gpt_mbr_record_t {
	uint8_t boot_indicator; /* unused by EFI, set to 0x80 for bootable */
	uint8_t start_head;		/* unused by EFI, pt start in CHS */
	uint8_t start_sector;   /* unused by EFI, pt start in CHS */
	uint8_t start_track;
	uint8_t os_type;	   /* EFI and legacy non-EFI OS types */
	uint8_t end_head;	  /* unused by EFI, pt end in CHS */
	uint8_t end_sector;	/* unused by EFI, pt end in CHS */
	uint8_t end_track;	 /* unused by EFI, pt end in CHS */
	uint32_t starting_lba; /* used by EFI - start addr of the on disk pt */
	uint32_t size_in_lba;  /* used by EFI - size of pt in LBA */
};

struct __attribute__((packed)) legacy_mbr_t {
	uint8_t boot_code[440];
	uint32_t unique_mbr_signature;
	uint16_t unknown;
	gpt_mbr_record_t partition_record[4];
	uint16_t signature;
};

#endif
