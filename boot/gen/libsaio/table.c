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
/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * 			INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *	This software is supplied under the terms of a license  agreement or 
 *	nondisclosure agreement with Intel Corporation and may not be copied 
 *	nor disclosed except in accordance with the terms of that agreement.
 *
 *	Copyright 1988, 1989 Intel Corporation
 */

/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */
 
#include "memory.h"

#define NGDTENT		6
#define GDTLIMIT	48	/* NGDTENT * 8 */

/*  Segment Descriptor
 *
 * 31          24         19   16                 7           0
 * ------------------------------------------------------------
 * |             | |B| |A|       | |   |1|0|E|W|A|            |
 * | BASE 31..24 |G|/|0|V| LIMIT |P|DPL|  TYPE   | BASE 23:16 |
 * |             | |D| |L| 19..16| |   |1|1|C|R|A|            |
 * ------------------------------------------------------------
 * |                             |                            |
 * |        BASE 15..0           |       LIMIT 15..0          |
 * |                             |                            |
 * ------------------------------------------------------------
 */

struct seg_desc {
	unsigned short	limit_15_0;
	unsigned short	base_15_0;
	unsigned char	base_23_16;
	unsigned char	bit_15_8;
	unsigned char	bit_23_16;
	unsigned char	base_31_24;
	};


struct seg_desc	Gdt[NGDTENT] = {
    {0x0,    0x0,     0x0, 0x0,  0x0,  0x0},	/* 0x0 : null */
    // byte granularity, 1Mb limit, MEMBASE offset
    {0xFFFF, MEMBASE, 0x0, 0x9E, 0x4F, 0x0},	/* 0x8 : boot code */
    // dword granularity, 2Gb limit, MEMBASE offset
    {0xFFFF, MEMBASE, 0x0, 0x92, 0xCF, 0x0},	/* 0x10 : boot data */
    {0xFFFF, MEMBASE, 0x0, 0x9E, 0xF,  0x0},	/* 0x18 : boot code, 16 bits */
    {0xFFFF, 0x0,     0x0, 0x92, 0xCF, 0x0},	/* 0x20 : init data */
    {0xFFFF, 0x0,     0x0, 0x9E, 0xCF, 0x0}	/* 0x28 : init code */
};

