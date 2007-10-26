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
 *  ufs.c - File System Module for UFS.
 *
 *  Copyright (c) 1998-2004 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>

#include "ufs_byteorder.h"

#include <dirent.h>		// for MAXNAMLEN

typedef struct dinode Inode, *InodePtr;

// Private function prototypes

static char *ReadBlock(long fragNum, long fragOffset, long length,
		       char *buffer, long cache);
static long ReadInode(long inodeNum, InodePtr inode, long *flags, long *time);
static long ResolvePathToInode(char *filePath, long *flags,
			       InodePtr fileInode, InodePtr dirInode);
static long ReadDirEntry(InodePtr dirInode, long *fileInodeNum,
			 unsigned long *dirIndex, char **name);
static long FindFileInDir(char *fileName, long *flags,
			  InodePtr fileInode, InodePtr dirInode);
static char *ReadFileBlock(InodePtr fileInode, long fragNum, long blockOffset,
			   long length, char *buffer, long cache);
static long ReadFile(InodePtr fileInode, long *length,
		     void *base, long offset);


static CICell    gCurrentIH;
static long long gPartitionBase;
static char      gFSBuf[SBSIZE];
static struct fs *gFS;
static struct ufslabel  gUFSLabel;   // for UUID
static long      gBlockSize;
static long      gBlockSizeOld;
static long      gFragSize;
static long      gFragsPerBlock;
static char      *gTempBlock;
static char      gTempName[MAXNAMLEN + 1];
static char      gTempName2[MAXNAMLEN + 1];
static Inode     gRootInode;
static Inode     gFileInode;

// Public functions

long UFSInitPartition(CICell ih)
{
  int ret;

  if (ih == gCurrentIH) return 0;
  
  printf("UFSInitPartition: %x\n", ih);
  
  gCurrentIH = 0;

  // Assume UFS starts at the beginning of the device
  gPartitionBase = 0;
  
  // read the disk label to get the UUID
  // (rumor has it that UFS headers can be either-endian on disk; hopefully
  // that isn't true for this UUID field).
  Seek(ih, gPartitionBase + UFS_LABEL_OFFSET);
  ret = Read(ih, (long)&gUFSLabel, UFS_LABEL_SIZE);
  if(ret != UFS_LABEL_SIZE)
    bzero(&gUFSLabel, UFS_LABEL_SIZE);
  
  // Look for the Super Block
  Seek(ih, gPartitionBase + SBOFF);
  Read(ih, (long)gFSBuf, SBSIZE);
  
  gFS = (struct fs *)gFSBuf;
//printf("looking for UFS magic ... \n");
  if (gFS->fs_magic != FS_MAGIC) {
    return -1;
  }
//printf("continuing w/UFS\n");
  
  // Calculate the block size and set up the block cache.
  gBlockSize = gFS->fs_bsize;
  gFragSize  = gFS->fs_fsize;
  gFragsPerBlock = gBlockSize / gFragSize;
  
  if (gBlockSizeOld <= gBlockSize) {
    gTempBlock = AllocateBootXMemory(gBlockSize);
  }
  
  CacheInit(ih, gBlockSize);
  
  gBlockSizeOld = gBlockSize;
  
  gCurrentIH = ih;
  
  // Read the Root Inode
  ReadInode(ROOTINO, &gRootInode, 0, 0);
  
  return 0;
}


long UFSGetUUID(CICell ih, char *uuidStr)
{
  long long uuid = gUFSLabel.ul_uuid;

  if (UFSInitPartition(ih) == -1) return -1;
  if (uuid == 0LL)  return -1;

  return CreateUUIDString((uint8_t*)(&uuid), sizeof(uuid), uuidStr);
}

long UFSLoadFile(CICell ih, char *filePath)
{
  return UFSReadFile(ih, filePath, (void *)kLoadAddr, 0, 0);
}

long UFSReadFile(CICell ih, char *filePath, void *base,
		 unsigned long offset, unsigned long length)
{
  long ret, flags;
  
  if (UFSInitPartition(ih) == -1) return -1;
  
  printf("%s UFS file: [%s] from %x.\n",
	 (((offset == 0) && (length == 0)) ? "Loading" : "Reading"),
	 filePath, ih);
  
  // Skip one or two leading '\'.
  if (*filePath == '\\') filePath++;
  if (*filePath == '\\') filePath++;
  ret = ResolvePathToInode(filePath, &flags, &gFileInode, &gRootInode);
  if ((ret == -1) || ((flags & kFileTypeMask) != kFileTypeFlat)) return -1;
  
  if (flags & (kOwnerNotRoot | kPermGroupWrite | kPermOtherWrite)) {
    printf("%s: permissions incorrect\n", filePath);
    return -1;
  }
  
  ret = ReadFile(&gFileInode, &length, base, offset);
  if (ret == -1) return -1;
  
  return length;
}


long UFSGetDirEntry(CICell ih, char *dirPath, unsigned long *dirIndex,
		    char **name, long *flags, long *time)
{
  long  ret, fileInodeNum, dirFlags;
  Inode tmpInode;
  
  if (UFSInitPartition(ih) == -1) return -1;
  
  // Skip a leading '\' if present
  if (*dirPath == '\\') dirPath++;
  if (*dirPath == '\\') dirPath++;
  ret = ResolvePathToInode(dirPath, &dirFlags, &gFileInode, &gRootInode);
  if ((ret == -1) || ((dirFlags & kFileTypeMask) != kFileTypeDirectory))
    return -1;
  
  ret = ReadDirEntry(&gFileInode, &fileInodeNum, dirIndex, name);
  if (ret != 0) return ret;
  
  ReadInode(fileInodeNum, &tmpInode, flags, time);
  
  return 0;
}

// Private functions

static char *ReadBlock(long fragNum, long blockOffset, long length,
		       char *buffer, long cache)
{
  long long offset;
  long blockNum;
  
  blockNum = fragNum / gFragsPerBlock;
  fragNum -= blockNum * gFragsPerBlock;
  
  blockOffset += fragNum * gFragSize;
  
  offset = gPartitionBase + 1ULL * blockNum * gBlockSize;
  
  if (cache && ((blockOffset + length) <= gBlockSize)) {
    CacheRead(gCurrentIH, gTempBlock, offset, gBlockSize, 1);
    if (buffer != 0) bcopy(gTempBlock + blockOffset, buffer, length);
    else buffer = gTempBlock + blockOffset;
  } else {
    offset += blockOffset;
    CacheRead(gCurrentIH, buffer, offset, length, 0);
  }
  
  return buffer;
}


static long ReadInode(long inodeNum, InodePtr inode, long *flags, long *time)
{
  long fragNum = ino_to_fsba(gFS, inodeNum);
  long blockOffset = ino_to_fsbo(gFS, inodeNum) * sizeof(Inode);
  
  ReadBlock(fragNum, blockOffset, sizeof(Inode), (char *)inode, 1);
  
  if (time != 0) *time = inode->di_mtime;
  
  if (flags != 0) {
    switch (inode->di_mode & IFMT) {
    case IFREG: *flags = kFileTypeFlat; break;
    case IFDIR: *flags = kFileTypeDirectory; break;
    case IFLNK: *flags = kFileTypeLink; break;
    default :   *flags = kFileTypeUnknown; break;
    }
    
    *flags |= inode->di_mode & kPermMask;
    
    if (inode->di_uid != 0) {
      static int nwarnings = 0;
      if(nwarnings++ < 25)  // so we don't warn for all in an Extensions walk
      printf("non-root file owner detected: %d\n", inode->di_uid);
      *flags |= kOwnerNotRoot;
    }
  }
  
  return 0;
}


static long ResolvePathToInode(char *filePath, long *flags,
			       InodePtr fileInode, InodePtr dirInode)
{
  char *restPath;
  long ret, cnt;
  
  // if filePath is empty the we want this directory.
  if (*filePath == '\0') {
    bcopy((char *)dirInode, (char *)fileInode, sizeof(Inode));
    return 0;
  }
  
  // Copy the file name to gTempName
  cnt = 0;
  while ((filePath[cnt] != '\\') && (filePath[cnt] != '\0')) cnt++;
  strncpy(gTempName, filePath, cnt);
  
  // Move restPath to the right place.
  if (filePath[cnt] != '\0') cnt++;
  restPath = filePath + cnt;
  
  // gTempName is a name in the current Dir.
  // restPath is the rest of the path if any.
  
  ret = FindFileInDir(gTempName, flags, fileInode, dirInode);
  if (ret == -1) return -1;
  
  if ((*restPath != '\0') && ((*flags & kFileTypeMask) == kFileTypeDirectory))
    ret = ResolvePathToInode(restPath, flags, fileInode, fileInode);
  
  return ret;
}


static long ReadDirEntry(InodePtr dirInode, long *fileInodeNum,
			 unsigned long *dirIndex, char **name)
{
  struct direct *dir;
  char          *buffer;
  long          index;
  long          dirBlockNum, dirBlockOffset;
  
  while (1) {
    index = *dirIndex;
    
    dirBlockOffset = index % DIRBLKSIZ;
    dirBlockNum    = index / DIRBLKSIZ;
    
    buffer = ReadFileBlock(dirInode, dirBlockNum, 0, DIRBLKSIZ, 0, 1);
    if (buffer == 0) return -1;
    
    dir = (struct direct *)(buffer + dirBlockOffset);
    *dirIndex += dir->d_reclen;
    
    if (dir->d_ino != 0) break;
    
    if (dirBlockOffset != 0) return -1;
  }
  
  *fileInodeNum = dir->d_ino;
  *name = strncpy(gTempName2, dir->d_name, dir->d_namlen);
  
  return 0;
}


static long FindFileInDir(char *fileName, long *flags,
			  InodePtr fileInode, InodePtr dirInode)
{
  long ret, inodeNum;
  unsigned long index = 0;
  char *name;
  
  while (1) {
    ret = ReadDirEntry(dirInode, &inodeNum, &index, &name);
    if (ret == -1) return -1;
    
    if (strcmp(fileName, name) == 0) break;
  }
  
  ReadInode(inodeNum, fileInode, flags, 0);
  
  return 0;
}


static char *ReadFileBlock(InodePtr fileInode, long fragNum, long blockOffset,
			   long length, char *buffer, long cache)
{
  long fragCount, blockNum;
  long diskFragNum, indFragNum, indBlockOff, refsPerBlock;
  char *indBlock;
  
  fragCount = (fileInode->di_size + gFragSize - 1) / gFragSize;
  if (fragNum >= fragCount) return 0;
  
  refsPerBlock = gBlockSize / sizeof(ufs_daddr_t);
  
  blockNum = fragNum / gFragsPerBlock;
  fragNum -= blockNum * gFragsPerBlock;
  
  // Get Direct Block Number.
  if (blockNum < NDADDR) {
    diskFragNum = fileInode->di_db[blockNum];
  } else {
    blockNum -= NDADDR;
    
    // Get Single Indirect Fragment Number.
    if (blockNum < refsPerBlock) {
      indFragNum = fileInode->di_ib[0];
    } else {
      blockNum -= refsPerBlock;
      
      // Get Double Indirect Fragment Number.
      if (blockNum < (refsPerBlock * refsPerBlock)) {
	indFragNum = fileInode->di_ib[1];
      } else {
	blockNum -= refsPerBlock * refsPerBlock;
	
	// Get Triple Indirect Fragment Number.
	indFragNum = fileInode->di_ib[2];
	
	indBlock = ReadBlock(indFragNum, 0, gBlockSize, 0, 1);
	indBlockOff = blockNum / (refsPerBlock * refsPerBlock);
	blockNum %= (refsPerBlock * refsPerBlock);
	indFragNum = ((ufs_daddr_t *)indBlock)[indBlockOff];
      }
      
      indBlock = ReadBlock(indFragNum, 0, gBlockSize, 0, 1);
      indBlockOff = blockNum / refsPerBlock;
      blockNum %= refsPerBlock;
      indFragNum = ((ufs_daddr_t *)indBlock)[indBlockOff];
    }
    
    indBlock = ReadBlock(indFragNum, 0, gBlockSize, 0, 1);
    diskFragNum = ((ufs_daddr_t *)indBlock)[blockNum];
  }
  
  buffer = ReadBlock(diskFragNum+fragNum, blockOffset, length, buffer, cache);
  
  return buffer;
}


static long ReadFile(InodePtr fileInode, long *length, void *base, long offset)
{
  long bytesLeft, curSize, curFrag;
  char *buffer, *curAddr = (char *)base;
  
  bytesLeft = fileInode->di_size;
  
  if (offset > bytesLeft) {
    printf("Offset is too large.\n");
    return -1;
  }
  
  if ((*length == 0) || ((offset + *length) > bytesLeft)) {
    *length = bytesLeft - offset;
  }
  
  if (*length > kLoadSize) {
    printf("File is too large.\n");
    return -1;
  }
  
  bytesLeft = *length;
  curFrag = (offset / gBlockSize) * gFragsPerBlock;
  offset %= gBlockSize;
  
  while (bytesLeft) {
    curSize = gBlockSize;
    if (curSize > bytesLeft) curSize = bytesLeft;
    if (offset != 0) curSize -= offset;
    
    buffer = ReadFileBlock(fileInode, curFrag, offset, curSize, curAddr, 0);
    if (buffer == 0) break;
    
    if (offset != 0) offset = 0;
    
    curFrag += gFragsPerBlock;
    curAddr += curSize;
    bytesLeft -= curSize;
  }
  
  return bytesLeft;
}
