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

#define DRIVER_PRIVATE

#include "sys/types.h"
#include "legacy/disk.h"
#include "legacy/fdisk.h"
#include "libsaio.h"
#include "memory.h"

/*
 * Type and constant definitions.
 */
typedef struct disk_blk0   boot_sector;
#define BOOT_SIGNATURE     DISK_SIGNATURE
#define PART_TYPE_EXT      0x05
#define PART_TYPE_APPLE    0xa8
#define UFS_FRONT_PORCH    0

#if		DEBUG
#define DPRINT(x)       { printf x; }
#define DSPRINT(x)      { printf x; sleep(1); }
#else
#define DPRINT(x)
#define DSPRINT(x)
#endif

/*
 * Function prototypes.
 */
extern void   spinActivityIndicator();
static void   diskActivityHook();
static int    Biosread(int biosdev, int secno);
static struct fdisk_part * find_partition(u_int8_t type,
                                          u_int8_t biosdev,
                                          BOOL     mba);

/*
 * diskinfo unpacking.
 */
#define SPT(di)         ((di) & 0xff)
#define HEADS(di)       ((((di)>>8) & 0xff) + 1)
#define SPC(di)         (SPT(di) * HEADS(di))
#define BPS             512     /* sector size of the device */
#define N_CACHE_SECS    (BIOS_LEN / BPS)

/*
 * Stores the geometry of the disk in order to
 * perform LBA to CHS translation for non EBIOS calls.
 */
static struct diskinfo {
    int spt;            /* sectors per track */
    int spc;            /* sectors per cylinder */
} diskinfo;

/*
 * Globals variables.
 */
int     label_secsize = BPS;
char *  b[NBUFS];
daddr_t blknos[NBUFS];
struct  iob iob[NFILES];

/*
 * intbuf points to the start of the sector cache. BIOS calls will
 * store the sectors read into this memory area. If cache_valid
 * is TRUE, then intbuf contents are valid. Otherwise, ignore the
 * cache and read from disk.
 *
 * biosbuf points to a sector within the sector cache.
 */
static char * const intbuf = (char *)ptov(BIOS_ADDR);
static BOOL   cache_valid  = FALSE;
static char * biosbuf;

/*==========================================================================
 * 
 */
void
devopen(char * name, struct iob * io)
{
    static int          last_biosdev = -1;
    static daddr_t      last_offset  = 0;

    struct fdisk_part * part;
    long                di;

    io->i_error = 0;
    io->dirbuf_blkno = -1;

    // Use cached values if possible.
    //
    if (io->biosdev == last_biosdev) {
        io->i_boff = last_offset;
        return;
    }

    // initialize disk parameters -- spt and spc
    // must do this before doing reads from the device.

    di = get_diskinfo(io->biosdev);
    if (di == 0) {
        io->i_error = ENXIO;
        return;
    }

    diskinfo.spt = SPT(di);
    diskinfo.spc = diskinfo.spt * HEADS(di);

    // FIXME - io->partition is ignored. Doesn't make sense anymore.
    //         we also don't overwrite the 'name' argument.
    //         Whats special about "$LBL" ?

    part = find_partition(PART_TYPE_APPLE, io->biosdev, FALSE);
    if (part == NULL) {
        io->i_error = EIO;
        DSPRINT(("Unable to find partition: IO error\n"));
    } else {
        last_offset  = io->i_boff = part->relsect + UFS_FRONT_PORCH/BPS;
        last_biosdev = io->biosdev;
        DSPRINT(("partition offset: %x\n", io->i_boff));
    }
}

/*==========================================================================
 * 
 */
void devflush()
{
    cache_valid = FALSE;   // invalidate the sector cache (intbuf)
}

/*==========================================================================
 * 
 */
int devread(struct iob * io)
{
    long      sector;
    int       offset;
//  int       dev;

    io->i_flgs |= F_RDDATA;

    io->i_error = 0;        // assume the best

//  dev = io->i_ino.i_dev;

    sector = io->i_bn * (label_secsize/BPS);

    for (offset = 0; offset < io->i_cc; offset += BPS) {

        io->i_error = Biosread(io->biosdev, sector);
        if (io->i_error)
            return (-1);

        /* copy 1 sector from the internal buffer biosbuf into buf */
        bcopy(biosbuf, &io->i_ma[offset], BPS);

        sector++;
    }

    io->i_flgs &= ~F_TYPEMASK;

    return (io->i_cc);
}

/*==========================================================================
 * Maps (E)BIOS return codes to message strings.
 */
struct bios_error_info {
    int          errno;
    const char * string;
};

#define ECC_CORRECTED_ERR 0x11

static struct bios_error_info bios_errors[] = {
    {0x10, "Media error"},
    {0x11, "Corrected ECC error"},
    {0x20, "Controller or device error"},
    {0x40, "Seek failed"},
    {0x80, "Device timeout"},
    {0xAA, "Drive not ready"},
    {0x00, 0}
};

static const char *
bios_error(int errno)
{
    struct bios_error_info * bp;
    
    for (bp = bios_errors; bp->errno; bp++) {
        if (bp->errno == errno)
            return bp->string;
    }
    return "Error 0x%02x";   // No string, print error code only
}

/*==========================================================================
 * Use BIOS INT13 calls to read the sector specified. This function will
 * also perform read-ahead to cache a few subsequent sector to the sector
 * cache.
 *
 * The fields in diskinfo structure must be filled in before calling this
 * function.
 *
 * Return:
 *   Return code from INT13/F2 or INT13/F42 call. If operation was
 *   successful, 0 is returned.
 *
 */
static int
Biosread(int biosdev, int secno)
{
    static int xbiosdev, xcyl, xhead, xsec, xnsecs;
    
    extern unsigned char uses_ebios[];

    int  rc;
    int  cyl, head, sec;
    int  spt, spc;
    int  tries = 0;

    DSPRINT(("Biosread %d \n", secno));

    // To read the disk sectors, use EBIOS if we can. Otherwise,
    // revert to the standard BIOS calls.
    //
    if ((biosdev >= BIOS_DEV_HD) && uses_ebios[biosdev - BIOS_DEV_HD]) {
        if (cache_valid &&
            (biosdev == xbiosdev) &&
            (secno >= xsec) &&
            (secno < (xsec + xnsecs)))
        {
            biosbuf = intbuf + (BPS * (secno - xsec));
            return 0;
        }

        xnsecs = N_CACHE_SECS;
        xsec   = secno;

        while ((rc = ebiosread(biosdev, secno, xnsecs)) && (++tries < 5))
        {
            if (rc == ECC_CORRECTED_ERR) {
                /* Ignore corrected ECC errors */
                break;
            }
            error("  EBIOS read error: %s\n", bios_error(rc), rc);
            error("    Block %d Sectors %d\n", secno, xnsecs);
            sleep(1);
        }
    }
    else {
        spt = diskinfo.spt;    // From previous INT13/F8 call.
        spc = diskinfo.spc;

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
            // this sector is in intbuf cache
            biosbuf = intbuf + (BPS * (sec - xsec));
            return 0;
        }

        // Cache up to a track worth of sectors, but do not cross a
        // track boundary.
        //
        xcyl   = cyl;
        xhead  = head;
        xsec   = sec;
        xnsecs = ((sec + N_CACHE_SECS) > spt) ? (spt - sec) : N_CACHE_SECS;

        while ((rc = biosread(biosdev, cyl, head, sec, xnsecs)) &&
               (++tries < 5))
        {
            if (rc == ECC_CORRECTED_ERR) {
                /* Ignore corrected ECC errors */
                break;
            }
            error("  BIOS read error: %s\n", bios_error(rc), rc);
            error("    Block %d, Cyl %d Head %d Sector %d\n",
                  secno, cyl, head, sec);
            sleep(1);
        }
    }

    // If the BIOS reported success, mark the sector cache as valid.
    //
    if (rc == 0) {
        cache_valid = TRUE;
    }
    biosbuf  = intbuf;
    xbiosdev = biosdev;
    
    diskActivityHook();

    return rc;
}

/*==========================================================================
 * Replace this function if you want to change
 * the way disk activity is indicated to the user.
 */
void
diskActivityHook(void)
{
    spinActivityIndicator();
}

/*==========================================================================
 * Returns YES if the partition type specified is an extended fdisk
 * partition.
 */
static BOOL
isExtendedPartition(u_int8_t type)
{
	int i;

	u_int8_t extended_partitions[] = {
		0x05,	/* Extended */
		0x0f,	/* Win95 extended */
		0x85,	/* Linux extended */
	};

	for (i = 0;
		 i < sizeof(extended_partitions)/sizeof(extended_partitions[0]);
		 i++)
	{
		if (extended_partitions[i] == type)
			return YES;
	}
	return NO;
}

/*==========================================================================
 * Traverse the fdisk partition tables on disk until a partition is found
 * that matches the specified type.
 *
 * Arguments:
 *   type    - Partition type to search for (e.g. 0xa7 for NeXTSTEP).
 *   biosdev - BIOS device unit. 0x80 and up for hard-drive.
 *   mba     - If true, the partition found must be marked active.
 *
 * Return:
 *   A pointer to the matching partition entry in biosbuf memory.
 *   Note that the starting LBA field in the partition entry is
 *   modified to contain the absolute sector address, rather than
 *   the relative address.
 *   A NULL is returned if a match is not found.
 *
 * There are certain fdisk rules that allows us to simplify the search.
 *
 * - There can be 0-1 extended partition entry in any partition table.
 * - In the MBR, there can be 0-4 primary partitions entries.
 * - In the extended partition, there can be 0-1 logical partition entry.
 *
 */
struct fdisk_part *
find_partition(u_int8_t type, u_int8_t biosdev, BOOL mba)
{
#define MAX_ITERATIONS  128

    static u_int32_t    iter = 0;
    static u_int32_t    offset_root;
    static u_int32_t    offset;

    int                 n;
    int                 rc;
    boot_sector *       bootsect;
    struct fdisk_part * match = 0;
    struct fdisk_part * parts;

    if (iter == 0) {
        if (rc = Biosread(biosdev, 0))  // Read MBR at sector zero.
            return 0;
        offset = 0;
    }

    bootsect = (boot_sector *) biosbuf;
    if (bootsect->signature != BOOT_SIGNATURE)
        return 0;

    // Find a primary or a logical partition that matches the partition
    // type specified.
    //
    for (n = 0, parts = (struct fdisk_part *) bootsect->parts;
         n < 4;
         n++, parts++)
    {
        DSPRINT(("fdisk: [%d] %02x\n", iter, parts->systid));

        if (mba && ((parts->bootid & 0x80) == 0))
            continue;

        if (parts->systid == type) {
            //
            // Found it!!!
            // Make the relsect field (LBA starting sector) absolute by
            // adding in the offset.
            //
            parts->relsect += offset;

			DSPRINT(("Found: %x (%d)\n", parts->relsect, parts->numsect));

            return parts;
        }
    }

    // Find if there is an extended partition entry that points to
    // an extended partition table. Note that we only allow up to
    // one extended partition per partition table.
    //
    for (n = 0, parts = (struct fdisk_part *) bootsect->parts;
         n < 4;
         n++, parts++)
    {
        DSPRINT(("fdisk: [E%d] %02x\n", iter, parts->systid));

        if (isExtendedPartition(parts->systid))
        {
            if (iter > MAX_ITERATIONS)  // limit recursion depth
                return 0;

            if (iter == 0)
                offset = offset_root = parts->relsect;
            else
                offset = parts->relsect + offset_root;

            iter++;

            // Load extended partition table.
            //
            if (((rc = Biosread(biosdev, offset)) == 0) &&
                (bootsect->signature == BOOT_SIGNATURE))
            {
                match = find_partition(type, biosdev, mba);
            }
            
            iter--;

            break;
        }
    }
    
    return match;
}
