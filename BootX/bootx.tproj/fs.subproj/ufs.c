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
 *  ufs.c - File System Module for UFS.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>
#include "ufs_byteorder.h"

typedef struct dinode Inode, *InodePtr;

// Private function prototypes

static char *ReadBlock(long fragNum, long fragOffset, long length,
		       char *buffer, long cache);
static long ReadInode(long inodeNum, InodePtr inode);
static long ResolvePathToInode(char *filePath, long *flags,
			       InodePtr fileInode, InodePtr dirInode);
static long ReadDirEntry(InodePtr dirInode, long *fileInodeNum,
			 long *dirIndex, char **name, long *flags, long *time);
static long FindFileInDir(char *fileName, long *flags,
			  InodePtr fileInode, InodePtr dirInode);
static char *ReadFileBlock(InodePtr fileInode, long fragNum, long blockOffset,
			   long length, char *buffer, long cache);
static long ReadFile(InodePtr fileInode, long *length);


#define kDevBlockSize (0x200)    // Size of each disk block.
#define kDiskLableBlock (15)     // Block the DL is in.

static CICell    gCurrentIH;
static long long gPartitionBase;
static char      gDLBuf[8192];
static char      gFSBuf[SBSIZE];
static struct fs *gFS;
static long      gBlockSize;
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
  disk_label_t *dl;
  partition_t  *part;
  
  if (ih == gCurrentIH) return 0;
  
  printf("UFSInitPartition: %x\n", ih);
  
  gCurrentIH = 0;
  
  // Assume there is no Disk Label
  gPartitionBase = 0;
  
  // Look for the Super Block
  Seek(ih, gPartitionBase + SBOFF);
  Read(ih, (long)gFSBuf, SBSIZE);
  
  gFS = (struct fs *)gFSBuf;
  if (gFS->fs_magic != FS_MAGIC) {
    // Did not find it... Look for the Disk Label.
    // Look for the Disk Label
    Seek(ih, 1ULL * kDevBlockSize * kDiskLableBlock);
    Read(ih, (long)gDLBuf, 8192);
    
    dl = (disk_label_t *)gDLBuf;
    byte_swap_disklabel_in(dl);
    
    if (dl->dl_version != DL_VERSION) {
      return -1;
    }
    
    part = &dl->dl_part[0];
    gPartitionBase = (1ULL * (dl->dl_front + part->p_base) * dl->dl_secsize) -
      (1ULL * (dl->dl_label_blkno - kDiskLableBlock) * kDevBlockSize);
    
    // Re-read the Super Block.
    Seek(ih, gPartitionBase + SBOFF);
    Read(ih, (long)gFSBuf, SBSIZE);
    
    gFS = (struct fs *)gFSBuf;
    if (gFS->fs_magic != FS_MAGIC) {
      return -1;
    }
  }
  
  // Calculate the block size and set up the block cache.
  gBlockSize = gFS->fs_bsize;
  gFragSize  = gFS->fs_fsize;
  gFragsPerBlock = gBlockSize / gFragSize;
  if (gTempBlock != 0) free(gTempBlock);
  gTempBlock = malloc(gBlockSize);
  CacheInit(ih, gBlockSize);
  
  gCurrentIH = ih;
  
  // Read the Root Inode
  ReadInode(ROOTINO, &gRootInode);
  
  return 0;
}


long UFSLoadFile(CICell ih, char *filePath)
{
  long ret, length, flags;
  
  if (UFSInitPartition(ih) == -1) return -1;
  
  printf("Loading UFS file: [%s] from %x.\n", filePath, ih);
  
  // Skip one or two leading '\'.
  if (*filePath == '\\') filePath++;
  if (*filePath == '\\') filePath++;
  ret = ResolvePathToInode(filePath, &flags, &gFileInode, &gRootInode);
  if ((ret == -1) || (flags != kFlatFileType)) return -1;
  
  ret = ReadFile(&gFileInode, &length);
  if (ret == -1) return -1;
  
  return length;
}


long UFSGetDirEntry(CICell ih, char *dirPath, long *dirIndex,
		    char **name, long *flags, long *time)
{
  long ret, fileInodeNum, dirFlags;
  
  if (UFSInitPartition(ih) == -1) return -1;
  
  // Skip a leading '\' if present
  if (*dirPath == '\\') dirPath++;
  if (*dirPath == '\\') dirPath++;
  ret = ResolvePathToInode(dirPath, &dirFlags, &gFileInode, &gRootInode);
  if ((ret == -1) || (dirFlags != kDirectoryFileType)) return -1;
  
  do {
    ret = ReadDirEntry(&gFileInode, &fileInodeNum, dirIndex,
		       name, flags, time);
  } while ((ret != -1) && (*flags == kUnknownFileType));
  
  return ret;
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


static long ReadInode(long inodeNum, InodePtr inode)
{
  long fragNum = ino_to_fsba(gFS, inodeNum);
  long blockOffset = ino_to_fsbo(gFS, inodeNum) * sizeof(Inode);
  
  ReadBlock(fragNum, blockOffset, sizeof(Inode), (char *)inode, 1);
  
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
  
  if ((*restPath != '\0') && (*flags == kDirectoryFileType))
    ret = ResolvePathToInode(restPath, flags, fileInode, fileInode);
  
  return ret;
}


static long ReadDirEntry(InodePtr dirInode, long *fileInodeNum,
			 long *dirIndex, char **name, long *flags, long *time)
{
  struct direct *dir;
  char          *buffer;
  long          index = *dirIndex;
  long          dirBlockNum, dirBlockOffset;
  Inode         tmpInode;
  
  dirBlockOffset = index % DIRBLKSIZ;
  dirBlockNum    = index / DIRBLKSIZ;
  
  buffer = ReadFileBlock(dirInode, dirBlockNum, 0, DIRBLKSIZ, 0, 1);
  if (buffer == 0) return -1;
  
  dir = (struct direct *)(buffer + dirBlockOffset);
  if (dir->d_ino == 0) return -1;
  
  *dirIndex += dir->d_reclen;
  *fileInodeNum = dir->d_ino;
  *name = strncpy(gTempName2, dir->d_name, dir->d_namlen);
  
  ReadInode(dir->d_ino, &tmpInode);
  
  *time = tmpInode.di_mtime;
  
  switch (tmpInode.di_mode & IFMT) {
  case IFREG: *flags = kFlatFileType; break;
  case IFDIR: *flags = kDirectoryFileType; break;
  case IFLNK: *flags = kLinkFileType; break;
  default :   *flags = kUnknownFileType; break;
  }
  
  return 0;
}


static long FindFileInDir(char *fileName, long *flags,
			  InodePtr fileInode, InodePtr dirInode)
{
  long ret, inodeNum, time, index = 0;
  char *name;
  
  while (1) {
    ret = ReadDirEntry(dirInode, &inodeNum, &index, &name, flags, &time);
    if (ret == -1) return -1;
    
    if (*flags == kUnknownFileType) continue;
    
    if (strcmp(fileName, name) == 0) break;
  }
  
  ReadInode(inodeNum, fileInode);
  
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


static long ReadFile(InodePtr fileInode, long *length)
{
  long bytesLeft, curSize, curFrag = 0;
  char *buffer, *curAddr = (char *)kLoadAddr;
  
  bytesLeft = *length = fileInode->di_size;
  
  if (*length > kLoadSize) {
    printf("File is too large.\n");
    return -1;
  }
  
  while (bytesLeft) {
    if (bytesLeft > gBlockSize) curSize = gBlockSize;
    else curSize = bytesLeft;
    
    buffer = ReadFileBlock(fileInode, curFrag, 0, curSize, curAddr, 0);
    if (buffer == 0) break;
    
    curFrag += gFragsPerBlock;
    curAddr += curSize;
    bytesLeft -= curSize;
  }
  
  return bytesLeft;
}
