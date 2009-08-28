/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/mount.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <string.h>
#include <unistd.h>

#include "fsck.h"
#if	REV_ENDIAN_FS
#include "ufs_byte_order.h"
#endif	/* REV_ENDIAN_FS */

#define _PATH_SBIN	"/sbin"

int	returntosingle;

static int argtoi __P((int flag, char *req, char *str, int base));
static int docheck __P((struct fstab *fsp));
static int checkfilesys __P((char *filesys, char *mntpt, char *vfstype,
		int child));
static int check_other __P((char *vfstype, char *filesys));
int main __P((int argc, char *argv[]));

/* Decls carried from fsck.h due dyld probs */
char	*cdevname;		/* name of device being checked */
long	dev_bsize;		/* computed value of DEV_BSIZE */
long	secsize;		/* actual disk sector size */
char	nflag;			/* assume a no response */
char	yflag;			/* assume a yes response */
int	bflag;			/* location of alternate super block */
#ifdef __APPLE__
int	    qflag;		/* quick check returns clean, dirty, or failure */
#endif /* __APPLE__ */
int	debug;			/* output debugging info */
int	fflag=0;		/* force fsck even if clean */
int	cvtlevel;		/* convert to newer file system format */
int	doinglevel1;		/* converting to new cylinder group format */
int	doinglevel2;		/* converting to new inode format */
int	newinofmt;		/* filesystem has new inode format */
char	preen;			/* just fix normal inconsistencies */
char	havesb;			/* superblock has been read */
int	fsmodified;		/* 1 => write done to file system */
int	fsreadfd;		/* file descriptor for reading file system */
int	fswritefd;		/* file descriptor for writing file system */

ufs_daddr_t maxfsblock;		/* number of blocks in the file system */
char	*blockmap;		/* ptr to primary blk allocation map */
u_int32_t	maxino;			/* number of inodes in file system */
u_int32_t	lastino;		/* last inode in use */
char	*statemap;		/* ptr to inode state table */
u_char	*typemap;		/* ptr to inode type table */
short	*lncntp;		/* ptr to link count table */

u_int32_t	lfdir;			/* lost & found directory inode number */

ufs_daddr_t n_blks;		/* number of blocks in use */
ufs_daddr_t n_files;		/* number of files in use */

struct	dinode zino;

struct bufarea bufhead;		/* head of list of other blks in filesys */
struct bufarea sblk;		/* file system superblock */
struct bufarea cgblk;		/* cylinder group blocks */
struct bufarea *pdirbp;		/* current directory contents */
struct bufarea *pbp;		/* current inode block */
struct dups *duplist;		/* head of dup list */
struct dups *muldup;		/* end of unique duplicate dup block numbers */
struct zlncnt *zlnhead;		/* head of zero link count list */
struct inoinfo **inphead, **inpsort;
long numdirs, listmax, inplast;
#if REV_ENDIAN_FS
int rev_endian=0;
#endif /* REV_ENDIAN_FS */
int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int ch;
	int ret, maxrun = 0;
	extern char *optarg;
	extern int optind;

	sync();
#ifdef __APPLE__
	while ((ch = getopt(argc, argv, "dfpqnNyYb:l:m:")) != EOF) {
#else
	while ((ch = getopt(argc, argv, "dfpnNyYb:l:m:")) != EOF) {
#endif /* __APPLE__ */
		switch (ch) {
		case 'p':
			preen++;
			break;

		case 'b':
			bflag = argtoi('b', "number", optarg, 10);
			printf("Alternate super block location: %d\n", bflag);
			break;
#if 0
		// Needs to be tested; disabled for developer:1678097
		case 'c':
			cvtlevel = argtoi('c', "conversion level", optarg, 10);
			break;
#endif 0		
		case 'd':
			debug++;
			break;

		case 'f':
			fflag++;
			break;

		case 'l':
			maxrun = argtoi('l', "number", optarg, 10);
			break;

		case 'm':
			lfmode = argtoi('m', "mode", optarg, 8);
			if (lfmode &~ 07777)
				errx(EEXIT, "bad mode to -m: %o", lfmode);
			printf("** lost+found creation mode %o\n", lfmode);
			break;

		case 'n':
		case 'N':
			nflag++;
			yflag = 0;
			break;

		case 'y':
		case 'Y':
			yflag++;
			nflag = 0;
			break;
#ifdef __APPLE__
		case 'q':
		    qflag++;
			break;
#endif /* __APPLE__ */

		default:
			errx(EEXIT, "%c option?", ch);
		}
	}
	argc -= optind;
	argv += optind;
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, catch);
	if (preen)
		(void)signal(SIGQUIT, catchquit);
	if (argc) {
		ret = 0;
		while (argc-- > 0)
			ret |= checkfilesys(blockcheck(*argv++), 0, 0, 0);
		exit(ret);
	}
	ret = checkfstab(preen, maxrun, docheck, checkfilesys);
	if (returntosingle)
		exit(2);

	exit(ret);
}

static int
argtoi(flag, req, str, base)
	int flag;
	char *req, *str;
	int base;
{
	char *cp;
	int ret;

	ret = (int)strtol(str, &cp, base);
	if (cp == str || *cp)
		errx(EEXIT, "-%c flag requires a %s", flag, req);
	return (ret);
}

/*
 * Determine whether a filesystem should be checked.
 *
 * Zero indicates that no check should be performed.
 */
static int
docheck(fsp)
	register struct fstab *fsp;
{
	/* Should we just allow all vfstypes? */
	if (strcmp(fsp->fs_vfstype, "ufs")	&&
		strcmp(fsp->fs_vfstype, "hfs")	&&
		strcmp(fsp->fs_vfstype, "msdos"))
		return 0;

	if ((strcmp(fsp->fs_type, FSTAB_RW) && strcmp(fsp->fs_type, FSTAB_RO)) ||
	    fsp->fs_passno == 0)
		return 0;

#define	DISKARB_LABEL	"LABEL="
#define	DISKARB_UUID	"UUID="

	if ((strncmp(fsp->fs_spec, DISKARB_LABEL, strlen(DISKARB_LABEL)) == 0)
		|| (strncmp(fsp->fs_spec, DISKARB_UUID, strlen(DISKARB_UUID)) == 0))
		return 0;

	return 1;
}

/*
 * Check the specified filesystem.
 */
/* ARGSUSED */
static int
checkfilesys(char *filesys, char *mntpt, char *vfstype, int child)
{
	ufs_daddr_t n_ffree, n_bfree;
	struct dups *dp;
	struct zlncnt *zlnp;
	int cylno, flags;

	if (preen && child)
		(void)signal(SIGQUIT, voidquit);

	if (vfstype) {
		/* Use a filesystem-specific fsck for non-UFS */
		if (strcmp(vfstype, "ufs") != 0)
			return check_other(vfstype, filesys);
	}

	cdevname = filesys;
	if (debug && preen)
		pwarn("starting\n");

        switch (setup(filesys)) {
        case 0:
            if (preen)
                pfatal("CAN'T CHECK UFS FILE SYSTEM.");
	    else
		return (EEXIT);
        case -1:
            return (0);
#ifdef __APPLE__
        case DIRTYEXIT:
            if (qflag)				/* File system is dirty */
            return (DIRTYEXIT);
            break;
        case EEXIT:
            if (qflag)				/* Other file system inconsistency */
            return (EEXIT);
            break;
	case CANTFIXIT:
	    return (CANTFIXIT);
	    break;
#endif /* __APPLE__ */
        }
    
        /*
         * 1: scan inodes tallying blocks used
         */
        if (preen == 0) {
            printf("** Last Mounted on %s\n", sblock.fs_fsmnt);
            if (hotroot)
                printf("** Root file system\n");
            printf("** Phase 1 - Check Blocks and Sizes\n");
        }
        pass1();
    
        /*
         * 1b: locate first references to duplicates, if any
         */
        if (duplist) {
            if (preen)
                pfatal("INTERNAL ERROR: dups with -p");
            printf("** Phase 1b - Rescan For More DUPS\n");
            pass1b();
        }
    
        /*
         * 2: traverse directories from root to mark all connected directories
         */
        if (preen == 0)
            printf("** Phase 2 - Check Pathnames\n");
        pass2();
    
        /*
         * 3: scan inodes looking for disconnected directories
         */
        if (preen == 0)
            printf("** Phase 3 - Check Connectivity\n");
        pass3();
    
        /*
         * 4: scan inodes looking for disconnected files; check reference counts
         */
        if (preen == 0)
            printf("** Phase 4 - Check Reference Counts\n");
        pass4();
    
        /*
         * 5: check and repair resource counts in cylinder groups
         */
        if (preen == 0)
            printf("** Phase 5 - Check Cyl groups\n");
        pass5();
    
        /*
         * print out summary statistics
         */
        n_ffree = sblock.fs_cstotal.cs_nffree;
        n_bfree = sblock.fs_cstotal.cs_nbfree;
        pwarn("%ld files, %ld used, %ld free ",
            n_files, n_blks, n_ffree + sblock.fs_frag * n_bfree);
        printf("(%u frags, %u blocks, %d.%d%% fragmentation)\n",
            n_ffree, n_bfree, (n_ffree * 100) / sblock.fs_dsize,
            ((n_ffree * 1000 + sblock.fs_dsize / 2) / sblock.fs_dsize) % 10);
        if (debug &&
            (n_files -= maxino - ROOTINO - sblock.fs_cstotal.cs_nifree))
            printf("%u files missing\n", n_files);
        if (debug) {
            n_blks += sblock.fs_ncg *
                (cgdmin(&sblock, 0) - cgsblock(&sblock, 0));
            n_blks += cgsblock(&sblock, 0) - cgbase(&sblock, 0);
            n_blks += howmany(sblock.fs_cssize, sblock.fs_fsize);
            if (n_blks -= maxfsblock - (n_ffree + sblock.fs_frag * n_bfree))
                printf("%u blocks missing\n", n_blks);
            if (duplist != NULL) {
                printf("The following duplicate blocks remain:");
                for (dp = duplist; dp; dp = dp->next)
                    printf(" %u,", dp->dup);
                printf("\n");
            }
            if (zlnhead != NULL) {
                printf("The following zero link count inodes remain:");
                for (zlnp = zlnhead; zlnp; zlnp = zlnp->next)
                    printf(" %u,", zlnp->zlncnt);
                printf("\n");
            }
        }
        zlnhead = (struct zlncnt *)0;
        duplist = (struct dups *)0;
        muldup = (struct dups *)0;
        inocleanup();
        if (fsmodified) {
	    time_t tmp;
            (void)time(&tmp);
	    sblock.fs_time = tmp;
            sbdirty();
        }
        if (cvtlevel && sblk.b_dirty) {
            /* 
             * Write out the duplicate super blocks
             */
            for (cylno = 0; cylno < sblock.fs_ncg; cylno++) {
#if	REV_ENDIAN_FS
                if (rev_endian) byte_swap_sbout(&sblock);
#endif	/* REV_ENDIAN_FS */
                bwrite(fswritefd, (char *)&sblock,
                    fsbtodb(&sblock, cgsblock(&sblock, cylno)), SBSIZE);
#if	REV_ENDIAN_FS
                if (rev_endian) byte_swap_sbin(&sblock);
#endif	/* REV_ENDIAN_FS */
            }
        }
        flags = 0;
        if (!hotroot) {
            ckfini(1);
        } else {
            struct statfs stfs_buf;
            /*
             * Check to see if root is mounted read-write.
             */
            if (statfs("/", &stfs_buf) == 0)
                flags = stfs_buf.f_flags;
            ckfini(flags & MNT_RDONLY);
        }
        free(blockmap);
        free(statemap);
        free((char *)lncntp);
        if (!fsmodified)
            return (0);
    
	if (!preen)
		printf("\n***** FILE SYSTEM WAS MODIFIED *****\n");
	if (hotroot) {
		struct ufs_args args;
		int ret;
		/*
		 * We modified the root.  Do a mount update on
		 * it, unless it is read-write, so we can continue.
		 */
		if (flags & MNT_RDONLY) {
			args.fspec = 0;
			flags |= MNT_UPDATE | MNT_RELOAD;
			ret = mount("ufs", "/", flags, &args);
			if (ret == 0)
				return (0);
		}
		if (!preen)
			printf("\n***** REBOOT NOW *****\n");
		sync();
		return (4);
	}
	return (0);
}


/*
 * Check non-UFS file system
 */
static int
check_other(char *vfstype, char *filesys)
{
#define ARGC_MAX 4	/* cmd-name, options, device, NULL-termination */

	const char *argv[ARGC_MAX];
	int argc;
	pid_t pid;
	int status = 0;
	char options[] = "-pdfnyq";  /* constant strings are not on the stack */
	char progname[NAME_MAX];
	char execname[MAXPATHLEN + 1];

	bzero(options, sizeof(options));
	snprintf(options, sizeof(options), "-%s%s%s%s%s%s",
				(preen) ? "p" : "",
				(debug) ? "d" : "",
				(fflag) ? "f" : "",
				(nflag) ? "n" : "",
				(yflag) ? "y" : "",
				(qflag) ? "q" : ""
				);

	argc = 0;
	snprintf(progname, sizeof(progname), "fsck_%s", vfstype);
	argv[argc++] = progname;
	if (strlen(options) > 1)
		argv[argc++] = options;
	argv[argc++] = filesys;
	argv[argc] = NULL;

	(void)snprintf(execname, sizeof(execname), "%s/fsck_%s", _PATH_SBIN, vfstype);

	pid = vfork();
	switch (pid) {
		case -1:
			/* The vfork failed. */
			pfatal("vfork");
			return 8;	/* Is this the right value? */
		break;
		
		case 0:
			/* The child */
			execv(execname, (char * const *)argv);
			pfatal("exec %s", execname);
			_exit(8);
		break;
		
		default:
			/* The parent; child is process "pid" */
			waitpid(pid, &status, 0);
			if (WIFEXITED(status))
				status = WEXITSTATUS(status);
			else
				status = 0;
			if (WIFSIGNALED(status)) {
				printf("%s (%s) EXITED WITH SIGNAL %d\n", filesys, vfstype, WTERMSIG(status));
				status = 8;
			}
		break;
	}

	return status;
}
