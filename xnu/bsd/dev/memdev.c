/*
 * Copyright (c) 2004-2019 Apple Inc. All rights reserved.
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
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah Hdr: vn.c 1.13 94/04/02
 *
 *	from: @(#)vn.c	8.6 (Berkeley) 4/1/94
 * $FreeBSD: src/sys/dev/vn/vn.c,v 1.105.2.4 2001/11/18 07:11:00 dillon Exp $
 */

/*
 * RAM disk driver.
 *
 * Block interface to a ramdisk.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/vm.h>
#include <sys/uio_internal.h>
#include <libkern/libkern.h>

#include <vm/pmap.h>
#include <vm/vm_pager_xnu.h>
#include <mach/memory_object_types.h>
#include <kern/debug.h>

#include <miscfs/devfs/devfs.h>


void            mdevinit(int the_cnt);

static open_close_fcn_t mdevopen;
static open_close_fcn_t mdevclose;
static psize_fcn_t              mdevsize;
static strategy_fcn_t   mdevstrategy;
static int                              mdevbioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p);
static int                              mdevcioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p);
static int                              mdevrw(dev_t dev, struct uio *uio, int ioflag);

#ifdef CONFIG_MEMDEV_INSECURE
static char *                   nonspace(char *pos, char *end);
static char *                   getspace(char *pos, char *end);
static char *                   cvtnum(char *pos, char *end, uint64_t *num);
#endif /* CONFIG_MEMDEV_INSECURE */

extern void             bcopy_phys(addr64_t from, addr64_t to, vm_size_t bytes);
extern void             mapping_set_mod(ppnum_t pn);
extern ppnum_t  pmap_find_phys(pmap_t pmap, addr64_t va);

/*
 * Maximal number of memory devices.
 */
#define NB_MAX_MDEVICES (16)

/*
 * cdevsw
 *	D_DISK		we want to look like a disk
 *	D_CANFREE	We support B_FREEBUF
 */

static const struct bdevsw mdevbdevsw = {
	.d_open     = mdevopen,
	.d_close    = mdevclose,
	.d_strategy = mdevstrategy,
	.d_ioctl    = mdevbioctl,
	.d_dump     = eno_dump,
	.d_psize    = mdevsize,
	.d_type     = D_DISK,
};

static const struct cdevsw mdevcdevsw = {
	.d_open       = mdevopen,
	.d_close      = mdevclose,
	.d_read       = mdevrw,
	.d_write      = mdevrw,
	.d_ioctl      = mdevcioctl,
	.d_stop       = eno_stop,
	.d_reset      = eno_reset,
	.d_ttys       = NULL,
	.d_select     = eno_select,
	.d_mmap       = eno_mmap,
	.d_strategy   = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type       = D_DISK,
};

struct mdev {
	uint64_t        mdBase;         /* base page number (pages are assumed to be 4K). Multiply by 4096 to find actual address */
	uint32_t        mdSize;         /* size in pages (pages are assumed to be 4K). Multiply by 4096 to find actual size. */
	int                     mdFlags;        /* flags */
	int                     mdSecsize;      /* sector size */
	int                     mdBDev;         /* Block device number */
	int                     mdCDev;         /* Character device number */
	void *          mdbdevb;
	void *          mdcdevb;
} mdev[NB_MAX_MDEVICES];

/* mdFlags */
#define mdInited        0x01    /* This device defined */
#define mdRO            0x02    /* This device is read-only */
#define mdPhys          0x04    /* This device is in physical memory */

int mdevBMajor = -1;
int mdevCMajor = -1;

static int      mdevioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p, int is_char);
dev_t           mdevadd(int devid, uint64_t base, unsigned int size, int phys);
dev_t           mdevlookup(int devid);
void            mdevremoveall(void);
int             mdevgetrange(int devid, uint64_t *base, uint64_t *size);

static  int
mdevclose(__unused dev_t dev, __unused int flags,
    __unused int devtype, __unused struct proc *p)
{
	return 0;
}

static  int
mdevopen(dev_t dev, int flags, __unused int devtype, __unused struct proc *p)
{
	int devid;

	devid = minor(dev);                                                                     /* Get minor device number */

	if (devid >= NB_MAX_MDEVICES || devid < 0) {
		return ENXIO;                                                                 /* Not valid */
	}
	if ((flags & FWRITE) && (mdev[devid].mdFlags & mdRO)) {
		return EACCES;                                                /* Currently mounted RO */
	}
	return 0;
}

static int
mdevrw(dev_t dev, struct uio *uio, __unused int ioflag)
{
	int                     status;
	addr64_t                mdata;
	int                     devid;
	enum uio_seg            saveflag;
	int                     count;

	devid = minor(dev);                                                                     /* Get minor device number */

	if (devid >= NB_MAX_MDEVICES || devid < 0) {
		return ENXIO;                                                                 /* Not valid */
	}
	if (!(mdev[devid].mdFlags & mdInited)) {
		return ENXIO;                                 /* Have we actually been defined yet? */
	}
	if (uio->uio_offset < 0) {
		return EINVAL;  /* invalid offset */
	}
	if (uio_resid(uio) < 0) {
		return EINVAL;
	}
	mdata = ((addr64_t)mdev[devid].mdBase << 12) + uio->uio_offset; /* Point to the area in "file" */

	saveflag = uio->uio_segflg;                                                     /* Remember what the request is */
	/* Make sure we are moving from physical ram if physical device */
	if (mdev[devid].mdFlags & mdPhys) {
		if (uio->uio_segflg == UIO_USERSPACE64) {
			uio->uio_segflg = UIO_PHYS_USERSPACE64;
		} else if (uio->uio_segflg == UIO_USERSPACE32) {
			uio->uio_segflg = UIO_PHYS_USERSPACE32;
		} else {
			uio->uio_segflg = UIO_PHYS_USERSPACE;
		}
	}

	if (uio->uio_offset > (mdev[devid].mdSize << 12)) {
		count = 0;
	} else {
		count = imin(uio_resid(uio), (mdev[devid].mdSize << 12) - uio->uio_offset);
	}

	status = uiomove64(mdata, count, uio);     /* Move the data */
	uio->uio_segflg = saveflag;                                                     /* Restore the flag */

	return status;
}

static void
mdevstrategy(struct buf *bp)
{
	unsigned int left, lop, csize;
	vm_offset_t vaddr, blkoff;
	int devid;
	addr64_t paddr, fvaddr;
	ppnum_t pp;

	devid = minor(buf_device(bp));                                                  /* Get minor device number */

	if ((mdev[devid].mdFlags & mdInited) == 0) {            /* Have we actually been defined yet? */
		buf_seterror(bp, ENXIO);
		buf_biodone(bp);
		return;
	}

	buf_setresid(bp, buf_count(bp));                                                /* Set byte count */

	blkoff = buf_blkno(bp) * mdev[devid].mdSecsize;         /* Get offset into file */

/*
 *	Note that reading past end is an error, but reading at end is an EOF.  For these
 *	we just return with resid == count.
 */

	if (blkoff >= (mdev[devid].mdSize << 12)) {                     /* Are they trying to read/write at/after end? */
		if (blkoff != (mdev[devid].mdSize << 12)) {              /* Are we trying to read after EOF? */
			buf_seterror(bp, EINVAL);                                               /* Yeah, this is an error */
		}
		buf_biodone(bp);                                                                /* Return */
		return;
	}

	if ((blkoff + buf_count(bp)) > (mdev[devid].mdSize << 12)) {            /* Will this read go past end? */
		buf_setcount(bp, (uint32_t)((mdev[devid].mdSize << 12) - blkoff));  /* Yes, trim to max */
	}
	/*
	 * make sure the buffer's data area is
	 * accessible
	 */
	if (buf_map(bp, (caddr_t *)&vaddr)) {
		panic("ramstrategy: buf_map failed");
	}

	fvaddr = (mdev[devid].mdBase << 12) + blkoff;           /* Point to offset into ram disk */

	if (buf_flags(bp) & B_READ) {                                   /* Is this a read? */
		if (!(mdev[devid].mdFlags & mdPhys)) {                   /* Physical mapped disk? */
			bcopy((void *)((uintptr_t)fvaddr),
			    (void *)vaddr, (size_t)buf_count(bp));      /* This is virtual, just get the data */
		} else {
			left = buf_count(bp);                                           /* Init the amount left to copy */
			while (left) {                                                           /* Go until it is all copied */
				lop = min((4096 - (vaddr & 4095)), (4096 - (fvaddr & 4095)));   /* Get smallest amount left on sink and source */
				csize = min(lop, left);                                 /* Don't move more than we need to */

				pp = pmap_find_phys(kernel_pmap, (addr64_t)((uintptr_t)vaddr)); /* Get the sink physical address */
				if (!pp) {                                                               /* Not found, what gives? */
					panic("mdevstrategy: sink address %016llX not mapped", (addr64_t)((uintptr_t)vaddr));
				}
				paddr = (addr64_t)(((addr64_t)pp << 12) | (addr64_t)(vaddr & 4095));    /* Get actual address */
				bcopy_phys(fvaddr, paddr, csize);               /* Copy this on in */
				mapping_set_mod((ppnum_t)(paddr >> 12));        /* Make sure we know that it is modified */

				left = left - csize;                                    /* Calculate what is left */
				vaddr = vaddr + csize;                                  /* Move to next sink address */
				fvaddr = fvaddr + csize;                                /* Bump to next physical address */
			}
		}
	} else {                                                                                        /* This is a write */
		if (!(mdev[devid].mdFlags & mdPhys)) {                   /* Physical mapped disk? */
			bcopy((void *)vaddr, (void *)((uintptr_t)fvaddr),
			    (size_t)buf_count(bp));             /* This is virtual, just put the data */
		} else {
			left = buf_count(bp);                                           /* Init the amount left to copy */
			while (left) {                                                           /* Go until it is all copied */
				lop = min((4096 - (vaddr & 4095)), (4096 - (fvaddr & 4095)));   /* Get smallest amount left on sink and source */
				csize = min(lop, left);                                 /* Don't move more than we need to */

				pp = pmap_find_phys(kernel_pmap, (addr64_t)((uintptr_t)vaddr)); /* Get the source physical address */
				if (!pp) {                                                               /* Not found, what gives? */
					panic("mdevstrategy: source address %016llX not mapped", (addr64_t)((uintptr_t)vaddr));
				}
				paddr = (addr64_t)(((addr64_t)pp << 12) | (addr64_t)(vaddr & 4095));    /* Get actual address */

				bcopy_phys(paddr, fvaddr, csize);               /* Move this on out */

				left = left - csize;                                    /* Calculate what is left */
				vaddr = vaddr + csize;                                  /* Move to next sink address */
				fvaddr = fvaddr + csize;                                /* Bump to next physical address */
			}
		}
	}
	/*
	 * buf_unmap takes care of all the cases
	 * it will unmap the buffer from kernel
	 * virtual space if that was the state
	 * when we mapped it.
	 */
	buf_unmap(bp);

	buf_setresid(bp, 0);                                                                    /* Nothing more to do */
	buf_biodone(bp);                                                                        /* Say we've finished */
}

static int
mdevbioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return mdevioctl(dev, cmd, data, flag, p, 0);
}

static int
mdevcioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return mdevioctl(dev, cmd, data, flag, p, 1);
}

static int
mdevioctl(dev_t dev, u_long cmd, caddr_t data, __unused int flag,
    struct proc *p, int is_char)
{
	int error;
	u_int32_t *f;
	u_int64_t *o;
	int devid;
	dk_memdev_info_t * memdev_info;

	devid = minor(dev);                                                                     /* Get minor device number */

	if (devid >= NB_MAX_MDEVICES || devid < 0) {
		return ENXIO;                                                                 /* Not valid */
	}
	error = proc_suser(p);                  /* Are we superman? */
	if (error) {
		return error;                                                         /* Nope... */
	}
	f = (u_int32_t*)data;
	o = (u_int64_t *)data;
	memdev_info = (dk_memdev_info_t *) data;

	switch (cmd) {
	case DKIOCGETMAXBLOCKCOUNTREAD:
		*o = 32;
		break;

	case DKIOCGETMAXBLOCKCOUNTWRITE:
		*o = 32;
		break;

	case DKIOCGETMAXSEGMENTCOUNTREAD:
		*o = 32;
		break;

	case DKIOCGETMAXSEGMENTCOUNTWRITE:
		*o = 32;
		break;

	case DKIOCGETBLOCKSIZE:
		*f = mdev[devid].mdSecsize;
		break;

	case DKIOCSETBLOCKSIZE:
		if (is_char) {
			return ENODEV;                                        /* We can only do this for a block */
		}
		if (*f < DEV_BSIZE) {
			return EINVAL;                                /* Too short? */
		}
		mdev[devid].mdSecsize = *f;                                             /* set the new block size */
		break;

	case DKIOCISWRITABLE:
		*f = 1;
		break;

	case DKIOCGETBLOCKCOUNT:
		if (!(mdev[devid].mdFlags & mdInited)) {
			return ENXIO;
		}
		*o = ((mdev[devid].mdSize << 12) + mdev[devid].mdSecsize - 1) / mdev[devid].mdSecsize;
		break;

	/*
	 * We're interested in the following bits of information:
	 *   Are you a memory-backed device (always yes, in this case)?
	 *   Physical memory (mdPhys)?
	 *   What is your base page?
	 *   What is your size?
	 */
	case DKIOCGETMEMDEVINFO:
		if (!(mdev[devid].mdFlags & mdInited)) {
			return ENXIO;
		}
		memdev_info->mi_mdev = TRUE;
		memdev_info->mi_phys = (mdev[devid].mdFlags & mdPhys) ? TRUE : FALSE;
		memdev_info->mi_base = (uint32_t)mdev[devid].mdBase;
		memdev_info->mi_size = mdev[devid].mdSize;
		break;

	default:
		error = ENOTTY;
		break;
	}
	return error;
}


static  int
mdevsize(dev_t dev)
{
	int devid;

	devid = minor(dev);                                                                     /* Get minor device number */
	if (devid >= NB_MAX_MDEVICES || devid < 0) {
		return ENXIO;                                                                 /* Not valid */
	}
	if ((mdev[devid].mdFlags & mdInited) == 0) {
		return -1;                                            /* Not inited yet */
	}
	return mdev[devid].mdSecsize;
}

#include <pexpert/pexpert.h>

void
mdevinit(__unused int the_cnt)
{
#ifdef CONFIG_MEMDEV_INSECURE

	int devid, phys;
	uint64_t base;
	uint64_t size;
	char *ba, *lp;
	dev_t dev;


	ba = PE_boot_args();                                                            /* Get the boot arguments */
	lp = ba + 256;                                                                          /* Point to the end */

	while (1) {                                                                                      /* Step through, looking for our keywords */
		phys = 0;                                                                               /* Assume virtual memory device */
		ba = nonspace(ba, lp);                                                  /* Find non-space */
		if (ba >= lp) {
			return;                                                         /* We are done if no more... */
		}
		if (((ba[0] != 'v') && (ba[0] != 'p'))
		    || (ba[1] != 'm') || (ba[2] != 'd') || (ba[4] != '=')
		    || (ba[3] < '0') || (ba[3] > 'f')
		    || ((ba[3] > '9') && (ba[3] < 'a'))) {              /* Is this of form "vmdx=" or "pmdx=" where x is hex digit? */
			ba = getspace(ba, lp);                                          /* Find next white space or end */
			continue;                                                                       /* Start looking for the next one */
		}

		if (ba[0] == 'p') {
			phys = 1;                                                       /* Set physical memory disk */
		}
		devid = ba[3] & 0xF;                                                    /* Assume digit */
		if (ba[3] > '9') {
			devid += 9;                                                     /* Adjust for hex digits */
		}
		ba = &ba[5];                                                                    /* Step past keyword */
		ba = cvtnum(ba, lp, &base);                                             /* Convert base of memory disk */
		if (ba >= lp) {
			return;                                                         /* Malformed one at the end, leave */
		}
		if (ba[0] != '.') {
			continue;                                                       /* If not length separater, try next... */
		}
		if (base & 0xFFF) {
			continue;                                                       /* Only allow page aligned stuff */
		}
		ba++;                                                                                   /* Step past '.' */
		ba = cvtnum(ba, lp, &size);                                             /* Try to convert it */
		if (!size || (size & 0xFFF)) {
			continue;                                       /* Allow only non-zer page size multiples */
		}
		if (ba < lp) {                                                                   /* If we are not at end, check end character */
			if ((ba[0] != ' ') && (ba[0] != 0)) {
				continue;                               /* End must be null or space */
			}
		}

		dev = mdevadd(devid, base >> 12, (unsigned)size >> 12, phys);   /* Go add the device */
	}

#endif /* CONFIG_MEMDEV_INSECURE */

	return;
}

#ifdef CONFIG_MEMDEV_INSECURE

char *
nonspace(char *pos, char *end)                                          /* Find next non-space in string */
{
	if (pos >= end) {
		return end;                                                             /* Don't go past end */
	}
	if (pos[0] == 0) {
		return end;                                                             /* If at null, make end */
	}
	while (1) {                                                                                      /* Keep going */
		if (pos[0] != ' ') {
			return pos;                                             /* Leave if we found one */
		}
		pos++;                                                                                  /* Stop */
		if (pos >= end) {
			return end;                                                     /* Quit if we run off end */
		}
	}
}

char *
getspace(char *pos, char *end)                                          /* Find next non-space in string */
{
	while (1) {                                                                                      /* Keep going */
		if (pos >= end) {
			return end;                                                     /* Don't go past end */
		}
		if (pos[0] == 0) {
			return end;                                                     /* Leave if we hit null */
		}
		if (pos[0] == ' ') {
			return pos;                                             /* Leave if we found one */
		}
		pos++;                                                                                  /* Stop */
	}
}

char *
cvtnum(char *pos, char *end, uint64_t *num)                     /* Convert to a number */
{
	int rad, dig;

	*num = 0;                                                                                       /* Set answer to 0 to start */
	rad = 10;

	if (pos >= end) {
		return end;                                                             /* Don't go past end */
	}
	if (pos[0] == 0) {
		return end;                                                             /* If at null, make end */
	}
	if (pos[0] == '0' && ((pos[1] == 'x') || (pos[1] == 'x'))) {     /* A hex constant? */
		rad = 16;
		pos += 2;                                                                               /* Point to the number */
	}

	while (1) {                                                                                      /* Convert it */
		if (pos >= end) {
			return end;                                                     /* Don't go past end */
		}
		if (pos[0] == 0) {
			return end;                                                     /* If at null, make end */
		}
		if (pos[0] < '0') {
			return pos;                                             /* Leave if non-digit */
		}
		dig = pos[0] & 0xF;                                                             /* Extract digit */
		if (pos[0] > '9') {                                                              /* Is it bigger than 9? */
			if (rad == 10) {
				return pos;                                             /* Leave if not base 10 */
			}
			if (!(((pos[0] >= 'A') && (pos[0] <= 'F'))
			    || ((pos[0] >= 'a') && (pos[0] <= 'f')))) {
				return pos;                                     /* Leave if bogus char */
			}
			dig = dig + 9;                                                          /* Adjust for character */
		}
		*num = (*num * rad) + dig;                                              /* Accumulate the number */
		pos++;                                                                                  /* Step on */
	}
}

#endif /* CONFIG_MEMDEV_INSECURE */

dev_t
mdevadd(int devid, uint64_t base, unsigned int size, int phys)
{
	int i;

	if (devid < 0) {
		devid = -1;
		for (i = 0; i < NB_MAX_MDEVICES; i++) {                                          /* Search all known memory devices */
			if (!(mdev[i].mdFlags & mdInited)) {                     /* Is this a free one? */
				if (devid < 0) {
					devid = i;                                      /* Remember first free one */
				}
				continue;                                                               /* Skip check */
			}
			if (!(((base + size - 1) < mdev[i].mdBase) || ((mdev[i].mdBase + mdev[i].mdSize - 1) < base))) { /* Is there any overlap? */
				panic("mdevadd: attempt to add overlapping memory device at %016llX-%016llX", mdev[i].mdBase, mdev[i].mdBase + mdev[i].mdSize - 1);
			}
		}
		if (devid < 0) {                                                                 /* Do we have free slots? */
			panic("mdevadd: attempt to add more than %d memory devices", NB_MAX_MDEVICES);
		}
	} else {
		if (devid >= NB_MAX_MDEVICES) {                                                          /* Giving us something bogus? */
			panic("mdevadd: attempt to explicitly add a bogus memory device: %08X", devid);
		}
		if (mdev[devid].mdFlags & mdInited) {                    /* Already there? */
			panic("mdevadd: attempt to explicitly add a previously defined memory device: %08X", devid);
		}
	}

	if (mdevBMajor < 0) {                                                            /* Have we gotten a major number yet? */
		mdevBMajor = bdevsw_add(-1, &mdevbdevsw);               /* Add to the table and figure out a major number */
		if (mdevBMajor < 0) {
			printf("mdevadd: error - bdevsw_add() returned %d\n", mdevBMajor);
			return -1;
		}
	}

	if (mdevCMajor < 0) {                                                            /* Have we gotten a major number yet? */
		mdevCMajor = cdevsw_add_with_bdev(-1, &mdevcdevsw, mdevBMajor);         /* Add to the table and figure out a major number */
		if (mdevCMajor < 0) {
			printf("ramdevice_init: error - cdevsw_add() returned %d\n", mdevCMajor);
			return -1;
		}
	}

	mdev[devid].mdBDev = makedev(mdevBMajor, devid);        /* Get the device number */
	mdev[devid].mdbdevb = devfs_make_node(mdev[devid].mdBDev, DEVFS_BLOCK,  /* Make the node */
	    UID_ROOT, GID_OPERATOR,
	    0600, "md%d", devid);
	if (mdev[devid].mdbdevb == NULL) {                                      /* Did we make one? */
		printf("mdevadd: devfs_make_node for block failed!\n");
		return -1;                                                                              /* Nope... */
	}

	mdev[devid].mdCDev = makedev(mdevCMajor, devid);        /* Get the device number */
	mdev[devid].mdcdevb = devfs_make_node(mdev[devid].mdCDev, DEVFS_CHAR,           /* Make the node */
	    UID_ROOT, GID_OPERATOR,
	    0600, "rmd%d", devid);
	if (mdev[devid].mdcdevb == NULL) {                                      /* Did we make one? */
		printf("mdevadd: devfs_make_node for character failed!\n");
		return -1;                                                                              /* Nope... */
	}

	mdev[devid].mdBase = base;                                                      /* Set the base address of ram disk */
	mdev[devid].mdSize = size;                                                      /* Set the length of the ram disk */
	mdev[devid].mdSecsize = DEV_BSIZE;                                      /* Set starting block size */
	if (phys) {
		mdev[devid].mdFlags |= mdPhys;                          /* Show that we are in physical memory */
	}
	mdev[devid].mdFlags |= mdInited;                                        /* Show we are all set up */
	printf("Added memory device md%x/rmd%x (%08X/%08X) at %016llX for %016llX\n",
	    devid, devid, mdev[devid].mdBDev, mdev[devid].mdCDev, base << 12, (uint64_t)size << 12);
	return mdev[devid].mdBDev;
}


dev_t
mdevlookup(int devid)
{
	if ((devid < 0) || (devid >= NB_MAX_MDEVICES)) {
		return -1;                                                              /* Filter any bogus requests */
	}
	if (!(mdev[devid].mdFlags & mdInited)) {
		return -1;                                      /* This one hasn't been defined */
	}
	return mdev[devid].mdBDev;                                                      /* Return the device number */
}

void
mdevremoveall(void)
{
	int i;

	for (i = 0; i < NB_MAX_MDEVICES; i++) {
		if (!(mdev[i].mdFlags & mdInited)) {
			continue;                               /* Ignore unused mdevs */
		}
		devfs_remove(mdev[i].mdbdevb);                  /* Remove the block device */
		devfs_remove(mdev[i].mdcdevb);                  /* Remove the character device */

		mdev[i].mdBase = 0;                             /* Clear the mdev's storage */
		mdev[i].mdSize = 0;
		mdev[i].mdSecsize = 0;
		mdev[i].mdFlags = 0;
		mdev[i].mdBDev = 0;
		mdev[i].mdCDev = 0;
		mdev[i].mdbdevb = 0;
		mdev[i].mdcdevb = 0;
	}
}

int
mdevgetrange(int devid, uint64_t *base, uint64_t *size)
{
	assert(base);
	assert(size);

	/* filter invalid request */
	if ((devid < 0) || (devid >= NB_MAX_MDEVICES)) {
		return -1;
	}

	/* filter non-initialized memory devices */
	if ((mdev[devid].mdFlags & mdInited) == 0) {
		return -1;
	}

	*base = mdev[devid].mdBase << 12;
	*size = mdev[devid].mdSize << 12;

	/* make sure (base, size) is a valid range and will not overflow */
	assert(*size < (UINT64_MAX - *base));

	return 0;
}
