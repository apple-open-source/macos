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
 * Copyright (c) 1992 NeXT Computer, Inc.
 *
 * IBM PC disk partitioning data structures.
 *
 * HISTORY
 *
 * 8 July 1992 David E. Bohman at NeXT
 *    Created.
 */
 
#ifndef __LIBSAIO_FDISK_H
#define __LIBSAIO_FDISK_H

#define DISK_BLK0       0       /* blkno of boot block */
#define DISK_BLK0SZ     512     /* size of boot block */
#define DISK_BOOTSZ     446     /* size of boot code in boot block */
#define DISK_SIGNATURE  0xAA55  /* signature of the boot record */
#define FDISK_NPART     4       /* number of entries in fdisk table */
#define FDISK_ACTIVE    0x80    /* indicator of active partition */
#define FDISK_NEXTNAME  0xA7    /* indicator of NeXT partition */
#define FDISK_DOS12     0x01    /* 12-bit fat < 10MB dos partition */
#define FDISK_DOS16S    0x04    /* 16-bit fat < 32MB dos partition */
#define FDISK_DOSEXT    0x05    /* extended dos partition */
#define FDISK_DOS16B    0x06    /* 16-bit fat >= 32MB dos partition */
#define FDISK_UFS       0xa8    /* Apple UFS partition */
#define FDISK_HFS       0xaf    /* Apple HFS partition */
#define FDISK_BOOTER    0xab    /* Apple booter partition */

/*
 * Format of fdisk partion entry (if present).
 */
struct fdisk_part {
    unsigned char    bootid;    /* bootable or not */
    unsigned char    beghead;   /* begining head, sector, cylinder */
    unsigned char    begsect;   /* begcyl is a 10-bit number */
    unsigned char    begcyl;    /* High 2 bits are in begsect */
    unsigned char    systid;    /* OS type */
    unsigned char    endhead;   /* ending head, sector, cylinder */
    unsigned char    endsect;   /* endcyl is a 10-bit number */
    unsigned char    endcyl;    /* High 2 bits are in endsect */
    unsigned long    relsect;   /* partion physical offset on disk */
    unsigned long    numsect;   /* number of sectors in partition */
};

/*
 * Format of boot block.
 */
struct disk_blk0 {
    unsigned char    bootcode[DISK_BOOTSZ];
    unsigned char    parts[FDISK_NPART][sizeof (struct fdisk_part)];
    unsigned short   signature;
};

#endif /* !__LIBSAIO_FDISK_H */
