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
 *	ns_generic.c
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2011 Apple Inc.
 */

#pragma ident	"@(#)ns_generic.c	1.33	05/06/08 SMI"

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <mntopts.h>

#include <os/activity.h>
#include <os/log.h>

#include "autofs.h"
#include "automount.h"
#include "auto_mntopts.h"

/*
 * Each name service is represented by a ns_info structure.
 */
struct ns_info {
	char    *ns_name;               /* service name */
	void    (*ns_init)(char **, char ***);
	/* initialization routine */
	int     (*ns_getmapent)(const char *, const char *, struct mapline *,
	    char **, char ***, bool_t *, bool_t);
	/* get map entry given key */
	int     (*ns_loadmaster)(char *, char *, char **, char ***);
	/* load master map */
	int     (*ns_loaddirect)(char *, char *, char *,
	    char **, char ***);         /* load direct map */
	int     (*ns_getmapkeys)(char *, struct dir_entry **,
	    int *, int *, char **, char ***);
	/* readdir */
};

static struct ns_info ns_info[] = {
	{ "files", init_files, getmapent_files,
	  loadmaster_files, loaddirect_files,
	  getmapkeys_files },

	{ "od", init_od, getmapent_od,
	  loadmaster_od, loaddirect_od,
	  getmapkeys_od },

	{ NULL, NULL, NULL, NULL, NULL, NULL }
};

void
ns_setup(char **stack, char ***stkptr)
{
	struct ns_info *nsp;

	for (nsp = ns_info; nsp->ns_name; nsp++) {
		nsp->ns_init(stack, stkptr);
	}
}

int
getmapent(const char *key, const char *mapname, struct mapline *ml, char **stack, char ***stkptr, bool_t *iswildcard, bool_t isrestricted)
{
	int ns_err, err;
	struct ns_info *nsp;

	if (*mapname == '/') {          /* must be a file */
		return getmapent_files(key, mapname, ml, stack, stkptr,
		           iswildcard, isrestricted);
	}

	ns_err = __NSW_NOTFOUND;
	for (nsp = ns_info; nsp->ns_name; nsp++) {
		err = nsp->ns_getmapent(key, mapname, ml, stack, stkptr,
		    iswildcard, isrestricted);
		if (err == __NSW_SUCCESS) {
			return __NSW_SUCCESS;
		}
		if (err != __NSW_NOTFOUND) {
			ns_err = err;
		}
	}

	return ns_err;
}

int
loadmaster_map(char *mapname, char *defopts, char **stack, char ***stkptr)
{
	int ns_err;
	struct ns_info *nsp;
	struct os_activity_scope_state_s state;

	os_activity_t activity = os_activity_create("loadmaster_map", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
	os_activity_scope_enter(activity, &state);

	os_log_debug(OS_LOG_DEFAULT, "loadmaster_map:%s", mapname);

	if (*mapname == '/') {
		/* must be a file */
		ns_err = loadmaster_files(mapname, defopts, stack, stkptr);
		os_log_debug(OS_LOG_DEFAULT, "loadmaster_map:%s:finish1:%d", mapname, ns_err);
		os_activity_scope_leave(&state);
		return ns_err;
	}

	for (nsp = ns_info; nsp->ns_name; nsp++) {
		os_log_debug(OS_LOG_DEFAULT, "loadmaster_map:%s:loading:%s", mapname, nsp->ns_name);
		ns_err = nsp->ns_loadmaster(mapname, defopts, stack, stkptr);
		if (ns_err == __NSW_SUCCESS) {
			os_log_debug(OS_LOG_DEFAULT, "loadmaster_map:%s:finish2:%d", mapname, ns_err);
			os_activity_scope_leave(&state);
			return ns_err;
		}
	}

	os_log_debug(OS_LOG_DEFAULT, "loadmaster_map:%s:finish3:%d", mapname, __NSW_UNAVAIL);
	os_activity_scope_leave(&state);
	return __NSW_UNAVAIL;
}

int
loaddirect_map(char *mapname, char *localmap, char *defopts, char **stack, char ***stkptr)
{
	int ns_err = __NSW_SUCCESS;
	struct ns_info *nsp;

	struct os_activity_scope_state_s state;

	os_activity_t activity = os_activity_create("loaddirect_map", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
	os_activity_scope_enter(activity, &state);

	os_log_debug(OS_LOG_DEFAULT, "loaddirect_map:%s", mapname);

	if (*mapname == '/') {
		/* must be a file */
		ns_err = loaddirect_files(mapname, localmap, defopts, stack, stkptr);
		os_log_debug(OS_LOG_DEFAULT, "loaddirect_map:%s:finish1:%d", mapname, ns_err);
		os_activity_scope_leave(&state);
		return ns_err;
	}

	if (strcmp(mapname, "-static") == 0) {
		ns_err = loaddirect_static(localmap, defopts, stack, stkptr);
		os_log_debug(OS_LOG_DEFAULT, "loaddirect_map:%s:finish2:%d", mapname, ns_err);
		os_activity_scope_leave(&state);
		return ns_err;
	}

	for (nsp = ns_info; nsp->ns_name; nsp++) {
		ns_err = nsp->ns_loaddirect(mapname, localmap, defopts, stack, stkptr);
		if (ns_err == __NSW_SUCCESS) {
			os_log_debug(OS_LOG_DEFAULT, "loaddirect_map:%s:finish3:%d", mapname, ns_err);
			os_activity_scope_leave(&state);
			return __NSW_SUCCESS;
		}
	}

	os_log_debug(OS_LOG_DEFAULT, "loaddirect_map:%s:finish4:%d", mapname, __NSW_UNAVAIL);
	os_activity_scope_leave(&state);
	return __NSW_UNAVAIL;
}

/*
 * XXX - this assumes that gethostent() returns a pointer to
 * thread-specific data.  It currently does....
 */
static int
gethostkeys(struct dir_entry **list, int *error, int *cache_time)
{
	char **p;
	struct dir_entry *last = NULL;
	struct hostent *ent;
	int err;

	*cache_time = RDDIR_CACHE_TIME * 2;
	*error = 0;
	if (trace > 1) {
		trace_prt(1, "gethostkeys called\n");
	}

	sethostent(1);

	while ((ent = gethostent()) != NULL) {
		/*
		 * add canonical name
		 *
		 * A return of -1 means the name isn't valid.
		 */
		err = add_dir_entry(ent->h_name, NULL, NULL, list, &last);
		if (err == -1) {
			continue;
		}
		if (err != 0) {
			*error = err;
			goto done;
		}
		if (ent->h_aliases == NULL) {
			goto done;      /* no aliases */
		}
		for (p = ent->h_aliases; *p != 0; p++) {
			if (strcmp(*p, ent->h_name) != 0) {
				/*
				 * add alias only if different
				 * from canonical name
				 */
				err = add_dir_entry(*p, NULL, NULL, list,
				    &last);
				if (err == -1) {
					continue;
				}
				if (err != 0) {
					*error = err;
					goto done;
				}
			}
		}
		assert(last != NULL);
	}
done:   if (*list != NULL) {
		/*
		 * list of entries found
		 */
		*error = 0;
	}
	endhostent();

	return __NSW_SUCCESS;
}

/*
 * enumerate all entries in the map in the various name services.
 */
int
getmapkeys(char *mapname, struct dir_entry **list, int *error, int *cache_time, char **stack, char ***stkptr)
{
	int success = 0;
	struct ns_info *nsp;

	if (*mapname == '/') {          /* must be a file */
		return getmapkeys_files(mapname, list, error, cache_time,
		           stack, stkptr);
	}
	if (strcmp(mapname, "-hosts") == 0) {
		return gethostkeys(list, error, cache_time);
	}
	if (strcmp(mapname, "-static") == 0) {
		pr_msg(LOG_ERR, "-static is a collection of direct maps");
		return __NSW_UNAVAIL;
	}
	if (strcmp(mapname, "-fstab") == 0) {
		return getfstabkeys(list, error, cache_time);
	}

	for (nsp = ns_info; nsp->ns_name; nsp++) {
		/* XXXab: it looks like lower levels will set error based on ns_err they will also
		 *        return, so that's why we are not checking it. For now just cast it void,
		 *        we need to refactor this to be more readable.
		 */
		(void)nsp->ns_getmapkeys(mapname, list, error,
		    cache_time, stack, stkptr);
		if (*error == 0) {
			/*
			 * return success if listing was successful
			 * for at least one name service
			 */
			success++;
		}
	}
	if (success) {
		/*
		 * if succeeded at least once, return error=0
		 */
		*error = 0;
	}
	;

	return success ? __NSW_SUCCESS : __NSW_NOTFOUND;
}
