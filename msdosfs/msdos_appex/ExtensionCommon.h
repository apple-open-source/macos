/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#include <sys/types.h>

#define ROOT_DIR_FILENUM (1)

/*
 * We use the 64bit cookie in the following way:
 * Lower 32 bits = offset in dir cluster.
 * Upper 32 bits = index of the desired dir entry in dir (index = 0 --> first file in dir).
 */
#define OFFSET_FROM_COOKIE(cookie)                      (cookie & 0xFFFFFFFFU)
#define INDEX_FROM_COOKIE(cookie)                       ((cookie >> 32) & 0xFFFFFFFFU)
#define COOKIE_FROM_OFFSET_AND_INDEX(offset, index)     (((index & 0x00000000FFFFFFFFLLU) << 32) + (offset & 0xFFFFFFFFU))
#define DOT_COOKIE                                      (0)
#define DOT_SIZE                                        (2)
#define DOT_DOT_COOKIE                                  (DOT_SIZE)
#define SYNTHESIZE_ROOT_DOTS_SIZE                       (2*DOT_SIZE)
#define NUM_DOT_ENTRIES                                 (2)

/* Dir enumeration error codes */
#define READDIR_BAD_COOKIE          (-1002)

/*
 * The maximum file size on FAT is 4GB-1, which is the largest value that fits
 * in an unsigned 32-bit integer.
 */
#define    DOS_FILESIZE_MAX    0xffffffff

#define MSDOS_VALID_BSD_FLAGS_MASK (SF_ARCHIVED | SF_IMMUTABLE | UF_IMMUTABLE | UF_HIDDEN)

/* Zero-length file ID */
#define INITIAL_ZERO_LENGTH_FILEID (0xFFFFFFFFFFFFFFFF)
#define WRAPAROUND_ZERO_LENGTH_FILEID (0xFFFFFFFF00000000)

#define MAX_META_BLOCK_RANGES (8) // -> fskit defines, leave original name

#define FAT_MAX_FILENAME_UTF8  (WIN_MAXLEN * 3 + 1)

#define DOS_SYMLINK_LENGTH_LENGTH    (4)
#define DOS_SYMLINK_MAGIC_LENGTH    (5)

#ifndef SYMLINK_LINK_MAX
static const char symlink_magic[5] = "XSym\n";

#define SYMLINK_LINK_MAX 1024

struct symlink {
    char magic[5];        /* == symlink_magic */
    char length[4];        /* four decimal digits */
    char newline1;        /* '\n' */
    char md5[32];        /* MD5 hex digest of "length" bytes of "link" field */
    char newline2;        /* '\n' */
    char link[SYMLINK_LINK_MAX]; /* "length" bytes, padded by '\n' and spaces */
};
#endif

#ifndef UNISTR255
#define UNISTR255
struct unistr255 {
    uint16_t length;
    uint16_t chars[255];
};
#endif

/* Preallocation / KOIO definitions */
#define PREALLOCATE_ALLOCATEALL         0x00000002
#define PREALLOCATE_ALLOCATECONTIG      0x00000004
#define PREALLOCATE_ALLOCATEFROVOL      0x00000020

typedef struct FSKit_KOIO_Extent_t { // leave here, Tal to remove soon
    uint32_t reserved:24,
             type:8;
    uint32_t length;
    uint64_t offset;
} FSKit_KOIO_Extent;

#define PREALLOCATE_MAX_EXTENTS (8)

typedef struct preallocate_args_s { // TODO: handle when adding proper preallocate support
    off_t offset;
    off_t length;
    uint32_t flags;
    off_t bytesAllocated;
    FSKit_KOIO_Extent extentsList[PREALLOCATE_MAX_EXTENTS];
    int extentsCount;
} preAllocateArgs;
