/*
 * dlsof.h - Linux header file for /proc-based lsof
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


/*
 * $Id: dlsof.h,v 1.10 2000/12/04 14:31:02 abe Exp $
 */


#if	!defined(LINUX_LSOF_H)
#define	LINUX_LSOF_H	1

#include <dirent.h>
#define	DIRTYPE	dirent			/* for arg.c's enter_dir() */
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <netinet/in.h>

# if	defined(GLIBCV)
#include <netinet/tcp.h>
# else	/* !defined(GLIBCV) */
#include <linux/tcp.h>
# endif	/* defined(GLIBCV) */

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>


/*
 * This definition is needed for the common function prototype  definitions
 * in "proto.h", but isn't used in /proc-based lsof.
 */

typedef	off_t		KA_T;


/*
 * Local definitions
 */

#define	COMP_P		const void
#define DEVINCR		1024	/* device table malloc() increment */
#define	FSNAMEL		4
#define MALLOC_P	void
#define FREE_P		MALLOC_P
#define MALLOC_S	size_t
#define	PROCFS		"/proc"
#define QSORT_P		void
#define	READLEN_T	size_t
#define STRNCPY_L	size_t
#define	STRNML		32
#define	XDR_PMAPLIST	(xdrproc_t)xdr_pmaplist
#define	XDR_VOID	(xdrproc_t)xdr_void


/*
 * Global storage definitions (including their structure definitions)
 */

struct mounts {
        char *dir;              	/* directory (mounted on) */
	char *fsname;           	/* file system
					 * (symbolic links unresolved) */
	char *fsnmres;           	/* file system
					 * (symbolic links resolved) */
        dev_t dev;              	/* directory st_dev */
	dev_t rdev;			/* directory st_rdev */
	ino_t inode;			/* directory st_ino */
	mode_t mode;			/* directory st_mode */
	mode_t fs_mode;			/* file system st_mode */
	int ty;				/* node type -- e.g., N_REGLR, N_NFS */
        struct mounts *next;    	/* forward link */
};

struct sfile {
	char *aname;			/* argument file name */
	char *name;			/* file name (after readlink()) */
	char *devnm;			/* device name (optional) */
	dev_t dev;			/* device */
	dev_t rdev;			/* raw device */
	mode_t mode;			/* S_IFMT mode bits from stat() */
	int type;			/* file type: 0 = file system
				 	 *	      1 = regular file */
	ino_t i;			/* inode number */
	int f;				/* file found flag */
	struct sfile *next;		/* forward link */
};

extern	int	HasNFS;

#endif	/* LINUX_LSOF_H	*/
