/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  BLGenerateOFLabel.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Sat Feb 23 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGenerateOFLabel.c,v 1.5 2002/05/03 04:23:55 ssen Exp $
 *
 *  $Log: BLGenerateOFLabel.c,v $
 *  Revision 1.5  2002/05/03 04:23:55  ssen
 *  Consolidate APIs, and update bless to use it
 *
 *  Revision 1.4  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.3  2002/03/05 01:47:53  ssen
 *  Add CG compat files and dynamic loading
 *
 *  Revision 1.2  2002/03/04 22:25:05  ssen
 *  implement CLUT for antialiasing
 *
 *  Revision 1.1  2002/02/24 11:30:52  ssen
 *  Add OF label support
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>

#include "compatCoreGraphics.h"
#include "bless.h"

static const unsigned char clut[] =
  {
    0x00, /* 0x00 0x00 0x00 white */
    0xF6, /* 0x11 0x11 0x11 */
    0xF7, /* 0x22 0x22 0x22 */

    0x2A, /* 0x33 = 1*6^2 + 1*6 + 1 = 43 colors */

    0xF8, /* 0x44 */
    0xF9, /* 0x55 */

    0x55, /* 0x66 = 2*(36 + 6 + 1) = 86 colors */

    0xFA, /* 0x77 */
    0xFB, /* 0x88 */

    0x80, /* 0x99 = (3*43) = 129 colors*/

    0xFC, /* 0xAA */
    0xFD, /* 0xBB */

    0xAB, /* 0xCC = 4*43 = 172 colors */

    0xFE, /* 0xDD */
    0xFF, /* 0xEE */

    0xD6, /* 0xFF = 5*43 = 215 */
  };

static int makeLabelOfSize(char *label, char *bitmapData,
        u_int16_t width, u_int16_t height, u_int16_t *newwidth);

static int refitToWidth(char *bitmapData,
        u_int16_t width, u_int16_t height, u_int16_t newwidth);

int BLGenerateOFLabel(BLContext context,
                    unsigned char label[],
                    void ** /* CFDataRef* */ data) {
                    
                    
        u_int16_t width = 300;
        u_int16_t height = 12;
        u_int16_t newwidth;
        int err;
        int i;
        CFDataRef bits = NULL;
        unsigned char *bitmapData;
        
        if(!isCoreGraphicsAvailable()) {
                contextprintf(context, kBLLogLevelError,
                    "CoreGraphics is not available for rendering\n");
                return 1;        
        }
        
        bitmapData = malloc(width*height+5);
        if(!bitmapData) {
                contextprintf(context, kBLLogLevelError,
                    "Could not alloc CoreGraphics backing store\n");
                return 1;
        }
        bzero(bitmapData, width*height+5);

        err = makeLabelOfSize(label, bitmapData+5, width, height, &newwidth);
	if(err) {
	  free(bitmapData);
	  *data = NULL;
	  return 2;
	}

	err = refitToWidth(bitmapData+5, width, height, newwidth);
	if(err) {
	  free(bitmapData);
	  *data = NULL;
	  return 3;
	}

	bitmapData = realloc(bitmapData, newwidth*height+5);
	if(NULL == bitmapData) {
                contextprintf(context, kBLLogLevelError,
                    "Could not realloc to shrink CoreGraphics backing store\n");
		
                return 4;
	}

        bitmapData[0] = 1;
        *(u_int16_t *)&bitmapData[1] = newwidth;
        *(u_int16_t *)&bitmapData[3] = height;
        
        for(i=5; i < newwidth*height+5; i++) {
            bitmapData[i] = clut[bitmapData[i] >> 4];
        }

	//	bits = CFDataCreate(kCFAllocatorDefault, bitmapData, newwidth*height+5);
	bits = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bitmapData, newwidth*height+5, kCFAllocatorMalloc);
	//        free(bitmapData);
        
	if(bits == NULL) {
                contextprintf(context, kBLLogLevelError,
                    "Could not create CFDataRef\n");
		return 6;
	}

        *data = (void *)bits;
        
        return 0;
}


static int makeLabelOfSize(char *label, char *bitmapData,
        u_int16_t width, u_int16_t height, u_int16_t *newwidth) {

        int bitmapByteCount;
        int bitmapBytesPerRow;
        
        BLCGContextRef    context = NULL;
        BLCGColorSpaceRef colorSpace = NULL;
        BLCGPoint pt;


        bitmapBytesPerRow = width*1;
        bitmapByteCount = bitmapBytesPerRow * height;

       colorSpace = _BLCGColorSpaceCreateDeviceGray();


        context = _BLCGBitmapContextCreate( bitmapData,
                                        width,
                                        height,
                                        8,
                                        bitmapBytesPerRow,
                                        colorSpace,
                                     kCGImageAlphaNone);

        if(context == NULL) {
                fprintf(stderr, "Could not init CoreGraphics context\n");
                return 1;
        }

    _BLCGContextSetTextDrawingMode(context, kCGTextFill);
    _BLCGContextSelectFont(context, "Helvetica", 10.0, kCGEncodingMacRoman);
    _BLCGContextSetGrayFillColor(context, 1.0, 1.0);
    _BLCGContextSetShouldAntialias(context, 1);
    _BLCGContextSetCharacterSpacing(context, 0.5);
        
    pt = _BLCGContextGetTextPosition(context);

    _BLCGContextShowTextAtPoint(context, 2.0, 2.0, label, strlen(label));

    pt = _BLCGContextGetTextPosition(context);

    if(newwidth) { *newwidth = (int)pt.x + 2; }

    _BLCGColorSpaceRelease(colorSpace);
    _BLCGContextRelease(context);

    return 0;

}

/*
 * data is of the form:
 *  111111000111111000111111000 ->
 *  111111111111111111
 */

static int refitToWidth(char *bitmapData,
        u_int16_t width, u_int16_t height, u_int16_t newwidth)
{
  u_int16_t row;
  for(row=0; row < height; row++) {
    bcopy(&bitmapData[row*width], &bitmapData[row*newwidth], newwidth);
  }

  return 0;
}

