/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 *  sl.h - Headers for configuring the Secondary Loader
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#ifndef _BOOTX_SL_H_
#define _BOOTX_SL_H_

#define kFailToBoot (1)

#if ! defined(kMacOSXServer)
#define kMacOSXServer (0)
#endif

/*

Memory Map...  Assumes 32 MB

Physical Address

Open Firmware Version     1x, 2x                        3x
00000000 - 00003FFF  :             Exception Vectors
00004000 - 002FFFFF  :                Free Memory
00300000 - 004FFFFF  :   OF Image         /            Free Memory
00500000 - 01DFFFFF  :                Free Memory
01E00000 - 01FFFFFF  :   Free Memory      /            OF Image


Logical Address

00000000 - 00003FFF  : Exception Vectors
00004000 - 013FFFFF  : Kernal Image, Boot Struct and Drivers
01400000 - 01BFFFFF  : File Load Area
01C00000 - 01CFFFFF  : Secondary Loader Image
01D00000 - 01DFFFFF  : Malloc Area
01E00000 - 01FFFFFF  : Unused

To provide a consistant Logical Memory Usage between OF 1,2 and OF 3
the Logical Addresses 0x00300000 - 0x004FFFFF will be mapped to
Physical Address 0x01E00000 - 0x01FFFFFF and will be copied back
just before the kernel is loaded.


*/

#define kVectorAddr     (0x00000000)
#define kVectorSize     (0x00004000)

// OF 3.x
#define kImageAddr      (0x00004000)
#define kImageSize      (0x013FC000)

// OF 1.x 2.x
#define kImageAddr0     (0x00004000)
#define kImageSize0     (0x002FC000)
#define kImageAddr1     (0x00300000)
#define kImageSize1     (0x00200000)
#define kImageAddr1Phys (0x01E00000)
#define kImageAddr2     (0x00500000)
#define kImageSize2     (0x00F00000)

#define kLoadAddr       (0x01400000)
#define kLoadSize       (0x00800000)

#define kMallocAddr     (0x01D00000)
#define kMallocSize     (0x00100000)

// Default Output Level
#define kOutputLevelOff  (0)
#define kOutputLevelFull (16)

// OF versions
#define kOFVersion1x    (0x01000000)
#define kOFVersion2x    (0x02000000)
#define kOFVersion3x    (0x03000000)

// Device Types
enum {
  kUnknownDeviceType = 0,
  kNetworkDeviceType,
  kBlockDeviceType
};

// File Type
enum {
  kUnknownFileType = 0,
  kFlatFileType,
  kDirectoryFileType,
  kLinkFileType
};

// Key Numbers
#define kCommandKey (0x200)
#define kOptKey     (0x201)
#define kShiftKey   (0x202)
#define kControlKey (0x203)

// Mac OS X Booter Signature 'MOSX'
#define kMacOSXSignature (0x4D4F5358)


#include <bsd/sys/types.h>

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

extern long gBootDeviceType;
extern long gBootFileType;
extern char gBootDevice[256];
extern char gBootFile[256];
extern char gRootDir[256];

extern char gTempStr[4096];

extern long *gDeviceTreeMMTmp;

extern long gOFVersion;

extern char *gKeyMap;

extern CICell gChosenPH;
extern CICell gOptionsPH;
extern CICell gScreenPH;
extern CICell gMemoryMapPH;
extern CICell gStdOutPH;

extern CICell gMMUIH;
extern CICell gMemoryIH;
extern CICell gStdOutIH;
extern CICell gKeyboardIH;

extern long GetDeviceType(char *devSpec);
extern long ConvertFileSpec(char *fileSpec, char *devSpec, char **filePath);
extern long MatchThis(CICell phandle, char *string);
extern void *AllocateBootXMemory(long size);
extern long AllocateKernelMemory(long size);
extern long AllocateMemoryRange(char *rangeName, long start, long length);
extern unsigned long Alder32(unsigned char *buffer, long length);

// Externs for macho.c
extern long DecodeMachO(void);

// Externs for elf.c
extern long DecodeElf(void);

// Externs for device_tree.c
extern long FlattenDeviceTree(void);
extern CICell SearchForNode(CICell ph, long top, char *prop, char *value);
extern CICell SearchForNodeMatching(CICell ph, long top, char *value);

// Externs for display.c
extern long InitDisplays(void);
extern long LoadDisplayDrivers(void);
extern long DrawSplashScreen(void);
extern long DrawBrokenSystemFolder(void);
extern void GetMainScreenPH(Boot_Video_Ptr video);

// Externs for drivers.c
extern long LoadDrivers(char *dirPath);

// Externs for config.c
extern long InitConfig(void);
extern long ParseConfigFile(char *addr);

// Externs for PEFSupport.c
extern unsigned long GetSymbolFromPEF(char *name, char *pef,
				      void *desc, long descSize);

#endif /* ! _BOOTX_SL_H_ */
