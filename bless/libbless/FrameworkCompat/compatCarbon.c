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
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <mach-o/dyld.h>

#include "compat.h"
#include "compatCarbon.h"
#include "bless.h"


#define CCPath "/System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/CarbonCore"

/* Dynamic loading code taken from CoreFoundation (CF) URL/URLAccess */

static void * loadCarbonCore() {

    static const void *image = NULL;

    if (NULL == image) {
        struct stat statbuf;
        const char *suffix = getenv("DYLD_IMAGE_SUFFIX");
        char path[MAXPATHLEN];
        strcpy(path, CCPath);
        if (suffix) strcat(path, suffix);
        if (0 <= stat(path, &statbuf)) {
#ifdef _BUILD_CHEETAH
            NSAddLibrary(path);
            image = "CarbonCore";
        } else if(0 <= stat(CCPath, &statbuf)) {
            NSAddLibrary(CCPath);
            image = "CarbonCore";
#else
            image = NSAddImage(path, NSADDIMAGE_OPTION_NONE);
        } else if(0 <= stat(CCPath, &statbuf)){
            image = NSAddImage(CCPath, NSADDIMAGE_OPTION_NONE);
#endif
        } else {
            image = NULL;
        }
    }
    return (void *)image;
}

OSErr _BLNativePathNameToFSSpec(char * A, FSSpec * B, long C) {
    static OSErr (*dyfunc)(char *, FSSpec *, long) = NULL;
    if (NULL == dyfunc) {
        void *image = loadCarbonCore();
        dyfunc = GETFUNCTIONPTR(image, "_NativePathNameToFSSpec");
    }
    if (dyfunc) {
        return dyfunc(A, B, C);
    } else {
        return 1;
    }

}

OSErr _BLFSMakeFSSpec(short A, long B, signed char * C, FSSpec * D) {
  static OSErr (*dyfunc)(short, long, signed char *, FSSpec *) = NULL;

    if (NULL == dyfunc) {
        void *image = loadCarbonCore();
        dyfunc = GETFUNCTIONPTR(image, "_FSMakeFSSpec");
    }
    if (dyfunc) {
        return dyfunc(A, B, C, D);
    } else {
        return 1;
    }
}


short _BLFSpOpenResFile(const FSSpec *  A, SInt8 B) {
  static short (*dyfunc)(const FSSpec *, SInt8) = NULL;

    if (NULL == dyfunc) {
        void *image = loadCarbonCore();
        dyfunc = GETFUNCTIONPTR(image, "_FSpOpenResFile");
    }
    if (dyfunc) {
        return dyfunc(A, B);
    } else {
        return 1;
    }
}

Handle _BLGet1Resource( FourCharCode A, short B)  {
  static Handle (*dyfunc)(FourCharCode, short) = NULL;

    if (NULL == dyfunc) {
        void *image = loadCarbonCore();
        dyfunc = GETFUNCTIONPTR(image, "_Get1Resource");
    }
    if (dyfunc) {
        return dyfunc(A, B);
    } else {
        return NULL;
    }
}

void _BLDetachResource(Handle A)   {
  static void (*dyfunc)(Handle) = NULL;

    if (NULL == dyfunc) {
        void *image = loadCarbonCore();
        dyfunc = GETFUNCTIONPTR(image, "_DetachResource");
    }
    if (dyfunc) {
        return dyfunc(A);
    } else {
        return;
    }
}

void _BLDisposeHandle(Handle A)   {
  static void (*dyfunc)(Handle) = NULL;

    if (NULL == dyfunc) {
        void *image = loadCarbonCore();
        dyfunc = GETFUNCTIONPTR(image, "_DisposeHandle");
    }
    if (dyfunc) {
        return dyfunc(A);
    } else {
        return;
    }
}

void _BLCloseResFile(short A)  {
  static void (*dyfunc)(short) = NULL;

    if (NULL == dyfunc) {
        void *image = loadCarbonCore();
        dyfunc = GETFUNCTIONPTR(image, "_CloseResFile");
    }
    if (dyfunc) {
        return dyfunc(A);
    } else {
        return;
    }
}
