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
/* Useful types. */

#ifndef __LIBSAIO_SAIO_TYPES_H
#define __LIBSAIO_SAIO_TYPES_H

#include <sys/reboot.h>
#include <sys/types.h>
#include "bios.h"
#include "nbp_cmd.h"
#include <pexpert/i386/boot.h>

#if DEBUG
#define DEBUG_DISK(x)    printf x
#else
#define DEBUG_DISK(x)
#endif

typedef char BOOL;
#define NO   0
#define YES  1

typedef unsigned long entry_t;

typedef struct {
    unsigned int sectors:8;
    unsigned int heads:8;
    unsigned int cylinders:16;
} compact_diskinfo_t;

struct driveParameters {
    int cylinders;
    int sectors;
    int heads;
    int totalDrives;
};

struct driveInfo {
    boot_drive_info_t di;
    int uses_ebios;
    int no_emulation;
    int biosdev;
    int valid;
};


struct         BootVolume;
typedef struct BootVolume * BVRef;
typedef struct BootVolume * CICell;

typedef long (*FSInit)(CICell ih);
typedef long (*FSLoadFile)(CICell ih, char * filePath);
typedef long (*FSGetDirEntry)(CICell ih, char * dirPath, long * dirIndex,
                              char ** name, long * flags, long * time);
typedef void (*BVGetDescription)(CICell ih, char * str, long strMaxLen);

struct iob {
    unsigned int   i_flgs;          /* see F_* below */
    unsigned int   i_offset;        /* seek byte offset in file */
    int            i_filesize;      /* size of file */
    char *         i_buf;           /* file load address */
};

#define F_READ     0x1              /* file opened for reading */
#define F_WRITE    0x2              /* file opened for writing */
#define F_ALLOC    0x4              /* buffer allocated */
#define F_FILE     0x8              /* file instead of device */
#define F_NBSF     0x10             /* no bad sector forwarding */
#define F_SSI      0x40             /* set skip sector inhibit */
#define F_MEM      0x80             /* memory instead of file or device */

struct dirstuff {
    char *         dir_path;        /* directory path */
    long           dir_index;       /* directory entry index */
    BVRef          dir_bvr;         /* volume reference */
};

#define BVSTRLEN 32

struct BootVolume {
    BVRef            next;            /* list linkage pointer */
    int              biosdev;         /* BIOS device number */
    int              type;            /* device type (floppy, hd, network) */
    unsigned int     flags;           /* attribute flags */
    BVGetDescription description;     /* BVGetDescription function */
    int              part_no;         /* partition number (1 based) */
    unsigned int     part_boff;       /* partition block offset */
    unsigned int     part_type;       /* partition type */
    unsigned int     fs_boff;         /* 1st block # of next read */
    FSLoadFile       fs_loadfile;     /* FSLoadFile function */
    FSGetDirEntry    fs_getdirentry;  /* FSGetDirEntry function */
    unsigned int     bps;             /* bytes per sector for this device */
    char             name[BVSTRLEN];  /* (name of partition) */
    char             type_name[BVSTRLEN]; /* (type of partition, eg. Apple_HFS) */

};

enum {
    kBVFlagPrimary     = 0x01,
    kBVFlagNativeBoot  = 0x02,
    kBVFlagForeignBoot = 0x04
};

enum {
    kBIOSDevTypeFloppy    = 0x00,
    kBIOSDevTypeHardDrive = 0x80,
    kBIOSDevTypeNetwork   = 0xE0,
    kBIOSDevUnitMask      = 0x0F,
    kBIOSDevTypeMask      = 0xF0,
    kBIOSDevMask          = 0xFF
};

//#define BIOS_DEV_TYPE(d)  ((d) & kBIOSDevTypeMask)
#define BIOS_DEV_UNIT(bvr)  ((bvr)->biosdev - (bvr)->type)

/*
 * KernBootStruct device types.
 */
enum {
    DEV_SD = 0,
    DEV_HD = 1,
    DEV_FD = 2,
    DEV_EN = 3
};

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define MAKEKERNDEV(t, u, p)  MAKEBOOTDEV(t, 0, 0, u, p)

enum {
    kNetworkDeviceType = kBIOSDevTypeNetwork,
    kBlockDeviceType   = kBIOSDevTypeHardDrive
} gBootFileType_t;

#endif /* !__LIBSAIO_SAIO_TYPES_H */
