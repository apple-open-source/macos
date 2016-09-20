/*
 * Copyright (c) 2015 Apple Inc.  All rights reserved.
 */

#include <mach-o/loader.h>

#ifndef _LOADER_ADDITIONS_H
#define _LOADER_ADDITIONS_H

/*
 * Something like this should end up in <mach-o/loader.h>
 */
#define proto_LC_COREINFO	0x40	/* unofficial value!! */

#define proto_CORETYPE_KERNEL	1
#define proto_CORETYPE_USER	2
#define proto_CORETYPE_IBOOT	3

struct proto_coreinfo_command {
    uint32_t cmd;		/* LC_COREINFO */
    uint32_t cmdsize;	/* total size of this command */
    uint32_t version;	/* currently 1 */
    /*
     * 'type' determines the content of the corefile; interpretation
     * of the address and uuid fields are specific to the type.
     */
    uint32_t type;		/* CORETYPE_KERNEL, CORETYPE_USER etc. */
    uint64_t address;   /* load address of "main binary" */
    uint8_t uuid[16];   /* uuid of "main binary" */
    uint64_t dyninfo;   /* dynamic modules info */
};

/*
 * These are flag bits for the segment_command 'flags' field.
 */

#define proto_SG_COMP_ALG_MASK 0x7
/* carve out 3 bits for an enum i.e. allow for 7 flavors */
#define proto_SG_COMP_ALG_SHIFT 4 /* (bottom 4 bits taken) */

/* zero -> no compression */
#define proto_SG_COMP_LZ4   1   /* 0x100 */
#define proto_SG_COMP_ZLIB  2   /* 0x205 */
#define proto_SG_COMP_LZMA  3   /* 0x306 */
#define proto_SG_COMP_LZFSE 4   /* 0x801 */

#define proto_SG_COMP_ALG_TYPE(flags)   (((flags) >> proto_SG_COMP_ALG_SHIFT) & proto_SG_COMP_ALG_MASK)
#define proto_SG_COMP_MAKE_FLAGS(type)  (((type) & proto_SG_COMP_ALG_MASK) << proto_SG_COMP_ALG_SHIFT)

#define proto_LC_FILEREF 0x41 /* unofficial value! */

struct proto_fileref_command {
    uint32_t cmd; /* LC_FILEREF */
    uint32_t cmdsize;
    union lc_str filename;  /* filename these bits come from */
    uint8_t uuid[16];       /* uuid if known */
    uint64_t vmaddr; /* memory address of this segment */
    uint64_t vmsize; /* memory size of this segment */
    uint64_t fileoff; /* file offset of this segment */
    uint64_t filesize; /* amount to map from the file */
    vm_prot_t maxprot; /* maximum VM protection */
    vm_prot_t initprot; /* initial VM protection */
};

#endif /* _LOADER_ADDITIONS_H */
