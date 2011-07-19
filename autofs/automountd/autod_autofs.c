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
 * Portions Copyright 2007-2011 Apple Inc.
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
	{ MNTOPT_RESTRICT,		0, AUTOFS_MNT_RESTRICT, 1 },
	{ "browse",			1, AUTOFS_MNT_NOBROWSE, 1 },
	{ MNTOPT_HIDEFROMFINDER,	0, AUTOFS_MNT_HIDEFROMFINDER, 1 },
	{ NULL,				0, 0, 0 }
};

int
mount_autofs(
	const char *mapname,
	struct mapent *me,
	const char *mntpnt,
	fsid_t mntpnt_fsid,
	action_list **alpp,
	const char *rootp,
	const char *subdir,
	const char *key,
	fsid_t *fsidp,
	uint32_t *retflags
)
{
	action_list *alp;
	char rel_mntpnt[MAXPATHLEN];
	const char *trig_mntpnt;
	mntoptparse_t mp;
	int error;

	if (trace > 1)
		trace_prt(1, "  mount_autofs %s on %s\n",
		me->map_fs->mfs_dir, mntpnt);

	if (strcmp(mntpnt, "/-") == 0) {
		syslog(LOG_ERR, "invalid mountpoint: /-");
		*alpp = NULL;
		return (ENOENT);
	}

	/*
	 * get relative mountpoint
	 */
	if (snprintf(rel_mntpnt, sizeof (rel_mntpnt), ".%s",
	    mntpnt+strlen(rootp)) >= (int)sizeof (rel_mntpnt)) {
		syslog(LOG_ERR, "mountpoint too long: %s", mntpnt);
		*alpp = NULL;
		return (ENOENT);
	}

	if (trace > 2)
		trace_prt(1, "rel_mntpnt = %s\n", rel_mntpnt);

	/*
	 * Get the mount point for this autofs mount relative to the
	 * mount point for the file system atop which we're mounting it.
	 */
	trig_mntpnt = me->map_mntpnt;
	while (*trig_mntpnt == '/')
		trig_mntpnt++;

	alp = (action_list *)malloc(sizeof (action_list));
	if (alp == NULL) {
		syslog(LOG_ERR, "malloc of alp failed");
		*alpp = NULL;
		return (ENOMEM);
	}
	memset(alp, 0, sizeof (action_list));

	if ((alp->mounta.opts = malloc(AUTOFS_MAXOPTSLEN)) == NULL)
		goto free_mem;
	if (strlcpy(alp->mounta.opts, me->map_mntopts, AUTOFS_MAXOPTSLEN)
	    >= AUTOFS_MAXOPTSLEN) {
		syslog(LOG_ERR, "options \"%s\" for %s are too long",
		    me->map_mntopts, mntpnt);
		free(alp->mounta.opts);
		free(alp);
		*alpp = NULL;
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
	if ((alp->mounta.trig_mntpnt = strdup(trig_mntpnt)) == NULL)
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

	/*
	 * Is this a real autofs mount to be done now, rather than a
	 * trigger for a submount (me->map_modified) or a placeholder
	 * for subdirectories (me->map_faked)?
	 */
	if (!me->map_modified && !me->map_faked) {
		/*
		 * Yes.  Is it at level 0, i.e. is it supposed to
		 * be mounted atop the trigger vnode we're resolving?
		 */
		if (me->map_mntlevel == 0) {
			/*
			 * Yes.  Actually mount the map.
			 */
			struct autofs_args mnt_args;

			mnt_args.version = AUTOFS_ARGSVERSION;
			mnt_args.path = alp->mounta.path;
			mnt_args.opts = alp->mounta.opts;
			mnt_args.map = alp->mounta.map;
			mnt_args.subdir = "";	/* this is a top-level (auto)mount of a map - no subdir */
			mnt_args.key = alp->mounta.key == NULL ? "" : alp->mounta.key;
			mnt_args.mntflags = alp->mounta.mntflags;
			mnt_args.direct = alp->mounta.isdirect;
			/*
			 * This is a map that's being automounted on a trigger,
			 * rather than being mounted by automount.
			 */
			mnt_args.mount_type = MOUNT_TYPE_TRIGGERED_MAP;
			if (alp->mounta.isdirect)
				mnt_args.node_type = NT_TRIGGER;
			else
				mnt_args.node_type = 0;	/* not a trigger */

			if (mount(MNTTYPE_AUTOFS, mntpnt,
			    alp->mounta.flags|MNT_AUTOMOUNTED|MNT_DONTBROWSE,
			    &mnt_args) == -1)
				error = errno;
			else {
				error = get_triggered_mount_info(mntpnt,
				    mntpnt_fsid, fsidp, retflags);
			}

			/*
			 * This has already been processed; it doesn't belong
			 * on the list of future triggered mounts to be set up.
			 */
		    	free_action_list_fields(alp);
		    	free(alp);
		    	*alpp = NULL;
			return (error);
		} else {
			/*
			 * No.  We need to request a subtrigger to
			 * be planted to mount the map.
			 */
			if (!alp->mounta.isdirect) {
				/*
				 * When we resolve the subtrigger, we
				 * should look in the map in which
				 * we found this entry, *not* in
				 * the map that's going to be mounted.
				 */
				free(alp->mounta.map);
				if ((alp->mounta.map = strdup(mapname)) == NULL)
					goto free_mem;

				/*
				 * And the key should be the key used to
				 * look up this entry.
				 */
				if ((alp->mounta.key = strdup(key)) == NULL)
					goto free_mem;
			}
		}
	}

	/*
	 * This is a subtrigger to be planted.  For autofs and NFS
	 * mounts, we want the mount to be done directly atop the
	 * subtrigger, with no autofs mount involved; for other
	 * file systems, we want an autofs mount stuck in between
	 * them.
	 */
	if (strcmp(me->map_fstype, MNTTYPE_NFS) == 0 ||
	    strcmp(me->map_fstype, MNTTYPE_AUTOFS) == 0)
		alp->mounta.needs_subtrigger = 0;
	else
		alp->mounta.needs_subtrigger = 1;
	*alpp = alp;
	return (0);

free_mem:
	/*
	 * We got an error, free the memory we allocated.
	 */
	syslog(LOG_ERR, "mount_autofs: memory allocation failure");
	free_action_list_fields(alp);
	free(alp);

	/*
	 * Return no action list entry.
	 */
	*alpp = NULL;

	return (ENOMEM);
}

/*
 * Set *directp to 1 if "direct" is found, and 0 otherwise
 * (mounts are indirect by default).  If both "direct" and "indirect" are
 * found, the last one wins.
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
	if (alp->mounta.trig_mntpnt)
		free(alp->mounta.trig_mntpnt);
	if (alp->mounta.key)
		free(alp->mounta.key);
}
