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
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 */
/*
 * HISTORY
 * Revision 2.3  88/08/08  13:47:07  rvb
 * Allocate buffers dynamically vs statically.
 * Now b[i] and i_fs and i_buf, are allocated dynamically.
 * boot_calloc(size) allocates and zeros a  buffer rounded to a NPG
 * boundary.
 * Generalize boot spec to allow, xx()/mach, xx(n,[a..h])/mach,
 * xx([a..h])/mach, ...
 * Also default "xx" if unspecified and alloc just "/mach",
 * where everything is defaulted
 * Add routine, ptol(), to parse partition letters.
 *
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)sys.c	7.1 (Berkeley) 6/5/86
 */

#include <sys/param.h>
#include <ufs/ufs/dir.h>
#include <sys/reboot.h>
#include <architecture/byte_order.h>
#include "ufs_byteorder.h"
#include "libsaio.h"
#include "cache.h"
#include "kernBootStruct.h"
#include "stringConstants.h"
#include <ufs/ffs/fs.h>
#include "nbp.h"
#include "memory.h"

char * gFilename;

#ifdef DISABLED
static char * deviceDirectory;
#endif

extern int ram_debug_sarld;		// in load.c

#define DCACHE            1
#define ICACHE            1
#define SYS_MESSAGES      1
#define CHECK_CAREFULLY   0
#define COMPRESSION       1

// #define DEBUG 1

#ifdef	DEBUG
#define DPRINT(x)       { printf x; }
#define DSPRINT(x)      { printf x; sleep(2); }
#define RDPRINT(x)      { if (ram_debug_sarld) printf x; }
#define RDSPRINT(x)     { if (ram_debug_sarld) printf x; sleep(2); }
#else
#define DPRINT(x)
#define DSPRINT(x)
#define RDPRINT(x)
#define RDSPRINT(x)
#endif

char * devsw[] = {
    "sd",
    "hd",
    "fd",
    "en",
    NULL
};

//#############################################################################
//#
//# Disk filesystem functions.
//#
//#############################################################################

static ino_t dlook(char *s, struct iob *io);
static char * xx(char *str, struct iob *file);
static int ffs(register long mask);

extern int label_secsize;

#define BIG_ENDIAN_INTEL_FS __LITTLE_ENDIAN__

#if ICACHE
#define ICACHE_SIZE       256
#define ICACHE_READAHEAD  8    // read behind and read ahead
static cache_t * icache;
#endif ICACHE

#if DCACHE
#define DCACHE_SIZE       16   // 1k (DIRBLKSIZ) blocks
static cache_t * dcache;
#endif

#define	DEV_BSIZE         label_secsize

#if CHECK_CAREFULLY
static int open_init;
#endif

static struct fs * fs_block;
static int         fs_block_valid;

#define SUPERBLOCK_ERROR  "Bad superblock: error %d\n"

/*==========================================================================
 *
 *
 */
static struct iob * iob_from_fdesc(int fdesc)
{
    register struct iob * file;
    
    if (fdesc < 0 || fdesc >= NFILES ||
        ((file = &iob[fdesc])->i_flgs & F_ALLOC) == 0)
	    return NULL;
    else
	    return file;
}


/***************************************************************************
 *
 * Disk functions.
 *
 ***************************************************************************/

/*==========================================================================
 *
 *
 */
static int
openi(int n, struct iob * io)
{
	struct dinode * dp;
	int    cc, i, j, n_round;

#if ICACHE
	struct dinode *ip;
	
	if (icache == 0) {
	    icache = cacheInit(ICACHE_SIZE, sizeof(struct dinode));
	}
#endif /* ICACHE */

	io->i_offset = 0;
	io->i_bn = fsbtodb(io->i_ffs, ino_to_fsba(io->i_ffs, n)) + io->i_boff;
	io->i_cc = io->i_ffs->fs_bsize;
	io->i_ma = io->i_buf;

#if ICACHE
	if (cacheFind(icache, n, 0, (char **)&ip) == 1) {
		io->i_ino.i_din = *ip;
		cc = 0;
	} else {
#endif ICACHE
        cc = devread(io);
	    dp = (struct dinode *)io->i_buf;
	    n_round = (n / INOPB(io->i_ffs)) * INOPB(io->i_ffs);
#if ICACHE
	    /* Read multiple inodes into cache */
        for (i = max(ino_to_fsbo(io->i_ffs, n) - ICACHE_READAHEAD, 0),
             j = min(i+2*ICACHE_READAHEAD, INOPB(io->i_ffs)); i < j; i++) {
            cacheFind(icache, n_round + i, 0, (char **)&ip);

#if	BIG_ENDIAN_INTEL_FS
#warning Building with Big Endian changes
            byte_swap_dinode_in(&dp[i]);
#endif	/* BIG_ENDIAN_INTEL_FS */

            *ip = dp[i];
            if (i == ino_to_fsbo(io->i_ffs, n)) {
                io->i_ino.i_din = *ip;
            }
	    }
	}
#else ICACHE

#if     BIG_ENDIAN_INTEL_FS
    byte_swap_dinode_in(&dp[ino_to_fsbo(io->i_ffs, n)]);
#endif  /* BIG_ENDIAN_INTEL_FS */

    io->i_ino.i_din = dp[ino_to_fsbo(io->i_ffs, n)];
#endif ICACHE
	
    io->i_ino.i_number = n;
	return (cc);
}

/*==========================================================================
 *
 *
 */
static int
readlink(struct iob * io, char * buf, int len)
{
	register struct inode * ip;
	
	ip = &io->i_ino;
#if 1
    /* read contents */
    io->i_offset = 0;
    io->i_cc = 0;
    io->i_flgs |= F_FILE;
    if (read(io - iob, buf, len) < 0)
        return -1;
#else    
	if (ip->i_icflags & IC_FASTLINK) {
	   if (ip->i_size > len)
           return -1;
	    bcopy(ip->i_symlink, buf, ip->i_size);
	} else {
	    /* read contents */
	    io->i_offset = 0;
	    io->i_cc = 0;
	    io->i_flgs |= F_FILE;
	    if (read(io - iob, buf, len) < 0)
		return -1;
	}
#endif
	return 0;
}

/*==========================================================================
 *
 *
 */
static int
find(char * path, struct iob * file)
{
	char * q;
	char   c;
	int    n, parent;
	char * lbuf = malloc(MAXPATHLEN + 1);
	int    ret;

#if	CHECK_CAREFULLY
	if (path==NULL || *path=='\0') {
		error("null path\n");
		ret = 0; goto out;
	}
#endif	CHECK_CAREFULLY

    DSPRINT(("in find: path=%s\n", path));

root:
	n = ROOTINO;
	if (openi(n, file) < 0)
	{
        DPRINT(("openi failed\n"));

#if	SYS_MESSAGES
		error("bad root inode\n");
#endif

		ret = 0; goto out;
	}
    DPRINT(("openi ok\n"));

	while (*path)
	{
		while (*path == '/')
			path++;
		q = path;
		while(*q != '/' && *q != '\0')
			q++;
		c = *q;
		*q = '\0';
		if (q == path) path = "." ;	/* "/" means "/." */

		parent = n;
		if ((n = dlook(path, file)) != 0)
		{
			if (c == '\0')
				break;
			if (openi(n, file) < 0)
			{
				*q = c;
				ret = 0; goto out;
			}
			*q = c;
			path = q;

			/* Check for symlinks */
			if (file->i_ino.i_mode & IFLNK) {
			    char *buf = malloc(MAXPATHLEN + 1);
			    if (readlink(file, buf, MAXPATHLEN + 1) < 0)
				return -1;
			    strcat(buf, q);
			    strcpy(lbuf, buf);
			    free(buf);
			    path = lbuf;
			    if (*path == '/')
				goto root;
			    if (openi(parent, file) < 0) {
				ret = 0; goto out;
			    }
			}
			continue;
		}
		else
		{
			*q = c;
			ret = 0; goto out;
		}
	}
	ret = n;
out:
	free(lbuf);
	return (ret);
}

/*==========================================================================
 *
 *
 */
static daddr_t
sbmap(struct iob * io, daddr_t bn)
{
	register struct inode * ip;
	int     i, j, sh;
	daddr_t nb, * bap;

	ip = &io->i_ino;

	if (bn < 0) {
#if	SYS_MESSAGES
		error("bn negative\n");
#endif
		return ((daddr_t)0);
	}

	/*
	 * blocks 0..NDADDR are direct blocks
	 */
	if (bn < NDADDR)
	{
		nb = ip->i_db[bn];
		return (nb);
	}

	/*
	 * addresses NIADDR have single and double indirect blocks.
	 * the first step is to determine how many levels of indirection.
	 */
    RDPRINT(("In NINADDR\n"));

	sh = 1;
	bn -= NDADDR;
	for (j = NIADDR; j > 0; j--) {
		sh *= NINDIR(io->i_ffs);
		if (bn < sh)
			break;
		bn -= sh;
	}

	if (j == 0) {
#if	SYS_MESSAGES
		error("bn ovf %d\n", bn);
#endif
		return ((daddr_t)0);
	}

	/*
	 * fetch the first indirect block address from the inode
	 */
	nb = ip->i_ib[NIADDR - j];
	if (nb == 0) {
#if	SYS_MESSAGES
		error("bn void %d\n",bn);
#endif
		return ((daddr_t)0);
	}

	/*
	 * fetch through the indirect blocks
	 */
	for (; j <= NIADDR; j++) {
		if (blknos[j] != nb) {
			io->i_bn = fsbtodb(io->i_ffs, nb) + io->i_boff;
			if (b[j] == (char *)0)
				b[j] = malloc(MAXBSIZE);
			io->i_ma = b[j];
			io->i_cc = io->i_ffs->fs_bsize;

            RDPRINT(("Indir block read\n"));

			if (devread(io) != io->i_ffs->fs_bsize) {
#if	SYS_MESSAGES
				error("bn %d: read error\n", io->i_bn);
#endif
				return ((daddr_t)0);
			}
			blknos[j] = nb;
		}
		bap = (daddr_t *)b[j];
		sh /= NINDIR(io->i_ffs);
		i = (bn / sh) % NINDIR(io->i_ffs);
#if	BIG_ENDIAN_INTEL_FS
		nb = NXSwapBigLongToHost(bap[i]);
#else	/* BIG_ENDIAN_INTEL_FS */
		nb = bap[i];
#endif	/* BIG_ENDIAN_INTEL_FS */
		if (nb == 0) {
#if	SYS_MESSAGES
			error("bn void %d\n",bn);
#endif
			return ((daddr_t)0);
		}
	}

	return (nb);
}

#ifdef DISABLED
/*==========================================================================
 *
 *
 */
static struct dirstuff *
disk_opendir(char * path)
{
    register struct dirstuff * dirp;
    register int fd;
    
    dirp = (struct dirstuff *)malloc(sizeof(struct dirstuff));
    if (dirp == (struct dirstuff *)-1)
        return 0;

    DPRINT(("Calling open in opendir\n"));
    fd = open(path,0);
    if (fd == -1) {
        DPRINT(("open failed \n"));
        free((void *)dirp);
        return 0;
    }

    DPRINT(("open ok fd is %d \n", fd));
    dirp->io = &iob[fd];
    dirp->loc = 0;
    iob[fd].dirbuf_blkno = -1;

    return dirp;
}

/*==========================================================================
 *
 *
 */
static int
disk_closedir(struct dirstuff * dirp)
{
    close(dirp->io - iob);
    free((void *)dirp);
    return 0;
}
#endif /* DISABLED */

/*==========================================================================
 * get next entry in a directory.
 */
static struct direct *
disk_readdir(struct dirstuff * dirp)
{
	struct direct *       dp;
	register struct iob * io;
	daddr_t               lbn, d;
	int                   off;
#if DCACHE
	char *                bp;
	int                   dirblkno;

	if (dcache == 0)
		dcache = cacheInit(DCACHE_SIZE, DIRBLKSIZ);
#endif DCACHE

	io = dirp->io;
	for(;;)
	{
		if (dirp->loc >= io->i_ino.i_size)
			return (NULL);
		off = blkoff(io->i_ffs, dirp->loc);
		lbn = lblkno(io->i_ffs, dirp->loc);

#if DCACHE
		dirblkno = dirp->loc / DIRBLKSIZ;
		if (cacheFind(dcache, io->i_ino.i_number, dirblkno, &bp)) {
		    dp = (struct direct *)(bp + (dirp->loc % DIRBLKSIZ));
		} else
#else DCACHE
		if (io->dirbuf_blkno != lbn)
#endif DCACHE
		{
		    if((d = sbmap(io, lbn)) == 0)
			    return NULL;
		    io->i_bn = fsbtodb(io->i_ffs, d) + io->i_boff;
		    io->i_ma = io->i_buf;
		    io->i_cc = blksize(io->i_ffs, &io->i_ino, lbn);
		
		    if (devread(io) < 0)
		    {
#if	SYS_MESSAGES
			    error("bn %d: directory read error\n", io->i_bn);
#endif
			    return (NULL);
		    }
#if	BIG_ENDIAN_INTEL_FS
		    byte_swap_dir_block_in(io->i_buf, io->i_cc);
#endif	/* BIG_ENDIAN_INTEL_FS */

#if DCACHE
		    bcopy(io->i_buf + dirblkno * DIRBLKSIZ, bp, DIRBLKSIZ);
		    dp = (struct direct *)(io->i_buf + off);
#endif
		}
#if !DCACHE
		dp = (struct direct *)(io->i_buf + off);
#endif
		dirp->loc += dp->d_reclen;

		if (dp->d_ino != 0) return (dp);
	}
}

/*==========================================================================
 *
 *
 */
static ino_t
dlook(
	char *       s,
	struct iob * io
)
{
	struct direct *         dp;
	register struct inode * ip;
	struct dirstuff         dirp;
	int                     len;

	if (s == NULL || *s == '\0')
		return (0);
	ip = &io->i_ino;
	if ((ip->i_mode & IFMT) != IFDIR) {
#if	SYS_MESSAGES
		error(". before %s not a dir\n", s);
#endif
		return (0);
	}
	if (ip->i_size == 0) {
#if	SYS_MESSAGES
		error("%s: 0 length dir\n", s);
#endif
		return (0);
	}
	len = strlen(s);
	dirp.loc = 0;
	dirp.io = io;
	io->dirbuf_blkno = -1;

	for (dp = disk_readdir(&dirp); dp != NULL; dp = disk_readdir(&dirp)) {
        DPRINT(("checking name %s\n", dp->d_name));
		if(dp->d_ino == 0)
			continue;
		if (dp->d_namlen == len && !strcmp(s, dp->d_name))
			return (dp->d_ino);
	}
	return (0);
}

/*==========================================================================
 *
 *
 */
static int
getch(int fdesc)
{
	register struct iob * io;
	struct fs *           fs;
	char *                p;
	int                   c, lbn, off, size, diff;

	if ((io = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}

    RDPRINT(("In getch\n"));

    p = io->i_ma;
	if (io->i_cc <= 0) {
		if ((io->i_flgs & F_FILE) != 0) {
			diff = io->i_ino.i_size - io->i_offset;
			if (diff <= 0)
				return (-1);
			fs = io->i_ffs;
			lbn = lblkno(fs, io->i_offset);
#if 1
			io->i_bn = fsbtodb(fs, sbmap(io, lbn)) + io->i_boff;
#else
			io->i_bn = fsbtodb(fs, sbmap(io, lbn)) + io->i_boff;
#endif
			off = blkoff(fs, io->i_offset);
			size = blksize(fs, &io->i_ino, lbn);
		} else {
			diff = 0;
#ifndef	SMALL
			io->i_bn = io->i_offset / DEV_BSIZE;
			off = 0;
			size = DEV_BSIZE;
#endif	SMALL
		}

        RDPRINT(("gc: bn=%x; off=%x\n",io->i_bn, io->i_offset));
	
        io->i_ma = io->i_buf;
		io->i_cc = size;
		if (devread(io) < 0) {
			return (-1);
		}
		if ((io->i_flgs & F_FILE) != 0) {
			if (io->i_offset - off + size >= io->i_ino.i_size)
				io->i_cc = diff + off;
			io->i_cc -= off;
		}
		p = &io->i_buf[off];
	}
	io->i_cc--;
	io->i_offset++;
	c = (unsigned)*p++;
	io->i_ma = p;
	return (c);
}

/*==========================================================================
 *
 */
static int
disk_read(int fdesc, char * buf, int count)
{
	int                   i, size;
	register struct iob * file;
	struct fs *           fs;
	int                   lbn, off;

    RDSPRINT(("IN READ\n"));

	if ((file = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}
#if CHECK_CAREFULLY
	if ((file->i_flgs&F_READ) == 0) {
		return (-1);
	}
#endif
	if ((file->i_flgs & F_MEM) != 0) {

        RDSPRINT(("In read FMEM\n"));

	    if (file->i_offset < file->i_boff) {
            if (count > (file->i_boff - file->i_offset))
                count = file->i_boff - file->i_offset;
            bcopy(file->i_buf + file->i_offset, buf, count);
            file->i_offset += count;
	    } else {
            count = 0;
	    }
	    return count;
	}
	
#ifndef	SMALL
	if ((file->i_flgs & F_FILE) == 0) {
		file->i_cc = count;
		file->i_ma = buf;
		file->i_bn = file->i_boff + (file->i_offset / DEV_BSIZE);

        RDPRINT(("In read nsmall fbn=%x; offset=%x;", file->i_bn,
                 file->i_offset));
        RDSPRINT(("boff=%x\n", file->i_boff));

		i = devread(file);
		file->i_offset += count;
		return (i);
	}
#endif /* SMALL */

	if (file->i_offset+count > file->i_ino.i_size)
		count = file->i_ino.i_size - file->i_offset;

    RDSPRINT(("In read nsmall count=%x;", count));

    if ((i = count) <= 0)
        return (0);

	/*
	 * While reading full blocks, do I/O into user buffer.
	 * Anything else uses getc().
	 */
	fs = file->i_ffs;
	while (i) {
        RDSPRINT(("In lread while\n"));
		off = blkoff(fs, file->i_offset);
		lbn = lblkno(fs, file->i_offset);
		size = blksize(fs, &file->i_ino, lbn);
		if (off == 0 && size <= i) {
			file->i_bn = fsbtodb(fs, sbmap(file, lbn)) +
			    file->i_boff;
			file->i_cc = size;
			file->i_ma = buf;

            RDPRINT(("In read->devread\n"));
            RDPRINT(("In read fbn=%x; offset=%x;", file->i_bn,
                     file->i_offset));
            RDSPRINT((" boff=%x\n", file->i_boff));

            if (devread(file) < 0) {
                return (-1);
            }
            file->i_offset += size;
            file->i_cc = 0;
            buf += size;
            i -= size;
		}
        else {
            RDSPRINT(("IN while nonread\n"));
			size -= off;
			if (size > i)
				size = i;
			i -= size;
			do {
				*buf++ = getch(fdesc);
			} while (--size);
		}
	}

	return (count);
}

/*==========================================================================
 * Disk (block device) functions.
 */
static BOOL
disk_open(char * name, struct iob * file, int how)
{
	int    i;

	file->i_cc = SBSIZE;
//	file->i_bn = (SBLOCK / DEV_BSIZE) + file->i_boff;
	file->i_bn = (SBOFF/label_secsize) + file->i_boff;
	file->i_offset = 0;

	if (file->i_ffs == 0) {
		if (fs_block == 0) {
            DPRINT(("No super block; reading one \n"));
		    fs_block = (struct fs *) malloc(SBSIZE);
		}
		if (fs_block_valid == 0) {
		    file->i_ma = (char *)fs_block;
		    if (devread(file) < 0) {
#ifndef SMALL
			    error(SUPERBLOCK_ERROR, 1);
#endif
			    return NO;
		    }
#if	BIG_ENDIAN_INTEL_FS
		    byte_swap_superblock(fs_block);
#endif	/* BIG_ENDIAN_INTEL_FS */
            DPRINT(("Read SB \n"));
		    fs_block_valid = 1;
		}
		file->i_ffs = fs_block;
		file->i_buf = malloc(MAXBSIZE);
	}
#if	BIG_ENDIAN_INTEL_FS
    DPRINT(("IN BE_FS code \n"));

	if (file->i_ffs->fs_magic != FS_MAGIC) {
        DPRINT(("Bad magic in FS %d ; got %d\n", FS_MAGIC,
                file->i_ffs->fs_magic));
		error(SUPERBLOCK_ERROR, 2);
		return NO;
	}
	/*
	 *  The following is a gross hack to boot disks that have an actual
	 *  blocksize of 512 bytes but were written with a theoretical 1024
	 *  byte blocksize (fsbtodb == 0).
	 *
	 *  We can make this assumption because we can only boot disks with
	 *  a 512 byte sector size.
	 */
    DPRINT(("SB  magic ok \n"));
	if (file->i_ffs->fs_fsize == 0) {
		error(SUPERBLOCK_ERROR,3);
		return NO;
	}
	file->i_ffs->fs_fsbtodb = ffs(file->i_ffs->fs_fsize / DEV_BSIZE) - 1;
#endif	/* BIG_ENDIAN_INTEL_FS */

	if ((i = find(name, file)) == 0) {
        DPRINT(("find() failed\n"));
		return NO;
	}

#if	CHECK_CAREFULLY
	if (how != 0) {
		error("Can't write files\n");
		return NO;
	}
#endif	CHECK_CAREFULLY

    DPRINT(("calling openi \n"));
	if (openi(i, file) < 0) {
        DPRINT(("openi failed \n"));
		return NO;
	}

    DPRINT(("openi ok \n"));

	file->i_offset = 0;
	file->i_cc = 0;
	file->i_flgs |= F_FILE | (how+1);

	return YES;
}

/*==========================================================================
 *
 *
 */
static int
ffs(register long mask)
{
	register int cnt;

	if (mask == 0) return(0);

	for (cnt = 1; !(mask & 1); cnt++)
		mask >>= 1;
	return(cnt);
}

/*==========================================================================
 *
 *
 */
static void
disk_flushdev()
{
    register int i;
    
    devflush();
    for (i = 0; i < NFILES; i++)
        if (iob[i].i_flgs & (F_READ | F_WRITE))
            error("flushdev: fd %d is open\n",i);

    fs_block_valid = 0;
#if ICACHE
    cacheFlush(icache);
#endif
#if DCACHE
    cacheFlush(dcache);
#endif

#ifdef DISABLED
    deviceDirectory = NULL;
#endif /* DISABLED */
}


/***************************************************************************
 *
 * Network functions.
 *
 ***************************************************************************/

static int
en_read(int fdesc, char * buf, int count)
{
	struct iob * file;

	if ((file = iob_from_fdesc(fdesc)) == 0)
		return (-1);
	
	DSPRINT(("read[%d]: %x %x %d\n",
		fdesc, TFTP_ADDR + file->i_offset, (unsigned) buf, count)); 

	bcopy((char *)(TFTP_ADDR + file->i_offset), buf, count);
	file->i_offset += count;

	return count;
}
 
static BOOL
en_open(char * name, struct iob * file, int how)
{
	unsigned long  txferSize = TFTP_LEN;
	
	if (nbpTFTPReadFile(name, &txferSize, TFTP_ADDR) != nbpStatusSuccess)
		return NO;

	file->i_buf        = NULL;
	file->i_offset     = 0;
	file->i_ino.i_size = txferSize;		// update the real size

	return YES;
}

static void
en_devopen(char * name, struct iob * io)
{
	io->i_error = 0;
}


/***************************************************************************
 *
 * Dispatch functions.
 *
 ***************************************************************************/

/*==========================================================================
 *
 */
static int
gen_read(int fdesc, char * buf, int count)
{
	struct iob * file;
	
	if ((file = iob_from_fdesc(fdesc)) == 0)
		return (-1);
	
	return (file->i_ino.i_dev == DEV_EN) ?
		en_read(fdesc, buf, count) :
		disk_read(fdesc, buf, count);
}

/*==========================================================================
 *
 */
static int
gen_open(char * name, struct iob * file, int how)
{
	return (file->i_ino.i_dev == DEV_EN) ?
		en_open(name, file, how) : disk_open(name, file, how);
}

/*==========================================================================
 *
 */
static void
gen_devopen(char * name, struct iob * io)
{
	return (io->i_ino.i_dev == DEV_EN) ?
		en_devopen(name, io) : devopen(name, io);
}

/*==========================================================================
 *
 */
static void
gen_flushdev()
{
	return disk_flushdev();
}

/*==========================================================================
 *
 */
static char *
gen_usrDevices()
{
#define NET_ARCH_DEVICES ""

	return (((currentdev() >> B_TYPESHIFT) & B_TYPEMASK) == DEV_EN) ?
		NET_ARCH_DEVICES : ARCH_DEVICES;
}

/***************************************************************************
 *
 * External functions.
 *
 ***************************************************************************/

/*==========================================================================
 *
 */
struct dirstuff *
opendir(char * path)
{
	error("Unsupported function: opendir\n");
	return 0;
}

/*==========================================================================
 *
 */
int
closedir(struct dirstuff * dirp)
{
	error("Unsupported function: closedir\n");
	return 0;
}

/*==========================================================================
 * get next entry in a directory.
 */
struct direct *
readdir(struct dirstuff * dirp)
{
	error("Unsupported function: readdir\n");
	return NULL;
}

/*==========================================================================
 *
 */
int
b_lseek(int fdesc, unsigned int addr, int ptr)
{
	register struct iob * io;

    RDPRINT(("In lseek addr= %x\n", addr));

#if	CHECK_CAREFULLY
	if (ptr != 0) {
		error("Seek not from beginning of file\n");
		return (-1);
	}
#endif /* CHECK_CAREFULLY */

	if ((io = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}
	io->i_offset = addr;
	io->i_bn = addr / DEV_BSIZE;
	io->i_cc = 0;

    RDPRINT(("In end of lseek offset %x; bn %x\n", io->i_offset,io->i_bn));

	return (0);
}

/*==========================================================================
 *
 */
int
tell(int fdesc)
{
	return iob[fdesc].i_offset;
}

/*==========================================================================
 *
 */
int
read(int fdesc, char * buf, int count)
{
	return gen_read(fdesc, buf, count);
}

/*==========================================================================
 *
 */
int
openmem(char * buf, int len)
{
	register struct iob * file;
	int                   fdesc;

	for (fdesc = 0; fdesc < NFILES; fdesc++)
		if (iob[fdesc].i_flgs == 0)
			goto gotfile;
 	stop("Out of file descriptor slots");

gotfile:
	(file = &iob[fdesc])->i_flgs |= F_ALLOC;
	file->i_buf = buf;
	file->i_boff = len;
	file->i_offset = 0;
	file->i_flgs |= F_MEM;
	return fdesc;
}

/*==========================================================================
 * Generic open call.
 */
int
open(char * str, int how)
{
	register char *       cp;
	register struct iob * file;
	int                   fdesc;

    DSPRINT(("In open %s\n", str));

#if CHECK_CAREFULLY	/* iob[] is in BSS, so it is guaranteed to be zero. */
	if (open_init == 0) {
		for (i = 0; i < NFILES; i++)
			iob[i].i_flgs = 0;
		open_init = 1;
	}
#endif

	for (fdesc = 0; fdesc < NFILES; fdesc++)
		if (iob[fdesc].i_flgs == 0)
			goto gotfile;
 	stop("Out of file descriptor slots");

gotfile:
	(file = &iob[fdesc])->i_flgs |= F_ALLOC;

	if ((cp = xx(str, file)) == (char *) -1)
	{
		close(fdesc);
		return -1;
	}

	if (*cp == '\0') {
		file->i_flgs |= how+1;
		file->i_cc = 0;
		file->i_offset = 0;
		return (fdesc);
	}
	
	if (gen_open(cp, file, how) == NO) {
		close(fdesc);
		return -1;
	}
	return (fdesc);
}

/*==========================================================================
 *
 */
#define LP '('
#define RP ')'

static char * xx(char *str, struct iob *file)
{
	register char *cp = str, *xp;
	char ** dp;
	int old_dev = kernBootStruct->kernDev;
	int dev  = (kernBootStruct->kernDev >> B_TYPESHIFT) & B_TYPEMASK;
	int unit = (kernBootStruct->kernDev >> B_UNITSHIFT) & B_UNITMASK;
	int part = (kernBootStruct->kernDev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
	int i;
	int no_dev;
	int biosOffset;

	biosOffset = unit;		// set the device

	for (; *cp && *cp != LP; cp++) ;
	if (no_dev = !*cp) {    // no left paren found
		cp = str;
		xp = devsw[dev];
	} else if (cp == str) {	// paren but no device
		cp++;
		xp = devsw[dev];
	} else {
		xp = str;
		cp++;
	}

	for (dp = devsw; *dp; dp++)
	{
		if ((xp[0] == *dp[0]) && (xp[1] == *(dp[0] + 1)))
			goto gotdev;
	}

	error("Unknown device '%c%c'\n",xp[0],xp[1]);
	return ((char *)-1);

gotdev:
	if (no_dev)
		goto none;
	i = 0;
	while (*cp >= '0' && *cp <= '9')
	{
		i = i * 10 + *cp++ - '0';
        unit = i;           // get the specified unit number
	}

	biosOffset = unit;		// set the device

	if (*cp == RP || no_dev)
		/* do nothing since ptol(")") returns 0 */ ;
	else if (*cp == ',')
        part = ptol(++cp);  // get the specified partition number
	else if (cp[-1] == LP) 
		part = ptol(cp);
	else {
badoff:
		error("Missing offset specification\n");
		return ((char *)-1);
	}

	for ( ;!no_dev ;) {     // skip after the right paren
		if (*cp == RP)
			break;
		if (*cp++)
			continue;
		goto badoff;
	}

none:
	file->i_ino.i_dev = dev = dp - devsw;
	file->partition   = part;
	file->biosdev     = (BIOSDEV(dev)) + biosOffset;

	if (dev == DEV_SD) {
		file->biosdev += kernBootStruct->numIDEs;
	}
    else if (dev == DEV_EN) {
        file->biosdev = BIOS_DEV_EN;
    }
    else if (dev == DEV_HD && kernBootStruct->numIDEs == 0) {
		error("No IDE drives detected\n");
		return ((char *)-1);
	}

	kernBootStruct->kernDev = (dev << B_TYPESHIFT) | 
                              (unit << B_UNITSHIFT) |
                              (part << B_PARTITIONSHIFT);

	if (kernBootStruct->kernDev != old_dev)
	    flushdev();

	gen_devopen(str, file);

	if (file->i_error) 
		return (char *)-1;
	if (!no_dev && *cp) cp++;

	gFilename = cp;

	return cp;
}

/*==========================================================================
 *
 */
int
close(int fdesc)
{
	register struct iob * file;
	register int          i;

	if ((file = iob_from_fdesc(fdesc)) == 0)
		return (-1);

	if ((file->i_flgs & F_MEM) == 0) {
//		free((char *)file->i_ffs);
	    file->i_ffs = NULL;
	    if (file->i_buf) {
			free(file->i_buf);
			file->i_buf = NULL;
		}
	    for (i=0;i<NBUFS;i++)
	    {
		    if (b[i])
		    {	free(b[i]); 
			    b[i] = NULL;
		    }
		    blknos[i] = 0;
	    }
	}

	file->i_flgs = 0;
	return (0);
}

/*==========================================================================
 *
 */
int
file_size(int fdesc)
{
	register struct iob * io;
    
	if ((io = iob_from_fdesc(fdesc)) == 0)
		return (-1);

	return io->i_ino.i_size;
}

/*==========================================================================
 * ensure that all device caches are flushed,
 * because we are about to change the device media
 */
void
flushdev()
{
	gen_flushdev();
}

/*==========================================================================
 *
 */
void
stop(char * s)
{
#if	CHECK_CAREFULLY
	register int i;
	
	for (i = 0; i < NFILES; i++)
		if (iob[i].i_flgs != 0)
			close(i);
#endif	CHECK_CAREFULLY

    /* textMode(); */    // can't call this function from here
	error("\n%s\n", s);
	sleep(4);            // about to halt
	halt();
}

/*==========================================================================
 *
 */
int currentdev()
{
    return kernBootStruct->kernDev;
}

/*==========================================================================
 *
 */
int
switchdev(int dev)
{
    flushdev();
    kernBootStruct->kernDev = dev;
    return dev;
}

/*==========================================================================
 *
 */
char *
usrDevices()
{
	return gen_usrDevices();
}
