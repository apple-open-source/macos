/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __BOOT_MEMORY_H
#define __BOOT_MEMORY_H

/* Memory addresses used by booter and friends */

#define BASE_SEG          0x2000
#define STACK_SEG         0x3000
#define STACK_OFS         0xFFF0      // stack pointer

#define BOOT2_SEG         BASE_SEG
#define BOOT2_OFS         0x0200      // 512 byte disk sector offset

#define BIOS_ADDR         0x0C00      // BIOS disk I/O buffer
#define BIOS_LEN          0x2400      // 9K

/* These are all "virtual" addresses...
 * which are physical addresses plus MEMBASE.
 */
#define ADDR32(seg, ofs)  (((seg) << 4 ) + (ofs))

#define MEMBASE           0x0

#define BOOTSTRUCT_ADDR   0x011000    // it's slightly smaller
#define BOOTSTRUCT_LEN    0x00F000

#define BASE_ADDR         ADDR32(BASE_SEG, 0)
#define BOOT2_ADDR        ADDR32(BOOT2_SEG, BOOT2_OFS)

#define VIDEO_ADDR        0x0A0000    // unusable space
#define VIDEO_LEN         0x060000

#define KERNEL_ADDR       0x100000    // 15M kernel + drivers
#define KERNEL_LEN        0xF00000

#define ZALLOC_ADDR       0x1000000   // 4M zalloc area
#define ZALLOC_LEN        0x400000

#define LOAD_ADDR         0x01400000  // File download buffer
#define LOAD_LEN          0x800000    // Max file size

#define TFTP_ADDR         LOAD_ADDR   // tftp download buffer
#define TFTP_LEN          LOAD_LEN

#define kLoadAddr         LOAD_ADDR
#define kLoadSize         LOAD_LEN

#define CONVENTIONAL_LEN  0x0A0000    // 640k
#define EXTENDED_ADDR     0x100000    // 1024k

#define ptov(paddr)       ((paddr) - MEMBASE)
#define vtop(vaddr)       ((vaddr) + MEMBASE)

/*
 * Extract segment/offset from a linear address.
 */
#define OFFSET16(addr)    ((addr) - BASE_ADDR)
#define OFFSET(addr)      ((addr) & 0xFFFF)
#define SEGMENT(addr)     (((addr) & 0xF0000) >> 4)

/*
 * We need a minimum of 32MB of system memory.
 */
#define MIN_SYS_MEM_KB  (32 * 1024)

/*
 * The number of descriptor entries in the GDT.
 */
#define NGDTENT   7

/*
 * The total size of the GDT in bytes.
 * Each descriptor entry require 8 bytes.
 */
#define GDTLIMIT  ( NGDTENT * 8 )

#endif /* !__BOOT_MEMORY_H */
