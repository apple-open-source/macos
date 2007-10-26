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
 *  ext2.c - File System Module for Ext2.
 *
 *  Copyright (c) 1999-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>

#include "ext2fs.h"
#include "ext2fs_dinode.h"
#include "ext2fs_dir.h"

typedef struct ext2fs_dinode Inode, *InodePtr;

// Private function prototypes

static long HowMany(long bufferSize, long unitSize);
static char *ReadBlock(long blockNum, long blockOffset, long length,
		       char *buffer, long cache);
static long ReadInode(long inodeNum, InodePtr inode, long *flags, long *time);
static long ResolvePathToInode(char *filePath, long *flags,
                               InodePtr fileInode, InodePtr dirInode);
static long ReadDirEntry(InodePtr dirInode, long *fileInodeNum,
			 unsigned long *dirIndex, char **name);
static long FindFileInDir(char *fileName, long *flags,
                          InodePtr fileInode, InodePtr dirInode);
static char *ReadFileBlock(InodePtr fileInode, long blockNum, long blockOffset,
			   long length, char *buffer, long cache);
static long ReadFile(InodePtr fileInode, long *length);


static CICell          gCurrentIH;
static char            gFSBuf[SBSIZE * 2];
static struct m_ext2fs *gFS;
static long            gBlockSize;
static long            gBlockSizeOld;
static char            *gTempBlock;
static char            gTempName[EXT2FS_MAXNAMLEN + 1];
static char            gTempName2[EXT2FS_MAXNAMLEN + 1];
static Inode           gRootInode;
static Inode           gFileInode;

// Public functions

long Ext2InitPartition(CICell ih)
{
  long cnt, gdPerBlock;
  
  if (ih == gCurrentIH) return 0;
  
  printf("Ext2InitPartition: %x\n", ih);
  
  gCurrentIH = 0;
  
  // Read for the Super Block.
  Seek(ih, SBOFF);
  Read(ih, (long)gFSBuf, SBSIZE);
  
  gFS = (struct m_ext2fs *)gFSBuf;
  e2fs_sb_bswap(&gFS->e2fs, &gFS->e2fs);
  if (gFS->e2fs.e2fs_magic != E2FS_MAGIC) return -1;
  
  // Calculate the block size and set up the block cache.
  gBlockSize = 1024 << gFS->e2fs.e2fs_log_bsize;
  if (gBlockSizeOld <= gBlockSize) {
    gTempBlock = AllocateBootXMemory(gBlockSize);
  }
  CacheInit(ih, gBlockSize);
  
  gBlockSizeOld = gBlockSize;
  
  gCurrentIH = ih;
  
  gdPerBlock = gBlockSize / sizeof(struct ext2_gd);
  
  // Fill in the in memory super block fields.
  gFS->e2fs_bsize = 1024 << gFS->e2fs.e2fs_log_bsize;
  gFS->e2fs_bshift = LOG_MINBSIZE + gFS->e2fs.e2fs_log_bsize;
  gFS->e2fs_qbmask = gFS->e2fs_bsize - 1;
  gFS->e2fs_bmask = ~gFS->e2fs_qbmask;
  gFS->e2fs_fsbtodb = gFS->e2fs.e2fs_log_bsize + 1;
  gFS->e2fs_ncg = HowMany(gFS->e2fs.e2fs_bcount - gFS->e2fs.e2fs_first_dblock,
			 gFS->e2fs.e2fs_bpg);
  gFS->e2fs_ngdb = HowMany(gFS->e2fs_ncg, gdPerBlock);
  gFS->e2fs_ipb = gFS->e2fs_bsize / EXT2_DINODE_SIZE;
  gFS->e2fs_itpg = gFS->e2fs.e2fs_ipg / gFS->e2fs_ipb;
  gFS->e2fs_gd = AllocateBootXMemory(gFS->e2fs_ngdb * gFS->e2fs_bsize);
  
  // Read the summary information from disk.
  for (cnt = 0; cnt < gFS->e2fs_ngdb; cnt++) {
    ReadBlock(((gBlockSize > 1024) ? 0 : 1) + cnt + 1, 0, gBlockSize,
	      (char *)&gFS->e2fs_gd[gdPerBlock * cnt], 0);
    e2fs_cg_bswap(&gFS->e2fs_gd[gdPerBlock * cnt],
		  &gFS->e2fs_gd[gdPerBlock * cnt], gBlockSize);
  }
  
  // Read the Root Inode
  ReadInode(EXT2_ROOTINO, &gRootInode, 0, 0);
  
  return 0;
}


long Ext2LoadFile(CICell ih, char *filePath)
{
  long ret, length, flags;
  
  if (Ext2InitPartition(ih) == -1) return -1;
  
  printf("Loading Ext2 file: [%s] from %x.\n", filePath, ih);
  
  // Skip one or two leading '\'.
  if (*filePath == '\\') filePath++;
  if (*filePath == '\\') filePath++;
  ret = ResolvePathToInode(filePath, &flags, &gFileInode, &gRootInode);
  if ((ret == -1) || ((flags & kFileTypeMask) != kFileTypeFlat)) return -1;
  
  if (flags & (kOwnerNotRoot | kPermGroupWrite | kPermOtherWrite)) return -1;
  
  ret = ReadFile(&gFileInode, &length);
  if (ret != 0) return -1;
  
  return length;
}

long Ext2GetDirEntry(CICell ih, char *dirPath, unsigned long *dirIndex,
		     char **name, long *flags, long *time)
{
  long  ret, fileInodeNum, dirFlags;
  Inode tmpInode;
  
  if (Ext2InitPartition(ih) == -1) return -1;
  
  // Skip a leading '\' if present
  if (dirPath[0] == '\\') dirPath++;
  ret = ResolvePathToInode(dirPath, &dirFlags, &gFileInode, &gRootInode);
  if ((ret == -1) || ((dirFlags & kFileTypeMask) != kFileTypeDirectory))
    return -1;
  
  ret = ReadDirEntry(&gFileInode, &fileInodeNum, dirIndex, name);
  if (ret != 0) return ret;
  
  ReadInode(fileInodeNum, &tmpInode, flags, time);
  
  return 0;
}

// XX no support in AppleFileSystemDriver yet
long Ext2GetUUID(CICell ih, char *uuidStr)
{
  uint8_t *uuid = gFS->e2fs.e2fs_uuid;

  if (Ext2InitPartition(ih) == -1) return -1;

  return CreateUUIDString(uuid, sizeof(gFS->e2fs.e2fs_uuid), uuidStr);
}

// Private functions

static long HowMany(long bufferSize, long unitSize)
{
  return (bufferSize + unitSize - 1) / unitSize;
}


static char *ReadBlock(long blockNum, long blockOffset, long length,
		      char *buffer, long cache)
{
  long long offset;
  
  offset = 1ULL * blockNum * gBlockSize;
  
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
  long blockNum = ino_to_fsba(gFS, inodeNum);
  long blockOffset = ino_to_fsbo(gFS, inodeNum) * sizeof(Inode);
  
  ReadBlock(blockNum, blockOffset, sizeof(Inode), (char *)inode, 1);
  e2fs_i_bswap(inode, inode);
  
  if (time != 0) *time = inode->e2di_mtime;
  
  if (flags != 0) {
    switch (inode->e2di_mode & EXT2_IFMT) {
    case EXT2_IFREG: *flags = kFileTypeFlat; break;
    case EXT2_IFDIR: *flags = kFileTypeDirectory; break;
    case EXT2_IFLNK: *flags = kFileTypeLink; break;
    default :        *flags = kFileTypeUnknown; break;
    }
    
    *flags |= inode->e2di_mode & kPermMask;
    
    if (inode->e2di_uid != 0) *flags |= kOwnerNotRoot;
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
  struct ext2fs_direct *dir;
  char                 *buffer;
  long                 offset, index;
  long                 blockNum, inodeNum;
  
  while (1) {
    index = *dirIndex;
    
    offset = index % gBlockSize;
    blockNum = index / gBlockSize;
    
    buffer = ReadFileBlock(dirInode, blockNum, 0, gBlockSize, 0, 1);
    if (buffer == 0) return -1;
    
    dir = (struct ext2fs_direct *)(buffer + offset);
    *dirIndex += bswap16(dir->e2d_reclen);
    
    inodeNum = bswap32(dir->e2d_ino);
    if (inodeNum != 0) break;
    
    if (offset != 0) return -1;
  }
  
  *fileInodeNum = inodeNum;
  *name = strncpy(gTempName2, dir->e2d_name, dir->e2d_namlen);
  
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


static char *ReadFileBlock(InodePtr fileInode, long blockNum, long blockOffset,
			   long length, char *buffer, long cache)
{
  long diskBlockNum, indBlockNum, indBlockOff, refsPerBlock;
  char *indBlock;
  
  if (blockNum >= fileInode->e2di_nblock) return 0;
  
  refsPerBlock = gBlockSize / sizeof(u_int32_t);
  
  // Get Direct Block Number.
  if (blockNum < NDADDR) {
    diskBlockNum = bswap32(fileInode->e2di_blocks[blockNum]);
  } else {
    blockNum -= NDADDR;
    
    // Get Single Indirect Block Number.
    if (blockNum < refsPerBlock) {
      indBlockNum = bswap32(fileInode->e2di_blocks[NDADDR]);
    } else {
      blockNum -= refsPerBlock;
      
      // Get Double Indirect Block Number.
      if (blockNum < (refsPerBlock * refsPerBlock)) {
		indBlockNum = bswap32(fileInode->e2di_blocks[NDADDR + 1]);
      } else {
		blockNum -= refsPerBlock * refsPerBlock;
	
	 	// Get Triple Indirect Block Number.
	 	indBlockNum = bswap32(fileInode->e2di_blocks[NDADDR + 2]);
	
	 	indBlock = ReadBlock(indBlockNum, 0, gBlockSize, 0, 1);
	 	indBlockOff = blockNum / (refsPerBlock * refsPerBlock);
	 	blockNum %= (refsPerBlock * refsPerBlock);
	 	indBlockNum = bswap32(((u_int32_t *)indBlock)[indBlockOff]);
      }
      
      indBlock = ReadBlock(indBlockNum, 0, gBlockSize, 0, 1);
      indBlockOff = blockNum / refsPerBlock;
      blockNum %= refsPerBlock;
      indBlockNum = bswap32(((u_int32_t *)indBlock)[indBlockOff]);
    }
    
    indBlock = ReadBlock(indBlockNum, 0, gBlockSize, 0, 1);
    diskBlockNum = bswap32(((u_int32_t *)indBlock)[blockNum]);
  }
  
  buffer = ReadBlock(diskBlockNum, blockOffset, length, buffer, cache);
  
  return buffer;
}

static long ReadFile(InodePtr fileInode, long *length)
{
  long bytesLeft, curSize, curBlock = 0;
  char *buffer, *curAddr = (char *)kLoadAddr;
  
  bytesLeft = *length = fileInode->e2di_size;
  
  if (*length > kLoadSize) {
    printf("File is too large.\n");
    return -1;
  }
  
  while (bytesLeft) {
    if (bytesLeft > gBlockSize) curSize = gBlockSize;
    else curSize = bytesLeft;
    
    buffer = ReadFileBlock(fileInode, curBlock, 0, curSize, curAddr, 0);
    if (buffer == 0) break;
    
    curBlock++;
    curAddr += curSize;
    bytesLeft -= curSize;
  }
  
  return bytesLeft;
}
