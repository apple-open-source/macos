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

#include <paths.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"


SCDStatus
SCDNotifierInformViaFD(SCDSessionRef	session,
		       int32_t		identifier,
		       int		*fd)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	SCDStatus		scd_status;
	struct sockaddr_un	un;
	int			sock;

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierInformViaFD:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;	/* you must have an open session to play */
	}

	if (sessionPrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return SCD_NOTIFIERACTIVE;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		SCDLog(LOG_NOTICE, CFSTR("socket: %s"), strerror(errno));
		return SCD_FAILED;
	}

	/* establish a UNIX domain socket for server->client notification */
	bzero(&un, sizeof(un));
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path,
		 sizeof(un.sun_path)-1,
		 "%s%s-%d",
		 _PATH_VARTMP,
		 "SCDNotifierInformViaFD",
		 sessionPrivate->server);

	if (bind(sock, (struct sockaddr *)&un, sizeof(un)) == -1) {
		SCDLog(LOG_NOTICE, CFSTR("bind: %s"), strerror(errno));
		(void) close(sock);
		return SCD_FAILED;
	}

	if (listen(sock, 0) == -1) {
		SCDLog(LOG_NOTICE, CFSTR("listen: %s"), strerror(errno));
		(void) close(sock);
		return SCD_FAILED;
	}

	status = notifyviafd(sessionPrivate->server,
			     un.sun_path,
			     strlen(un.sun_path),
			     identifier,
			     (int *)&scd_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifyviafd(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	*fd = accept(sock, 0, 0);
	if (*fd == -1) {
		SCDLog(LOG_NOTICE, CFSTR("accept: %s"), strerror(errno));
		(void) close(sock);
		return SCD_FAILED;
	}
	(void) close(sock);

	/* set notifier active */
	sessionPrivate->notifyStatus = Using_NotifierInformViaFD;

	return scd_status;
}
