/*
 * pdvn.c -- print device name functions for lsof library
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


#include "../machine.h"

#if	defined(HASBLKDEV) || defined(USE_LIB_PRINTCHDEVNAME)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: pdvn.c,v 1.6 2001/08/14 12:25:05 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"

#else	/* !defined(HASBLKDEV) && !defined(USE_LIB_PRINTCHDEVNAME) */
static char d1[] = "d"; static char *d2 = d1;
#endif	/* defined(HASBLKDEV) || defined(USE_LIB_PRINTCHDEVNAME) */


/*
 * To use this source file:
 *
 * 1.  Define HASBLKDEV, or USE_LIB_PRINCHDEVNAME, or both.
 *
 * 2.  Define HAS_STD_CLONE to enable standard clone searches in
 *     printchdevname().
 */


/*
 * Local definitions
 */

#define	LIKE_NODE_TTL	"like device special "


#if	defined(HASBLKDEV)
/*
 * printbdevname() - print block device name
 */

int
printbdevname(dev, rdev, f)
	dev_t *dev;			/* device */
	dev_t *rdev;			/* raw device */
	int f;				/* 1 = follow with '\n' */
{
	struct l_dev *dp;

	if ((dp = lkupbdev(dev, rdev, 1, 1))) {
	    safestrprt(dp->name, stdout, f);
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASBLKDEV) */


#if	defined(USE_LIB_PRINTCHDEVNAME)
/*
 * printchdevname() - print character device name
 */

int
printchdevname(dev, rdev, f)
	dev_t *dev;			/* device */
	dev_t *rdev;			/* raw device */
	int f;				/* 1 = print trailing '\n' */
{

# if	defined(HAS_STD_CLONE)
	struct clone *c;
# endif	/* defined(HAS_STD_CLONE) */

	struct l_dev *dp;
	int r = 1;

# if	defined(HASDCACHE)

printchdevname_again:

# endif	/* defined(HASDCACHE) */

#if	defined(HAS_STD_CLONE)
/*
 * Search for clone.
 */
	if (Lf->is_stream && Clone && (*dev == DevDev)) {
	    r = 0;	/* Don't let lkupdev() rebuild the device cache,
			 * because when it has been rebuilt we want to
			 * search again for clones. */
	    readdev(0);
	    for (c = Clone; c; c = c->next) {
		if (GET_MAJ_DEV(*rdev) == GET_MIN_DEV(Devtp[c->dx].rdev)) {

# if	defined(HASDCACHE)
		    if (DCunsafe && !Devtp[c->dx].v && !vfy_dev(&Devtp[c->dx]))
			goto printchdevname_again;
# endif	/* defined(HASDCACHE) */

		    safestrprt(Devtp[c->dx].name, stdout, f);
		    return(1);
		}
	    }
	}
#endif	/* defined(HAS_STD_CLONE) */

/*
 * Search device table for a full match.
 */
	if ((dp = lkupdev(dev, rdev, 1, r))) {
	    safestrprt(dp->name, stdout, f);
	    return(1);
	}
/*
 * Search device table for a match without inode number and dev.
 */
	if ((dp = lkupdev(&DevDev, rdev, 0, r))) {

	/*
	 * A raw device match was found.  Record it as a name column addition.
	 */
	    char *cp;
	    int len;

	    len = (int)(1 + strlen(LIKE_NODE_TTL) + strlen(dp->name) + 1);
	    if (!(cp = (char *)malloc((MALLOC_S)(len + 1)))) {
		(void) fprintf(stderr, "%s: no nma space for: (%s%s)\n",
		    Pn, LIKE_NODE_TTL, dp->name);
		Exit(1);
	    }
	    (void) snpf(cp, len + 1, "(%s%s)", LIKE_NODE_TTL, dp->name);
	    (void) add_nma(cp, len);
	    (void) free((MALLOC_P *)cp);
	    return(0);
	}

#if	defined(HASDCACHE)
/*
 * We haven't found a match.
 *
 * If lkupdev()'s rebuilding the device cache was suppressed
 * and the device cache is "unsafe," rebuild it.
 */
	if (!r && DCunsafe) {
	    (void) rereaddev();
	    goto printchdevname_again;
	}
#endif	/* defined(HASDCACHE) */

	return(0);
}
#endif	/* defined(USE_LIB_PRINTCHDEVNAME) */
