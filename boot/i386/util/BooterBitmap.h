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
/* Two-plane bitmap. */

#import <objc/Object.h>
#import "bitmap.h"

@interface BooterBitmap : Object
{
    char *filename;
    id bitmapImageRep;
    unsigned char *planes[5];
    int bytes_per_plane;
    int bg_color;
    int width, height;
    
    unsigned char *packed_planes[2];
    int plane_len[2];
    int packed;
}
- initFromTiffFile: (char *)filename;
- (BOOL) writeAsCFile: (char *)filename;
- (BOOL) writeAsBinaryFile: (char *)filename;

- (int) width;
- (int) height;
- (int) setWidth: (int)newWidth;
- (int) setHeight: (int)newHeight;

- (BOOL)setTwoBitsPerPixelColorData: (unsigned char *)bits;
- (BOOL)setTwoBitsPerPixelAlphaData: (unsigned char *)bits;
- (unsigned char *)twoBitsPerPixelColorData;
- (unsigned char *)twoBitsPerPixelAlphaData;

- (int) colorDataBytes;
- (int) setColorDataBytes: (int)bpp;

- (int)bgColor;
- (int)setBgColor: (int) color;

- (char *)filename;

- (BOOL)_convertPlanes;
- (BOOL)_allocPlanes;
@end

#define NPLANES 2
#define BITS_PER_PIXEL 2
#define BG_COLOR 2	/* light gray */
