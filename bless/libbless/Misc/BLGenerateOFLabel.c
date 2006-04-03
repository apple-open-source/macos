/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 *  BLGenerateOFLabel.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Sat Feb 23 2002.
 *  Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>

#include "bless.h"
#include "bless_private.h"

static const char clut[] =
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

static int makeLabelOfSize(const char *label, char *bitmapData,
        uint16_t width, uint16_t height, uint16_t *newwidth);

static int refitToWidth(char *bitmapData,
        uint16_t width, uint16_t height, uint16_t newwidth);

int BLGenerateOFLabel(BLContextPtr context,
                    const char label[],
                    CFDataRef* data) {
                    
                    
        uint16_t width = 340;
        uint16_t height = 12;
        uint16_t newwidth;
        int err;
        int i;
        CFDataRef bits = NULL;
        char *bitmapData;

        contextprintf(context, kBLLogLevelError,
		      "CoreGraphics is not available for rendering\n");
        return 1;
	
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

	// cap at 300 pixels wide.
	if(newwidth > width) newwidth = width;
	
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
        *(uint16_t *)&bitmapData[1] = CFSwapInt16HostToBig(newwidth);
        *(uint16_t *)&bitmapData[3] = CFSwapInt16HostToBig(height);
        
        for(i=5; i < newwidth*height+5; i++) {
            bitmapData[i] = clut[bitmapData[i] >> 4];
        }

	//	bits = CFDataCreate(kCFAllocatorDefault, bitmapData, newwidth*height+5);
	bits = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *)bitmapData, newwidth*height+5, kCFAllocatorMalloc);
	//        free(bitmapData);
        
	if(bits == NULL) {
                contextprintf(context, kBLLogLevelError,
                    "Could not create CFDataRef\n");
		return 6;
	}

        *data = (void *)bits;
        
        return 0;
}

#define USE_CG 0



#if USE_CG
#include <ApplicationServices/ApplicationServices.h>

static int makeLabelOfSize(const char *label, char *bitmapData,
        uint16_t width, uint16_t height, uint16_t *newwidth) {

        int bitmapByteCount;
        int bitmapBytesPerRow;
        
        CGContextRef    context = NULL;
        CGColorSpaceRef colorSpace = NULL;
        CGPoint pt;


        bitmapBytesPerRow = width*1;
        bitmapByteCount = bitmapBytesPerRow * height;

       colorSpace = CGColorSpaceCreateDeviceGray();


        context = CGBitmapContextCreate( bitmapData,
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

    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextSelectFont(context, "Helvetica", 10.0, kCGEncodingMacRoman);
    CGContextSetGrayFillColor(context, 1.0, 1.0);
    CGContextSetShouldAntialias(context, 1);
    CGContextSetCharacterSpacing(context, 0.5);
        
    pt = CGContextGetTextPosition(context);

    CGContextShowTextAtPoint(context, 2.0, 2.0, label, strlen(label));

    pt = CGContextGetTextPosition(context);

    if(newwidth) { *newwidth = (int)pt.x + 2; }

    CGColorSpaceRelease(colorSpace);
    CGContextRelease(context);

    return 0;

}

#else
static int makeLabelOfSize(const char *label, char *bitmapData,
						   uint16_t width, uint16_t height, uint16_t *newwidth) {
	return 1;
}
#endif // USE_CG

/*
 * data is of the form:
 *  111111000111111000111111000 ->
 *  111111111111111111
 */

static int refitToWidth(char *bitmapData,
        uint16_t width, uint16_t height, uint16_t newwidth)
{
  uint16_t row;
  for(row=0; row < height; row++) {
    bcopy(&bitmapData[row*width], &bitmapData[row*newwidth], newwidth);
  }

  return 0;
}
