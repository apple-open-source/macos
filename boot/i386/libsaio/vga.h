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
/*
 * VGA Graphics Controller Port
 */

#ifndef __LIBSAIO_VGA_H
#define __LIBSAIO_VGA_H

#define VGA_BUF_ADDR		    0xA0000
#define	VGA_BUF_LENGTH		    0x10000

#define VGA_GFX_INDEX           0x3CE
#define VGA_GFX_DATA            0x3CF
#define	VGA_GFX_RD_MAP_SEL	    0x4
#define	VGA_GFX_BIT_MASK        0x8
#define VGA_NUM_GFX_REGS        6   // number of graphics controller
                                    // registers to preserve
#define VGA_SEQ_INDEX           0x3C4
#define VGA_SEQ_DATA		    0x3C5
#define	VGA_SEQ_MAP_MASK	    0x2
#define VGA_NUM_SEQ_REGS	    5   // number of sequencer
                                    // registers to preserve
#define VGA_AC_ADDR             0x3C0
#define VGA_NUM_AC_REGS         0x14

#define VGA_CRTC_INDEX          0x3D4
#define VGA_CRTC_DATA		    0x3D5

#define	VGA_READ_ATTR_INDEX	    0x3C1
#define VGA_READ_ATTR_DATA	    0x3C1
#define VGA_WRITE_ATTR_INDEX    0x3C0
#define VGA_WRITE_ATTR_DATA	    0x3C0

#define VGA_PALETTE_WRITE	    0x3C8
#define VGA_PALETTE_READ	    0x3C7
#define VGA_PALETTE_DATA	    0x3C9

#define	VGA_INPUT_STATUS_1	    0x3DA
#define	VGA_READ_MISC_PORT	    0x3CC
#define	VGA_WRITE_MISC_PORT	    0x3C2

#define	VGA_READ_FEATURE_PORT	0x3CA
#define	VGA_WRITE_FEATURE_PORT	0x3DA

#endif /* !__LIBSAIO_VGA_H */
