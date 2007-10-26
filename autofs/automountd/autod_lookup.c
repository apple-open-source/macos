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
 *	autod_lookup.c
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007 Apple Inc.
 *
 * $Id$
 */

#pragma ident	"@(#)autod_lookup.c	1.13	05/06/08 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "automount.h"

int
do_lookup1(autofs_pathname mapname, char *key, autofs_pathname subdir,
    autofs_opts mapopts, boolean_t isdirect, uid_t sendereuid, int *action)
{
	struct mapent *mapents = NULL;
	int err;
	struct rddir_cache *rdcp;
	int found = 0;
	bool_t iswildcard = FALSE;
	bool_t isrestricted = hasrestrictopt(mapopts);

	/*
	 * Default action is for no work to be done by kernel AUTOFS.
	 */
	*action = AUTOFS_NONE;

	/*
	 * Is there a cache for this map?
	 */
	pthread_rwlock_rdlock(&rddir_cache_lock);
	err = rddir_cache_lookup(mapname, &rdcp);
	if (!err && rdcp->full) {
		pthread_rwlock_unlock(&rddir_cache_lock);
		/*
		 * Try to lock readdir cache entry for reading, if
		 * the entry can not be locked, then avoid blocking
		 * and go to the name service. I'm assuming it is
		 * faster to go to the name service than to wait for
		 * the cache to be populated.
		 */
		if (pthread_rwlock_tryrdlock(&rdcp->rwlock) == 0) {
			found = (rddir_entry_lookup(key, rdcp->entp) != NULL);
			pthread_rwlock_unlock(&rdcp->rwlock);
		}
	} else
		pthread_rwlock_unlock(&rddir_cache_lock);

	if (!err) {
		/*
		 * release reference on cache entry
		 */
		pthread_mutex_lock(&rdcp->lock);
		rdcp->in_use--;
		assert(rdcp->in_use >= 0);
		pthread_mutex_unlock(&rdcp->lock);
	}

	if (found)
		return (0);

	/*
	 * entry not found in cache, try the name service now
	 * call parser w default mount_access = TRUE
	 */
	mapents = parse_entry(key, mapname, mapopts, subdir, isdirect,
		&iswildcard, isrestricted, TRUE, &err);
	if (err) {
		/*
		 * The entry wasn't found in the map; err was set to
		 * the appropriate value by parse_entry().
		 *
		 * Now we indulge in a bit of hanky-panky.  If the
		 * name begins with an "=" then we assume that
		 * the name is an undocumented control message
		 * for the daemon.  This is accessible only
		 * to superusers.
		 */
		if (*key == '=' && sendereuid == 0) {
			if (isdigit(*(key+1))) {
				/*
				 * If next character is a digit
				 * then set the trace level.
				 */
				trace = atoi(key+1);
				trace_prt(1, "Automountd: trace level = %d\n",
					trace);
			} else if (*(key+1) == 'v') {
				/*
				 * If it's a "v" then
				 * toggle verbose mode.
				 */
				verbose = !verbose;
				trace_prt(1, "Automountd: verbose %s\n",
						verbose ? "on" : "off");
			}
		}
		return (err);
	}

	/*
	 * The entry was found in the map; err was set to 0
	 * by parse_entry().  
	 *
	 * Each mapent in the list describes a mount to be done.
	 * Since I'm only doing a lookup, I only care whether a
	 * mapentry was found or not; I don't need any of the
	 * mapentries. The mount will be done on a later upcall
	 * to do_mount1.
	 */
	if (mapents)
		free_mapent(mapents);

	if (iswildcard) {
		*action = AUTOFS_MOUNT_RQ;
	}
	if (trace > 1) {
		trace_prt(1, "  do_lookup1: action=%d wildcard=%s error=%d\n",
			*action, iswildcard ? "TRUE" : "FALSE", err);
	}
	return (err);
}
