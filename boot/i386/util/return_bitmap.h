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
unsigned char return_bitmap_plane_0[] =
 {
// plane 0
0x00, 0x02, 0x00, 0x02, 0x0c, 0x02, 0x14, 0x02, 0x20, 0x02, 0x40, 
0x02, 0x00, 0x02, 0x07, 0xfe, 0x04, 0x00, 0x04, 0x00, };
unsigned char return_bitmap_plane_1[] =
 {
// plane 1
0xff, 0xc0, 0xf3, 0xde, 0xe3, 0xde, 0xc8, 0x1e, 0x9f, 0xfe, 0x3f, 
0xfe, 0xbf, 0xfe, 0xdf, 0xfe, 0xef, 0xfe, 0xf7, 0xfe, };
struct bitmap return_bitmap = {
0,	// packed
20,	// bytes_per_plane
2,	// bytes_per_row
1,	// bits per pixel
15,	// width
10,	// height
{
  20,
  20,
},
{
  return_bitmap_plane_0,
  return_bitmap_plane_1
}
};

#define return_bitmap_WIDTH	15
#define return_bitmap_HEIGHT	10
