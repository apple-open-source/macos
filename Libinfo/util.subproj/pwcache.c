/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)pwcache.c	8.1 (Berkeley) 6/4/93
 */


#include <sys/types.h>

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utmp.h>

#define	NCACHE	64			/* power of 2 */
#define	MASK	(NCACHE - 1)		/* bits to store with */

char *
user_from_uid(uid, nouser)
	uid_t uid;
	int nouser;
{
	static struct ncache {
		uid_t	uid;
		char	name[UT_NAMESIZE + 1];
	} *c_uid[NCACHE];
	static int pwopen;
	static char nbuf[15];		/* 32 bits == 10 digits */
	register struct passwd *pw;
	register struct ncache **cp;

	cp = &c_uid[uid & MASK];
	if (*cp == NULL || (*cp)->uid != uid || !*(*cp)->name) {
		if (pwopen == 0) {
			setpassent(1);
			pwopen = 1;
		}
		if ((pw = getpwuid(uid)) == NULL) {
err:
			if (nouser)
				return (NULL);
			(void)snprintf(nbuf, sizeof(nbuf), "%u", uid);
			return (nbuf);
		}
		if (*cp == NULL) {
			*cp = malloc(sizeof(struct ncache));
			if (*cp == NULL)
				goto err;
		}
		(*cp)->uid = uid;
		(void)strncpy((*cp)->name, pw->pw_name, UT_NAMESIZE);
		(*cp)->name[UT_NAMESIZE] = '\0';
	}
	return ((*cp)->name);
}

char *
group_from_gid(gid, nogroup)
	gid_t gid;
	int nogroup;
{
	static struct ncache {
		gid_t	gid;
		char	name[UT_NAMESIZE + 1];
	} *c_gid[NCACHE];
	static int gropen;
	static char nbuf[15];		/* 32 bits == 10 digits */
	struct group *gr;
	struct ncache **cp = NULL;

	cp = &c_gid[gid & MASK];
	if (*cp == NULL || (*cp)->gid != gid || !*(*cp)->name) {
		if (gropen == 0) {
			setgroupent(1);
			gropen = 1;
		}
		if ((gr = getgrgid(gid)) == NULL) {
err:
			if (nogroup)
				return (NULL);
			(void)snprintf(nbuf, sizeof(nbuf), "%u", gid);
			return (nbuf);
		}
		if (*cp == NULL) {
			*cp = malloc(sizeof(struct ncache));
			if (*cp == NULL)
				goto err;
		}
		(*cp)->gid = gid;
		(void)strncpy((*cp)->name, gr->gr_name, UT_NAMESIZE);
		(*cp)->name[UT_NAMESIZE] = '\0';
	}
	return ((*cp)->name);
}
