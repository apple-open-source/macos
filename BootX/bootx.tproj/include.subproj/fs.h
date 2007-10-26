/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
 
/*
 *  fs.h - Externs for the File System Modules
 *
 *  Copyright (c) 1999-2004 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#ifndef _BOOTX_FS_H_
#define _BOOTX_FS_H_

// Externs for fs.c
extern long LoadFile(char *fileSpec);
extern long LoadThinFatFile(char *fileSpec, void **binary);
extern long GetFileInfo(char *dirSpec, char *name, long *flags, long *time);
extern long GetDirEntry(char *dirSpec, unsigned long *dirIndex, char **name,
			long *flags, long *time);
extern long DumpDir(char *dirSpec);
extern long GetFSUUID(char *devSpec, char *uuidStr);
extern long CreateUUIDString(uint8_t uubytes[], int nbytes, char *uuidStr);

// Externs for cache.c
extern unsigned long gCacheHits;
extern unsigned long gCacheMisses;
extern unsigned long gCacheEvicts;

extern void CacheInit(CICell ih, long chunkSize);
extern long CacheRead(CICell ih, char *buffer, long long offset,
		      long length, long cache);

// Externs for net.c
extern CICell NetInitPartition(char *devSpec);
extern long   NetLoadFile(CICell ih, char *filePath);
extern long   NetGetDirEntry(CICell ih, char *dirPath,
			     unsigned long *dirIndex, char **name,
			     long *flags, long *time);

// Externs for hfs.c
// Note: the offset parameters are only used by mach-o routines.
// We don't support mach-o binaries > 4 GB.
extern long HFSInitPartition(CICell ih);
extern long HFSLoadFile(CICell ih, char *filePath);
extern long HFSReadFile(CICell ih, char *filePath,
			void *base, unsigned long offset,
			unsigned long length);
extern long HFSGetDirEntry(CICell ih, char *dirPath,
			   unsigned long *dirIndex, char **name,
			   long *flags, long *time);
extern long HFSGetUUID(CICell ih, char *uuidStr);

// Externs for ufs.c
extern long UFSInitPartition(CICell ih);
extern long UFSLoadFile(CICell ih, char *filePath);
extern long UFSReadFile(CICell ih, char *filePath,
			void *base, unsigned long offset,
			unsigned long length);
extern long UFSGetDirEntry(CICell ih, char *dirPath,
			   unsigned long *dirIndex, char **name,
			   long *flags, long *time);
extern long UFSGetUUID(CICell ih, char *uuidStr);

// Externs for ext2.c
extern long Ext2InitPartition(CICell ih);
extern long Ext2LoadFile(CICell ih, char *filePath);
extern long Ext2GetDirEntry(CICell ih, char *dirPath,
			   unsigned long *dirIndex, char **name,
			    long *flags, long *time);
extern long Ext2GetUUID(CICell ih, char *uuidStr);

#endif /* ! _BOOTX_FS_H_ */
