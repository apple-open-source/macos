/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 2002-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <string.h>
#include "boot1u.h"
#include <memory.h>
#include <saio_types.h>
#include <saio_internal.h>
#include <fdisk.h>

#include "ufs.h"

/*
 * trackbuf points to the start of the track cache. Biosread()
 * will store the sectors read from disk to this memory area.
 *
 * biosbuf points to a sector within the track cache, and is
 * updated by Biosread().
 */
#if 0
static const char * const trackbuf = (char *) ptov(BIOS_ADDR);
#endif
static const char * biosbuf = (char *) ptov(BIOS_ADDR);

/*
 * diskinfo unpacking.
 */
#define SPT(di)          ((di) & 0xff)
#define HEADS(di)        ((((di)>>8) & 0xff) + 1)
#define SPC(di)          (SPT(di) * HEADS(di))

#define BPS              512     /* sector size of the device */
#define N_CACHE_SECS     (BIOS_LEN / BPS)
#define UFS_FRONT_PORCH  0

#define ECC_CORRECTED_ERR 0x11

extern void   bios(biosBuf_t *bb);

static biosBuf_t bb;

int ebiosread(int dev, long sec, int count)
{
    int i;
    static struct {
        unsigned char  size;
        unsigned char  reserved;
        unsigned char  numblocks;
        unsigned char  reserved2;
        unsigned short bufferOffset;
        unsigned short bufferSegment;
        unsigned long  long startblock;
    } addrpacket = {0};
    addrpacket.size = sizeof(addrpacket);

    for (i=0;;) {
        bb.intno   = 0x13;
        bb.eax.r.h = 0x42;
        bb.edx.r.l = dev;
        bb.esi.rr  = OFFSET((unsigned)&addrpacket);
        bb.ds      = SEGMENT((unsigned)&addrpacket);
        addrpacket.reserved = addrpacket.reserved2 = 0;
        addrpacket.numblocks     = count;
        addrpacket.bufferOffset  = OFFSET(ptov(BIOS_ADDR));
        addrpacket.bufferSegment = SEGMENT(ptov(BIOS_ADDR));
        addrpacket.startblock    = sec;
        bios(&bb);
        if ((bb.eax.r.h == 0x00) || (i++ >= 5))
            break;

        /* reset disk subsystem and try again */
        bb.eax.r.h = 0x00;
        bios(&bb);
    }
    return bb.eax.r.h;
}

//==========================================================================
// Use BIOS INT13 calls to read the sector specified. This function will
// also perform read-ahead to cache a few subsequent sector to the sector
// cache.
// 
// Return:
//   0 on success, or an error code from INT13/F2 or INT13/F42 BIOS call.

static int Biosread( int biosdev, unsigned int secno )
{
    int rc;

    DEBUG_DISK(("Biosread dev %x sec %d \n", biosdev, secno));

    rc = ebiosread(biosdev, secno, 1);
    if (rc == ECC_CORRECTED_ERR) {
	rc = 0;
    }
    return rc;
}

//==========================================================================

static int readBytes( int biosdev, unsigned int blkno,
                      unsigned int byteCount, void * buffer )
{

    char * cbuf = (char *) buffer;
    int    error;
    int    copy_len;

    DEBUG_DISK(("%s: dev %x block %x [%d] -> 0x%x...", __FUNCTION__,
                biosdev, blkno, byteCount, (unsigned)cbuf));

    for ( ; byteCount; cbuf += BPS, blkno++ )
    {
        error = Biosread( biosdev, blkno );
        if ( error )
        {
            DEBUG_DISK(("error\n"));
            return (-1);
        }

        copy_len = (byteCount > BPS) ? BPS : byteCount;
        bcopy( biosbuf, cbuf, copy_len );
        byteCount -= copy_len;
    }

    DEBUG_DISK(("done\n"));

    return 0;    
}

//==========================================================================

//==========================================================================
// Handle seek request from filesystem modules.

void diskSeek( BVRef bvr, long long position )
{
    bvr->fs_boff = position / BPS;
}

//==========================================================================
// Handle read request from filesystem modules.

int diskRead( BVRef bvr, long addr, long length )
{
    return readBytes( bvr->biosdev,
                      bvr->fs_boff + bvr->part_boff,
                      length,
                      (void *) addr );
}


int
findUFSPartition(int dev, struct fdisk_part *_fp)
{
    struct disk_blk0 *db;
    struct fdisk_part *fp;
    int i, cc;
    unsigned long offset = 0;
    unsigned long firstoff = 0;
    int ext_index = -1;

    db = (struct disk_blk0 *)biosbuf;
    do {
        DEBUG_DISK(("reading at offset %d\n", offset));
        cc = Biosread(dev, offset);
        if (cc < 0) {
            return cc;
        }
        for (i=0; i<FDISK_NPART; i++) {
            DEBUG_DISK(("i=%d, offset %ld\n", i, offset));
            fp = (struct fdisk_part *)&db->parts[i];
            DEBUG_DISK(("systid %x\n", fp->systid));
            if (fp->systid == FDISK_UFS) {
                bcopy(fp, _fp, sizeof(struct fdisk_part));
                _fp->relsect += offset;
                DEBUG_DISK(("** found UFS at partition %d\n", i));
                return 0;
            } else if (fp->systid == FDISK_DOSEXT ||
                       fp->systid == 0x0F ||
                       fp->systid == 0x85) {
                ext_index = i;
            }
        }
        if (ext_index != -1) {
            DEBUG_DISK(("Found ext part at %d\n", ext_index));
            fp = (struct fdisk_part *)&db->parts[ext_index];
            ext_index = -1;
            offset = firstoff + fp->relsect;
            if (firstoff == 0) {
                firstoff = fp->relsect;
            }
            continue;
        }
        break;
    } while (1);
    return -1;
}


void
initUFSBVRef( BVRef bvr, int biosdev, const struct fdisk_part * part)
{
    bvr->biosdev        = biosdev;
    bvr->part_no        = 0;
    bvr->part_boff      = part->relsect + UFS_FRONT_PORCH/BPS,
        bvr->part_type      = part->systid;
    bvr->fs_loadfile    = UFSLoadFile;
    //bvr->fs_getdirentry = UFSGetDirEntry;
    bvr->description    = 0;
}

void putc(int ch)
{  
    bb.intno = 0x10;
    bb.ebx.r.h = 0x00;  /* background black */
    bb.ebx.r.l = 0x0F;  /* foreground white */
    bb.eax.r.h = 0x0e;
    bb.eax.r.l = ch;
    bios(&bb);
}

int bgetc(void)
{
    bb.intno = 0x16;
    bb.eax.r.h = 0x00;
    bios(&bb);
    return bb.eax.rr;
}

void delay(int ms)
{
    bb.intno = 0x15;
    bb.eax.r.h = 0x86;
    bb.ecx.rr = ms >> 16;
    bb.edx.rr = ms & 0xFFFF;
    bios(&bb);
}
