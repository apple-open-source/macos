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
 *  fs.c - Generic access to the file system modules.
 *
 *  Copyright (c) 1999-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>

typedef long (* FSLoadFile)(CICell ih, char *filePath);
typedef long (* FSGetDirEntry)(CICell ih, char *dirPath,
			       long *dirIndex, char **name,
			       long *flags, long *time);

#define kPartNet     (0)
#define kPartHFS     (1)
#define kPartUFS     (2)
#define kPartExt2    (3)

struct PartInfo {
  long            partType;
  CICell          partIH;
  FSLoadFile      loadFile;
  FSGetDirEntry   getDirEntry;
  char            partName[1024];
};
typedef struct PartInfo PartInfo, *PartInfoPtr;

#define kNumPartInfos  (16)
static PartInfo gParts[kNumPartInfos];
static char gMakeDirSpec[1024];

// Private function prototypes
long LookupPartition(char *devSpec);


// Public functions

long LoadFile(char *fileSpec)
{
  char       devSpec[256];
  char       *filePath;
  FSLoadFile loadFile;
  long       ret, length, partIndex;
  
  ret = ConvertFileSpec(fileSpec, devSpec, &filePath);
  if ((ret == -1) || (filePath == NULL)) return -1;
  
  // Get the partition index for devSpec.
  partIndex = LookupPartition(devSpec);
  if (partIndex == -1) return -1;
  
  loadFile = gParts[partIndex].loadFile;
  length = loadFile(gParts[partIndex].partIH, filePath);
  
//  if (length == 0) return -1;
  
  return length;
}

long GetFileInfo(char *dirSpec, char *name, long *flags, long *time)
{
  long ret, index = 0;
  char *curName;

  if (!dirSpec) {
    long       idx, len;

    len = strlen(name);

    for (idx = len; idx && (name[idx] != '\\'); idx--) {}
    idx++;
    strncpy(gMakeDirSpec, name, idx);
    name += idx;
    dirSpec = gMakeDirSpec;
  }
  
  while (1) {
    ret = GetDirEntry(dirSpec, &index, &curName, flags, time);
    if (ret == -1) break;
    
    if (!strcmp(name, curName)) break;
  }
  
  return ret;
}

long GetDirEntry(char *dirSpec, long *dirIndex, char **name,
		 long *flags, long *time)
{
  char          devSpec[256];
  char          *dirPath;
  FSGetDirEntry getDirEntry;
  long          ret, partIndex;
  
  ret = ConvertFileSpec(dirSpec, devSpec, &dirPath);
  if ((ret == -1) || (dirPath == NULL)) return -1;
  
  // Get the partition index for devSpec.
  partIndex = LookupPartition(devSpec);
  if (partIndex == -1) return -1;
  
  getDirEntry = gParts[partIndex].getDirEntry;
  ret = getDirEntry(gParts[partIndex].partIH, dirPath,
		    dirIndex, name, flags, time);
  
  return ret;
}

long DumpDir(char *dirSpec)
{
  long ret, flags, time, index = 0;
  char *name;
  
  printf("DumpDir on [%s]\n", dirSpec);
  
  while (1) {
    ret = GetDirEntry(dirSpec, &index, &name, &flags, &time);
    if (ret == -1) break;
    
    printf("%x %x [%s]\n", flags, time, name);
  }
  
  return 0;
}


// Private functions

long LookupPartition(char *devSpec)
{
  CICell partIH;
  long   partIndex, partType;
  long   deviceType;
  
  // See if the devSpec has already been opened.
  for (partIndex = 0; partIndex < kNumPartInfos; partIndex++) {
    if (!strcmp(gParts[partIndex].partName, devSpec)) break;
  }
  
  // If it has not been opened, do so now.
  if (partIndex == kNumPartInfos) {
    // Find a free slot.
    for (partIndex = 0; partIndex < kNumPartInfos; partIndex++) {
      if (gParts[partIndex].partIH == 0) break;
    }
    // No free slots, so return error.
    if (partIndex == kNumPartInfos) return -1;
    
    deviceType = GetDeviceType(devSpec);
    switch (deviceType) {
    case kNetworkDeviceType :
      partIH = NetInitPartition(devSpec);
      if (partIH == 0) return -1;
      partType = kPartNet;
      break;
      
    case kBlockDeviceType :
      printf("Opening partition [%s]...\n", devSpec);
      partIH = Open(devSpec);
      if (partIH == 0) {
	printf("Failed to open partition [%s].\n", devSpec);
	return -1;
      }
      
      // Find out what kind of partition it is.
      if      (HFSInitPartition(partIH)  != -1) partType = kPartHFS;
      else if (UFSInitPartition(partIH)  != -1) partType = kPartUFS;
      else if (Ext2InitPartition(partIH) != -1) partType = kPartExt2;
      else return -1;
      break;
      
    default :
      return -1;
    }
    
    gParts[partIndex].partIH = partIH;
    gParts[partIndex].partType = partType;
    strcpy(gParts[partIndex].partName, devSpec);
    
    switch (partType) {
    case kPartNet:
      gParts[partIndex].loadFile      = NetLoadFile;
      gParts[partIndex].getDirEntry   = NetGetDirEntry;
      break;
      
    case kPartHFS:
      gParts[partIndex].loadFile      = HFSLoadFile;
      gParts[partIndex].getDirEntry   = HFSGetDirEntry;
      break;
      
    case kPartUFS:
      gParts[partIndex].loadFile      = UFSLoadFile;
      gParts[partIndex].getDirEntry   = UFSGetDirEntry;
      break;
      
    case kPartExt2:
      gParts[partIndex].loadFile      = Ext2LoadFile;
      gParts[partIndex].getDirEntry   = Ext2GetDirEntry;
      break;
    }
  }
  
  return partIndex;
}
