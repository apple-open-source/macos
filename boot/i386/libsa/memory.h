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

/* These are all "virtual" addresses...
 * which are physical addresses plus MEMBASE.
 */
 
#define MEMBASE             0x0
#define BASE_SEG            0x0

#define BIOS_ADDR           0x000C00    // BIOS buffer
#define BIOS_LEN            0x002400    // 9k
#define BOOTER_LOAD_ADDR    0x003000    // loaded here for compat.
#define BOOTER_ADDR         0x003000    // start of booter code
#define BOOTER_LEN          0x00B000
#define STACK_ADDR          0x00FFF0
#define BOOTSTRUCT_ADDR     0x011000
#define BOOTSTRUCT_LEN      0x00F000    // it's slightly smaller
#define EISA_CONFIG_ADDR    0x020000
#define EISA_CONFIG_LEN     0x010000
#define RLD_ADDR            0x030000
#define RLD_LEN             0x070000
#define VIDEO_ADDR          0x0A0000    // unusable space
#define VIDEO_LEN           0x060000
#define KERNEL_ADDR         0x100000
#define KERNEL_LEN          0x400000
#define RLD_MEM_ADDR        0x500000
#define RLD_MEM_LEN         0x100000
#define ZALLOC_ADDR         0x600000
#define ZALLOC_LEN          0x100000
#define MODULE_ADDR         0x700000    // to be used for compression..
#define MODULE_LEN          0x080000
#define KSYM_ADDR           0x780000
#define KSYM_LEN            0x080000    // 512k
#define TFTP_ADDR           0x800000    // 8MB
#define TFTP_LEN            0x400000    // 4MB buffer size

/* these are physical values */

#define CONVENTIONAL_LEN    0x0A0000    // 640k
#define EXTENDED_ADDR       0x100000    // 1024k
#define KERNEL_BOOT_ADDR    KERNEL_ADDR /* load at 1Mb */

#define SAIO_TABLE_POINTER      (BOOTER_ADDR + SAIO_TABLE_PTR_OFFSET)
#define SAIO_TABLE_PTR_OFFSET   0x30

#define ptov(paddr) ((paddr) - MEMBASE)
#define vtop(vaddr) ((vaddr) + MEMBASE)

/*
 * Limits to the size of various things...
 */

/* We need a minimum of 12Mb of system memory. */
#define MIN_SYS_MEM_KB  (12 * 1024)

#endif /* !__BOOT_MEMORY_H */
