/*
 * Copyright (c) 2015 Apple Inc.  All rights reserved.
 */

#include <mach-o/loader.h>

#ifndef _LOADER_ADDITIONS_H
#define _LOADER_ADDITIONS_H

/*
 * Something like this should end up in <mach-o/loader.h>
 */

#define proto_LC_COREINFO	0x140	/* unofficial value!! */

#define proto_CORETYPE_KERNEL	1
#define proto_CORETYPE_USER		2
#define proto_CORETYPE_IBOOT	3

struct proto_coreinfo_command {
    uint32_t cmd;		/* LC_COREINFO */
    uint32_t cmdsize;	/* total size of this command */
    uint32_t version;	/* currently 1 */
    uint16_t type;		/* CORETYPE_KERNEL, CORETYPE_USER etc. */
	uint16_t pageshift;	/* log2 host pagesize */
	/* content & interpretation depends on 'type' field */
    uint64_t address;   /* load address of "main binary" */
    uint8_t uuid[16];   /* uuid of "main binary" */
    uint64_t dyninfo;   /* dynamic modules info */
};

#define proto_LC_FILEREF 0x141 /* unofficial value! */

#define FREF_ID_SHIFT		0
#define FREF_ID_MASK		0x7	/* up to 8 flavors */

typedef enum {
	kFREF_ID_NONE = 0,			/* file has no available verifying ID */
	kFREF_ID_UUID = 1,			/* file has an associated UUID */
	kFREF_ID_MTIMESPEC_LE = 2,	/* file has a specific mtime */
								/* one day: file has a computed hash? */
} fref_type_t;

#define FREF_ID_TYPE(f)		((fref_type_t)(((f) >> FREF_ID_SHIFT) & FREF_ID_MASK))
#define FREF_MAKE_FLAGS(t)	(((t) & FREF_ID_MASK) << FREF_ID_SHIFT)

struct proto_fileref_command {
    uint32_t cmd;	/* LC_FILEREF */
    uint32_t cmdsize;
    union lc_str filename;  /* filename these bits come from */
    uint8_t id[16];		/* uuid, size or hash etc. */
    uint64_t vmaddr;	/* memory address of this segment */
    uint64_t vmsize;	/* memory size of this segment */
    uint64_t fileoff;	/* file offset of this segment */
    uint64_t filesize;	/* amount to map from the file */
    vm_prot_t maxprot;	/* maximum VM protection */
    vm_prot_t prot;	/* current VM protection */
	uint32_t flags;
    uint8_t share_mode;	/* SM_COW etc. */
    uint8_t purgable;	/* VM_PURGABLE_NONVOLATILE etc. */
    uint8_t tag;	/* VM_MEMORY_MALLOC etc. */
    uint8_t extp;	/* external pager */
};

#define proto_LC_COREDATA	0x142	/* unofficial value! */

/*
 * These are flag bits for the segment_command 'flags' field.
 */

#define COMP_ALG_MASK 0x7
/* carve out 3 bits for an enum i.e. allow for 7 flavors */
#define COMP_ALG_SHIFT 4 /* (bottom 4 bits taken) */

/* zero -> no compression */
typedef enum {
	kCOMP_NONE	= 0,
	kCOMP_LZ4   = 1,	/* 0x100 */
	kCOMP_ZLIB  = 2,	/* 0x205 */
	kCOMP_LZMA  = 3,	/* 0x306 */
	kCOMP_LZFSE = 4,	/* 0x801 */
} compression_flavor_t;

#define COMP_ALG_TYPE(f)	((compression_flavor_t)((f) >> COMP_ALG_SHIFT) & COMP_ALG_MASK)
#define COMP_MAKE_FLAGS(t)	(((t) & COMP_ALG_MASK) << COMP_ALG_SHIFT)

struct proto_coredata_command {
    uint32_t cmd;	/* LC_COREDATA */
    uint32_t cmdsize;
    uint64_t vmaddr;	/* memory address of this segment */
    uint64_t vmsize;	/* memory size of this segment */
    uint64_t fileoff;	/* file offset of this segment */
    uint64_t filesize;	/* amount to map from the file */
    vm_prot_t maxprot;	/* maximum VM protection */
    vm_prot_t prot;	/* current VM protection */
    uint32_t flags;
    uint8_t share_mode;	/* SM_COW etc. */
    uint8_t purgable;	/* VM_PURGABLE_NONVOLATILE etc. */
    uint8_t tag;	/* VM_MEMORY_MALLOC etc. */
    uint8_t extp;	/* external pager */
};

#endif /* _LOADER_ADDITIONS_H */
