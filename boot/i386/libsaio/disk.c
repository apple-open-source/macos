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

#include "libsaio.h"
#include "fdisk.h"

/*
 * diskinfo unpacking.
 */
#define SPT(di)          ((di) & 0xff)
#define HEADS(di)        ((((di)>>8) & 0xff) + 1)
#define SPC(di)          (SPT(di) * HEADS(di))

#define BPS              512     /* sector size of the device */
#define N_CACHE_SECS     (BIOS_LEN / BPS)
#define UFS_FRONT_PORCH  0

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

extern long HFSInitPartition(CICell ih);
extern long HFSLoadFile(CICell ih, char * filePath);
extern long HFSGetDirEntry(CICell ih, char * dirPath, long * dirIndex,
                           char ** name, long * flags, long * time);

extern long UFSInitPartition(CICell ih);
extern long UFSLoadFile(CICell ih, char * filePath);
extern long UFSGetDirEntry(CICell ih, char * dirPath, long * dirIndex,
                           char ** name, long * flags, long * time);

extern void spinActivityIndicator();

static void getVolumeDescription(BVRef bvr, char * str, long strMaxLen);

//==========================================================================

static int getDiskGeometry( int biosdev, int * spt, int * spc )
{
    static int cached_biosdev = -1;
    static int cached_spt = 0;
    static int cached_spc = 0;
    
    if ( biosdev != cached_biosdev )
    {
        long di = get_diskinfo(biosdev);
        if (di == 0) return (-1); // BIOS call error

        cached_spt = SPT(di);
        cached_spc = cached_spt * HEADS(di);

        DEBUG_DISK(("%s: %d sectors, %d heads\n",
                    __FUNCTION__, cached_spt, (int)HEADS(di)));
    }

    *spt = cached_spt;
    *spc = cached_spc;

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

    int  rc = -1;
    int  cyl, head, sec;
    int  spt, spc;
    int  tries = 0;

    // DEBUG_DISK(("Biosread dev %x sec %d \n", biosdev, secno));

    // To read the disk sectors, use EBIOS if we can. Otherwise,
    // revert to the standard BIOS calls.

    if ((biosdev >= kBIOSDevTypeHardDrive) &&
        (uses_ebios[biosdev - kBIOSDevTypeHardDrive] & EBIOS_FIXED_DISK_ACCESS))
    {
        if (cache_valid &&
            (biosdev == xbiosdev) &&
            (secno >= xsec) &&
            (secno < (xsec + xnsecs)))
        {
            biosbuf = trackbuf + (BPS * (secno - xsec));
            return 0;
        }

        xnsecs = N_CACHE_SECS;
        xsec   = secno;
        cache_valid = FALSE;

        while ((rc = ebiosread(biosdev, secno, xnsecs)) && (++tries < 5))
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
    else if ( getDiskGeometry(biosdev, &spt, &spc) == 0 )
    {
        cyl  = secno / spc;
        head = (secno % spc) / spt;
        sec  = secno % spt;

        if (cache_valid &&
            (biosdev == xbiosdev) &&
            (cyl == xcyl) &&
            (head == xhead) &&
            (sec >= xsec) &&
            (sec < (xsec + xnsecs)))
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
        xnsecs = ((sec + N_CACHE_SECS) > spt) ? (spt - sec) : N_CACHE_SECS;
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
    biosbuf  = trackbuf;
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

    int i;

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
        *partno  = sExtDepth ? (sExtDepth + 4) : sNextPartNo;

        break;
    }

    return (part != NULL);
}

//==========================================================================

static BVRef newFDiskBVRef( int biosdev, int partno, unsigned int blkoff,
                            const struct fdisk_part * part,
                            FSInit initFunc, FSLoadFile loadFunc,
                            FSGetDirEntry getdirFunc, int probe )
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

BVRef diskScanBootVolumes( int biosdev, int * countPtr )
{
    const struct fdisk_part * part;
    struct DiskBVMap *        map;
    int                       partno  = -1;
    BVRef                     bvr;
    BVRef                     booterUFS = NULL;
    int                       spc, spt;

    do {
        // Find an existing mapping for this device.

        for ( map = gDiskBVMap; map; map = map->next )
        {
            if ( biosdev == map->biosdev ) break;
        }
        if ( map ) break;

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
                                      0 );
                        break;

                    case FDISK_HFS:
                        bvr = newFDiskBVRef(
                                      biosdev, partno,
                                      part->relsect,
                                      part,
                                      HFSInitPartition,
                                      HFSLoadFile,
                                      HFSGetDirEntry,
                                      0 );
                        break;

                    case FDISK_BOOTER:
                        if (getDiskGeometry(biosdev, &spt, &spc) != 0)
                            break;

                        booterUFS = newFDiskBVRef(
                                      biosdev, partno,
                                      ((part->relsect + spc - 1) / spc) * spc,
                                      part,
                                      UFSInitPartition,
                                      UFSLoadFile,
                                      UFSGetDirEntry,
                                      0 );
                        break;

                    default:
                        bvr = newFDiskBVRef(
                                      biosdev, partno,
                                      part->relsect,
                                      part,
                                      0, 0, 0, 0 );
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

    if (countPtr) *countPtr = map ? map->bvrcnt : 0;

    return map ? map->bvr : NULL;
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
    { 0x00,         0                }  /* must be last */
};

static void getVolumeDescription( BVRef bvr, char * str, long strMaxLen )
{
    unsigned char type = (unsigned char) bvr->part_type;
    const char * name = getNameForValue( fdiskTypes, type );

    if ( name )
        sprintf( str, "hd(%d,%d) %s",
                 BIOS_DEV_UNIT(bvr->biosdev), bvr->part_no, name );
    else
        sprintf( str, "hd(%d,%d) TYPE %02x",
                 BIOS_DEV_UNIT(bvr->biosdev), bvr->part_no, type );
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
