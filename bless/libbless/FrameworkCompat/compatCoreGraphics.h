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
 *  compatCoreGraphics.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Mar 4 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: compatCoreGraphics.h,v 1.1 2002/03/05 01:47:53 ssen Exp $
 *
 *  $Log: compatCoreGraphics.h,v $
 *  Revision 1.1  2002/03/05 01:47:53  ssen
 *  Add CG compat files and dynamic loading
 *
 *
 */

#include <sys/types.h>
#include <stdbool.h>

typedef void *		BLCGContextRef;
typedef void *		BLCGColorSpaceRef;

struct BLCGPoint {
    float x;
    float y;
};
typedef struct BLCGPoint BLCGPoint;

enum BLCGImageAlphaInfo {
    kCGImageAlphaNone,
};
typedef enum BLCGImageAlphaInfo BLCGImageAlphaInfo;

enum BLCGTextDrawingMode {
    kCGTextFill,
    kCGTextStroke,
    kCGTextFillStroke,
    kCGTextInvisible,
    kCGTextFillClip,
    kCGTextStrokeClip,
    kCGTextFillStrokeClip,
    kCGTextClip
};
typedef enum BLCGTextDrawingMode BLCGTextDrawingMode;

enum BLCGTextEncoding {
    kCGEncodingFontSpecific,
    kCGEncodingMacRoman
};
typedef enum BLCGTextEncoding BLCGTextEncoding;

BLCGColorSpaceRef _BLCGColorSpaceCreateDeviceGray(void);
BLCGContextRef _BLCGBitmapContextCreate(void *A, size_t B, size_t C, size_t D,
        size_t E, BLCGColorSpaceRef F, BLCGImageAlphaInfo G);
void _BLCGContextSetTextDrawingMode(BLCGContextRef A, BLCGTextDrawingMode B);
void _BLCGContextSelectFont(BLCGContextRef A, const char *B, float C,
        BLCGTextEncoding D);
void _BLCGContextSetGrayFillColor(BLCGContextRef A, float B, float C);
void _BLCGContextSetShouldAntialias(BLCGContextRef A, bool B);
void _BLCGContextSetCharacterSpacing(BLCGContextRef A, float B);
void _BLCGContextShowTextAtPoint(BLCGContextRef A, float B, float C,
        const char *D, size_t E);
BLCGPoint _BLCGContextGetTextPosition(BLCGContextRef A);
void _BLCGColorSpaceRelease(BLCGColorSpaceRef A);
void _BLCGContextRelease(BLCGContextRef A);

int isCoreGraphicsAvailable();