/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/mount.h>
#include <dirent.h>


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFDictionary.h>

#define FS_TYPE_HFS		"hfs"
#define FS_TYPE_UFS		"ufs"
#define FS_TYPE_CD9660		"cd9660"
#define FS_TYPE_UDF		"udf"

// globals

extern CFMutableDictionaryRef plistDict;

char * daCreateCStringFromCFString(CFStringRef string);

mode_t mountModeForFS(char *fsname);

int sortfs(const void *v1, const void *v2);
int suffixfs(struct dirent *dp);


char *fsDirForFS(char *fsname);
char *utilPathForFS(char *fsname);

int renameUFSDevice(const char *devName, const char *mountPoint);

char *fsNameForFSWithMediaName(char *fsname, char *mediaName);

char *verifyArgsForFileSystem(char *fsname);
char *repairPathForFileSystem(char *fsname);
char *verifyPathForFileSystem(char *fsname);
char *repairArgsForFileSystem(char *fsname);

// for debugging

void printArgsForFsname(char *fsname);

// replacement codes

void cacheFileSystemDictionaries();
void cacheFileSystemMatchingArray();


char *valueForFileSystem(char *fsname, char *fskey);


// internal only
CFDictionaryRef dictionaryForFileSystem(char *fsname);
char *valueForFSDictionary(CFDictionaryRef dict, char *fskey);
