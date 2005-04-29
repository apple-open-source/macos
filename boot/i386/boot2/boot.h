/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
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
/*
 * Copyright 1994 NeXT Computer, Inc.
 * All rights reserved.
 */

#ifndef __BOOT2_BOOT_H
#define __BOOT2_BOOT_H

#include "libsaio.h"

/*
 * Keys used in system Boot.plist
 */
#define kGraphicsModeKey    "Graphics Mode"
#define kTextModeKey        "Text Mode"
#define kBootGraphicsKey    "Boot Graphics"
#define kQuietBootKey       "Quiet Boot"
#define kKernelFlagsKey     "Kernel Flags"
#define kMKextCacheKey      "MKext Cache"
#define kKernelNameKey      "Kernel"
#define kKernelCacheKey     "Kernel Cache"
#define kBootDeviceKey      "Boot Device"
#define kTimeoutKey         "Timeout"
#define kRootDeviceKey      "rd"
#define kPlatformKey        "platform"
#define kACPIKey            "acpi"
#define kDefaultKernel      "mach_kernel"

/*
 * Flags to the booter or kernel
 *
 */
#define kVerboseModeFlag     "-v"
#define kSafeModeFlag        "-f"
#define kIgnoreBootFileFlag  "-F"
#define kSingleUserModeFlag  "-s"

/*
 * Booter behavior control
 */
#define kBootTimeout         8

/*
 * A global set by boot() to record the device that the booter
 * was loaded from.
 */
extern int  gBIOSDev;
extern long gBootMode;
extern BOOL sysConfigValid;
extern char bootBanner[];
extern char bootPrompt[];
extern BOOL gOverrideKernel;
extern char *gPlatformName;
extern char gMKextName[];
extern BVRef gBootVolume;

// Boot Modes
enum {
    kBootModeNormal = 0,
    kBootModeSafe   = 1,
    kBootModeSecure = 2,
    kBootModeQuiet  = 4
};

/*
 * graphics.c
 */
extern void printVBEInfo();
extern void printVBEModeInfo();
extern void setVideoMode(int mode);
extern int  getVideoMode();
extern void spinActivityIndicator();
extern void clearActivityIndicator();
extern void drawColorRectangle( unsigned short x,
                         unsigned short y,
                         unsigned short width,
                         unsigned short height,
                         unsigned char  colorIndex );
extern void drawDataRectangle( unsigned short  x,
                        unsigned short  y,
                        unsigned short  width,
                        unsigned short  height,
                               unsigned char * data );
extern int
convertImage( unsigned short width,
              unsigned short height,
              const unsigned char *imageData,
              unsigned char **newImageData );
extern char * decodeRLE( const void * rleData, int rleBlocks, int outBytes );
extern void drawBootGraphics(void);

/*
 * drivers.c
 */
extern long LoadDrivers(char * dirSpec);
extern long DecodeKernel(void *binary, entry_t *rentry, char **raddr, int *rsize);

/*
 * options.c
 */
extern int  getBootOptions(BOOL firstRun);
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
