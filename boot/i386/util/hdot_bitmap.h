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
unsigned char hdot_bitmap_plane_0[] =
 {
// plane 0
0xe4, 0xd4, 0xa8, 0xd4, 0x2c, 0xdc, };
unsigned char hdot_bitmap_plane_1[] =
 {
// plane 1
0x8c, 0x00, 0x00, 0x00, 0x80, 0x84, };
struct bitmap hdot_bitmap = {
0,	// packed
6,	// bytes_per_plane
1,	// bytes_per_row
1,	// bits per pixel
6,	// width
6,	// height
{
  6,
  6,
},
{
  hdot_bitmap_plane_0,
  hdot_bitmap_plane_1
}
};

#define hdot_bitmap_WIDTH	6
#define hdot_bitmap_HEIGHT	6
