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
/*
 * Copyright 1994 NeXT Computer, Inc.
 * All rights reserved.
 */

#ifndef __BOOT2_BOOT_H
#define __BOOT2_BOOT_H

#include "libsaio.h"

/*
 * Keys used in system Default.table / Instance0.table
 */
#define kGraphicsModeKey    "Graphics Mode"
#define kTextModeKey        "Text Mode"
#define kBootGraphicsKey    "Boot Graphics"
#define kQuietBootKey       "Quiet Boot"
#define kKernelFlagsKey     "Kernel Flags"
#define kKernelNameKey      "Kernel"
#define kKernelCacheKey     "Kernel Cache"

/*
 * A global set by boot() to record the device that the booter
 * was loaded from.
 */
extern int  gBIOSDev;
extern BOOL gBootGraphics;
extern long gBootMode;
extern BOOL sysConfigValid;
extern char bootBanner[];
extern char bootPrompt[];

// Boot Modes
enum {
    kBootModeNormal = 0,
    kBootModeSafe,
    kBootModeSecure
};

/*
 * graphics.c
 */
extern void printVBEInfo();
extern void setVideoMode(int mode);
extern int  getVideoMode();
extern void spinActivityIndicator();
extern void clearActivityIndicator();

/*
 * drivers.c
 */
extern long LoadDrivers(char * dirSpec);
extern long DecodeKernel(void *binary, entry_t *rentry, char **raddr, int *rsize);

/*
 * options.c
 */
extern void getBootOptions();
extern int  processBootOptions();

/*
 * lzss.c
 */
extern int decompress_lzss(u_int8_t *dst, u_int8_t *src, u_int32_t srclen);

struct compressed_kernel_header {
  u_int32_t signature;
  u_int32_t compress_type;
  u_int32_t adler32;
  u_int32_t uncompressed_size;
  u_int32_t compressed_size;
  u_int32_t reserved[11];
  char      platform_name[64];
  char      root_path[256];
  u_int8_t  data[0];
};
typedef struct compressed_kernel_header compressed_kernel_header;

#endif /* !__BOOT2_BOOT_H */
