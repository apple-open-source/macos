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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * April 5, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */

#include <paths.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

Boolean
SCDynamicStoreNotifyFileDescriptor(SCDynamicStoreRef	store,
				   int32_t		identifier,
				   int			*fd)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	int				sc_status;
	struct sockaddr_un		un;
	int				sock;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreNotifyFileDescriptor:"));

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return FALSE;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		/* sorry, you must have an open session to play */
		_SCErrorSet(kSCStatusNoStoreServer);
		return FALSE;
	}

	if (storePrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		_SCErrorSet(kSCStatusNotifierActive);
		return FALSE;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		_SCErrorSet(errno);
		SCLog(_sc_verbose, LOG_NOTICE, CFSTR("socket: %s"), strerror(errno));
		return FALSE;
	}

	/* establish a UNIX domain socket for server->client notification */
	bzero(&un, sizeof(un));
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path,
		 sizeof(un.sun_path)-1,
		 "%s%s-%d",
		 _PATH_VARTMP,
		 "SCDynamicStoreNotifyFileDescriptor",
		 storePrivate->server);

	if (bind(sock, (struct sockaddr *)&un, sizeof(un)) == -1) {
		_SCErrorSet(errno);
		SCLog(_sc_verbose, LOG_NOTICE, CFSTR("bind: %s"), strerror(errno));
		(void) close(sock);
		return FALSE;
	}

	if (listen(sock, 0) == -1) {
		_SCErrorSet(errno);
		SCLog(_sc_verbose, LOG_NOTICE, CFSTR("listen: %s"), strerror(errno));
		(void) close(sock);
		return FALSE;
	}

	status = notifyviafd(storePrivate->server,
			     un.sun_path,
			     strlen(un.sun_path),
			     identifier,
			     (int *)&sc_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("notifyviafd(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return FALSE;
	}

	*fd = accept(sock, 0, 0);
	if (*fd == -1) {
		_SCErrorSet(errno);
		SCLog(_sc_verbose, LOG_NOTICE, CFSTR("accept: %s"), strerror(errno));
		(void) close(sock);
		return FALSE;
	}
	(void) close(sock);

	/* set notifier active */
	storePrivate->notifyStatus = Using_NotifierInformViaFD;

	return TRUE;
}
