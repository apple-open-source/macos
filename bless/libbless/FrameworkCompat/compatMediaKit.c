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
 *  compatMediaKit.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jun 28 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: compatMediaKit.c,v 1.4 2002/04/25 19:46:07 ssen Exp $
 *
 *  $Log: compatMediaKit.c,v $
 *  Revision 1.4  2002/04/25 19:46:07  ssen
 *  add NSAddImage with searching
 *
 *  Revision 1.3  2002/03/10 23:51:10  ssen
 *  BLFrmatHFS now just takes the number of bytes to reserve
 *
 *  Revision 1.2  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.14  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.12  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
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
#include "compatMediaKit.h"
#include "bless.h"

#define MKPath "/System/Library/PrivateFrameworks/MediaKit.framework/MediaKit"

static void * loadMediaKit() {

    static const void *image = NULL;

    if (NULL == image) {
        struct stat statbuf;
        const char *suffix = getenv("DYLD_IMAGE_SUFFIX");
        char path[MAXPATHLEN];
        strcpy(path, MKPath);
        if (suffix) strcat(path, suffix);
        if (0 <= stat(path, &statbuf)) {
#ifdef _BUILD_CHEETAH
            NSAddLibrary(path);
            image = "MediaKit";
        } else if (0 <= stat(MKPath, &statbuf)) {
            NSAddLibrary(MKPath);
            image = "MediaKit";
#else
            image = NSAddImage(path, NSADDIMAGE_OPTION_RETURN_ON_ERROR
                                    |NSADDIMAGE_OPTION_WITH_SEARCHING);
        } else if (0 <= stat(MKPath, &statbuf)) {
            image = NSAddImage(MKPath, NSADDIMAGE_OPTION_RETURN_ON_ERROR
                                    |NSADDIMAGE_OPTION_WITH_SEARCHING);
#endif
        } else {
            image = NULL;
        }
    }
    return (void *)image;
}

#if !defined(DARWIN)

int16_t _BLMKMediaDeviceIO(void *A,u_int8_t B,u_int16_t C,u_int32_t D,u_int32_t E,void *F) {

    static int16_t (*dyfunc)(void *,u_int8_t,u_int16_t,u_int32_t,u_int32_t,void *) = NULL;
    if (NULL == dyfunc) {
        void *image = loadMediaKit();
        dyfunc = GETFUNCTIONPTR(image, "_MKMediaDeviceIO");
    }
    if (dyfunc) {
        return dyfunc(A, B, C, D, E, F);
    } else {
        return 1;
    }
}

int _BLMKMediaDeviceOpen(const char *A,int B,MediaDescriptor **C) {

    static int (*dyfunc)(const char *,int,MediaDescriptor **) = NULL;
    if (NULL == dyfunc) {
        void *image = loadMediaKit();
        dyfunc = GETFUNCTIONPTR(image, "_MKMediaDeviceOpen");
    }
    if (dyfunc) {
        return dyfunc(A, B, C);
    } else {
        return 1;
    }
}

int _BLMKMediaDeviceClose(MediaDescriptor *A) {
    static int (*dyfunc)(MediaDescriptor *) = NULL;
    if (NULL == dyfunc) {
        void *image = loadMediaKit();
        dyfunc = GETFUNCTIONPTR(image, "_MKMediaDeviceClose");
    }
    if (dyfunc) {
        return dyfunc(A);
    } else {
        return 1;
    }
}

int _BLMKCreateStartupFile(BIOVector A, void *B, u_int32_t C) {
    static int (*dyfunc)(BIOVector, void *, u_int32_t) = NULL;
    if (NULL == dyfunc) {
        void *image = loadMediaKit();
        dyfunc = GETFUNCTIONPTR(image, "_MKCreateStartupFile");
    }
    if (dyfunc) {
        return dyfunc(A, B, C);
    } else {
        return 1;
    }
}

int _BLMKStartupFileSize(BIOVector A, void *B, u_int32_t *C) {
    static int (*dyfunc)(BIOVector, void *, u_int32_t *) = NULL;
    if (NULL == dyfunc) {
        void *image = loadMediaKit();
        dyfunc = GETFUNCTIONPTR(image, "_MKStartupFileSize");
    }
    if (dyfunc) {
        return dyfunc(A, B, C);
    } else {
        return 1;
    }
}

int _BLMKReadWriteStartupFile(BIOVector A, void *B, u_int8_t C, u_int32_t D, u_int32_t E, void *F) {
    static int (*dyfunc)(BIOVector, void *, u_int8_t, u_int32_t, u_int32_t, void *) = NULL;
    if (NULL == dyfunc) {
        void *image = loadMediaKit();
        dyfunc = GETFUNCTIONPTR(image, "_MKReadWriteStartupFile");
    }
    if (dyfunc) {
        return dyfunc(A, B, C, D, E, F);
    } else {
        return 1;
    }
}

int _BLMKWriteStartupPartInfo(BIOVector A, void *B, u_int16_t C) {
    static int (*dyfunc)(BIOVector, void *, u_int16_t) = NULL;
    if (NULL == dyfunc) {
        void *image = loadMediaKit();
        dyfunc = GETFUNCTIONPTR(image, "_MKWriteStartupPartInfo");
    }
    if (dyfunc) {
        return dyfunc(A, B, C);
    } else {
        return 1;
    }
}


#endif /* !DARWIN */

int isMediaKitAvailable() {
  void *mk = loadMediaKit();

  return (mk != NULL);
}
