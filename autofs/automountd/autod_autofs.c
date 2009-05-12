/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 *	autod_autofs.c
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

#pragma ident	"@(#)autod_autofs.c	1.27	05/06/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/param.h>
#include <mntopts.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "automount.h"

static int process_opts(char *options, uint32_t *directp);

static const struct mntopt mopts_autofs[] = {
	MOPT_STDOPTS,
	{ MNTOPT_RESTRICT,	0, AUTOFS_MNT_RESTRICT, 1 },
	{ MNTOPT_RDDIR,		1, AUTOFS_MNT_NORDDIR, 1 },
	{ NULL,			0, 0, 0 }
};

int
mount_autofs(
	struct mapent *me,
	char *mntpnt,
	action_list *alp,
	char *rootp,
	char *subdir,
	char *key
)
{
	char rel_mntpnt[MAXPATHLEN];
	mntoptparse_t mp;

	if (trace > 1)
		trace_prt(1, "  mount_autofs %s on %s\n",
		me->map_fs->mfs_dir, mntpnt);

	if (strcmp(mntpnt, "/-") == 0) {
		syslog(LOG_ERR, "invalid mountpoint: /-");
		return (ENOENT);
	}

	/*
	 * get relative mountpoint
	 */
	if (snprintf(rel_mntpnt, sizeof (rel_mntpnt), ".%s",
	    mntpnt+strlen(rootp)) >= (int)sizeof (rel_mntpnt)) {
		syslog(LOG_ERR, "mountpoint too long: %s", mntpnt);
		return (ENOENT);
	}

	if (trace > 2)
		trace_prt(1, "rel_mntpnt = %s\n", rel_mntpnt);

	if ((alp->mounta.opts = malloc(AUTOFS_MAXOPTSLEN)) == NULL)
		goto free_mem;
	if (strlcpy(alp->mounta.opts, me->map_mntopts, AUTOFS_MAXOPTSLEN)
	    >= AUTOFS_MAXOPTSLEN) {
		syslog(LOG_ERR, "options \"%s\" for %s are too long",
		    me->map_mntopts, mntpnt);
		free(alp->mounta.opts);
		return (ENOENT);
	}

	if (process_opts(alp->mounta.opts, &alp->mounta.isdirect) != 0)
		goto free_mem;

	/*
	 * get absolute mountpoint
	 */
	if ((alp->mounta.path = strdup(mntpnt)) == NULL)
		goto free_mem;

	if ((alp->mounta.map = strdup(me->map_fs->mfs_dir)) == NULL)
		goto free_mem;
	if ((alp->mounta.subdir = strdup(subdir)) == NULL)
		goto free_mem;

	if (alp->mounta.isdirect) {
		if (me->map_modified == TRUE || me->map_faked == TRUE) {
			if ((alp->mounta.key = strdup(key)) == NULL)
				goto free_mem;
		} else {
			/* wierd case of a direct map pointer in another map */
			if ((alp->mounta.key = strdup(alp->mounta.path)) == NULL)
				goto free_mem;
		}
	} else {
		alp->mounta.key = NULL;
	}

	/*
	 * Fill out action list.
	 */
	if ((alp->mounta.dir = strdup(rel_mntpnt)) == NULL)
		goto free_mem;

	/*
	 * Parse the mount options and fill in "flags" and "mntflags".
	 */
	alp->mounta.flags = alp->mounta.mntflags = 0;
	getmnt_silent = 1;
	mp = getmntopts(alp->mounta.opts, mopts_autofs, &alp->mounta.flags,
	    &alp->mounta.mntflags);
	if (mp == NULL)
		goto free_mem;
	freemntopts(mp);

	return (0);

free_mem:
	/*
	 * We got an error, free the memory we allocated.
	 */
	syslog(LOG_ERR, "mount_autofs: memory allocation failure");
	free_action_list_fields(alp);

	return (ENOMEM);
}

/*
 * Set *directp to 1 if "direct" is found, and 0 otherwise
 * (mounts are indirect by default).  If both "direct" and "indirect" are
 * found, the last one wins.
 *
 * Also, map "browse"/"nobrowse" to "rddir"/"norddir".
 */
static int
process_opts(char *options, uint32_t *directp)
{
	char *opt, *opts, *lasts;
	char buf[AUTOFS_MAXOPTSLEN];

	CHECK_STRCPY(buf, options, sizeof buf);
	opts = buf;
	options[0] = '\0';
	*directp = 0;

	while ((opt = strtok_r(opts, ",", &lasts)) != NULL) {
		opts = NULL;
		while (isspace(*opt)) {
			opt++;
		}
		if (strcmp(opt, "direct") == 0) {
			*directp = 1;
		} else if (strcmp(opt, "indirect") == 0) {
			*directp = 0;
		} else if (strcmp(opt, "browse") == 0) {
			/*
			 * We already have a "browse" option, which
			 * governs whether the file system will show
			 * up on the desktop and in the sidebar; map
			 * Solaris's "browse" option, which governs
			 * whether readdir is supported on an autofs
			 * file system, to our "rddir" option.
			 */
			if (options[0] != '\0') {
				CHECK_STRCAT(options, ",", AUTOFS_MAXOPTSLEN);
			}
			CHECK_STRCAT(options, MNTOPT_RDDIR, AUTOFS_MAXOPTSLEN);
		} else if (strcmp(opt, "nobrowse") == 0) {
			/*
			 * As with "browse" and "rddir", so with
			 * "nobrowse" and "norddir".
			 */
			if (options[0] != '\0') {
				CHECK_STRCAT(options, ",", AUTOFS_MAXOPTSLEN);
			}
			CHECK_STRCAT(options, "no" MNTOPT_RDDIR, AUTOFS_MAXOPTSLEN);
		} else if (strcmp(opt, "ignore") != 0) {
			if (options[0] != '\0') {
				CHECK_STRCAT(options, ",", AUTOFS_MAXOPTSLEN);
			}
			CHECK_STRCAT(options, opt, AUTOFS_MAXOPTSLEN);
		}
	};
	return (0);
}

/*
 * free items pointed to by members of an action list structure
 */
void
free_action_list_fields(action_list *alp)
{
	if (alp == NULL)
		return;
	if (alp->mounta.dir)
		free(alp->mounta.dir);
	if (alp->mounta.opts)
		free(alp->mounta.opts);
	if (alp->mounta.path)
		free(alp->mounta.path);
	if (alp->mounta.map)
		free(alp->mounta.map);
	if (alp->mounta.subdir)
		free(alp->mounta.subdir);
	if (alp->mounta.key)
		free(alp->mounta.key);
}
