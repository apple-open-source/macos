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
 *  $Id: compatCoreGraphics.c,v 1.1 2002/03/05 01:47:53 ssen Exp $
 *
 *  $Log: compatCoreGraphics.c,v $
 *  Revision 1.1  2002/03/05 01:47:53  ssen
 *  Add CG compat files and dynamic loading
 *
 *
 */
 
 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <mach-o/dyld.h>

#include "compat.h"
#include "compatCoreGraphics.h"
#include "bless.h"

#define CGPath "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/CoreGraphics.framework/CoreGraphics"

static void * loadCoreGraphics() {

    static const void *image = NULL;

    if (NULL == image) {
        struct stat statbuf;
        const char *suffix = getenv("DYLD_IMAGE_SUFFIX");
        char path[MAXPATHLEN];
        strcpy(path, CGPath);
        if (suffix) strcat(path, suffix);
        if (0 <= stat(path, &statbuf)) {
#ifdef _BUILD_CHEETAH
            NSAddLibrary(path);
            image = "CoreGraphics";
        } else if (0 <= stat(CGPath, &statbuf)) {
            NSAddLibrary(CGPath);
            image = "CoreGraphics";
#else
            image = NSAddImage(path, NSADDIMAGE_OPTION_NONE);
        } else if (0 <= stat(CGPath, &statbuf)) {
            image = NSAddImage(CGPath, NSADDIMAGE_OPTION_NONE);
#endif
        } else {
            image = NULL;
        }
    }
    return (void *)image;
}

BLCGColorSpaceRef _BLCGColorSpaceCreateDeviceGray(void) {

    static BLCGColorSpaceRef (*dyfunc)() = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGColorSpaceCreateDeviceGray");
    }
    if (dyfunc) {
        return dyfunc();
    } else {
        return NULL;
    }
}

BLCGContextRef _BLCGBitmapContextCreate(void *A, size_t B, size_t C,
                    size_t D, size_t E,
                    BLCGColorSpaceRef F, BLCGImageAlphaInfo G) {
    static BLCGContextRef (*dyfunc)(void *, size_t, size_t,
                    size_t, size_t,
                    BLCGColorSpaceRef,
                    BLCGImageAlphaInfo) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGBitmapContextCreate");
    }
    if (dyfunc) {
        return dyfunc(A, B, C, D, E, F, G);
    } else {
        return NULL;
    }
}

void _BLCGContextSetTextDrawingMode(BLCGContextRef A, BLCGTextDrawingMode B) {
    static void (*dyfunc)(BLCGContextRef, BLCGTextDrawingMode) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextSetTextDrawingMode");
    }
    if (dyfunc) {
        return dyfunc(A, B);
    } else {
        return;
    }
}

void _BLCGContextSelectFont(BLCGContextRef A, const char *B, float C,
        BLCGTextEncoding D) {
    static void (*dyfunc)(BLCGContextRef, const char *, float,
        BLCGTextEncoding) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextSelectFont");
    }
    if (dyfunc) {
        return dyfunc(A, B, C, D);
    } else {
        return;
    }        
}

void _BLCGContextSetGrayFillColor(BLCGContextRef A, float B, float C) {
    static void (*dyfunc)(BLCGContextRef, float, float) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextSetGrayFillColor");
    }
    if (dyfunc) {
        return dyfunc(A, B, C);
    } else {
        return;
    }        
}

void _BLCGContextSetShouldAntialias(BLCGContextRef A, bool B) {
    static void (*dyfunc)(BLCGContextRef, bool) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextSetShouldAntialias");
    }
    if (dyfunc) {
        return dyfunc(A, B);
    } else {
        return;
    }        

}
void _BLCGContextSetCharacterSpacing(BLCGContextRef A, float B) {
    static void (*dyfunc)(BLCGContextRef, float) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextSetCharacterSpacing");
    }
    if (dyfunc) {
        return dyfunc(A, B);
    } else {
        return;
    }        
}
void _BLCGContextShowTextAtPoint(BLCGContextRef A, float B, float C,
        const char *D, size_t E) {
    static void (*dyfunc)(BLCGContextRef, float, float, const char *, size_t) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextShowTextAtPoint");
    }
    if (dyfunc) {
        return dyfunc(A, B, C, D, E);
    } else {
        return;
    }        
}

BLCGPoint _BLCGContextGetTextPosition(BLCGContextRef A) {
    static BLCGPoint (*dyfunc)(BLCGContextRef) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextGetTextPosition");
    }
    if (dyfunc) {
        return dyfunc(A);
    } else {
        return (BLCGPoint){0.0, 0.0};
    }        
}

void _BLCGColorSpaceRelease(BLCGColorSpaceRef A) {
    static void (*dyfunc)(BLCGColorSpaceRef) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGColorSpaceRelease");
    }
    if (dyfunc) {
        return dyfunc(A);
    } else {
        return;
    }        

}

void _BLCGContextRelease(BLCGContextRef A) {
    static void (*dyfunc)(BLCGContextRef) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCoreGraphics();
        dyfunc = GETFUNCTIONPTR(image, "_CGContextRelease");
    }
    if (dyfunc) {
        return dyfunc(A);
    } else {
        return;
    }        
}


int isCoreGraphicsAvailable() {
  void *mk = loadCoreGraphics();

  return (mk != NULL);
}
