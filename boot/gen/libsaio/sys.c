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

#import <sys/param.h>
#import <ufs/fs.h>
#import <ufs/fsdir.h>
#import <sys/reboot.h>
#import <architecture/byte_order.h>
#import "libsaio.h"
#import "cache.h"
#import "kernBootStruct.h"

char *gFilename;

static ino_t dlook(char *s, struct iob *io);
static char * xx(char *str, struct iob *file);
volatile void stop(char *s);
static int ffs(register long mask);

extern int label_secsize;

#define DCACHE		1
#define ICACHE		1
#define SYS_MESSAGES	1
#define CHECK_CAREFULLY	0

#if ICACHE
#define ICACHE_SIZE	8
static cache_t *icache;
#endif ICACHE

#define	DEV_BSIZE	label_secsize

static struct iob *iob_from_fdesc(int fdesc)
{
    register struct iob *file;
    
    if (fdesc < 0 || fdesc >= NFILES ||
	((file = &iob[fdesc])->i_flgs & F_ALLOC) == 0)
	    return NULL;
    else
	    return file;
}
 
static int
openi(int n, struct iob *io)
{
	struct dinode *dp;
	int cc;
#if ICACHE
	struct icommon *ip;
	
	if (icache == 0) {
	    icache = cacheInit(ICACHE_SIZE, sizeof(struct icommon));
	}
#endif ICACHE
	io->i_offset = 0;
	io->i_bn = fsbtodb(io->i_fs, itod(io->i_fs, n)) + io->i_boff;
	io->i_cc = io->i_fs->fs_bsize;
	io->i_ma = io->i_buf;

#if ICACHE
	if (cacheFind(icache, n, 0, (char **)&ip) == 1) {
		io->i_ino.i_ic = *ip;
		cc = 0;
	} else {
#endif ICACHE
	    cc = devread(io);
	    dp = (struct dinode *)io->i_buf;
#if	BIG_ENDIAN_FS
	    byte_swap_inode_in(&dp[itoo(io->i_fs, n)].di_ic, &io->i_ino.i_ic);
#else
	    io->i_ino.i_ic = dp[itoo(io->i_fs, n)].di_ic;
#endif	BIG_ENDIAN_FS
#if ICACHE
	    *ip = io->i_ino.i_ic;
	}
#endif ICACHE
	io->i_ino.i_number = n;
	return (cc);
}

static int
find(char *path, struct iob *file)
{
	char *q;
	char c;
	int n = 0;

#if	CHECK_CAREFULLY
	if (path==NULL || *path=='\0') {
		printf("null path\n");
		return (0);
	}
#endif	CHECK_CAREFULLY
	if (openi((ino_t) ROOTINO, file) < 0)
	{
#if	SYS_MESSAGES
		error("bad root inode\n");
#endif
		return (0);
	}
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

		if ((n = dlook(path, file)) != 0)
		{
			if (c == '\0')
				break;
			if (openi(n, file) < 0)
			{
				*q = c;
				return (0);
			}
			*q = c;
			path = q;
			continue;
		}
		else
		{
			*q = c;
			return (0);
		}
	}
	return (n);
}

static daddr_t
sbmap(struct iob *io, daddr_t bn)
{
	register struct inode *ip;
	int i, j, sh;
	daddr_t nb, *bap;

	ip = &io->i_ino;

	if (ip->i_icflags & IC_FASTLINK)
	{
		error("fast symlinks unimplemented\n");
		return ((daddr_t)0);
	}

	if (bn < 0) {
#if	SYS_MESSAGES
		error("bn negative\n");
#endif
		return ((daddr_t)0);
	}

	/*
	 * blocks 0..NDADDR are direct blocks
	 */
	if(bn < NDADDR)
	{
		nb = ip->i_db[bn];
		return (nb);
	}

	/*
	 * addresses NIADDR have single and double indirect blocks.
	 * the first step is to determine how many levels of indirection.
	 */
	sh = 1;
	bn -= NDADDR;
	for (j = NIADDR; j > 0; j--) {
		sh *= NINDIR(io->i_fs);
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
			io->i_bn = fsbtodb(io->i_fs, nb) + io->i_boff;
			if (b[j] == (char *)0)
				b[j] = malloc(MAXBSIZE);
			io->i_ma = b[j];
			io->i_cc = io->i_fs->fs_bsize;
			if (devread(io) != io->i_fs->fs_bsize) {
#if	SYS_MESSAGES
				error("bn %d: read error\n", io->i_bn);
#endif
				return ((daddr_t)0);
			}
			blknos[j] = nb;
		}
		bap = (daddr_t *)b[j];
		sh /= NINDIR(io->i_fs);
		i = (bn / sh) % NINDIR(io->i_fs);
#if	BIG_ENDIAN_FS
#if 1
		// for now it is little endian FS for intel
		nb = bap[i];
#else
		nb = NXSwapBigLongToHost(bap[i]);
#endif 1
#else	BIG_ENDIAN_FS
		nb = bap[i];
#endif	BIG_ENDIAN_FS
		if(nb == 0) {
#if	SYS_MESSAGES
			error("bn void %d\n",bn);
#endif
			return ((daddr_t)0);
		}
	}

	return (nb);
}

static ino_t
dlook(
	char *s,
	struct iob *io
)
{
	struct direct *dp;
	register struct inode *ip;
	struct dirstuff dirp;
	int len;

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

	for (dp = readdir(&dirp); dp != NULL; dp = readdir(&dirp)) {
#if	DEBUG1
		printf("checking name %s\n", dp->d_name);
#endif	DEBUG1
		if(dp->d_ino == 0)
			continue;
		if (dp->d_namlen == len && !strcmp(s, dp->d_name))
			return (dp->d_ino);
	}
	return (0);
}

struct dirstuff *
opendir(char *path)
{
    register struct dirstuff *dirp;
    register int fd;
    
    dirp = (struct dirstuff *)malloc(sizeof(struct dirstuff));
    if (dirp == (struct dirstuff *)-1)
	return 0;
    fd = open(path,0);
    if (fd == -1) {
	free((void *)dirp);
	return 0;
    }
    dirp->io = &iob[fd];
    dirp->loc = 0;
    iob[fd].dirbuf_blkno = -1;
    return dirp;
}

int
closedir(struct dirstuff *dirp)
{
    close(iob - dirp->io);
    free((void *)dirp);
    return 0;
}

#if DCACHE
static cache_t *dcache;
#define DCACHE_SIZE 6
#endif

/*
 * get next entry in a directory.
 */
struct direct *
readdir(struct dirstuff *dirp)
{
	struct direct *dp;
	register struct iob *io;
	daddr_t lbn, d;
	int off;
#if DCACHE
	char *bp;
	int dirblkno;

	if (dcache == 0)
		dcache = cacheInit(DCACHE_SIZE, DIRBLKSIZ);
#endif DCACHE
	io = dirp->io;
	for(;;)
	{
		if (dirp->loc >= io->i_ino.i_size)
			return (NULL);
		off = blkoff(io->i_fs, dirp->loc);
		lbn = lblkno(io->i_fs, dirp->loc);

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
		    io->i_bn = fsbtodb(io->i_fs, d) + io->i_boff;
		    io->i_ma = io->i_buf;
		    io->i_cc = blksize(io->i_fs, &io->i_ino, lbn);
		
		    if (devread(io) < 0)
		    {
#if	SYS_MESSAGES
			    error("bn %d: directory read error\n",
				    io->i_bn);
#endif
			    return (NULL);
		    }
#if	BIG_ENDIAN_FS
		    byte_swap_dir_block_in(io->i_buf, io->i_cc);
#endif	BIG_ENDIAN_FS
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

int
b_lseek(int fdesc, unsigned int addr, int ptr)
{
	register struct iob *io;

#if	CHECK_CAREFULLY
	if (ptr != 0) {
		error("Seek not from beginning of file\n");
		return (-1);
	}
#endif	CHECK_CAREFULLY
	if ((io = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}
	io->i_offset = addr;
	io->i_bn = addr / DEV_BSIZE;
	io->i_cc = 0;
	return (0);
}

int
tell(int fdesc)
{
	return iob[fdesc].i_offset;
}

static int getch(int fdesc)
{
	register struct iob *io;
	struct fs *fs;
	char *p;
	int c, lbn, off, size, diff;

	if ((io = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}
	p = io->i_ma;
	if (io->i_cc <= 0) {
		if ((io->i_flgs & F_FILE) != 0) {
			diff = io->i_ino.i_size - io->i_offset;
			if (diff <= 0)
				return (-1);
			fs = io->i_fs;
			lbn = lblkno(fs, io->i_offset);
			io->i_bn = fsbtodb(fs, sbmap(io, lbn)) + io->i_boff;
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

int
read(int fdesc, char *buf, int count)
{
	int i, size;
	register struct iob *file;
	struct fs *fs;
	int lbn, off;

	if ((file = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}
	if ((file->i_flgs&F_READ) == 0) {
		return (-1);
	}
#ifndef	SMALL
	if ((file->i_flgs & F_FILE) == 0) {
		file->i_cc = count;
		file->i_ma = buf;
		file->i_bn = file->i_boff + (file->i_offset / DEV_BSIZE);
		i = devread(file);
		file->i_offset += count;
		return (i);
	}
#endif	SMALL
	if (file->i_offset+count > file->i_ino.i_size)
		count = file->i_ino.i_size - file->i_offset;
	if ((i = count) <= 0)
		return (0);
	/*
	 * While reading full blocks, do I/O into user buffer.
	 * Anything else uses getc().
	 */
	fs = file->i_fs;
	while (i) {
		off = blkoff(fs, file->i_offset);
		lbn = lblkno(fs, file->i_offset);
		size = blksize(fs, &file->i_ino, lbn);
		if (off == 0 && size <= i) {
			file->i_bn = fsbtodb(fs, sbmap(file, lbn)) +
			    file->i_boff;
			file->i_cc = size;
			file->i_ma = buf;
			if (devread(file) < 0) {
				return (-1);
			}
			file->i_offset += size;
			file->i_cc = 0;
			buf += size;
			i -= size;
		} else {
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

#if CHECK_CAREFULLY
static int		open_init;
#endif CHECK_CAREFULLY
static struct fs	*fs_block;
static int		fs_block_valid;

#define SUPERBLOCK_ERROR	"Bad superblock: error %d\n"

int
open(char *str, int how)
{
	register char *cp;
	register struct iob *file;
	int i, fdesc;

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
	file->i_cc = SBSIZE;
	file->i_bn = (SBLOCK / DEV_BSIZE) + file->i_boff;
	file->i_offset = 0;

	if (file->i_fs == 0) {
		if (fs_block == 0) {
		    fs_block = (struct fs *)malloc(SBSIZE);
		}
		if (fs_block_valid == 0) {
		    file->i_ma = (char *)fs_block;
		    if (devread(file) < 0) {
#ifndef SMALL
			    error(SUPERBLOCK_ERROR, 1);
#endif
			    close(fdesc);
			    return (-1);
		    }
		    byte_swap_superblock(fs_block);
		    fs_block_valid = 1;
		}
		file->i_fs = fs_block;
		file->i_buf = malloc(MAXBSIZE);
	}
#if	BIG_ENDIAN_FS

	if (file->i_fs->fs_magic != FS_MAGIC) {
		error(SUPERBLOCK_ERROR, 2);
		close(fdesc);
		return (-1);
	}
	/*
	 *  The following is a gross hack to boot disks that have an actual
	 *  blocksize of 512 bytes but were written with a theoretical 1024
	 *  byte blocksize (fsbtodb == 0).
	 *
	 *  We can make this assumption because we can only boot disks with
	 *  a 512 byte sector size.
	 */
	if (file->i_fs->fs_fsize == 0) {
		error(SUPERBLOCK_ERROR,3);
		close(fdesc);
		return (-1);
	}
	file->i_fs->fs_fsbtodb = ffs(file->i_fs->fs_fsize / DEV_BSIZE) - 1;
#endif	BIG_ENDIAN_FS

	if ((i = find(cp, file)) == 0) {
		close(fdesc);
		return (-1);
	}
#if	CHECK_CAREFULLY
	if (how != 0) {
		error("Can't write files\n");
		close(fdesc);
		return (-1);
	}
#endif	CHECK_CAREFULLY

	if (openi(i, file) < 0) {
		close(fdesc);
		return (-1);
	}
	file->i_offset = 0;
	file->i_cc = 0;
	file->i_flgs |= F_FILE | (how+1);

	return (fdesc);
}

#define LP '('
#define RP ')'

static char * xx(char *str, struct iob *file)
{
	register char *cp = str, *xp;
	char **dp;
	int old_dev = kernBootStruct->kernDev;
	int dev = (kernBootStruct->kernDev >> B_TYPESHIFT) & B_TYPEMASK;
	int unit = (kernBootStruct->kernDev >> B_UNITSHIFT) & B_UNITMASK;
	int part = (kernBootStruct->kernDev >> B_PARTITIONSHIFT) &
							B_PARTITIONMASK;
	int i;
	int no_dev;
	int biosOffset;

	biosOffset = unit;		// set the device

	for (; *cp && *cp != LP; cp++) ;
	if (no_dev = !*cp) {  // no paren found
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
		unit = i;
	}

	biosOffset = unit;		// set the device

	if (*cp == RP || no_dev)
		/* do nothing since ptol(")") returns 0 */ ;
	else if (*cp == ',' )
		part = ptol(++cp);
	else if (cp[-1] == LP) 
		part = ptol(cp);
	else {
badoff:
		error("Missing offset specification\n");
		return ((char *)-1);
	}

	for ( ;!no_dev ;) {
		if (*cp == RP)
			break;
		if (*cp++)
			continue;
		goto badoff;
	}

none:
	file->i_ino.i_dev = dev = dp-devsw;
	file->partition = part;
	file->biosdev = (BIOSDEV(dev)) + biosOffset;
	if (dev == DEV_SD) {
		file->biosdev += kernBootStruct->numIDEs;
	} else if (dev == DEV_HD && kernBootStruct->numIDEs == 0) {
		error("No IDE drives detected\n");
		return ((char *)-1);
	}

	kernBootStruct->kernDev = (dev << B_TYPESHIFT) | 
		(unit << B_UNITSHIFT) |
		(part << B_PARTITIONSHIFT);
		
	if (kernBootStruct->kernDev != old_dev)
	    flushdev();

	devopen(str, file);

	if (file->i_error) 
		return (char *)-1;
	if (!no_dev && *cp) cp++;

	gFilename = cp;

	return cp;
}

int
close(int fdesc)
{
	register struct iob *file;
	register int i;

	if ((file = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}
//	free((char *)file->i_fs);
	file->i_fs = NULL;
	free(file->i_buf); file->i_buf = NULL;
	for (i=0;i<NBUFS;i++)
	{
		if (b[i])
		{	free(b[i]); 
			b[i] = NULL;
		}
		blknos[i] = 0;
	}

	file->i_flgs = 0;
	return (0);
}

volatile void stop(char *s)
{
#if	CHECK_CAREFULLY
	register int i;
	
	for (i = 0; i < NFILES; i++)
		if (iob[i].i_flgs != 0)
			close(i);
#endif	CHECK_CAREFULLY
/* 	textMode();*/		// can't call this function from here
	printf("\n%s\n", s);
	sleep(4);		// about to halt
	halt();
}

static int ffs(register long mask)
{
	register int cnt;

	if (mask == 0) return(0);
	for (cnt = 1; !(mask & 1); cnt++)
		mask >>= 1;
	return(cnt);
}

int file_size(int fdesc)
{
	register struct iob *io;
    
	if ((io = iob_from_fdesc(fdesc)) == 0) {
		return (-1);
	}
	return io->i_ino.i_size;
}

/* ensure that all device caches are flushed,
 * because we are about to change the device media
 */
 
void flushdev(void)
{
    register int i;
    
    devflush();
    for (i = 0; i < NFILES; i++)
	if (iob[i].i_flgs & (F_READ | F_WRITE))
	    printf("flushdev: fd %d is open\n",i);
    fs_block_valid = 0;
#if ICACHE
    cacheFlush(icache);
#endif
#if DCACHE
    cacheFlush(dcache);
#endif
}

