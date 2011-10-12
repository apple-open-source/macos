/*
 *  kernelcache.h
 *  kext_tools
 *
 *  Created by Nik Gervae on 2010 10 04.
 *  Copyright 2010 Apple Computer, Inc. All rights reserved.
 *
 */
#ifndef _KERNELCACHE_H_
#define _KERNELCACHE_H_

#include <libc.h>
#include "kext_tools_util.h"

#define PLATFORM_NAME_LEN  (64)
#define ROOT_PATH_LEN     (256)

typedef struct prelinked_kernel_header {
    uint32_t  signature;
    uint32_t  compressType;
    uint32_t  adler32;
    uint32_t  uncompressedSize;
    uint32_t  compressedSize;
    uint32_t  reserved[11];
    char      platformName[PLATFORM_NAME_LEN]; // unused
    char      rootPath[ROOT_PATH_LEN];         // unused
    char      data[0];
} PrelinkedKernelHeader;

typedef struct platform_info {
    char platformName[PLATFORM_NAME_LEN];
    char rootPath[ROOT_PATH_LEN];
} PlatformInfo;

/*******************************************************************************
*******************************************************************************/

ExitStatus 
writeFatFile(
    const char                * filePath,
    CFArrayRef                  fileSlices,
    CFArrayRef                  fileArchs,
    mode_t                      fileMode,
    const struct timeval        fileTimes[2]);
ExitStatus writeToFile(
    int           fileDescriptor,
    const UInt8 * data,
    CFIndex       length);
void * mapAndSwapFatHeaderPage(
    int fileDescriptor);
void unmapFatHeaderPage(
    void *headerPage);
struct fat_arch * getFirstFatArch(
    u_char *headerPage);
struct fat_arch * getNextFatArch(
    u_char *headerPage, 
    struct fat_arch *lastArch);
struct fat_arch * getFatArchForArchInfo(
    u_char *headerPage, 
    const NXArchInfo *archInfo);
const NXArchInfo * 
getThinHeaderPageArch(
    const void *headerPage);
ExitStatus readFatFileArchsWithPath(
    const char        * filePath,
    CFMutableArrayRef * archsOut);
ExitStatus readFatFileArchsWithHeader(
    u_char            * headerPage,
    CFMutableArrayRef * archsOut);
ExitStatus readMachOSlices(
    const char        * filePath,
    CFMutableArrayRef * slicesOut,
    CFMutableArrayRef * archsOut,
    mode_t            * modeOut,
    struct timeval      machOTimesOut[2]);
CFDataRef  readMachOSliceForArch(
    const char        * filePath,
    const NXArchInfo  * archInfo,
    Boolean             checkArch);
CFDataRef readMachOSlice(
    int                 fileDescriptor,
    off_t       fileOffset,
    size_t      fileSliceSize);
int readFileAtOffset(
    int             fileDescriptor,
    off_t           fileOffset,
    size_t          fileSize,
    u_char        * buf);
int verifyMachOIsArch(
    const UInt8      * fileBuf,
    size_t              size,
    const NXArchInfo * archInfo);
CFDataRef uncompressPrelinkedSlice(
    CFDataRef prelinkImage);
CFDataRef compressPrelinkedSlice(
    CFDataRef prelinkImage);
ExitStatus writePrelinkedSymbols(
    CFURLRef    symbolDirURL,
    CFArrayRef  prelinkSymbols,
    CFArrayRef  prelinkArchs);
ExitStatus makeDirectoryWithURL(
    CFURLRef dirURL);

#endif /* _KERNELCACHE_H_ */
