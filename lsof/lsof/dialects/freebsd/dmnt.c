/*
 * dmnt.c - FreeBSD mount support functions for lsof
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
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
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dmnt.c,v 1.8 2000/08/09 14:56:29 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local static information
 */

static struct mounts *Lmi = (struct mounts *)NULL;	/* local mount info */
static int Lmist = 0;					/* Lmi status */

#undef	HAS_MNT_NAMES

#if	FREEBSDV<200
static char *mnt_names[] = { "none", "ufs", "nfs", "mfs", "pc", "iso9600",
			     "procfs", "devfs" };
#define	HAS_MNT_NAMES	1
#else	/* FREEBSDV>=200 */
# if	defined(MOUNT_NONE)
static char *mnt_names[] = INITMOUNTNAMES;
#define	HAS_MNT_NAMES	1
# endif	/* defined(MOUNT_NONE)) */
#endif	/* FREEBSDV<200 */


/*
 * readmnt() - read mount table
 */

struct mounts *
readmnt()
{
	char *dn = (char *)NULL;
	char *ln;
	struct statfs *mb = (struct statfs *)NULL;
	struct mounts *mtp;
	int n;
	struct stat sb;

#if	defined(HASPROCFS)
	unsigned char procfs = 0;
#endif	/* defined(HASPROCFS) */

	if (Lmi || Lmist)
	    return(Lmi);
/*
 * Access mount information.
 */
	if ((n = getmntinfo(&mb, MNT_NOWAIT)) <= 0) {
	    (void) fprintf(stderr, "%s: no mount information\n", Pn);
	    return(0);
	}
/*
 * Read mount information.
 */
	for (; n; n--, mb++) {

#if	defined(MOUNT_NONE)
	    if (mb->f_type == MOUNT_NONE || mb->f_type >= MOUNT_MAXTYPE)
#else	/* !defined(MOUNT_NONE) */
	    if (!mb->f_type)
#endif	/* defined(MOUNT_NONE) */

		continue;
	/*
	 * Interpolate a possible symbolic directory link.
	 */
	    if (dn)
		(void) free((FREE_P *)dn);
	    if (!(dn = mkstrcpy(mb->f_mntonname, (MALLOC_S *)NULL))) {

no_space_for_mount:

		(void) fprintf(stderr, "%s: no space for mount at ", Pn);
		safestrprt(mb->f_mntonname, stderr, 0);
		(void) fprintf(stderr, " (");
		safestrprt(mb->f_mntfromname, stderr, 0);
		(void) fprintf(stderr, ")\n");
		Exit(1);
	    }
	    if (!(ln = Readlink(dn))) {
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"      Output information may be incomplete.\n");
		}
		continue;
	    }
	    if (ln != dn) {
		(void) free((FREE_P *)dn);
		dn = ln;
	    }
	/*
	 * Stat() the directory.
	 */
	    if (statsafely(dn, &sb)) {
		if (!Fwarn) {
		    (void) fprintf(stderr, "%s: WARNING: can't stat() ", Pn);

#if	defined(HAS_MNT_NAMES)
		    safestrprt(mnt_names[mb->f_type], stderr, 0);
#else	/* !defined(HAS_MNT_NAMES) */
		    safestrprt(mb->f_fstypename, stderr, 0);
#endif	/* defined(HAS_MNT_NAMES) */

		    (void) fprintf(stderr, " file system ");
		    safestrprt(mb->f_mntonname, stderr, 1);
		    (void) fprintf(stderr,
			"      Output information may be incomplete.\n");
		}
		(void) bzero((char *)&sb, sizeof(sb));
		sb.st_dev = (dev_t)mb->f_fsid.val[0];
		sb.st_mode = S_IFDIR | 0777;
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"      assuming \"dev=%x\" from mount table\n",
			sb.st_dev);
		}
	    }
	/*
	 * Allocate and fill a local mount structure.
	 */
	    if (!(mtp = (struct mounts *)malloc(sizeof(struct mounts))))
		goto no_space_for_mount;
	    mtp->dir = dn;
	    dn = (char *)NULL;

#if	defined(HASPROCFS)

#if	defined(MOUNT_NONE)
	    if (mb->f_type == MOUNT_PROCFS)
#else	/* !defined(MOUNT_NONE) */
	    if (strcasecmp(mb->f_fstypename, "procfs") == 0)
#endif	/* defined(MOUNT_NONE) */

	    {

	    /*
	     * Save information on exactly one procfs file system.
	     */
		if (procfs)
		    Mtprocfs = (struct mounts *)NULL;
		else {
		    procfs = 1;
		    Mtprocfs = mtp;
		}
	    }
#endif	/* defined(HASPROCFS) */

	    mtp->next = Lmi;
	    mtp->dev = sb.st_dev;
	    mtp->rdev = sb.st_rdev;
	    mtp->inode = sb.st_ino;
	    mtp->mode = sb.st_mode;
	/*
	 * Interpolate a possible file system (mounted-on) device name link.
	 */
	    if (!(dn = mkstrcpy(mb->f_mntfromname, (MALLOC_S *)NULL)))
		goto no_space_for_mount;
	    mtp->fsname = dn;
	    ln = Readlink(dn);
	    dn = (char *)NULL;
	/*
	 * Stat() the file system (mounted-on) name and add file system
	 * information to the local mount table entry.
	 */
	    if (!ln || statsafely(ln, &sb))
		sb.st_mode = 0;
	    mtp->fsnmres = ln;
	    mtp->fs_mode = sb.st_mode;
	    Lmi = mtp;
	}
/*
 * Clean up and return the local mount info table address.
 */
	if (dn)
	    (void) free((FREE_P *)dn);
	Lmist = 1;
	return(Lmi);
}


/*
 * readvfs() - read vfs structure
 */

struct l_vfs *
readvfs(vm)
	KA_T vm;			/* kernel mount address from vnode */
{
	struct mount m;
	struct l_vfs *vp;
/*
 * Search for match on existing entry.
 */
	for (vp = Lvfs; vp; vp = vp->next) {
	    if (vm == vp->addr)
		return(vp);
	}
/*
 * Read the (new) mount structure, allocate a local entry, and fill it.
 */
	if (kread((KA_T)vm, (char *)&m, sizeof(m)) != 0)
	    return((struct l_vfs *)NULL);
	if (!(vp = (struct l_vfs *)malloc(sizeof(struct l_vfs)))) {
	    (void) fprintf(stderr, "%s: PID %d, no space for vfs\n",
		Pn, Lp->pid);
	    Exit(1);
	}
	if (!(vp->dir = mkstrcpy(m.m_stat.f_mntonname, (MALLOC_S *)NULL))
	||  !(vp->fsname = mkstrcpy(m.m_stat.f_mntfromname, (MALLOC_S *)NULL)))
	{
	    (void) fprintf(stderr, "%s: PID %d, no space for mount names\n",
		Pn, Lp->pid);
	    Exit(1);
	}
	vp->addr = vm;
	vp->fsid = m.m_stat.f_fsid;

#if	defined(MOUNT_NONE)
	vp->type = m.m_stat.f_type;
#else	/* !defined(MOUNT_NONE) */
	{
	    int len;

	    if ((len = strlen(m.m_stat.f_fstypename))) {
		if (len > (MFSNAMELEN - 1))
		    len = MFSNAMELEN - 1;
		if (!(vp->typnm = mkstrcat(m.m_stat.f_fstypename, len,
				  (char *)NULL, -1, (char *)NULL, -1,
				  (MALLOC_S *)NULL)))
		{
		    (void) fprintf(stderr,
			"%s: no space for fs type name: ", Pn);
		    safestrprt(m.m_stat.f_fstypename, stderr, 1);
		    Exit(1);
		}
	    } else
		vp->typnm = "";
	}
#endif	/* defined(MOUNT_NONE) */

	vp->next = Lvfs;
	Lvfs = vp;
	return(vp);
}
