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
 *  hfs.c - File System Module for HFS and HFS+.
 *
 *  Copyright (c) 1999-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>
#include <hfs/hfs_format.h>

#define kBlockSize (0x200)

#define kMDBBaseOffset (2 * kBlockSize)

#define kBTreeCatalog (0)
#define kBTreeExtents (1)

static CICell                  gCurrentIH;
static long long               gAllocationOffset;
static long                    gIsHFSPlus;
static long                    gBlockSize;
static char                    gBTreeHeaderBuffer[512];
static BTHeaderRec             *gBTHeaders[2];
static char                    gHFSMdbVib[kBlockSize];
static HFSMasterDirectoryBlock *gHFSMDB =(HFSMasterDirectoryBlock*)gHFSMdbVib;
static char                    gHFSPlusHeader[kBlockSize];
static HFSPlusVolumeHeader     *gHFSPlus =(HFSPlusVolumeHeader*)gHFSPlusHeader;
static char                    gLinkTemp[64];


static long ReadFile(void *file, long *length);
static long GetCatalogEntryInfo(void *entry, long *flags, long *time);
static long ResolvePathToCatalogEntry(char *filePath, long *flags,
				      void *entry, long dirID, long *dirIndex);

static long GetCatalogEntry(long *dirIndex, char **name,
			    long *flags, long *time);
static long ReadCatalogEntry(char *fileName, long dirID, void *entry,
			     long *dirIndex);
static long ReadExtentsEntry(long fileID, long startBlock, void *entry);

static long ReadBTreeEntry(long btree, void *key, char *entry, long *dirIndex);
static void GetBTreeRecord(long index, char *nodeBuffer, long nodeSize,
			   char **key, char **data);

static long ReadExtent(char *extent, long extentSize, long extentFile,
		       long offset, long size, void *buffer, long cache);

static long GetExtentStart(void *extents, long index);
static long GetExtentSize(void *extents, long index);

static long CompareHFSCatalogKeys(void *key, void *testKey);
static long CompareHFSPlusCatalogKeys(void *key, void *testKey);
static long CompareHFSExtentsKeys(void *key, void *testKey);
static long CompareHFSPlusExtentsKeys(void *key, void *testKey);

extern long FastRelString(char *str1, char *str2);
extern long FastUnicodeCompare(u_int16_t *uniStr1, u_int32_t len1,
			       u_int16_t *uniStr2, u_int32_t len2);
extern void utf_encodestr(const u_int16_t *ucsp, int ucslen,
			  u_int8_t *utf8p, u_int32_t bufsize);
extern void utf_decodestr(const u_int8_t *utf8p, u_int16_t *ucsp,
			  u_int16_t *ucslen, u_int32_t bufsize);


long HFSInitPartition(CICell ih)
{
  long extentSize, extentFile, nodeSize;
  void *extent;
  
  if (ih == gCurrentIH) return 0;
  
  printf("HFSInitPartition: %x\n", ih);
  
  gAllocationOffset = 0;
  gIsHFSPlus = 0;
  gBTHeaders[0] = 0;
  gBTHeaders[1] = 0;
  
  // Look for the HFS MDB
  Seek(ih, kMDBBaseOffset);
  Read(ih, (long)gHFSMdbVib, kBlockSize);
  
  if (gHFSMDB->drSigWord == kHFSSigWord) {
    gAllocationOffset = gHFSMDB->drAlBlSt * kBlockSize;
    
    // See if it is HFSPlus
    if (gHFSMDB->drEmbedSigWord != kHFSPlusSigWord) {
      // Normal HFS;
      gBlockSize = gHFSMDB->drAlBlkSiz;
      CacheInit(ih, gBlockSize);
      gCurrentIH = ih;
      
      // Get the Catalog BTree node size.
      extent     = (HFSExtentDescriptor *)&gHFSMDB->drCTExtRec;
      extentSize = gHFSMDB->drCTFlSize;
      extentFile = kHFSCatalogFileID;
      ReadExtent(extent, extentSize, extentFile, 0, 256,
		 gBTreeHeaderBuffer + kBTreeCatalog * 256, 0);
      nodeSize = ((BTHeaderRec *)(gBTreeHeaderBuffer + kBTreeCatalog * 256 + sizeof(BTNodeDescriptor)))->nodeSize;
      
      // If the BTree node size is larger than the block size, reset the cache.
      if (nodeSize > gBlockSize) CacheInit(ih, nodeSize);
      
      return 0;
    }
    
    // Calculate the offset to the embeded HFSPlus volume.
    gAllocationOffset += (long long)gHFSMDB->drEmbedExtent.startBlock * gHFSMDB->drAlBlkSiz;
  }
  
  // Look for the HFSPlus Header
  Seek(ih, gAllocationOffset + kMDBBaseOffset);
  Read(ih, (long)gHFSPlusHeader, kBlockSize);
  
  // Not a HFS[+] volume.
  if (gHFSPlus->signature != kHFSPlusSigWord) return -1;
  
  gIsHFSPlus = 1;
  gBlockSize = gHFSPlus->blockSize;
  CacheInit(ih, gBlockSize);
  gCurrentIH = ih;
  
  // Get the Catalog BTree node size.
  extent     = &gHFSPlus->catalogFile.extents;
  extentSize = gHFSPlus->catalogFile.logicalSize;
  extentFile = kHFSCatalogFileID;
  ReadExtent(extent, extentSize, extentFile, 0, 256,
	     gBTreeHeaderBuffer + kBTreeCatalog * 256, 0);
  nodeSize = ((BTHeaderRec *)(gBTreeHeaderBuffer + kBTreeCatalog * 256 + sizeof(BTNodeDescriptor)))->nodeSize;
  
  // If the BTree node size is larger than the block size, reset the cache.
  if (nodeSize > gBlockSize) CacheInit(ih, nodeSize);
  
  return 0;
}

long HFSLoadFile(CICell ih, char *filePath)
{
  char entry[512];
  long dirID, result, length, flags;
  
  if (HFSInitPartition(ih) == -1) return -1;
  
  printf("Loading HFS%s file: [%s] from %x.\n",
	 (gIsHFSPlus ? "+" : ""), filePath, ih);
  
  dirID = kHFSRootFolderID;
  // Skip a lead '\'.  Start in the system folder if there are two.
  if (filePath[0] == '\\') {
    if (filePath[1] == '\\') {
      if (gIsHFSPlus) dirID = ((long *)gHFSPlus->finderInfo)[5];
      else dirID = gHFSMDB->drFndrInfo[5];
      if (dirID == 0) return -1;
      filePath++;
    }
    filePath++;
  }
  
  result = ResolvePathToCatalogEntry(filePath, &flags, entry, dirID, 0);
  if ((result == -1) || ((flags & kFileTypeMask) != kFileTypeFlat)) return -1;
  
  // Check file owner and permissions.
  if (flags & (kOwnerNotRoot | kPermGroupWrite | kPermOtherWrite)) return -1;
  
  result = ReadFile(entry, &length);
  if (result == -1) return -1;
  
  return length;
}

long HFSGetDirEntry(CICell ih, char *dirPath, long *dirIndex, char **name,
		    long *flags, long *time)
{
  char entry[512];
  long dirID, dirFlags;
  
  if (HFSInitPartition(ih) == -1) return -1;
  
  if (*dirIndex == -1) return -1;
  
  dirID = kHFSRootFolderID;
  // Skip a lead '\'.  Start in the system folder if there are two.
  if (dirPath[0] == '\\') {
    if (dirPath[1] == '\\') {
      if (gIsHFSPlus) dirID = ((long *)gHFSPlus->finderInfo)[5];
      else dirID = gHFSMDB->drFndrInfo[5];
      if (dirID == 0) return -1;
      dirPath++;
    }
    dirPath++;
  }
  
  if (*dirIndex == 0) {
    ResolvePathToCatalogEntry(dirPath, &dirFlags, entry, dirID, dirIndex);
    if (*dirIndex == 0) *dirIndex = -1;
    if ((dirFlags & kFileTypeMask) != kFileTypeUnknown) return -1;
  }
  
  GetCatalogEntry(dirIndex, name, flags, time);
  if (*dirIndex == 0) *dirIndex = -1;
  if ((*flags & kFileTypeMask) == kFileTypeUnknown) return -1;
  
  return 0;
}


// Private Functions

static long ReadFile(void *file, long *length)
{
  void               *extents;
  long               fileID;
  HFSCatalogFile     *hfsFile     = file;
  HFSPlusCatalogFile *hfsPlusFile = file;
  
  if (gIsHFSPlus) {
    fileID  = hfsPlusFile->fileID;
    *length = hfsPlusFile->dataFork.logicalSize;
    extents = &hfsPlusFile->dataFork.extents;
  } else {
    fileID  = hfsFile->fileID;
    *length = hfsFile->dataLogicalSize;
    extents = &hfsFile->dataExtents;
  }
  
  if (*length > kLoadSize) {
    printf("File is too large.\n");
    return -1;
  }
  
  *length = ReadExtent((char *)extents, *length, fileID,
		       0, *length, (char *)kLoadAddr, 0);
  
  return 0;
}

static long GetCatalogEntryInfo(void *entry, long *flags, long *time)
{
  long tmpTime;
  
  // Get information about the file.
  switch (*(short *)entry) {
  case kHFSFolderRecord           :
    *flags = kFileTypeDirectory;
    tmpTime = ((HFSCatalogFolder *)entry)->modifyDate;
    break;
    
  case kHFSPlusFolderRecord       :
    *flags = kFileTypeDirectory |
      (((HFSPlusCatalogFolder *)entry)->bsdInfo.fileMode & kPermMask);
    if (((HFSPlusCatalogFolder *)entry)->bsdInfo.ownerID != 0)
      *flags |= kOwnerNotRoot;
    tmpTime = ((HFSPlusCatalogFolder *)entry)->contentModDate;
    break;
    
  case kHFSFileRecord             :
    *flags = kFileTypeFlat;
    tmpTime = ((HFSCatalogFile *)entry)->modifyDate;
    break;
    
  case kHFSPlusFileRecord         :
    *flags = kFileTypeFlat |
      (((HFSPlusCatalogFile *)entry)->bsdInfo.fileMode & kPermMask);
    if (((HFSPlusCatalogFile *)entry)->bsdInfo.ownerID != 0)
      *flags |= kOwnerNotRoot;
    tmpTime = ((HFSPlusCatalogFile *)entry)->contentModDate;
    break;
    
  case kHFSFileThreadRecord       :
  case kHFSPlusFileThreadRecord   :
  case kHFSFolderThreadRecord     :
  case kHFSPlusFolderThreadRecord :
    *flags = kFileTypeUnknown;
    tmpTime = 0;
    break;
  }
  
  if (time != 0) {
    // Convert base time from 1904 to 1970.
    *time = tmpTime - 2082844800;
  }
  
  return 0;
}

static long ResolvePathToCatalogEntry(char *filePath, long *flags,
				      void *entry, long dirID, long *dirIndex)
{
  char                 *restPath;
  long                 result, cnt, subFolderID, tmpDirIndex;
  HFSPlusCatalogFile   *hfsPlusFile;
  
  // Copy the file name to gTempStr
  cnt = 0;
  while ((filePath[cnt] != '\\') && (filePath[cnt] != '\0')) cnt++;
  strncpy(gTempStr, filePath, cnt);
  
  // Move restPath to the right place.
  if (filePath[cnt] != '\0') cnt++;
  restPath = filePath + cnt;
  
  // gTempStr is a name in the current Dir.
  // restPath is the rest of the path if any.
  
  result = ReadCatalogEntry(gTempStr, dirID, entry, dirIndex);
  if (result == -1) return -1;
  
  GetCatalogEntryInfo(entry, flags, 0);
  
  if ((*flags & kFileTypeMask) == kFileTypeDirectory) {
    if (gIsHFSPlus) subFolderID = ((HFSPlusCatalogFolder *)entry)->folderID;
    else subFolderID = ((HFSCatalogFolder *)entry)->folderID;
  }
  
  if ((*flags & kFileTypeMask) == kFileTypeDirectory)
    result = ResolvePathToCatalogEntry(restPath, flags, entry,
				       subFolderID, dirIndex);
  
  if (gIsHFSPlus && ((*flags & kFileTypeMask) == kFileTypeFlat)) {
    hfsPlusFile = (HFSPlusCatalogFile *)entry;
    if ((hfsPlusFile->userInfo.fdType == kHardLinkFileType) &&
	(hfsPlusFile->userInfo.fdCreator == kHFSPlusCreator)) {
      sprintf(gLinkTemp, "%s\\%s%d", HFSPLUSMETADATAFOLDER,
	      HFS_INODE_PREFIX, hfsPlusFile->bsdInfo.special.iNodeNum);
      result = ResolvePathToCatalogEntry(gLinkTemp, flags, entry,
					 kHFSRootFolderID, &tmpDirIndex);
    }
  }
  
  return result;
}

static long GetCatalogEntry(long *dirIndex, char **name,
			    long *flags, long *time)
{
  long              extentSize, nodeSize, curNode, index;
  void              *extent;
  char              *nodeBuf, *testKey, *entry;
  BTNodeDescriptor  *node;
  
  if (gIsHFSPlus) {
    extent     = &gHFSPlus->catalogFile.extents;
    extentSize = gHFSPlus->catalogFile.logicalSize;
  } else {
    extent     = (HFSExtentDescriptor *)&gHFSMDB->drCTExtRec;
    extentSize = gHFSMDB->drCTFlSize;
  }
  
  nodeSize = gBTHeaders[kBTreeCatalog]->nodeSize;
  nodeBuf = (char *)malloc(nodeSize);
  node = (BTNodeDescriptor *)nodeBuf;
  
  index   = *dirIndex % nodeSize;
  curNode = *dirIndex / nodeSize;
  
  // Read the BTree node and get the record for index.
  ReadExtent(extent, extentSize, kHFSCatalogFileID,
	     curNode * nodeSize, nodeSize, nodeBuf, 1);
  GetBTreeRecord(index, nodeBuf, nodeSize, &testKey, &entry);
  
  GetCatalogEntryInfo(entry, flags, time);
  
  // Get the file name.
  if (gIsHFSPlus) {
    utf_encodestr(((HFSPlusCatalogKey *)testKey)->nodeName.unicode,
		  ((HFSPlusCatalogKey *)testKey)->nodeName.length,
		  gTempStr, 256);
  } else {
    strncpy(gTempStr,
	    &((HFSCatalogKey *)testKey)->nodeName[1],
	    ((HFSCatalogKey *)testKey)->nodeName[0]);
  }
  *name = gTempStr;
  
  // Update dirIndex.
  index++;
  if (index == node->numRecords) {
    index = 0;
    curNode = node->fLink;
  }
  *dirIndex = curNode * nodeSize + index;
  
  free(nodeBuf);
  
  return 0;
}

static long ReadCatalogEntry(char *fileName, long dirID,
			     void *entry, long *dirIndex)
{
  long              length;
  char              key[sizeof(HFSPlusCatalogKey)];
  HFSCatalogKey     *hfsKey     = (HFSCatalogKey *)key;
  HFSPlusCatalogKey *hfsPlusKey = (HFSPlusCatalogKey *)key;
  
  // Make the catalog key.
  if (gIsHFSPlus) {
    hfsPlusKey->parentID = dirID;
    length = strlen(fileName);
    if (length > 255) length = 255;
    utf_decodestr(fileName, hfsPlusKey->nodeName.unicode,
		  &(hfsPlusKey->nodeName.length), 512);
  } else {
    hfsKey->parentID = dirID;
    length = strlen(fileName);
    if (length > 31) length = 31;
    hfsKey->nodeName[0] = length;
    strncpy(hfsKey->nodeName + 1, fileName, length);
  }
  
  return ReadBTreeEntry(kBTreeCatalog, &key, entry, dirIndex);
}

static long ReadExtentsEntry(long fileID, long startBlock, void *entry)
{
  char             key[sizeof(HFSPlusExtentKey)];
  HFSExtentKey     *hfsKey     = (HFSExtentKey *)key;
  HFSPlusExtentKey *hfsPlusKey = (HFSPlusExtentKey *)key;
  
  // Make the extents key.
  if (gIsHFSPlus) {
    hfsPlusKey->forkType   = 0;
    hfsPlusKey->fileID     = fileID;
    hfsPlusKey->startBlock = startBlock;
  } else {
    hfsKey->forkType   = 0;
    hfsKey->fileID     = fileID;
    hfsKey->startBlock = startBlock;
  }
  
  return ReadBTreeEntry(kBTreeExtents, &key, entry, 0);
}

static long ReadBTreeEntry(long btree, void *key, char *entry, long *dirIndex)
{
  long             extentSize;
  void             *extent;
  short            extentFile;
  char             *nodeBuf;
  BTNodeDescriptor *node;
  long             nodeSize, result, entrySize;
  long             curNode, index, lowerBound, upperBound;
  char             *testKey, *recordData;
  
  // Figure out which tree is being looked at.
  if (btree == kBTreeCatalog) {
    if (gIsHFSPlus) {
      extent     = &gHFSPlus->catalogFile.extents;
      extentSize = gHFSPlus->catalogFile.logicalSize;
    } else {
      extent     = (HFSExtentDescriptor *)&gHFSMDB->drCTExtRec;
      extentSize = gHFSMDB->drCTFlSize;
    }
    extentFile = kHFSCatalogFileID;
  } else {
    if (gIsHFSPlus) {
      extent     = &gHFSPlus->extentsFile.extents;
      extentSize = gHFSPlus->extentsFile.logicalSize;
    } else {
      extent     = (HFSExtentDescriptor *)&gHFSMDB->drXTExtRec;
      extentSize = gHFSMDB->drXTFlSize;
    }
    extentFile = kHFSExtentsFileID;
  }
  
  // Read the BTree Header if needed.
  if (gBTHeaders[btree] == 0) {
    ReadExtent(extent, extentSize, extentFile, 0, 256,
	       gBTreeHeaderBuffer + btree * 256, 0);
    gBTHeaders[btree] = (BTHeaderRec *)(gBTreeHeaderBuffer + btree * 256 +
				       sizeof(BTNodeDescriptor));
  }
  
  curNode = gBTHeaders[btree]->rootNode;
  
  nodeSize = gBTHeaders[btree]->nodeSize;
  nodeBuf = (char *)malloc(nodeSize);
  node = (BTNodeDescriptor *)nodeBuf;
  
  while (1) {
    // Read the current node.
    ReadExtent(extent, extentSize, extentFile,
	       curNode * nodeSize, nodeSize, nodeBuf, 1);
    
    // Find the matching key.
    lowerBound = 0;
    upperBound = node->numRecords - 1;
    while (lowerBound <= upperBound) {
      index = (lowerBound + upperBound) / 2;
      
      GetBTreeRecord(index, nodeBuf, nodeSize, &testKey, &recordData);
      
      if (gIsHFSPlus) {
	if (btree == kBTreeCatalog) {
	  result = CompareHFSPlusCatalogKeys(key, testKey);
	} else {
	  result = CompareHFSPlusExtentsKeys(key, testKey);
	}
      } else {
	if (btree == kBTreeCatalog) {
	  result = CompareHFSCatalogKeys(key, testKey);
	} else {
	  result = CompareHFSExtentsKeys(key, testKey);
	}
      }
      
      if (result < 0) upperBound = index - 1;        // search < trial
      else if (result > 0) lowerBound = index + 1;   // search > trial
      else break;                                    // search = trial
    }
    
    if (result < 0) {
      index = upperBound;
      GetBTreeRecord(index, nodeBuf, nodeSize, &testKey, &recordData);
    }
    
    // Found the closest key... Recurse on it if this is an index node.
    if (node->kind == kBTIndexNode) {
      curNode = *((long *)recordData);
    } else break;
  }
  
  // Return error if the file was not found.
  if (result != 0) return -1;
  
  if (btree == kBTreeCatalog) {
    switch (*(short *)recordData) {
    case kHFSFolderRecord           : entrySize = 70;  break;
    case kHFSFileRecord             : entrySize = 102; break;
    case kHFSFolderThreadRecord     : entrySize = 46;  break;
    case kHFSFileThreadRecord       : entrySize = 46;  break;
    case kHFSPlusFolderRecord       : entrySize = 88;  break;
    case kHFSPlusFileRecord         : entrySize = 248; break;
    case kHFSPlusFolderThreadRecord : entrySize = 264; break;
    case kHFSPlusFileThreadRecord   : entrySize = 264; break;
    }
  } else {
    if (gIsHFSPlus) entrySize = sizeof(HFSPlusExtentRecord);
    else entrySize = sizeof(HFSExtentRecord);
  }
  
  bcopy(recordData, entry, entrySize);
  
  // Update dirIndex.
  if (dirIndex != 0) {
    index++;
    if (index == node->numRecords) {
      index = 0;
      curNode = node->fLink;
    }
    *dirIndex = curNode * nodeSize + index;
  }
  
  free(nodeBuf);
  
  return 0;
}

static void GetBTreeRecord(long index, char *nodeBuffer, long nodeSize,
	     char **key, char **data)
{
  long keySize;
  long recordOffset;
  
  recordOffset = *((short *)(nodeBuffer + (nodeSize - 2 * index - 2)));
  *key = nodeBuffer + recordOffset;
  if (gIsHFSPlus) {
    keySize = *(short *)*key;
    *data = *key + 2 + keySize;
  } else {
    keySize = **key;
    *data = *key + 2 + keySize - (keySize & 1);
  }
}
 
static long ReadExtent(char *extent, long extentSize,
		       long extentFile, long offset, long size,
		       void *buffer, long cache)
{
  long      lastOffset, blockNumber, countedBlocks = 0;
  long      nextExtent = 0, sizeRead = 0, readSize;
  long      nextExtentBlock, currentExtentBlock = 0;
  long long readOffset;
  long      extentDensity, sizeofExtent, currentExtentSize;
  char      *currentExtent, *extentBuffer = 0, *bufferPos = buffer;
  
  if (offset >= extentSize) return 0;
  
  if (gIsHFSPlus) {
    extentDensity = kHFSPlusExtentDensity;
    sizeofExtent = sizeof(HFSPlusExtentDescriptor);
  } else {
    extentDensity = kHFSExtentDensity;
    sizeofExtent = sizeof(HFSExtentDescriptor);
  }
  
  lastOffset = offset + size;
  while (offset < lastOffset) {
    blockNumber = offset / gBlockSize;
    
    // Find the extent for the offset.
    for (; ; nextExtent++) {
      if (nextExtent < extentDensity) {
	if ((countedBlocks+GetExtentSize(extent, nextExtent)-1)<blockNumber) {
	  countedBlocks += GetExtentSize(extent, nextExtent);
	  continue;
	}
	
	currentExtent = extent + nextExtent * sizeofExtent;
	break;
      }
      
      if (extentBuffer == 0) {
	extentBuffer = malloc(sizeofExtent * extentDensity);
	if (extentBuffer == 0) return -1;
      }
      
      nextExtentBlock = nextExtent / extentDensity;
      if (currentExtentBlock != nextExtentBlock) {
	ReadExtentsEntry(extentFile, countedBlocks, extentBuffer);
	currentExtentBlock = nextExtentBlock;
      }
      
      currentExtentSize = GetExtentSize(extentBuffer,
					nextExtent % extentDensity);
      
      if ((countedBlocks + currentExtentSize - 1) >= blockNumber) {
	currentExtent = extentBuffer + sizeofExtent *
	  (nextExtent % extentDensity);
	break;
      }
      
      countedBlocks += currentExtentSize;
    }
    
    readOffset = ((blockNumber - countedBlocks) * gBlockSize) +
      (offset % gBlockSize);
    
    readSize = GetExtentSize(currentExtent, 0) * gBlockSize - readOffset;
    if (readSize > (size - sizeRead)) readSize = size - sizeRead;
    
    readOffset += (long long)GetExtentStart(currentExtent, 0) * gBlockSize;
    
    CacheRead(gCurrentIH, bufferPos, gAllocationOffset + readOffset,
	      readSize, cache);
    
    sizeRead += readSize;
    offset += readSize;
    bufferPos += readSize;
  }
  
  if (extentBuffer) free(extentBuffer);
  
  return sizeRead;
}

static long GetExtentStart(void *extents, long index)
{
  long                    start;
  HFSExtentDescriptor     *hfsExtents     = extents;
  HFSPlusExtentDescriptor *hfsPlusExtents = extents;
  
  if (gIsHFSPlus) start = hfsPlusExtents[index].startBlock;
  else start = hfsExtents[index].startBlock;
  
  return start;
}

static long GetExtentSize(void *extents, long index)
{
  long                    size;
  HFSExtentDescriptor     *hfsExtents     = extents;
  HFSPlusExtentDescriptor *hfsPlusExtents = extents;
  
  if (gIsHFSPlus) size = hfsPlusExtents[index].blockCount;
  else size = hfsExtents[index].blockCount;
  
  return size;
}

static long CompareHFSCatalogKeys(void *key, void *testKey)
{
  HFSCatalogKey *searchKey, *trialKey;
  long          result, searchParentID, trialParentID;
  
  searchKey = key;
  trialKey  = testKey;
  
  searchParentID = searchKey->parentID;
  trialParentID = trialKey->parentID;
  
  // parent dirID is unsigned
  if (searchParentID > trialParentID)  result = 1;
  else if (searchParentID < trialParentID) result = -1;
  else {
    // parent dirID's are equal, compare names
    result = FastRelString(searchKey->nodeName, trialKey->nodeName);
  }
  
  return result;
}

static long CompareHFSPlusCatalogKeys(void *key, void *testKey)
{
  HFSPlusCatalogKey *searchKey, *trialKey;
  long              result, searchParentID, trialParentID;
  
  searchKey = key;
  trialKey  = testKey;
  
  searchParentID = searchKey->parentID;
  trialParentID = trialKey->parentID;
  
  // parent dirID is unsigned
  if (searchParentID > trialParentID)  result = 1;
  else if (searchParentID < trialParentID) result = -1;
  else {
    // parent dirID's are equal, compare names
    if ((searchKey->nodeName.length == 0) || (trialKey->nodeName.length == 0))
      result = searchKey->nodeName.length - trialKey->nodeName.length;
    else
      result = FastUnicodeCompare(&searchKey->nodeName.unicode[0],
				  searchKey->nodeName.length,
				  &trialKey->nodeName.unicode[0],
				  trialKey->nodeName.length);
  }
  
  return result;
}

static long CompareHFSExtentsKeys(void *key, void *testKey)
{
  HFSExtentKey *searchKey, *trialKey;
  long         result;
  
  searchKey = key;
  trialKey  = testKey;
  
  // assume searchKey < trialKey
  result = -1;            
  
  if (searchKey->fileID == trialKey->fileID) {
    // FileNum's are equal; compare fork types
    if (searchKey->forkType == trialKey->forkType) {
      // Fork types are equal; compare allocation block number
      if (searchKey->startBlock == trialKey->startBlock) {
	// Everything is equal
	result = 0;
      } else {
	// Allocation block numbers differ; determine sign
	if (searchKey->startBlock > trialKey->startBlock) result = 1;
      }
    } else {
      // Fork types differ; determine sign
      if (searchKey->forkType > trialKey->forkType) result = 1;
    }
  } else {
    // FileNums differ; determine sign
    if (searchKey->fileID > trialKey->fileID) result = 1;
  }
  
  return result;
}

static long CompareHFSPlusExtentsKeys(void *key, void *testKey)
{
  HFSPlusExtentKey *searchKey, *trialKey;
  long             result;
  
  searchKey = key;
  trialKey  = testKey;
  
  // assume searchKey < trialKey
  result = -1;            
  
  if (searchKey->fileID == trialKey->fileID) {
    // FileNum's are equal; compare fork types
    if (searchKey->forkType == trialKey->forkType) {
      // Fork types are equal; compare allocation block number
      if (searchKey->startBlock == trialKey->startBlock) {
	// Everything is equal
	result = 0;
      } else {
	// Allocation block numbers differ; determine sign
	if (searchKey->startBlock > trialKey->startBlock) result = 1;
      }
    } else {
      // Fork types differ; determine sign
      if (searchKey->forkType > trialKey->forkType) result = 1;
    }
  } else {
    // FileNums differ; determine sign
    if (searchKey->fileID > trialKey->fileID) result = 1;
  }
  
  return result;
}
