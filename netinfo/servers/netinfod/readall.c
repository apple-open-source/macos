/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Read an entire NetInfo database.
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * This code is executing when a slave server needs to resynchronize
 * with the master server and issues a READALL call. The master reads
 * the entire database with the call readall().
 *
 * XXX: This hangs up master service while the database is read. This
 * should be rewritten to be multi-threaded with the appropriate locking
 * so that writes can still occur while readall() executes (a tough
 * problem, otherwise we would have done it already ;-)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ni_server.h"
#include "ni_globals.h"
#include <NetInfo/system_log.h>
#include <NetInfo/socket_lock.h>
#include <errno.h>

/*
 * Recursively reads node and its children, writing it into xdr. 
 * The guts of readall().
 */
static bool_t
doit(
     XDR *xdr,
     void *ni,
     ni_index object_id
     )
{
	ni_index i;
	bool_t true = TRUE;
	ni_object object;

	socket_unlock();
	object.nio_id.nii_object = object_id;
	if (ni_parent(ni, &object.nio_id, &object.nio_parent) != NI_OK) {
		socket_lock();
		system_log(LOG_DEBUG, "couldn't get parent of %d\n",
			object.nio_id.nii_object);
		return (FALSE);
	}
	NI_INIT(&object.nio_props);
	if (ni_read(ni, &object.nio_id, &object.nio_props) != NI_OK) {
		socket_lock();
		system_log(LOG_DEBUG, "couldn't read %d\n",
			object.nio_id.nii_object);
		return (FALSE);
	}
	NI_INIT(&object.nio_children);
	if (ni_children(ni, &object.nio_id, &object.nio_children) != NI_OK) {
		socket_lock();
		ni_proplist_free(&object.nio_props);
		system_log(LOG_DEBUG, "couldn't get children of %d\n",
			object.nio_id.nii_object);
		return (FALSE);
	}
	socket_lock();

	errno = 0;	/* Start with a known clean errno: no cruft here! */
	if (!xdr_bool(xdr, &true) ||
	    !xdr_ni_object(xdr, &object)) {
		system_log(LOG_ERR, "couldn't xdr %d: %m",
		       object.nio_id.nii_object);
		ni_proplist_free(&object.nio_props);
		ni_idlist_free(&object.nio_children);
		return (FALSE);
	}
	ni_proplist_free(&object.nio_props);

	for (i = 0; i < object.nio_children.niil_len; i++) {
		if (!doit(xdr, ni, object.nio_children.niil_val[i])) {
			ni_idlist_free(&object.nio_children);
			return (FALSE);
		}
	}
	ni_idlist_free(&object.nio_children);
	return (TRUE);
}

/*
 * The readall() function. Sends out the netinfo status code, and if
 * NI_OK, than sends the checksum, the highest id in the list and calls
 * doit() to recursively descend the database.
 *
 */
bool_t
readall(
	XDR *xdr,
	void *ni
	)
{
	ni_id root;
	bool_t false = FALSE;
	ni_status status;
	unsigned checksum;
	unsigned highestid;

	socket_unlock();
	status = ni_root(ni, &root);
	socket_lock();

	if (!xdr_ni_status(xdr, &status)) {
		return (FALSE);
	}
	if (status == NI_OK) {
		checksum = ni_getchecksum(db_ni);
		
		/*
		 * Send out the checksum...
		 */
		if (!xdr_u_int(xdr, &checksum)) {
			return (FALSE);
		}

		socket_unlock();
		highestid = ni_highestid(ni);
		socket_lock();

		/*
		 * The highest ID...
		 */
		if (!xdr_u_int(xdr, &highestid)) {
			return (FALSE);
		}

		/*
		 * All the data...
		 */
		if (!doit(xdr, ni, root.nii_object)) {
			return (FALSE);
		}
		
		/*
		 * And then terminate the list (more entries == false).
		 */
		if (!xdr_bool(xdr, &false)) {
			return (FALSE);
		}
	}
	return (TRUE);
}

/*
 * Ensure readall proxy terminates in a timely fashion upon
 * receiving a SIGUSR1.
 */
void
proxy_term(void)
{
    system_log(LOG_WARNING, "readall proxy terminating due to SIGUSR1");
    /*
     * It'd be nice just to call ni_shutdown.  But, that writes a 
     * checksum file into the database, and that's inappropriate here.
     * During normal shutdown, the database is flushed and synced.  We're
     * not writing here, so we only need to close the file.
     *
     * XXX There's no clean way to get at the contents of these opaque
     * handles (void * things) floating around: the structure declarations
     * are private.  So, we'll just punt, "knowing" that the kernel will
     * clean up any open file descriptors for us.
     */
    
    exit(NI_SYSTEMERR);
}
