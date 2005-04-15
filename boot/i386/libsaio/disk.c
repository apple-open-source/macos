/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
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
/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 *          INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *  This software is supplied under the terms of a license  agreement or 
 *  nondisclosure agreement with Intel Corporation and may not be copied 
 *  nor disclosed except in accordance with the terms of that agreement.
 *
 *  Copyright 1988, 1989 Intel Corporation
 */

/*
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 */

#include "bootstruct.h"
#include "libsaio.h"
#include "fdisk.h"
#include "ufs.h"
#include "hfs.h"

#include <limits.h>
#include <IOKit/storage/IOApplePartitionScheme.h>

#define BPS              512     /* sector size of the device */
#define CD_BPS           2048    /* CD-ROM block size */
#define N_CACHE_SECS     (BIOS_LEN / BPS)  /* Must be a multiple of 4 for CD-ROMs */
#define UFS_FRONT_PORCH  0
#define kAPMSector       2       /* Sector number of Apple partition map */
#define kAPMCDSector     8       /* Translated sector of Apple partition map on a CD */

/*
 * trackbuf points to the start of the track cache. Biosread()
 * will store the sectors read from disk to this memory area.
 *
 * biosbuf points to a sector within the track cache, and is
 * updated by Biosread().
 */
static const char * const trackbuf = (char *) ptov(BIOS_ADDR);
static const char * biosbuf;

/*
 * Map a disk drive to bootable volumes contained within.
 */
struct DiskBVMap {
    int                biosdev;  // BIOS device number (unique)
    BVRef              bvr;      // chain of boot volumes on the disk
    int                bvrcnt;   // number of boot volumes
    struct DiskBVMap * next;     // linkage to next mapping
};

static struct DiskBVMap * gDiskBVMap  = NULL;
static struct disk_blk0 * gBootSector = NULL;

extern void spinActivityIndicator();

static void getVolumeDescription(BVRef bvr, char * str, long strMaxLen);

//==========================================================================

static int getDriveInfo( int biosdev,  struct driveInfo *dip )
{
    static struct driveInfo cached_di;
    int cc;
    
    if ( !cached_di.valid || biosdev != cached_di.biosdev )
    {
	cc = get_drive_info(biosdev, &cached_di);
        if (cc < 0) {
	    cached_di.valid = 0;
	    return (-1); // BIOS call error
	}
    }

    bcopy(&cached_di, dip, sizeof(cached_di));

    return 0;
}

//==========================================================================
// Maps (E)BIOS return codes to message strings.

struct NamedValue {
    unsigned char value;
    const char *  name;
};

static const char * getNameForValue( const struct NamedValue * nameTable,
                                     unsigned char value )
{
    const struct NamedValue * np;

    for ( np = nameTable; np->value; np++)
        if (np->value == value)
            return np->name;

    return NULL;
}

#define ECC_CORRECTED_ERR 0x11

static const struct NamedValue bios_errors[] = {
    { 0x10, "Media error"                },
    { 0x11, "Corrected ECC error"        },
    { 0x20, "Controller or device error" },
    { 0x40, "Seek failed"                },
    { 0x80, "Device timeout"             },
    { 0xAA, "Drive not ready"            },
    { 0x00, 0                            }
};

static const char * bios_error(int errnum)
{
    static char  errorstr[] = "Error 0x00";
    const char * errname;

    errname = getNameForValue( bios_errors, errnum );
    if ( errname ) return errname;

    sprintf(errorstr, "Error 0x%02x", errnum);
    return errorstr;   // No string, print error code only
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
    static int xbiosdev, xcyl, xhead;
    static unsigned int xsec, xnsecs;
    static BOOL cache_valid = FALSE;
    struct driveInfo di;

    int  rc = -1;
    int  cyl, head, sec;
    int  tries = 0;
    int bps, divisor;

    if (getDriveInfo(biosdev, &di) < 0) {
	return -1;
    }
    if (di.no_emulation) {
	/* Always assume 2k block size; BIOS may lie about geometry */
	bps = 2048;
    } else {
	bps = di.di.params.phys_nbps;
    }
    divisor = bps / BPS;

    DEBUG_DISK(("Biosread dev %x sec %d bps %d\n", biosdev, secno, bps));

    // To read the disk sectors, use EBIOS if we can. Otherwise,
    // revert to the standard BIOS calls.

    if ((biosdev >= kBIOSDevTypeHardDrive) &&
        (di.uses_ebios & EBIOS_FIXED_DISK_ACCESS))
    {
        if (cache_valid &&
            (biosdev == xbiosdev) &&
            (secno >= xsec) &&
            ((unsigned int)secno < (xsec + xnsecs)))
        {
            biosbuf = trackbuf + (BPS * (secno - xsec));
            return 0;
        }

        xnsecs = N_CACHE_SECS;
        xsec   = (secno / divisor) * divisor;
        cache_valid = FALSE;

        while ((rc = ebiosread(biosdev, secno / divisor, xnsecs / divisor)) && (++tries < 5))
        {
            if (rc == ECC_CORRECTED_ERR) {
                /* Ignore corrected ECC errors */
                rc = 0;
                break;
            }
            error("  EBIOS read error: %s\n", bios_error(rc), rc);
            error("    Block %d Sectors %d\n", secno, xnsecs);
            sleep(1);
        }
    }
    else
    {
	/* spc = spt * heads */
	int spc = (di.di.params.phys_spt * di.di.params.phys_heads);
        cyl  = secno / spc;
        head = (secno % spc) / di.di.params.phys_spt;
        sec  = secno % di.di.params.phys_spt;

        if (cache_valid &&
            (biosdev == xbiosdev) &&
            (cyl == xcyl) &&
            (head == xhead) &&
            ((unsigned int)sec >= xsec) &&
            ((unsigned int)sec < (xsec + xnsecs)))
        {
            // this sector is in trackbuf cache
            biosbuf = trackbuf + (BPS * (sec - xsec));
            return 0;
        }

        // Cache up to a track worth of sectors, but do not cross a
        // track boundary.

        xcyl   = cyl;
        xhead  = head;
        xsec   = sec;
        xnsecs = ((unsigned int)(sec + N_CACHE_SECS) > di.di.params.phys_spt) ? (di.di.params.phys_spt - sec) : N_CACHE_SECS;
        cache_valid = FALSE;

        while ((rc = biosread(biosdev, cyl, head, sec, xnsecs)) &&
               (++tries < 5))
        {
            if (rc == ECC_CORRECTED_ERR) {
                /* Ignore corrected ECC errors */
                rc = 0;
                break;
            }
            error("  BIOS read error: %s\n", bios_error(rc), rc);
            error("    Block %d, Cyl %d Head %d Sector %d\n",
                  secno, cyl, head, sec);
            sleep(1);
        }
    }

    // If the BIOS reported success, mark the sector cache as valid.

    if (rc == 0) {
        cache_valid = TRUE;
    }
    biosbuf  = trackbuf + (secno % divisor) * BPS;
    xbiosdev = biosdev;
    
    spinActivityIndicator();

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

static int isExtendedFDiskPartition( const struct fdisk_part * part )
{
    static unsigned char extParts[] =
    {
        0x05,   /* Extended */
        0x0f,   /* Win95 extended */
        0x85,   /* Linux extended */
    };

    unsigned int i;

    for (i = 0; i < sizeof(extParts)/sizeof(extParts[0]); i++)
    {
        if (extParts[i] == part->systid) return 1;
    }
    return 0;
}

//==========================================================================

static int getNextFDiskPartition( int biosdev, int * partno,
                                  const struct fdisk_part ** outPart )
{
    static int                 sBiosdev = -1;
    static int                 sNextPartNo;
    static unsigned int        sExtBase;
    static unsigned int        sExtDepth;
    static struct fdisk_part * sExtPart;
    struct fdisk_part *        part;

    if ( sBiosdev != biosdev || *partno < 0 )
    {
        // Fetch MBR.
        if ( readBootSector( biosdev, DISK_BLK0, 0 ) ) return 0;

        sBiosdev    = biosdev;
        sNextPartNo = 0;
        sExtBase    = 0;
        sExtDepth   = 0;
        sExtPart    = NULL;
    }

    while (1)
    {
        part  = NULL;

        if ( sNextPartNo < FDISK_NPART )
        {
            part = (struct fdisk_part *) gBootSector->parts[sNextPartNo];
        }
        else if ( sExtPart )
        {
            unsigned int blkno = sExtPart->relsect + sExtBase;

            // Save the block offset of the first extended partition.

            if ( sExtDepth == 0 ) sExtBase = sExtPart->relsect;

            // Load extended partition table.

            if ( readBootSector( biosdev, blkno, 0 ) == 0 )
            {
                sNextPartNo = 0;
                sExtDepth++;
                sExtPart = NULL;
                continue;
            }
        }

        if ( part == NULL ) break;  // Reached end of partition chain.

        // Advance to next partition number.

        sNextPartNo++;

        // Assume at most one extended partition per table.

        if ( isExtendedFDiskPartition(part) )
        {
            sExtPart = part;
            continue;
        }

        // Skip empty slots.

        if ( part->systid == 0x00 )
        {
            continue;
        }

        // Change relative offset to an absolute offset.

        part->relsect += sExtBase;

        *outPart = part;
        *partno  = sExtDepth ? (int)(sExtDepth + 4) : sNextPartNo;

        break;
    }

    return (part != NULL);
}

//==========================================================================

static BVRef newFDiskBVRef( int biosdev, int partno, unsigned int blkoff,
                            const struct fdisk_part * part,
                            FSInit initFunc, FSLoadFile loadFunc,
                            FSGetDirEntry getdirFunc, int probe, int type )
{
    BVRef bvr = (BVRef) malloc( sizeof(*bvr) );
    if ( bvr )
    {
        bzero(bvr, sizeof(*bvr));

        bvr->biosdev        = biosdev;
        bvr->part_no        = partno;
        bvr->part_boff      = blkoff;
        bvr->part_type      = part->systid;
        bvr->fs_loadfile    = loadFunc;
        bvr->fs_getdirentry = getdirFunc;
        bvr->description    = getVolumeDescription;
	bvr->type           = type;

        if ( part->bootid & FDISK_ACTIVE )
            bvr->flags |= kBVFlagPrimary;

        // Probe the filesystem.

        if ( initFunc )
        {
            bvr->flags |= kBVFlagNativeBoot;

            if ( probe && initFunc( bvr ) != 0 )
            {
                // filesystem probe failed.

                DEBUG_DISK(("%s: failed probe on dev %x part %d\n",
                            __FUNCTION__, biosdev, partno));

                free(bvr);
                bvr = NULL;
            }
        }
        else if ( readBootSector( biosdev, blkoff, (void *)0x7e00 ) == 0 )
        {
            bvr->flags |= kBVFlagForeignBoot;
        }
        else
        {
            free(bvr);
            bvr = NULL;
        }
    }
    return bvr;
}

//==========================================================================

BVRef newAPMBVRef( int biosdev, int partno, unsigned int blkoff,
                   const DPME * part,
                   FSInit initFunc, FSLoadFile loadFunc,
                   FSGetDirEntry getdirFunc, int probe, int type )
{
    BVRef bvr = (BVRef) malloc( sizeof(*bvr) );
    if ( bvr )
    {
        bzero(bvr, sizeof(*bvr));

        bvr->biosdev        = biosdev;
        bvr->part_no        = partno;
        bvr->part_boff      = blkoff;
        bvr->fs_loadfile    = loadFunc;
        bvr->fs_getdirentry = getdirFunc;
        bvr->description    = getVolumeDescription;
	bvr->type           = type;
        strlcpy(bvr->name, part->dpme_name, DPISTRLEN);
        strlcpy(bvr->type_name, part->dpme_type, DPISTRLEN);

        /*
        if ( part->bootid & FDISK_ACTIVE )
            bvr->flags |= kBVFlagPrimary;
        */

        // Probe the filesystem.

        if ( initFunc )
        {
            bvr->flags |= kBVFlagNativeBoot;

            if ( probe && initFunc( bvr ) != 0 )
            {
                // filesystem probe failed.

                DEBUG_DISK(("%s: failed probe on dev %x part %d\n",
                            __FUNCTION__, biosdev, partno));

                free(bvr);
                bvr = NULL;
            }
        }
        /*
        else if ( readBootSector( biosdev, blkoff, (void *)0x7e00 ) == 0 )
        {
            bvr->flags |= kBVFlagForeignBoot;
        }
        */
        else
        {
            free(bvr);
            bvr = NULL;
        }
    }
    return bvr;
}

//==========================================================================

static BVRef diskScanFDiskBootVolumes( int biosdev, int * countPtr )
{
    const struct fdisk_part * part;
    struct DiskBVMap *        map;
    int                       partno  = -1;
    BVRef                     bvr;
    BVRef                     booterUFS = NULL;
    int                       spc;
    struct driveInfo          di;
    boot_drive_info_t         *dp;

    /* Initialize disk info */
    if (getDriveInfo(biosdev, &di) != 0) {
	return NULL;
    }
    dp = &di.di;
    spc = (dp->params.phys_spt * dp->params.phys_heads);
    if (spc == 0) {
	/* This is probably a CD-ROM; punt on the geometry. */
	spc = 1;
    }

    do {
        // Create a new mapping.

        map = (struct DiskBVMap *) malloc( sizeof(*map) );
        if ( map )
        {
            map->biosdev = biosdev;
            map->bvr     = NULL;
            map->bvrcnt  = 0;
            map->next    = gDiskBVMap;
            gDiskBVMap   = map;

            // Create a record for each partition found on the disk.

            while ( getNextFDiskPartition( biosdev, &partno, &part ) )
            {
                DEBUG_DISK(("%s: part %d [%x]\n", __FUNCTION__,
                            partno, part->systid));
                bvr = 0;

                switch ( part->systid )
                {
                    case FDISK_UFS:
                       bvr = newFDiskBVRef(
                                      biosdev, partno,
                                      part->relsect + UFS_FRONT_PORCH/BPS,
                                      part,
                                      UFSInitPartition,
                                      UFSLoadFile,
                                      UFSGetDirEntry,
                                      0,
				      kBIOSDevTypeHardDrive);
                        break;

                    case FDISK_HFS:
                        bvr = newFDiskBVRef(
                                      biosdev, partno,
                                      part->relsect,
                                      part,
                                      HFSInitPartition,
                                      HFSLoadFile,
                                      HFSGetDirEntry,
                                      0,
				      kBIOSDevTypeHardDrive);
                        break;

                    case FDISK_BOOTER:
                        booterUFS = newFDiskBVRef(
                                      biosdev, partno,
                                      ((part->relsect + spc - 1) / spc) * spc,
                                      part,
                                      UFSInitPartition,
                                      UFSLoadFile,
                                      UFSGetDirEntry,
                                      0,
				      kBIOSDevTypeHardDrive);
                        break;

                    default:
                        bvr = newFDiskBVRef(
                                      biosdev, partno,
                                      part->relsect,
                                      part,
                                      0, 0, 0, 0,
				      kBIOSDevTypeHardDrive);
                        break;
                }

                if ( bvr )
                {
                    bvr->next = map->bvr;
                    map->bvr  = bvr;
                    map->bvrcnt++;
                }
            }

            // Booting from a CD with an UFS filesystem embedded
            // in a booter partition.

            if ( booterUFS )
            {
                if ( map->bvrcnt == 0 )
                {
                    map->bvr = booterUFS;
                    map->bvrcnt++;
                }
                else free( booterUFS );
            }
        }
    } while (0);

    /*
     * If no FDisk partition, then we will check for
     * an Apple partition map elsewhere.
     */
#if 0
    if (map->bvrcnt == 0) {
	static struct fdisk_part cdpart;
	cdpart.systid = 0xCD;

	/* Let's try assuming we are on a hybrid HFS/ISO9660 CD. */
	bvr = newFDiskBVRef(
			    biosdev, 0,
			    0,
			    &cdpart,
			    HFSInitPartition,
			    HFSLoadFile,
			    HFSGetDirEntry,
			    0,
			    kBIOSDevTypeHardDrive);
	bvr->next = map->bvr;
	map->bvr = bvr;
	map->bvrcnt++;
    }
#endif

    if (countPtr) *countPtr = map ? map->bvrcnt : 0;

    return map ? map->bvr : NULL;
}

//==========================================================================

static BVRef diskScanAPMBootVolumes( int biosdev, int * countPtr )
{
    struct DiskBVMap *        map;
    struct Block0 *block0_p;
    unsigned int blksize;
    unsigned int factor;
    void *buffer = malloc(BPS);

    /* Check for alternate block size */
    if (readBytes( biosdev, 0, BPS, buffer ) != 0) {
        return NULL;
    }
    block0_p = buffer;
    if (NXSwapBigShortToHost(block0_p->sbSig) == BLOCK0_SIGNATURE) {
        blksize = NXSwapBigShortToHost(block0_p->sbBlkSize);
        if (blksize != BPS) {
            free(buffer);
            buffer = malloc(blksize);
        }
        factor = blksize / BPS;
    } else {
        blksize = BPS;
        factor = 1;
    }
    
    do {
        // Create a new mapping.

        map = (struct DiskBVMap *) malloc( sizeof(*map) );
        if ( map )
        {
            int error;
            DPME *dpme_p = (DPME *)buffer;
            UInt32 i, npart = UINT_MAX;
            BVRef bvr;

            map->biosdev = biosdev;
            map->bvr     = NULL;
            map->bvrcnt  = 0;
            map->next    = gDiskBVMap;
            gDiskBVMap   = map;

            for (i=0; i<npart; i++) {
                error = readBytes( biosdev, (kAPMSector + i) * factor, blksize, buffer );

                if (error || NXSwapBigShortToHost(dpme_p->dpme_signature) != DPME_SIGNATURE) {
                    break;
                }

                if (i==0) {
                    npart = NXSwapBigLongToHost(dpme_p->dpme_map_entries);
                }
                /*
                printf("name = %s, %s%s  %d -> %d [%d -> %d] {%d}\n",
                       dpme.dpme_name, dpme.dpme_type, (dpme.dpme_flags & DPME_FLAGS_BOOTABLE) ? "(bootable)" : "",
                       dpme.dpme_pblock_start, dpme.dpme_pblocks,
                       dpme.dpme_lblock_start, dpme.dpme_lblocks,
                       dpme.dpme_boot_block);
                */

                if (strcmp(dpme_p->dpme_type, "Apple_HFS") == 0) {
                    bvr = newAPMBVRef(biosdev,
                                      i,
                                      NXSwapBigLongToHost(dpme_p->dpme_pblock_start) * factor,
                                      dpme_p,
                                      HFSInitPartition,
                                      HFSLoadFile,
                                      HFSGetDirEntry,
                                      0,
                                      kBIOSDevTypeHardDrive);
                    bvr->next = map->bvr;
                    map->bvr = bvr;
                    map->bvrcnt++;
                }
            }
        }
    } while (0);

    free(buffer);

    if (countPtr) *countPtr = map ? map->bvrcnt : 0;

    return map ? map->bvr : NULL;
}

//==========================================================================

BVRef diskScanBootVolumes( int biosdev, int * countPtr )
{
    struct DiskBVMap *        map;
    BVRef bvr;
    int count = 0;

    // Find an existing mapping for this device.

    for ( map = gDiskBVMap; map; map = map->next ) {
        if ( biosdev == map->biosdev ) {
            count = map->bvrcnt;
            break;
        }
    }

    if (map == NULL) {
        bvr = diskScanFDiskBootVolumes(biosdev, &count);
        if (bvr == NULL) {
            bvr = diskScanAPMBootVolumes(biosdev, &count);
        }
    } else {
        bvr = map->bvr;
    }
    if (countPtr)  *countPtr = count;
    return bvr;
}


//==========================================================================

static const struct NamedValue fdiskTypes[] =
{
    { 0x07,         "Windows NTFS"   },
    { 0x0c,         "Windows FAT32"  },
    { 0x83,         "Linux"          },
    { FDISK_UFS,    "Apple UFS"      },
    { FDISK_HFS,    "Apple HFS"      },
    { FDISK_BOOTER, "Apple Boot/UFS" },
    { 0xCD,         "CD-ROM"         },
    { 0x00,         0                }  /* must be last */
};

static void getVolumeDescription( BVRef bvr, char * str, long strMaxLen )
{
    unsigned char type = (unsigned char) bvr->part_type;
    const char * name = getNameForValue( fdiskTypes, type );

    if (name == NULL)
        name = bvr->type_name;

    if ( name )
        sprintf( str, "hd(%d,%d) %s",
                 BIOS_DEV_UNIT(bvr), bvr->part_no, name );
    else
        sprintf( str, "hd(%d,%d) TYPE %02x",
                 BIOS_DEV_UNIT(bvr), bvr->part_no, type );
}


//==========================================================================

int readBootSector( int biosdev, unsigned int secno, void * buffer )
{
    struct disk_blk0 * bootSector = (struct disk_blk0 *) buffer;
    int                error;

    if ( bootSector == NULL )
    {
        if ( gBootSector == NULL )
        {
            gBootSector = (struct disk_blk0 *) malloc(sizeof(*gBootSector));
            if ( gBootSector == NULL ) return -1;
        }
        bootSector = gBootSector;
    }

    error = readBytes( biosdev, secno, BPS, bootSector );
    if ( error || bootSector->signature != DISK_SIGNATURE )
        return -1;

    return 0;
}

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

void turnOffFloppy(void)
{
    /*
     * Disable floppy:
     * Hold controller in reset,
     * disable DMA and IRQ,
     * turn off floppy motors.
     */
    outb(0x3F2, 0x00);
}
