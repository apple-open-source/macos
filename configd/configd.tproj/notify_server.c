/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"
#include "notify.h"

boolean_t
notify_server(mach_msg_header_t *request, mach_msg_header_t *reply)
{
	mach_no_senders_notification_t	*notify = (mach_no_senders_notification_t *)request;

	if ((notify->not_header.msgh_id > MACH_NOTIFY_LAST) ||
	    (notify->not_header.msgh_id < MACH_NOTIFY_FIRST)) {
		return FALSE;	/* if this is not a notification message */
	}

	switch (notify->not_header.msgh_id) {
		case MACH_NOTIFY_NO_SENDERS :
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("No more senders for port %d, closing."),
				 notify->not_header.msgh_local_port);
			cleanupSession(notify->not_header.msgh_local_port);

			(void) mach_port_destroy(mach_task_self(), notify->not_header.msgh_local_port);

			notify->not_header.msgh_remote_port = MACH_PORT_NULL;
			return TRUE;
		case MACH_NOTIFY_DEAD_NAME :
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Dead name for port %d, closing."),
				 notify->not_header.msgh_local_port);
			cleanupSession(notify->not_header.msgh_local_port);

			(void) mach_port_destroy(mach_task_self(), notify->not_header.msgh_local_port);

			notify->not_header.msgh_remote_port = MACH_PORT_NULL;
			return TRUE;
		default :
			break;
	}

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("HELP!, Received notification: port=%d, msgh_id=%d"),
	       notify->not_header.msgh_local_port,
	       notify->not_header.msgh_id);

	return FALSE;
}
