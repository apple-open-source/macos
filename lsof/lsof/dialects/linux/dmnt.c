/*
 * dmnt.c -- Linux mount support functions for /proc-based lsof
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

#ifndef	lint
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dmnt.c,v 1.10 2000/11/09 18:04:43 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local static definitions
 */

static struct mounts *Lmi = (struct mounts *)NULL;	/* local mount info */
static int Lmist = 0;					/* Lmi status */


/*
 * readmnt() - read mount table
 */

struct mounts *
readmnt()
{
	char buf[MAXPATHLEN], *cp, **fp;
	char *dn = (char *)NULL;
	char *ln;
	struct mounts *mp;
	FILE *ms;
	struct stat sb;

	if (Lmi || Lmist)
	    return(Lmi);
/*
 * Open access to /proc/mounts
 */
	(void) snpf(buf, sizeof(buf), "%s/mounts", PROCFS);
	if (!(ms = fopen(buf, "r"))) {
	    (void) fprintf(stderr, "%s: can't fopen(%s)\n", Pn, buf);
	    Exit(1);
	}
/*
 * Read mount table entries.
 */
	while (fgets(buf, sizeof(buf), ms)) {
	    if (get_fields(buf, (char *)NULL, &fp) < 3
	    ||  !fp[0] || !fp[1] || !fp[2])
		continue;
	/*
	 * Ignore an entry with a colon in the device name, followed by
	 * "(pid*" -- it's probably an automounter entry.
	 *
	 * Ignore autofs, pipefs, and sockfs entries.
	 */
	    if ((cp = strchr(fp[0], ':')) && !strncasecmp(++cp, "(pid", 4))
		continue;
	    if (!strcasecmp(fp[2], "autofs") || !strcasecmp(fp[2], "pipefs")
	    ||  !strcasecmp(fp[2], "sockfs"))
		continue;
	/*
	 * Interpolate a possible symbolic directory link.
	 */
	    if (dn)
		(void) free((FREE_P *)dn);
	    if (!(dn = mkstrcpy(fp[1], (MALLOC_S *)NULL))) {
		(void) fprintf(stderr, "%s: no space for: ", Pn);
		safestrprt(fp[1], stderr, 1);
		Exit(1);
	    }
	    if (!(ln = Readlink(dn))) {
		if (!Fwarn){
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
		    safestrprt(fp[2], stderr, 0);
		    (void) fprintf(stderr, " file system ");
		    safestrprt(fp[1], stderr, 1);
		    (void) fprintf(stderr,
			"      Output information may be incomplete.\n");
		}
		continue;
	    }
	/*
	 * Allocate and fill a local mount structure.
	 */
	    if (!(mp = (struct mounts *)malloc(sizeof(struct mounts)))) {
		(void) fprintf(stderr,
		    "%s: can't allocate mounts struct for: ", Pn);
		safestrprt(dn, stderr, 1);
		Exit(1);
	    }
	    mp->dir = dn;
	    dn = (char *)NULL;
	    mp->next = Lmi;
	    mp->dev = sb.st_dev;
	    mp->rdev = sb.st_rdev;
	    mp->inode = sb.st_ino;
	    mp->mode = sb.st_mode;
	    if (strcasecmp(fp[2], "nfs") == 0) {
		HasNFS = 1;
		mp->ty = N_NFS;
	    } else
		mp->ty = N_REGLR;
	/*
	 * Interpolate a possible file system (mounted-on) device name link.
	 */
	    if (!(dn = mkstrcpy(fp[0], (MALLOC_S *)NULL))) {
		(void) fprintf(stderr, "%s: no space for: ", Pn);
		safestrprt(fp[0], stderr, 1);
		Exit(1);
	    }
	    mp->fsname = dn;
	    ln = Readlink(dn);
	    dn = (char *)NULL;
	/*
	 * Stat() the file system (mounted-on) name and add file system
	 * information to the local mount table entry.
	 */
	    if (!ln || statsafely(ln, &sb))
		sb.st_mode = 0;
	    mp->fsnmres = ln;
	    mp->fs_mode = sb.st_mode;
	    Lmi = mp;
	}
/*
 * Clean up and return the local mount info table address.
 */
	(void) fclose(ms);
	if (dn)
	    (void) free((FREE_P *)dn);
	Lmist = 1;
	return(Lmi);
}
