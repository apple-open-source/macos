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
 * 			INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *	This software is supplied under the terms of a license  agreement or 
 *	nondisclosure agreement with Intel Corporation and may not be copied 
 *	nor disclosed except in accordance with the terms of that agreement.
 *
 *	Copyright 1988, 1989 Intel Corporation
 */

/*
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 */

#define DRIVER_PRIVATE

#import "sys/types.h"
#import <bsd/dev/disk.h>
#import <bsd/dev/i386/disk.h>
#import "libsaio.h"
#import "memory.h"

static Biosread(int biosdev, int secno);
static read_label(char *name, int biosdev, daddr_t *boff, int partition);
void diskActivityHook(void);
extern void spinActivityIndicator(void);

/* diskinfo unpacking */
#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	((((di)>>8)&0xff)+1)
#define	SPC(di)		(SPT(di)*HEADS(di))
#define BPS		512	/* sector size of the device */
#define N_CACHE_SECS	(BIOS_LEN / BPS)

#define	DOS_BSIZE	512
#define DISKLABEL	15	/* sector num of disk label */

char *devsw[] = {
	"sd",
	"hd",
	"fd",
	NULL
};

struct diskinfo {
	int	spt;			/* sectors per track */
	int	spc;			/* sectors per cylinder */
} diskinfo;

char	*b[NBUFS];
daddr_t	blknos[NBUFS];
struct	iob iob[NFILES];
int label_secsize;
static int label_cached;

// intbuf is the whole buffer, biosbuf is the current cached sector
static char * const intbuf = (char *)ptov(BIOS_ADDR);
char *biosbuf;

void
devopen(name, io)
	char		* name;
	struct iob	* io;
{
	long	di;

	di = get_diskinfo(io->biosdev);

	/* initialize disk parameters -- spt and spc */
	io->i_error = 0;
	io->dirbuf_blkno = -1;

	diskinfo.spt = SPT(di);
	diskinfo.spc = diskinfo.spt * HEADS(di);
	if (read_label(name, io->biosdev, &io->i_boff, io->partition) < 0)
	{
		io->i_error = EIO;
	}
}

void devflush()
{
	Biosread(0,-1);
}


int devread(io)
	struct iob *io;
{
	long sector;
	int offset;
	int dev;

	io->i_flgs |= F_RDDATA;

	/* assume the best */
	io->i_error = 0;

	dev = io->i_ino.i_dev;

	sector = io->i_bn * (label_secsize/DOS_BSIZE);

	for (offset = 0; offset < io->i_cc; offset += BPS) {

		io->i_error = Biosread(io->biosdev, sector);
		if (io->i_error) {
			return(-1);
		}

		/* copy 1 sector from the internal buffer biosbuf into buf */
		bcopy(biosbuf, &io->i_ma[offset], BPS);

		sector++;
	}

	io->i_flgs &= ~F_TYPEMASK;
	return (io->i_cc);
}

/* A haque: Biosread(0,-1) means flush the sector cache.
 */
static int
Biosread(int biosdev, int secno)
{
	static int xbiosdev, xcyl=-1, xhead, xsec, xnsecs;

	int	rc;
	int	cyl, head, sec;
	int	spt, spc;
	int tries = 0;

	if (biosdev == 0 && secno == -1) {
	    xcyl = -1;
	    label_cached = 0;
	    return 0;
	}
	spt = diskinfo.spt;
	spc = diskinfo.spc;

	cyl = secno / spc;
	head = (secno % spc) / spt;
	sec = secno % spt;

	if (biosdev == xbiosdev && cyl == xcyl && head == xhead &&
		sec >= xsec && sec < (xsec + xnsecs))
	{	// this sector is in intbuf cache
		biosbuf = intbuf + (BPS * (sec-xsec));
		return 0;
	}

	xcyl = cyl;
	label_cached = 1;
	xhead = head;
	xsec = sec;
	xbiosdev = biosdev;
	xnsecs = ((sec + N_CACHE_SECS) > spt) ? (spt - sec) : N_CACHE_SECS;
	biosbuf = intbuf;

	while ((rc = biosread(biosdev,cyl,head,sec, xnsecs)) && (++tries < 5))
	{
#ifndef	SMALL
		error("    biosread error 0x%x @ %d, C:%d H:%d S:%d\n",
			rc, secno, cyl, head, sec);
#endif
		sleep(1);	// on disk errors, bleh!
	}
	diskActivityHook();
	return rc;
}

// extern char name[];

#ifndef	SMALL
static int
read_label(
	char		*name,
	int		biosdev, 
	daddr_t		*boff,
	int		partition
)
{
	struct disk_label	*dlp;
	struct fdisk_part	*fd;
	struct disk_blk0	*blk0;
	char			*cp;
	int			n, rc;
	int			part_offset = 0;
	static int		cached_boff;

	if (label_cached) {
		*boff = cached_boff;
		return 0;
	}

	// read sector 0 into the internal buffer "biosbuf"
	if ( rc = Biosread(biosdev, 0) )
		return -1;

	// Check for a valid boot block.
	blk0 = (struct disk_blk0 *)biosbuf;
	if (blk0->signature == DISK_SIGNATURE) {
	    // Check to see if the disk has been partitioned with FDISK
	    // to allow DOS and ufs filesystems to exist on the same spindle.
	    fd = (struct fdisk_part *)blk0->parts;    
	    for (	n = 0; n < FDISK_NPART; n++, fd++)
		    if (fd->systid == FDISK_NEXTNAME)
		    {
			    part_offset = fd->relsect;
			    break;
		    }
	}
	/* It's not an error if there is not a valid boot block. */

	/* Read the NeXT disk label.
	 * Since we can't count on it fitting in the sector cache,
	 * we'll put it elsewhere.
	 */
	dlp = (struct disk_label *)malloc(sizeof(*dlp) + BPS);
	for(n = 0, cp = (char *)dlp;
		n < ((sizeof(*dlp) + BPS - 1) / BPS);
		n++, cp += BPS) {
	    if (rc = Biosread(biosdev, DISKLABEL + part_offset + n))
		goto error;
	    bcopy(biosbuf, cp, BPS);
	}
	
	byte_swap_disklabel_in(dlp);
	
	/* Check label */
	
	if (dlp->dl_version != DL_V3) {
		error("bad disk label magic\n");
		goto error;
	}
	    
	label_secsize = dlp->dl_secsize;
	
	if ((dlp->dl_part[partition].p_base) < 0) {
		error("no such partition\n");
		goto error;
	}

	*boff = cached_boff = dlp->dl_front + dlp->dl_part[partition].p_base;

	if (!strcmp(name,"$LBL")) strcpy(name, dlp->dl_bootfile);

	free((char *)dlp);
	return 0;
error:
	free((char *)dlp);
	return -1;
}

#endif	SMALL


/* replace this function if you want to change
 * the way disk activity is indicated to the user.
 */
 
void
diskActivityHook(void)
{
    spinActivityIndicator();
}



