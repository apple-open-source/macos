/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 *  sl.h - Headers for configuring the Secondary Loader
 *
 *  Copyright (c) 1998-2003 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#ifndef _BOOTX_SL_H_
#define _BOOTX_SL_H_

#define kFailToBoot (1)

/*

Memory Map:  assumes 96 MB

Physical Address

Open Firmware Version    3x, 4x, ...
00000000 - 00003FFF  :   Exception Vectors
00004000 - 057FFFFF  :   Free Memory
05800000 - 05FFFFFF  :   OF Image


Logical Address

00000000 - 00003FFF  : Exception Vectors
00004000 - 03FFFFFF  : Kernel Image, Boot Struct and Drivers
04000000 - 04FFFFFF  : File Load Area
05000000 - 053FFFFF  : FS Cache
05400000 - 055FFFFF  : Malloc Zone
05600000 - 057FFFFF  : BootX Image
05800000 - 05FFFFFF  : Unused

*/

#define kVectorAddr     (0x00000000)
#define kVectorSize     (0x00004000)

// OF 3.x
#define kImageAddr      (0x00004000)
#define kImageSize      (0x03FFC000)

// OF 1.x 2.x
#define kImageAddr0     (0x00004000)
#define kImageSize0     (0x002FC000)
#define kImageAddr1     (0x00300000)
#define kImageSize1     (0x00200000)
#define kImageAddr1Phys (0x05800000)
#define kImageAddr2     (0x00500000)
#define kImageSize2     (0x03B00000)

#define kLoadAddr       (0x04000000)
#define kLoadSize       (0x01000000)

#define kFSCacheAddr    (0x05000000)
#define kFSCacheSize    (0x00400000)

#define kMallocAddr     (0x05400000)
#define kMallocSize     (0x00200000)

// Default Output Level
#define kOutputLevelOff  (0)
#define kOutputLevelFull (16)

// OF versions
#define kOFVersion1x    (0x01000000)
#define kOFVersion2x    (0x02000000)
#define kOFVersion3x    (0x03000000)
#define kOFVersion4x    (0x04000000)

// Device Types
enum {
  kUnknownDeviceType = 0,
  kNetworkDeviceType,
  kBlockDeviceType
};

// File Permissions and Types
enum {
  kPermOtherExecute  = 1 << 0,
  kPermOtherWrite    = 1 << 1,
  kPermOtherRead     = 1 << 2,
  kPermGroupExecute  = 1 << 3,
  kPermGroupWrite    = 1 << 4,
  kPermGroupRead     = 1 << 5,
  kPermOwnerExecute  = 1 << 6,
  kPermOwnerWrite    = 1 << 7,
  kPermOwnerRead     = 1 << 8,
  kPermMask          = 0x1FF,
  kOwnerNotRoot      = 1 << 9,
  kFileTypeUnknown   = 0x0 << 16,
  kFileTypeFlat      = 0x1 << 16,
  kFileTypeDirectory = 0x2 << 16,
  kFileTypeLink      = 0x3 << 16,
  kFileTypeMask      = 0x3 << 16
};

// Key Numbers
#define kCommandKey (0x200)
#define kOptKey     (0x201)
#define kShiftKey   (0x202)
#define kControlKey (0x203)

// Mac OS X Booter Signature 'MOSX'
#define kMacOSXSignature (0x4D4F5358)

// Boot Modes
enum {
  kBootModeNormal = 0,
  kBootModeSafe,
  kBootModeSecure
};

#include <sys/stat.h>
#include <sys/types.h>

#include <ci.h>
#include <sl_words.h>
#include <libclite.h>
#include <fs.h>
#include <boot_args.h>

// Externs for main.c
extern char *gVectorSaveAddr;
extern long gKernelEntryPoint;
extern long gDeviceTreeAddr;
extern long gDeviceTreeSize;
extern long gBootArgsAddr;
extern long gBootArgsSize;
extern long gSymbolTableAddr;
extern long gSymbolTableSize;

extern long gBootMode;
extern long gBootDeviceType;
extern long gBootFileType;
extern char gHaveKernelCache;
extern char gBootDevice[256];
extern char gBootFile[256];

extern char gTempStr[4096];

extern long *gDeviceTreeMMTmp;

extern long gOFVersion;

extern char *gKeyMap;

extern long gRootAddrCells;
extern long gRootSizeCells;

extern CICell gChosenPH;
extern CICell gOptionsPH;
extern CICell gScreenPH;
extern CICell gMemoryMapPH;
extern CICell gStdOutPH;

extern CICell gMMUIH;
extern CICell gMemoryIH;
extern CICell gStdOutIH;
extern CICell gKeyboardIH;

extern long ThinFatBinary(void **binary, unsigned long *length);
extern long GetDeviceType(char *devSpec);
extern long ConvertFileSpec(char *fileSpec, char *devSpec, char **filePath);
extern long MatchThis(CICell phandle, char *string);
extern void *AllocateBootXMemory(long size);
extern long AllocateKernelMemory(long size);
extern long AllocateMemoryRange(char *rangeName, long start, long length);
extern unsigned long Alder32(unsigned char *buffer, long length);

// Externs for macho.c
extern long ThinFatBinaryMachO(void **binary, unsigned long *length);
extern long DecodeMachO(void *binary);

// Externs for elf.c
extern long ThinFatBinaryElf(void **binary, unsigned long *length);
extern long DecodeElf(void *binary);

// Externs for device_tree.c
extern long FlattenDeviceTree(void);
extern CICell SearchForNode(CICell ph, long top, char *prop, char *value);
extern CICell SearchForNodeMatching(CICell ph, long top, char *value);

// Externs for display.c
extern long InitDisplays(void);
extern long DrawSplashScreen(long stage);
extern long DrawFailedBootPicture(void);
extern void GetMainScreenPH(Boot_Video_Ptr video);

// Externs for drivers.c
extern long LoadDrivers(char *dirPath);

// Externs for config.c
extern long InitConfig(void);
extern long ParseConfigFile(char *addr);

// Externs for lzss.c
extern int decompress_lzss(u_int8_t *dst, u_int8_t *src, u_int32_t srclen);

#endif /* ! _BOOTX_SL_H_ */
