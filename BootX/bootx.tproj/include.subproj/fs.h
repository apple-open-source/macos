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
 *  fs.h - Externs for the File System Modules
 *
 *  Copyright (c) 1999-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#ifndef _BOOTX_FS_H_
#define _BOOTX_FS_H_

// Externs for fs.c
extern long LoadFile(char *fileSpec);
extern long CopyFile(char *fileSpec, char **addr, long *length);
extern long GetFileInfo(char *dirSpec, char *name, long *flags, long *time);
extern long GetDirEntry(char *dirSpec, long *dirIndex, char **name,
			long *flags, long *time);
extern long DumpDir(char *dirSpec);

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
			     long *dirIndex, char **name,
			     long *flags, long *time);

// Externs for hfs.c
extern long HFSInitPartition(CICell ih);
extern long HFSLoadFile(CICell ih, char *filePath);
extern long HFSGetDirEntry(CICell ih, char *dirPath,
			   long *dirIndex, char **name,
			   long *flags, long *time);

// Externs for ufs.c
extern long UFSInitPartition(CICell ih);
extern long UFSLoadFile(CICell ih, char *filePath);
extern long UFSGetDirEntry(CICell ih, char *dirPath,
			   long *dirIndex, char **name,
			   long *flags, long *time);

// Externs for ext2.c
extern long Ext2InitPartition(CICell ih);
extern long Ext2LoadFile(CICell ih, char *filePath);
extern long Ext2GetDirEntry(CICell ih, char *dirPath,
			   long *dirIndex, char **name,
			    long *flags, long *time);

#endif /* ! _BOOTX_FS_H_ */
