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
unsigned char ns_wait2_bitmap_plane_0[] =
 {
// plane 0
0x07, 0xe0, 0x1b, 0x80, 0x23, 0x18, 0x71, 0x32, 0x59, 0x62, 0x8d, 
0x4e, 0xc2, 0x1c, 0xf9, 0x60, 0xd6, 0x7c, 0x83, 0x4e, 0x9d, 0x22, 
0x39, 0x30, 0x31, 0x38, 0x13, 0x98, 0x07, 0x80, 0x00, 0x00, };
unsigned char ns_wait2_bitmap_plane_1[] =
 {
// plane 1
0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xcf, 0xc1, 0xc7, 0x81, 0x83, 
0x80, 0x81, 0x82, 0x80, 0x1e, 0xa8, 0x3e, 0xfe, 0x3e, 0xfe, 0x1c, 
0xfe, 0x0d, 0xfe, 0x05, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x1f, };
struct bitmap ns_wait2_bitmap = {
0,	// packed
32,	// bytes_per_plane
2,	// bytes_per_row
1,	// bits per pixel
16,	// width
16,	// height
{
  32,
  32,
},
{
  ns_wait2_bitmap_plane_0,
  ns_wait2_bitmap_plane_1
}
};

#define ns_wait2_bitmap_WIDTH	16
#define ns_wait2_bitmap_HEIGHT	16
