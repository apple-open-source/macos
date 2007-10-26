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
 *  fs.c - Generic access to the file system modules.
 *
 *  Copyright (c) 1999-2004 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>
#include "md5.h"

typedef long (* FSLoadFile)(CICell ih, char *filePath);
typedef long (* FSReadFile)(CICell ih, char *filePath,
			    void *base, unsigned long offset,
			    unsigned long length);
typedef long (* FSGetDirEntry)(CICell ih, char *dirPath,
			       unsigned long *dirIndex, char **name,
			       long *flags, long *time);
typedef long (* FSGetUUID)(CICell ih, char *uuidStr);

#define kPartNet     (0)
#define kPartHFS     (1)
#define kPartUFS     (2)
#define kPartExt2    (3)

struct PartInfo {
  long            partType;
  CICell          partIH;
  FSLoadFile      loadFile;
  FSReadFile      readFile;
  FSGetDirEntry   getDirEntry;
  FSGetUUID       getUUID;
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

long LoadThinFatFile(char *fileSpec, void **binary)
{
  char       devSpec[256];
  char       *filePath;
  FSLoadFile loadFile;
  FSReadFile readFile;
  long       ret, length, length2, partIndex;
  
  ret = ConvertFileSpec(fileSpec, devSpec, &filePath);
  if ((ret == -1) || (filePath == NULL)) return -1;
  
  // Get the partition index for devSpec.
  partIndex = LookupPartition(devSpec);
  if (partIndex == -1) return -1;
  
  *binary = (void *)kLoadAddr;
  
  readFile = gParts[partIndex].readFile;
  
  if (readFile != NULL) {
    // Read the first 4096 bytes (fat header)
    length = readFile(gParts[partIndex].partIH, filePath, *binary, 0, 0x1000);
    if (length > 0) {
      if (ThinFatBinary(binary, &length) == 0) {
        // We found a fat binary; read only the thin part
        length = readFile(gParts[partIndex].partIH, filePath,
                          (void *)kLoadAddr, (unsigned long)(*binary) - kLoadAddr, length);
        *binary = (void *)kLoadAddr;
      } else {
        // Not a fat binary; read the rest of the file
        length2 = readFile(gParts[partIndex].partIH, filePath, (void *)(kLoadAddr + length), length, 0);
        if (length2 == -1) return -1;
        length += length2;
      }
    }
  } else {
    loadFile = gParts[partIndex].loadFile;
    length = loadFile(gParts[partIndex].partIH, filePath);
    if (length > 0) {
      ThinFatBinary(binary, &length);
    }
  }
  
  return length;
}

long GetFSUUID(char *spec, char *uuidStr)
{
  long       rval = -1, partIndex;
  FSGetUUID  getUUID;
  char       devSpec[256];
  
  do {
    if(ConvertFileSpec(spec, devSpec, NULL))  break;
    
    // Get the partition index
    partIndex = LookupPartition(devSpec);
    if (partIndex == -1)  break;
    
    getUUID = gParts[partIndex].getUUID;
    if(getUUID)
      rval = getUUID(gParts[partIndex].partIH, uuidStr);
  } while(0);

  return rval;
}


// from our uuid/namespace.h (UFS and HFS uuids can live in the same space?)
static unsigned char kFSUUIDNamespaceSHA1[] = {0xB3,0xE2,0x0F,0x39,0xF2,0x92,0x11,0xD6,0x97,0xA4,0x00,0x30,0x65,0x43,0xEC,0xAC};

// filesystem-specific getUUID functions call this shared string generator
long CreateUUIDString(uint8_t uubytes[], int nbytes, char *uuidStr)
{
  unsigned  fmtbase, fmtidx, i;
  uint8_t   uuidfmt[] = { 4, 2, 2, 2, 6 };
  char     *p = uuidStr;
  MD5_CTX   md5c;
  uint8_t   mdresult[16];

  bzero(mdresult, sizeof(mdresult));

  // just like AppleFileSystemDriver
  MD5Init(&md5c);
  MD5Update(&md5c, kFSUUIDNamespaceSHA1, sizeof(kFSUUIDNamespaceSHA1));
  MD5Update(&md5c, uubytes, nbytes);
  MD5Final(mdresult, &md5c);

  // this UUID has been made version 3 style (i.e. via namespace) 
  // see "-uuid-urn-" IETF draft (which otherwise copies byte for byte)
  mdresult[6] = 0x30 | ( mdresult[6] & 0x0F );
  mdresult[8] = 0x80 | ( mdresult[8] & 0x3F );


  // generate the text: e.g. 5EB1869F-C4FA-3502-BDEB-3B8ED5D87292
  i = 0; fmtbase = 0;
  for(fmtidx = 0; fmtidx < sizeof(uuidfmt); fmtidx++) {
    for(i=0; i < uuidfmt[fmtidx]; i++) {
      uint8_t byte = mdresult[fmtbase+i];
      char nib;

      nib = byte >> 4;
      *p = nib + '0';  // 0x4 -> '4'
      if(*p > '9')  *p = (nib - 9 + ('A'-1));  // 0xB -> 'B'
      p++;

      nib = byte & 0xf;
      *p = nib + '0';  // 0x4 -> '4'
      if(*p > '9')  *p = (nib - 9 + ('A'-1));  // 0xB -> 'B'
      p++;

    }
    fmtbase += i;
    if(fmtidx < sizeof(uuidfmt)-1)
      *(p++) = '-';
    else
      *p = '\0';
  }

  return 0;
}

long GetFileInfo(char *dirSpec, char *name, long *flags, long *time)
{
  long ret;
  unsigned long index = 0;
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

long GetDirEntry(char *dirSpec, unsigned long *dirIndex, char **name,
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
  long ret, flags, time;
  unsigned long index = 0;
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
      gParts[partIndex].readFile      = NULL;
      gParts[partIndex].getDirEntry   = NetGetDirEntry;
      gParts[partIndex].getUUID       = NULL;
      break;
      
    case kPartHFS:
      gParts[partIndex].loadFile      = HFSLoadFile;
      gParts[partIndex].readFile      = HFSReadFile;
      gParts[partIndex].getDirEntry   = HFSGetDirEntry;
      gParts[partIndex].getUUID       = HFSGetUUID;
      break;
      
    case kPartUFS:
      gParts[partIndex].loadFile      = UFSLoadFile;
      gParts[partIndex].readFile      = UFSReadFile;
      gParts[partIndex].getDirEntry   = UFSGetDirEntry;
      gParts[partIndex].getUUID       = UFSGetUUID;
      break;
      
    case kPartExt2:
      gParts[partIndex].loadFile      = Ext2LoadFile;
      gParts[partIndex].readFile      = NULL;
      gParts[partIndex].getDirEntry   = Ext2GetDirEntry;
      gParts[partIndex].getUUID       = NULL;
      // Ext2GetUUID exists, but there's no kernel support
      break;
    }
  }
  
  return partIndex;
}
