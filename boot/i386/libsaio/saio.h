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
 * Copyright (c) 1988 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *  @(#)saio.h  7.1 (Berkeley) 6/5/86
 */

#ifndef __LIBSAIO_SAIO_H
#define __LIBSAIO_SAIO_H
 
#include <sys/types.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>

/*
 * Io block: includes an inode, cells for the use of seek, etc,
 * and a buffer.
 */
struct  iob {
    int             i_flgs;        /* see F_ below */
    struct inode    i_ino;         /* inode, if file */
    daddr_t         i_boff;        /* block offset on device */
    unsigned int    i_offset;      /* seek offset in file */
    daddr_t         i_bn;          /* 1st block # of next read */
    char *          i_ma;          /* memory address of i/o buffer */
    int             i_cc;          /* character count of transfer */
    int             i_error;       /* error # return */
    char *          i_buf;         /* i/o buffer */
    struct fs *     i_ffs;         /* file system super block info */
    int             biosdev;       /* bios device for file, i_ino inadequate */
    daddr_t         dirbuf_blkno;  /* blk of currently buffered dir */
    int             partition;     /* which partition */
};

struct dirstuff {
    int          loc;
    struct iob * io;
};

#define F_READ      0x1            /* file opened for reading */
#define F_WRITE     0x2            /* file opened for writing */
#define F_ALLOC     0x4            /* buffer allocated */
#define F_FILE      0x8            /* file instead of device */
#define F_NBSF      0x10           /* no bad sector forwarding */
#define F_SSI       0x40           /* set skip sector inhibit */
#define F_MEM       0x80           /* memory instead of file or device */

/* IO types */
#define F_RDDATA    0x0100         /* read data */
#define F_WRDATA    0x0200         /* write data */
#define F_HDR       0x0400         /* include header on next i/o */

#define F_TYPEMASK  0xff00

extern char * devsw[];

/*
 * Request codes. Must be the same a F_XXX above
 */
#define READ    1
#define WRITE   2

#define NBUFS   4
extern char *   b[NBUFS];
extern daddr_t  blknos[NBUFS];

#define NFILES  6
extern struct   iob iob[NFILES];

/* Error codes */
#define EBADF   1   /* bad file descriptor */
#define EOFFSET 2   /* relative seek not supported */
#define EDEV    3   /* improper device specification on open */
#define ENXIO   4   /* unknown device specified */
#define ESRCH   6   /* directory search for file failed */
#define EIO     7   /* generic error */
#define ECMD    10  /* undefined driver command */
#define EBSE    11  /* bad sector error */
#define EWCK    12  /* write check error */
#define EECC    13  /* uncorrectable ecc error */
#define EHER    14  /* hard error */

#define BIOS_DEV_FLOPPY  0x0
#define BIOS_DEV_HD      0x80
#define BIOS_DEV_WIN     BIOS_DEV_HD
#define BIOS_DEV_EN      0xff	/* not really a BIOS device number */

#define DEV_SD      0
#define DEV_HD      1
#define DEV_FLOPPY  2
#define DEV_EN      3

#define BIOSDEV(dev)    ((dev) == DEV_FLOPPY ? BIOS_DEV_FLOPPY : BIOS_DEV_WIN)

#define NSECS   16  /* number of buffered 512 byte sectors */

#define	Dev(x)		(((x)>>B_TYPESHIFT)&B_TYPEMASK)

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#endif /* !__LIBSAIO_SAIO_H */
