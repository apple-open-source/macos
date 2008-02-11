/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
 *  testcgtext.c
 *  bless
 *
 *  Created by Shantonu Sen on 9/7/06.
 *  Copyright 2006 __MyCompanyName__. All Rights Reserved.
 *
 */

#include <ApplicationServices/ApplicationServices.h>

int main1(int argc, char *argv[]) {
    
    char *path;
    char *str;
    CFStringRef s1;
    CFAttributedStringRef s2;
    CTLineRef ct1;
    
    CGContextRef context;
    CFURLRef url;
    CGRect rect;
    
    if(argc != 3) {
        fprintf(stderr, "Usage: %s file string\n", getprogname());
        exit(1);
    }
    
    path = argv[1];
    str = argv[2];
    
    s1 = CFStringCreateWithCString(kCFAllocatorDefault, str, kCFStringEncodingUTF8);
    s2 = CFAttributedStringCreate(kCFAllocatorDefault,s1,NULL);
    CFRelease(s1);
    
    CFShow(s2);
    
    ct1 = CTLineCreateWithAttributedString(s2);
    CFRelease(s2);
    
    CFShow(ct1);

    url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8 *)path, strlen(path), false);
    CFShow(url);
    
    bzero(&rect, sizeof(rect));
    rect.size.width = 500;
    rect.size.height = 400;
    
    //context = CGPDFContextCreateWithURL(url, &rect, NULL);
    char *data = calloc(100*100, 1);
    CGColorSpaceRef colorSpace = NULL;
    
    colorSpace = CGColorSpaceCreateDeviceGray();
    
    context = CGBitmapContextCreate( data,
    100,
    100,
    8,
    100*1,
    colorSpace,
    kCGImageAlphaNone);
    CGColorSpaceRelease(colorSpace);
    
    CFShow(context);
    
    //CGPDFContextBeginPage(context, NULL);
    
    rect.origin.x = 10;
    rect.origin.y = 15;
    rect.size.width = 10;
    rect.size.height = 10;
    
//    CGContextSetGrayFillColor(context, 1.0, 1.0);
    CGContextFillRect(context, rect);
    
    CFRelease(url);

    CGContextSetTextDrawingMode(context, kCGTextFill);
    CGContextSetTextPosition(context, 4.0, 4.0);
    CGContextSetFontSize(context, 10.0);
 //   CGContextSelectFont(context, "Helvetica", 10.0, kCGEncodingFontSpecific);
    CGContextSetShouldAntialias(context, 1);
    CGContextSetCharacterSpacing(context, 0.5);

    rect = CTLineGetImageBounds(ct1, context);
    printf("[%f,%f] (%f,%f)\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    
    
//    CTLineDraw(ct1, context);
    CGContextShowTextAtPoint(context, 2.0, 2.0, str, strlen(str));    
  
    CGContextFlush(context);
    
//    CGPDFContextEndPage(context);
//    CGPDFContextClose(context);
    
    CFRelease(context);
    CFRelease(ct1);
    
    write(1, data, 100*100);
    
    return 0;
}

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
    
//    CGContextSetTextDrawingMode(context, kCGTextFill);
     //  CGContextSelectFont(context, "Helvetica", 10.0, kCGEncodingFontSpecific);
  //  CGContextSetGrayFillColor(context, 1.0, 1.0);
  //  CGContextSetShouldAntialias(context, 1);
   // CGContextSetCharacterSpacing(context, 0.5);
    
    const CGFloat components[] = {
        1.0, 1.0
    };
//    CGContextSetFillColor(context, components);
    
    
#define USE_CORETEXT 1
#if USE_CORETEXT
    {
        CFStringRef s1;
        CFAttributedStringRef s2;
        CTLineRef ct1;
        CGRect rect;
        CFMutableDictionaryRef dict;
        
        CGColorRef color = CGColorCreate(colorSpace, components);
        
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(dict, kCTForegroundColorAttributeName, color);
        CFRelease(color);
        
        s1 = CFStringCreateWithCString(kCFAllocatorDefault, label, kCFStringEncodingUTF8);
        s2 = CFAttributedStringCreate(kCFAllocatorDefault,s1,dict);
        CFRelease(s1);
        
        ct1 = CTLineCreateWithAttributedString(s2);
        CFRelease(s2);
        
        rect = CTLineGetImageBounds(ct1, context);
        
        CGContextSetTextPosition(context, 2.0, 2.0);
        CFShow(ct1);
        CTLineDraw(ct1, context);
        
//        CGContextFlush(context);
        
        CFRelease(ct1);
        
        if(newwidth) { *newwidth = (int)rect.size.width + 4; }
            printf("[%f,%f] (%f,%f)\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        
    }
#else
    {
        CGPoint pt;
        
        pt = CGContextGetTextPosition(context);
        
        CGContextShowTextAtPoint(context, 2.0, 2.0, label, strlen(label));    
        
        pt = CGContextGetTextPosition(context);
        
        if(newwidth) { *newwidth = (int)pt.x + 2; }
        
    }
#endif
    
    
    
    
    CGColorSpaceRelease(colorSpace);
    CGContextRelease(context);
    
    
    return 0;
    
}


int main(int argc, char *argv[]) {
    char *path;
    char *str;
    int fd;
    unsigned char *bitmapData = NULL;
    
    uint16_t width = 40;
    uint16_t height = 12;
    uint16_t newwidth;
    
    if(argc != 3) {
        fprintf(stderr, "Usage: %s file string\n", getprogname());
        exit(1);
    }
    
    path = argv[1];
    str = argv[2];

    fd = open(path, O_WRONLY|O_TRUNC, 0600);
    if(fd < 0)
        err(1, "open");

    bitmapData = calloc(width*height*4, 1);
    
    makeLabelOfSize(str, bitmapData, width, height, &newwidth);
    
    write(fd, bitmapData, height*width*4);

    
    close(fd);
    
    return 0;
}