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
unsigned char ns_wait1_bitmap_plane_0[] =
 {
// plane 0
0x07, 0xe0, 0x19, 0x80, 0x31, 0x98, 0x79, 0x3e, 0x5d, 0x72, 0x85, 
0x66, 0xeb, 0x5e, 0xff, 0xa0, 0xd1, 0xf4, 0x86, 0xce, 0x9d, 0x62, 
0x39, 0x30, 0x31, 0xb8, 0x13, 0x38, 0x1f, 0xa0, 0x00, 0x00, };
unsigned char ns_wait1_bitmap_plane_1[] =
 {
// plane 1
0xff, 0xff, 0xfe, 0x1f, 0xfe, 0x07, 0xfe, 0x01, 0xfe, 0x0d, 0xfe, 
0x1e, 0x96, 0x3e, 0x80, 0x7e, 0x80, 0x0a, 0x81, 0x00, 0x83, 0x80, 
0xc7, 0xc1, 0xcf, 0xc1, 0xef, 0xc3, 0xe7, 0xc7, 0xf8, 0x1f, };
struct bitmap ns_wait1_bitmap = {
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
  ns_wait1_bitmap_plane_0,
  ns_wait1_bitmap_plane_1
}
};

#define ns_wait1_bitmap_WIDTH	16
#define ns_wait1_bitmap_HEIGHT	16
