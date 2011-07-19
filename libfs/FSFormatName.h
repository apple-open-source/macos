/* 
 * Copyright (c) 1999-2004 Apple Computer, Inc.  All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#if !defined(__FS_FORMATNAME__)
#define __FS_FORMATNAME__ 1

#include "FSPrivate.h"

#include "bootsect.h"	// for MSDOS
#include "bpb.h"

#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPriv.h>
#include <libkern/OSAtomic.h> 	// for OSSpinLock
#include <sys/mount.h> 			// for struct statfs
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>	
#include <stdio.h>
#include <hfs/hfs_format.h>	// for HFS

/* Currently HFS has maximum subtypes (5) */ 
#define MAX_FS_SUBTYPES			5

/* Maximun length of f_fstypename in /System/Library/Filesystem/ */
#define MAX_FSNAME			10

#define kFSSystemLibraryFileSystemsPath CFSTR("/System/Library/Filesystems")
#define KEY_FS_PERSONALITIES 	CFSTR("FSPersonalities")
#define KEY_FS_SUBTYPE 			CFSTR("FSSubType")
#define KEY_FS_NAME 			CFSTR("FSName")
#define UNKNOWN_FS_NAME			CFSTR("Unknown")


/* HFS type and subtype number */
#define HFS_NAME	"hfs" 
enum {
    kHFSPlusSubType     = 0,    /* HFS Plus */
    kHFSJSubType        = 1,    /* HFS Journaled */
    kHFSXSubType        = 2,    /* HFS Case-sensitive */
    kHFSXJSubType       = 3,    /* HFS Case-sensitive, Journaled */
    kHFSSubType         = 128   /* HFS */
};
#define MAX_HFS_BLOCK_READ  512

/* MSDOS type and subtype number */
#define MSDOS_NAME	"msdos"
enum {
    kFAT12SubType       = 0,    /* FAT 12 */
    kFAT16SubType       = 1,    /* FAT 16 */
    kFAT32SubType       = 2,    /* FAT 32 */
};
#define MAX_DOS_BLOCKSIZE	2048

/* Internal function */
CFStringRef FSCopyFormatNameForFSType(CFStringRef fsType, int16_t fsSubtype, bool localized, bool encrypted);
bool getfstype(char *devnode, char *fsname, int *fssubtype);
bool is_hfs(char *devnode, int *fssubtype);
bool is_msdos(char *devnode, int *fssubtype);
static int getblk(int fd, unsigned long blk, int blksize, char* buf);
static int getwrapper(const HFSMasterDirectoryBlock *mdbp, off_t *offset);

#endif /* !__FS_FORMATNAME__ */
