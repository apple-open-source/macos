/*
 * dnode1.c - FreeBSD node functions for lsof
 *
 * This module must be separate to keep separate the multiple kernel inode
 * structure definitions.
 */


/*
 * Copyright 1995 Purdue Research Foundation, West Lafayette, Indiana
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
static char *rcsid = "$Id: dnode1.c,v 1.4 1999/11/28 06:40:27 abe Exp $";
#endif


#include "lsof.h"

#if	defined(HAS9660FS)

/*
 * Do a little preparation for #include'ing cd9660_node.h, then #include it.
 */

#undef	i_size;
#undef	doff_t
#undef	IN_ACCESS

#  if	FREEBSDV>=400 && defined(__alpha__)
#define	dev_t	void *
#  endif	/* FREEBSDV>=400 && defined(__alpha__) */

#include "cd9660_node.h"

#  if	FREEBSDV>=400 && defined(__alpha__)
#undef	dev_t
#  endif	/* FREEBSDV>=400 && defined(__alpha__) */


/*
 * read_iso_node() -- read CD 9660 iso_node
 */

int
read_iso_node(v, d, dd, ino, nl, sz)
	struct vnode *v;		/* containing vnode */
	dev_t *d;			/* returned device number */
	int *dd;			/* returned device-defined flag */
	unsigned long *ino;		/* returned inode number */
	long *nl;			/* returned number of links */
	unsigned long *sz;		/* returned size */
{

# if	FREEBSDV200
	struct iso_node *ip;
# else	/* FREEBSD>=200 */
	struct iso_node i;
# endif	/* FREEBSDV<200 */

# if	FREEBSDV>=400
	struct specinfo udev;
# endif	/* FREEBSDV>=400 */

# if	FREEBSDV<200
	ip = (struct iso_node *)v->v_data;
	*d = ip->i_dev;
	*dd = 1;
	*ino = ip->i_number;
	*nl = (long)ip->inode.iso_links;
	*sz = ip->i_size;
# else	/* FREEBSDV>=200 */
	if (!v->v_data
	||  kread((KA_T)v->v_data, (char *)&i, sizeof(i)))
	    return(1);

# if	FREEBSDV>=400
	if (i.i_dev && !kread((KA_T)i.i_dev, (char *)&udev, sizeof(udev))) {
	    *d = udev.si_udev;
	    *dd = 1;
	}
# else	/* FREEBSDV<400 */
	*d = i.i_dev;
	*dd = 1;
# endif	/* FREEBSDV>=400 */

	*ino = i.i_number;
	*nl = (long)i.inode.iso_links;
	*sz = i.i_size;
# endif	/* FREEBSDV<200 */

	return(0);
}
#endif	/* defined(HAS9660FS) */
