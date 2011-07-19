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
 * Portions Copyright 2007-2011 Apple Inc.
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
do_lookup1(const autofs_pathname mapname, const char *key,
    const autofs_pathname subdir, const autofs_opts mapopts,
    boolean_t isdirect, uid_t sendereuid, int *node_type)
{
	struct mapent *mapents = NULL;
	int err;
	bool_t isrestricted = hasrestrictopt(mapopts);

	/*
	 * call parser w default mount_access = TRUE
	 */
	mapents = parse_entry(key, mapname, mapopts, subdir, isdirect,
		node_type, isrestricted, TRUE, &err);
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
	 */
	if (mapents)
		free_mapent(mapents);

	if (trace > 1) {
		trace_prt(1, "  do_lookup1: node_type=0x%08x error=%d\n",
			*node_type, err);
	}
	return (err);
}
