/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* memory addresses used by booter and friends */

/* these are all "virtual" addresses...
 * which are physical addresses plus MEMBASE.
 */
 
#define MEMBASE		0x0
#define BASE_SEG	0x0

/* these are virtual addresses */

#define BIOS_ADDR		0x00C00		// BIOS buffer
#define BIOS_LEN		0x02400		// 9k
#define BOOTER_LOAD_ADDR	0x03000		// loaded here for compat.
#define BOOTER_ADDR		0x03000		// start of booter code
#define BOOTER_LEN		0x08000		// max of 32k
#define STACK_ADDR		0x0FFF0
#define BOOTSTRUCT_ADDR		0x11000
#define BOOTSTRUCT_LEN		0x0F000		// it's slightly smaller
#define EISA_CONFIG_ADDR	0x20000
#define EISA_CONFIG_LEN		0x10000
#define RLD_ADDR		0x30000
#define RLD_LEN			0x24000		// 140k
#define ZALLOC_ADDR		0x54000
#define ZALLOC_LEN		0x12000		// 72k
#define MODULE_ADDR		0x66000
#define RLD_MEM_ADDR		0x680000
#define RLD_MEM_LEN		0x100000	// 1Mb
#define KSYM_ADDR		0x780000
#define KSYM_LEN		0x080000	// 512k

/* these are physical values */

//#define CONVENTIONAL_LEN	0xA0000		// 640k
#define EXTENDED_ADDR		0x100000	// 1024k
#define	KERNEL_BOOT_ADDR	0x100000	/* load at 1Mb */

#define KERNEL_SYMBOL_OFFSET	0x100000	/* load at top - 1Mb */

#define SAIO_TABLE_POINTER	(BOOTER_ADDR + SAIO_TABLE_PTR_OFFSET)
#define SAIO_TABLE_PTR_OFFSET	0x30

#define ptov(paddr)	((paddr) - MEMBASE)
#define vtop(vaddr)	((vaddr) + MEMBASE)

#define MIN_EXT_MEM_KB	(7 * 1024)	/* 8 Mb minimum total */
