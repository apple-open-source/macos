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
unsigned char ns_wait3_bitmap_plane_0[] =
 {
// plane 0
0x07, 0xe0, 0x1b, 0x98, 0x33, 0x9c, 0x59, 0x3e, 0x59, 0x32, 0x8d, 
0x06, 0xe6, 0x7e, 0xfd, 0x00, 0xd2, 0xf0, 0x86, 0x1e, 0x9d, 0x4e, 
0x39, 0x44, 0x31, 0x20, 0x13, 0x38, 0x1f, 0xa0, 0x00, 0x00, };
unsigned char ns_wait3_bitmap_plane_1[] =
 {
// plane 1
0xff, 0xff, 0xf8, 0x7f, 0xe0, 0x7f, 0xe0, 0xfd, 0xe0, 0xfd, 0xf0, 
0xf8, 0xf8, 0x80, 0xfe, 0x00, 0xfc, 0x00, 0xf8, 0xe0, 0xe0, 0xf0, 
0xc0, 0xf9, 0xc0, 0xf9, 0xe0, 0xf3, 0xe0, 0x47, 0xf8, 0x1f, };
struct bitmap ns_wait3_bitmap = {
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
  ns_wait3_bitmap_plane_0,
  ns_wait3_bitmap_plane_1
}
};

#define ns_wait3_bitmap_WIDTH	16
#define ns_wait3_bitmap_HEIGHT	16
