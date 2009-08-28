/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  Copyright (c) 2002-2007 Apple Inc. All Rights Reserved.
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

static int makeLabelOfSize(const char *label, unsigned char *bitmapData,
        uint16_t width, uint16_t height, uint16_t *newwidth);

static int refitToWidth(unsigned char *bitmapData,
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
    unsigned char *bitmapData;
    
    contextprintf(context, kBLLogLevelError,
    "CoreGraphics is not available for rendering\n");
    return 1;
	
    bitmapData = (unsigned char *)malloc(width*height+5);
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
	
    contextprintf(context, kBLLogLevelVerbose, "Refitting to width %d\n", newwidth);

    
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

#undef USE_COREGRAPHICS
#define USE_COREGRAPHICS 0

#if USE_COREGRAPHICS
#include <ApplicationServices/ApplicationServices.h>

static int makeLabelOfSize(const char *label, unsigned char *bitmapData,
uint16_t width, uint16_t height, uint16_t *newwidth) {
    
    int bitmapByteCount;
    int bitmapBytesPerRow;
    
    CGContextRef    context = NULL;
    CGColorSpaceRef colorSpace = NULL;
    
    
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
    
    
#if USE_CORETEXT
    {
        CFStringRef s1;
        CFAttributedStringRef s2;
        CTLineRef ct1;
        CGRect rect;
        CFMutableDictionaryRef dict;
        CGColorRef color;
        CTFontRef fontRef;
        
        // white text on black background, for OF/EFI bitmap
        const CGFloat components[] = {
            (CGFloat)1.0, (CGFloat)1.0
        };
        
        /* set to white background for testing
        CGContextSetGrayFillColor(context, 1.0, 1.0);
        CGContextFillRect(context,CGRectInfinite);
        */
        
        color = CGColorCreate(colorSpace, components);
        if(color == NULL) return 1;
        
        fontRef = CTFontCreateWithName(CFSTR("Helvetica"), (CGFloat)10.0, NULL);
        if(fontRef == NULL) return 1;
        
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(dict, kCTForegroundColorAttributeName, color);
        CFDictionarySetValue(dict, kCTFontAttributeName, fontRef);
        CFRelease(color);
        CFRelease(fontRef);
        
        s1 = CFStringCreateWithCString(kCFAllocatorDefault, label, kCFStringEncodingUTF8);
        s2 = CFAttributedStringCreate(kCFAllocatorDefault,s1,dict);
        CFRelease(s1);
        CFRelease(dict);
        
        ct1 = CTLineCreateWithAttributedString(s2);
        CFRelease(s2);
        if(ct1 == NULL) return 2;
        
        rect = CTLineGetImageBounds(ct1, context);
        
        CGContextSetTextPosition(context, (CGFloat)2.0, (CGFloat)2.0);
        
        CTLineDraw(ct1, context);
        
        CGContextFlush(context);
        
        CFRelease(ct1);
        
        if(newwidth) { *newwidth = (int)rect.size.width + 4; }
 //           printf("[%f,%f] (%f,%f)\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        
    }
#else
    {
        CGPoint pt;
        
        CGContextSetTextDrawingMode(context, kCGTextFill);
        CGContextSelectFont(context, "Helvetica", 10.0, kCGEncodingMacRoman);
        CGContextSetGrayFillColor(context, 1.0, 1.0);
        CGContextSetShouldAntialias(context, 1);
        CGContextSetCharacterSpacing(context, 0.5);
        
        pt = CGContextGetTextPosition(context);
        
        CGContextShowTextAtPoint(context, 2.0, 2.0, label, strlen(label));    
        
        pt = CGContextGetTextPosition(context);
        
        if(newwidth) { *newwidth = (int)pt.x + 2; }
        
    }
#endif

    
#if 0
//    CFShow(CGImageDestinationCopyTypeIdentifiers());
    CGDataProviderRef dataProvider = CGDataProviderCreateWithData(NULL, bitmapData, height*width, NULL);
    CGImageRef imageRef = CGImageCreate(width,height,8,8,bitmapBytesPerRow,colorSpace,0, dataProvider, NULL, 0, 0);
    CFURLRef output = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8 *)"foo.tiff", strlen("foo.tiff"), 0);
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(output, CFSTR("public.tiff"), 1, NULL);
    
    CGImageDestinationAddImage(dest, imageRef, NULL);

    CGImageDestinationFinalize(dest);
    
    CFRelease(dest);
    CFRelease(output);
    CFRelease(imageRef);
    CFRelease(dataProvider);
    //    write(1, bitmapData, height*width);
    
#endif
    
    CGColorSpaceRelease(colorSpace);
    CGContextRelease(context);

    
    return 0;
    
}

#else // !USE_COREGRAPHICS
static int makeLabelOfSize(const char *label, unsigned char *bitmapData,
						   uint16_t width, uint16_t height, uint16_t *newwidth) {

	// just make a blank label
	*newwidth = 10;
	return 0;
}
#endif // !USE_COREGRAPHICS

/*
 * data is of the form:
 *  111111000111111000111111000 ->
 *  111111111111111111
 */

static int refitToWidth(unsigned char *bitmapData,
        uint16_t width, uint16_t height, uint16_t newwidth)
{
  uint16_t row;
  for(row=0; row < height; row++) {
    bcopy(&bitmapData[row*width], &bitmapData[row*newwidth], newwidth);
  }

  return 0;
}
