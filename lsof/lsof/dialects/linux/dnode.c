/*
 * dnode.c - Linux node functions for /proc-based lsof
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

#ifndef lint
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dnode.c,v 1.12 2001/04/19 12:14:23 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local definitions
 */

#define	OFFSET_MAX	((off_t)0x7fffffff)	/* this is defined in
						 * .../src/fs/locks.c and not
						 * in a header file */
#define	PIDBUCKS	64			/* PID hash buckets */
#define	HASHPID(pid)	(((int)((pid * 31415) >> 3)) & (PIDBUCKS - 1))


/*
 * Local structure definitions
 */

struct llock {
	int pid;
	dev_t dev;
	ino_t inode;
	char type;
	struct llock *next;
};


/*
 * Local definitions
 */

struct llock **LckH = (struct llock **)NULL; /* PID-hashed locks */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void check_lock,(void));


/*
 * check_lock() - check lock for file *Lf, process *Lp
 */

static void
check_lock()
{
	int h;
	struct llock *lp;

	h = HASHPID(Lp->pid);
	for (lp = LckH[h]; lp; lp = lp->next) {
	    if (Lp->pid == lp->pid
	    &&  Lf->dev == lp->dev
	    &&  Lf->inode == (unsigned long)lp->inode)
	    {
		Lf->lock = lp->type;
		return;
	    }
	}
}


/*
 * get_fields() - separate a line into fields
 */

int
get_fields(ln, sep, fr)
	char *ln;			/* input line */
	char *sep;			/* separator list */
	char ***fr;			/* field pointer return address */
{
	char *bp, *cp, *sp;
	MALLOC_S len;
	int n;
	static char **fp = (char **)NULL;
	static int nfpa = 0;

	for (cp = ln, n = 0; cp && *cp;) {
	    for (bp = cp; *bp && (*bp == ' ' || *bp == '\t'); bp++);
		;
	    if (!*bp || *bp == '\n')
		break;
	    for (cp = bp; *cp; cp++) {
		if (*cp == '\n') {
		    *cp = '\0';
		    break;
		}
		if (*cp == ' ' || *cp == '\t')
		    break;
		if (sep) {
		    for (sp = sep; *sp; sp++) {
			if (*sp == *cp)
			    break;
		    }
		    if (*sp)
			break;
		}
	    }
	    if (*cp)
		*cp++ = '\0';
	    if (n >= nfpa) {
		nfpa += 32;
		len = (MALLOC_S)(nfpa * sizeof(char *));
		if (fp)
		    fp = (char **)realloc((MALLOC_P *)fp, len);
		else
		    fp = (char **)malloc(len);
		if (!fp) {
		    (void) fprintf(stderr,
			"%s: can't allocate %d bytes for field pointers.\n",
			Pn, len);
		    Exit(1);
		}
	    }
	    fp[n++] = bp;
	}
	*fr = fp;
	return(n);
}


/*
 * get_locks() - get lock information from /proc/locks
 */

void
get_locks(p)
	char *p;				/* /proc lock path */
{
	unsigned long bp, ep;
	char buf[MAXPATHLEN], *ec, **fp;
	dev_t dev;
	int ex, i, h, mode, pid;
	unsigned long inode;
	struct llock *lp, *np;
	long maj, min;
	FILE *ls;
	char type;
/*
 * Destroy previous lock information.
 */
	if (LckH) {
	    for (i = 0; i < PIDBUCKS; i++) {
		for (lp = LckH[i]; lp; lp = np) {
		    np = lp->next;
		    (void) free((FREE_P *)lp);
		}
		LckH[i] = (struct llock *)NULL;
	    }
	} else {

	/*
	 * If first time, allocate the lock PID hash buckets.
	 */
	    LckH = (struct llock **)calloc((MALLOC_S)PIDBUCKS,
					   sizeof(struct llock *));
	    if (!LckH) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d lock hash bytes\n",
		    Pn, sizeof(struct llock *) * PIDBUCKS);
		Exit(1);
	    }
	}
/*
 * Open /proc lock file and read it.
 */
	if (!(ls = fopen(p, "r")))
	    return;
	while (fgets(buf, sizeof(buf), ls)) {
	    if (get_fields(buf, ":", &fp) < 10)
		continue;
	    if (!fp[1] || strcmp(fp[1], "->") == 0)
		continue;
	/*
	 * Get lock type.
	 */
	    if (!fp[3])
		continue;
	    if (*fp[3] == 'R')
		mode = 0;
	    else if (*fp[3] == 'W')
		mode = 1;
	    else
		continue;
	/*
	 * Get PID.
	 */
	    if (!fp[4] || !*fp[4])
		continue;
	    pid = atoi(fp[4]);
	/*
	 * Get device number.
	 */
	    ec = (char *)NULL;
	    if (!fp[5] || !*fp[5]
	    ||  (maj = strtol(fp[5], &ec, 16)) == LONG_MIN || maj == LONG_MAX
	    ||  !ec || *ec)
		continue;
	    ec = (char *)NULL;
	    if (!fp[6] || !*fp[6]
	    ||  (min = strtol(fp[6], &ec, 16)) == LONG_MIN || min == LONG_MAX
	    ||  !ec || *ec)
		continue;
	    dev = (dev_t)makedev((int)maj, (int)min);
	/*
	 * Get inode number.
	 */
	    ec = (char *)NULL;
	    if (!fp[7] || !*fp[7]
	    ||  (inode = strtoul(fp[7], &ec, 0)) == ULONG_MAX
	    ||  !ec || *ec)
		continue;
	/*
	 * Get lock extent.  Convert it and the lock type to a lock character.
	 */
	    if (!fp[8] || !*fp[8] || !fp[9] || !*fp[9])
		continue;
	    ec = (char *)NULL;
	    if ((bp = strtoul(fp[8], &ec, 0)) == ULONG_MAX || !ec || *ec)
		continue;
	    if (!strcmp(fp[9], "EOF"))		/* for Linux 2.4.x */
		ep = OFFSET_MAX;
	    else {
		ec = (char *)NULL;
		if ((ep = strtoul(fp[9], &ec, 0)) == ULONG_MAX || !ec || *ec)
		    continue;
	    }
	    ex = ((off_t)bp == (off_t)0 && (off_t)ep == OFFSET_MAX) ? 1 : 0;
	    if (mode)
		type = ex ? 'W' : 'w';
	    else
		type = ex ? 'R' : 'r';
	/*
	 * Look for this lock via the hash buckets.
	 */
	    h = HASHPID(pid);
	    for (lp = LckH[h]; lp; lp = lp->next) {
		if (lp->pid == pid
		&&  lp->dev == dev
		&&  lp->inode == inode
		&&  lp->type == type)
		    break;
	    }
	    if (lp)
		continue;
	/*
	 * Allocate a new llock structure and link it to the PID hash bucket.
	 */
	    if (!(lp = (struct llock *)malloc(sizeof(struct llock)))) {
		(void) fprintf(stderr,
		    "%s: can't allocate llock: PID %d; dev %x; inode %ld\n",
		    Pn, pid, (int)dev, inode);
		Exit(1);
	    }
	    lp->pid = pid;
	    lp->dev = dev;
	    lp->inode = inode;
	    lp->type = type;
	    lp->next = LckH[h];
	    LckH[h] = lp;
	}
	(void) fclose(ls);
}


/*
 * process_proc_node() - process file node
 */

void
process_proc_node(p, s, l)
	char *p;			/* node's readlink() path */
	struct stat *s;			/* stat() result for path */
	struct stat *l;			/* lstat() result for FD (NULL for
					 * others) */
{
	mode_t access, type;
	char ch, *cp;
	dev_t dev;
	struct mounts *mp;
	size_t sz;
	char *tn;
/*
 * Set the access mode, if possible.
 */
	if (l && (l->st_mode & S_IFMT) == S_IFLNK) {
	    if ((access = l->st_mode & (S_IRUSR | S_IWUSR)) == S_IRUSR)
		Lf->access = 'r';
	    else if (access == S_IWUSR)
		Lf->access = 'w';
	    else
		Lf->access = 'u';
	}
/*
 * Determine node type.
 */
	type = s->st_mode & S_IFMT;
	switch (type) {
	case S_IFBLK:
	    Ntype = N_BLK;
	    break;
	case S_IFCHR:
	    Ntype = N_CHR;
	    break;
	case S_IFIFO:
	    Ntype = N_FIFO;
	    break;
	case S_IFSOCK:
	    process_proc_sock(p, s, l);
	    return;
	}
	if (Selinet)
	    return;
/*
 * Save the device.  If it is an NFS device, change the node type to N_NFS.
 */
	Lf->dev = s->st_dev;
	Lf->dev_def = 1;
	if ((Ntype == N_CHR || Ntype == N_BLK)) {
	    Lf->rdev = s->st_rdev;
	    Lf->rdev_def = 1;
	}
	if (Ntype == N_REGLR && HasNFS) {
	    for (mp = readmnt(); mp; mp = mp->next) {
		if (mp->ty == N_NFS && Lf->dev == mp->dev) {
		    Ntype = N_NFS;
		    break;
		}
	    }
	}
/*
 * Save the inode number.
 */
	Lf->inode = (unsigned long)s->st_ino;
	Lf->inp_ty = 1;
/*
 * Check for a lock.
 */
	(void) check_lock();
/*
 * Save the file size.
 */
	switch (Ntype) {
	case N_BLK:
	case N_CHR:
	case N_FIFO:
	    if (!Fsize && l) {
		Lf->off = (SZOFFTYPE)l->st_size;
		Lf->off_def = 1;
	    }
	    break;
	default:
	    if (Foffset) {
		if (l)
		    Lf->off = (SZOFFTYPE)l->st_size;
		Lf->off_def = 1;
	    } else if (!Foffset || Fsize) {
		Lf->sz = (SZOFFTYPE)s->st_size;
		Lf->sz_def = 1;
	    }
	}
/*
 * Record the link count.
 */
	if (Fnlink) {
	    Lf->nlink = (long)s->st_nlink;
	    Lf->nlink_def = 1;
	    if (Nlink && (Lf->nlink < Nlink))
		Lf->sf |= SELNLINK;
	}
/*
 * Format the type name.
 */
	switch (type) {
	case S_IFBLK:
	    tn = "BLK";
	    break;
	case S_IFCHR:
	    tn = "CHR";
	    break;
	case S_IFDIR:
	    tn = "DIR";
	    break;
	case S_IFIFO:
	    tn = "FIFO";
	    break;
	case S_IFREG:
	    tn = "REG";
	    break;
	case S_IFLNK:
	    tn = "LINK";
	    break;
	default:
	    if (type > 9999)
		(void) snpf(Lf->type, sizeof(Lf->type), "*%03d", type % 1000);
	    else
		(void) snpf(Lf->type, sizeof(Lf->type), "%04d", type);
	    (void) snpf(Namech, Namechl, "unknown inode type");
	    tn = (char *)NULL;
	}
	if (tn)
	    (void) snpf(Lf->type, sizeof(Lf->type), "%s", tn);
	Lf->ntype = Ntype;
/*
 * Record an NFS file selection.
 */
	if (Ntype == N_NFS && Fnfs)
	    Lf->sf |= SELNFS;
/*
 * Test for specified file.
 */
	if (Sfile
	&& is_file_named((char *)NULL,
			 ((type == S_IFCHR) || (type == S_IFBLK)) ? 1 : 0))
	    Lf->sf |= SELNM;
/*
 * If no NAME information has been stored, store the path.
 *
 * Store the remote host and mount point for an NFS file.
 */
	if (!Namech[0]) {
	    (void) snpf(Namech, Namechl, "%s", p);
	    if (Ntype == N_NFS && mp->fsname) {
		cp = endnm(&sz);
		(void) snpf(cp, sz, " (%s)", mp->fsname);
	    }
	}
	if (Namech[0])
	    enter_nm(Namech);
}
