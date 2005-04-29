/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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
 *  Copyright (c) 1998-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <sl.h>

#include "ufs.h"
#include "ufs_byteorder.h"

typedef struct dinode Inode, *InodePtr;

// Private function prototypes

static char *ReadBlock(long fragNum, long fragOffset, long length,
                       char *buffer, long cache);
static long ReadInode(long inodeNum, InodePtr inode, long *flags, long *time);
static long ResolvePathToInode(char *filePath, long *flags,
                               InodePtr fileInode, InodePtr dirInode);
static long ReadDirEntry(InodePtr dirInode, long *fileInodeNum,
                         long *dirIndex, char **name);
static long FindFileInDir(char *fileName, long *flags,
                          InodePtr fileInode, InodePtr dirInode);
static char *ReadFileBlock(InodePtr fileInode, long fragNum, long blockOffset,
                           long length, char *buffer, long cache);
static long ReadFile(InodePtr fileInode, long *length, void *base, long offset);

#define kDevBlockSize (0x200)    // Size of each disk block.
#define kDiskLabelBlock (15)     // Block the DL is in.

#ifdef __i386__

static CICell    gCurrentIH;
static long long gPartitionBase;
static char      *gULBuf;
static char      *gFSBuf;
static struct fs *gFS;
static long      gBlockSize;
static long      gFragSize;
static long      gFragsPerBlock;
static char      *gTempBlock;
static char      *gTempName;
static char      *gTempName2;
static InodePtr  gRootInodePtr;
static InodePtr  gFileInodePtr;

#else  /* !__i386__ */

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
static Inode     _gRootInode;
static Inode     _gFileInode;
static InodePtr  gRootInodePtr = &_gRootInode;
static InodePtr  gFileInodePtr = &_gFileInode;

#endif /* !__i386__ */

// Public functions

long UFSInitPartition( CICell ih )
{
    if (ih == gCurrentIH) {
#ifdef __i386__
        CacheInit(ih, gBlockSize);
#endif
        return 0;
    }

#if !BOOT1
    verbose("UFSInitPartition: %x\n", ih);
#endif

    gCurrentIH = 0;

#ifdef __i386__
    if (!gULBuf) gULBuf = (char *) malloc(UFS_LABEL_SIZE);
    if (!gFSBuf) gFSBuf = (char *) malloc(SBSIZE);
    if (!gTempName) gTempName = (char *) malloc(MAXNAMLEN + 1);
    if (!gTempName2) gTempName2 = (char *) malloc(MAXNAMLEN + 1);
    if (!gRootInodePtr) gRootInodePtr = (InodePtr) malloc(sizeof(Inode));
    if (!gFileInodePtr) gFileInodePtr = (InodePtr) malloc(sizeof(Inode));
    if (!gULBuf || !gFSBuf || !gTempName || !gTempName2 ||
        !gRootInodePtr || !gFileInodePtr) return -1;
#endif

    // Assume there is no Disk Label
    gPartitionBase = 0;

    // Look for the Super Block
    Seek(ih, gPartitionBase + SBOFF);
    Read(ih, (long)gFSBuf, SBSIZE);

    gFS = (struct fs *)gFSBuf;
    byte_swap_superblock(gFS);

    if (gFS->fs_magic != FS_MAGIC) {
        return -1;
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
    ReadInode(ROOTINO, gRootInodePtr, 0, 0);

    return 0;
}

long UFSLoadFile( CICell ih, char * filePath )
{
    return UFSReadFile(ih, filePath, (void *)gFSLoadAddress, 0, 0);
}

long UFSReadFile( CICell ih, char * filePath, void * base, unsigned long offset, unsigned long length )
{
    long ret, flags;

#if !BOOT1
    verbose("Loading UFS file: [%s] from %x.\n", filePath, (unsigned)ih);
#endif

    if (UFSInitPartition(ih) == -1) return -1;

    // Skip one or two leading '/'.
    if (*filePath == '/') filePath++;
    if (*filePath == '/') filePath++;

    ret = ResolvePathToInode(filePath, &flags, gFileInodePtr, gRootInodePtr);
    if ((ret == -1) || ((flags & kFileTypeMask) != kFileTypeFlat)) return -1;

    ret = ReadFile(gFileInodePtr, &length, base, offset);
    if (ret == -1) return -1;
    
    return length;
}

#ifndef BOOT1

long UFSGetDirEntry( CICell ih, char * dirPath, long * dirIndex,
                     char ** name, long * flags, long * time,
                     FinderInfo * finderInfo, long * infoValid)
{
    long  ret, fileInodeNum, dirFlags;
    Inode tmpInode;

    if (UFSInitPartition(ih) == -1) return -1;

    if (infoValid) *infoValid = 0;

    // Skip a leading '/' if present
    if (*dirPath == '/') dirPath++;
    if (*dirPath == '/') dirPath++;

    ret = ResolvePathToInode(dirPath, &dirFlags, gFileInodePtr, gRootInodePtr);
    if ((ret == -1) || ((dirFlags & kFileTypeMask) != kFileTypeDirectory))
        return -1;

    ret = ReadDirEntry(gFileInodePtr, &fileInodeNum, dirIndex, name);
    if (ret != 0) return ret;

    ReadInode(fileInodeNum, &tmpInode, flags, time);

    return 0;
}

void
UFSGetDescription(CICell ih, char *str, long strMaxLen)
{
    if (UFSInitPartition(ih) == -1) { return; }

    struct ufslabel *ul;

    // Look for the Disk Label
    Seek(ih, 1ULL * UFS_LABEL_OFFSET);
    Read(ih, (long)gULBuf, UFS_LABEL_SIZE);
    
    ul = (struct ufslabel *)gULBuf;
    
    unsigned char magic_bytes[] = UFS_LABEL_MAGIC;
    int i;
    unsigned char *p = (unsigned char *)&ul->ul_magic;
    
    for (i=0; i<sizeof(magic_bytes); i++, p++) {
        if (*p != magic_bytes[i])
            return;
    }
    strncpy(str, ul->ul_name, strMaxLen);
}

long
UFSGetFileBlock(CICell ih, char *filePath, unsigned long long *firstBlock)
{
    long ret, flags;

    if (UFSInitPartition(ih) == -1) return -1;

    // Skip one or two leading '/'.
    if (*filePath == '/') filePath++;
    if (*filePath == '/') filePath++;

    ret = ResolvePathToInode(filePath, &flags, gFileInodePtr, gRootInodePtr);
    if ((ret == -1) || ((flags & kFileTypeMask) != kFileTypeFlat)) return -1;

    *firstBlock = (gPartitionBase + 1ULL * gFileInodePtr->di_db[0] * gBlockSize) / 512ULL;

    return 0;
}


#endif /* !BOOT1 */

// Private functions

static char * ReadBlock( long fragNum, long blockOffset, long length,
                         char * buffer, long cache )
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

static long ReadInode( long inodeNum, InodePtr inode, long * flags, long * time )
{
    long fragNum = ino_to_fsba(gFS, inodeNum);
    long blockOffset = ino_to_fsbo(gFS, inodeNum) * sizeof(Inode);

    ReadBlock(fragNum, blockOffset, sizeof(Inode), (char *)inode, 1);
    byte_swap_dinode_in(inode);

    if (time != 0) *time = inode->di_mtime;

    if (flags != 0) {
        switch (inode->di_mode & IFMT) {
            case IFREG: *flags = kFileTypeFlat; break;
            case IFDIR: *flags = kFileTypeDirectory; break;
            case IFLNK: *flags = kFileTypeLink; break;
            default :   *flags = kFileTypeUnknown; break;
        }

        *flags |= inode->di_mode & kPermMask;

        if (inode->di_uid != 0) *flags |= kOwnerNotRoot;
    }

    return 0;
}

static long ResolvePathToInode( char * filePath, long * flags,
                                InodePtr fileInode, InodePtr dirInode )
{
    char * restPath;
    long   ret, cnt;

    // if filePath is empty the we want this directory.
    if (*filePath == '\0') {
        bcopy((char *)dirInode, (char *)fileInode, sizeof(Inode));
        return 0;
    }

    // Copy the file name to gTempName
    cnt = 0;
    while ((filePath[cnt] != '/') && (filePath[cnt] != '\0')) cnt++;
    strlcpy(gTempName, filePath, cnt+1);

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

static long ReadDirEntry( InodePtr dirInode, long * fileInodeNum,
                          long * dirIndex, char ** name )
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
        byte_swap_dir_block_in((char *)dir, 1);

        *dirIndex += dir->d_reclen;
        
        if (dir->d_ino != 0) break;
        
        if (dirBlockOffset != 0) return -1;
    }

    *fileInodeNum = dir->d_ino;
    *name = strlcpy(gTempName2, dir->d_name, dir->d_namlen+1);

    return 0;
}

static long FindFileInDir( char * fileName, long * flags,
                           InodePtr fileInode, InodePtr dirInode )
{
    long ret, inodeNum, index = 0;
    char *name;

    while (1) {
        ret = ReadDirEntry(dirInode, &inodeNum, &index, &name);
        if (ret == -1) return -1;

        if (strcmp(fileName, name) == 0) break;
    }

    ReadInode(inodeNum, fileInode, flags, 0);

    return 0;
}

static char * ReadFileBlock( InodePtr fileInode, long fragNum, long blockOffset,
                             long length, char * buffer, long cache )
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
                indFragNum = SWAP_BE32(((ufs_daddr_t *)indBlock)[indBlockOff]);
            }

            indBlock = ReadBlock(indFragNum, 0, gBlockSize, 0, 1);
            indBlockOff = blockNum / refsPerBlock;
            blockNum %= refsPerBlock;
            indFragNum = SWAP_BE32(((ufs_daddr_t *)indBlock)[indBlockOff]);
        }

        indBlock = ReadBlock(indFragNum, 0, gBlockSize, 0, 1);
        diskFragNum = SWAP_BE32(((ufs_daddr_t *)indBlock)[blockNum]);
    }

    buffer = ReadBlock(diskFragNum+fragNum, blockOffset, length, buffer, cache);

    return buffer;
}

static long ReadFile( InodePtr fileInode, long * length, void * base, long offset )
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

    if (bytesLeft > kLoadSize) {
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
