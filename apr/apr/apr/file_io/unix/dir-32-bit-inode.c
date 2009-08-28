/*
 * We assumes that 64-bit inodes are on by default, so we build the 32-bit
 * inode (undecorated) variants here (unless the platform only supports
 * 64-bit inodes, then which we do nothing).
 */

#undef __DARWIN_64_BIT_INO_T
#define __DARWIN_64_BIT_INO_T 0
#include <sys/cdefs.h>

#if __DARWIN_ONLY_64_BIT_INO_T == 0
#define __DARWIN_APR_BUILDING_32_BIT_INODE
#include "dir.c"
#endif /* __DARWIN_ONLY_64_BIT_INO_T == 0 */
