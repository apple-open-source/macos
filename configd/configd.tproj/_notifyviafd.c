/*
 * Copyright (c) 2000, 2001, 2003-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * April 5, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "configd.h"
#include "session.h"


__private_extern__
int
__SCDynamicStoreNotifyFileDescriptor(SCDynamicStoreRef	store,
				     int32_t		identifier,
				     int		*fd)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	int				sock;
	CFStringRef			sessionKey;
	CFDictionaryRef			info;

	if ((store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (storePrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		return kSCStatusNotifierActive;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("__SCDynamicStoreNotifyFileDescriptor socket() failed: %s"), strerror(errno));
		return kSCStatusFailed;
	}

	*fd = sock;

	/* push out a notification if any changes are pending */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), storePrivate->server);
	info = CFDictionaryGetValue(sessionData, sessionKey);
	CFRelease(sessionKey);
	if (info && CFDictionaryContainsKey(info, kSCDChangedKeys)) {
		CFNumberRef	sessionNum;

		if (needsNotification == NULL)
			needsNotification = CFSetCreateMutable(NULL,
							       0,
							       &kCFTypeSetCallBacks);

		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &storePrivate->server);
		CFSetAddValue(needsNotification, sessionNum);
		CFRelease(sessionNum);
	}

	return kSCStatusOK;
}


__private_extern__
kern_return_t
_notifyviafd(mach_port_t		server,
	     xmlData_t			pathRef,
	     mach_msg_type_number_t	pathLen,
	     int			identifier,
	     int			*sc_status
)
{
	int				bufSiz;
	serverSessionRef		mySession       = getSession(server);
	int				nbioYes;
	int				sock;
	kern_return_t			status;
	SCDynamicStorePrivateRef	storePrivate    = (SCDynamicStorePrivateRef)mySession->store;
	struct sockaddr_un		un;

	/*
	 * if socket currently open, close it!
	 */
	/* validate the UNIX domain socket path */
	if (pathLen > (sizeof(un.sun_path) - 1)) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("_notifyviafd(): domain socket path length too long!"));
		status = vm_deallocate(mach_task_self(), (vm_address_t)pathRef, pathLen);
#ifdef	DEBUG
		if (status != KERN_SUCCESS) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("_notifyviafd vm_deallocate() failed: %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
#endif	/* DEBUG */
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	/* un-serialize the UNIX domain socket path */
	un.sun_family = AF_UNIX;
	bcopy(pathRef, un.sun_path, pathLen);
	un.sun_path[pathLen] = '\0';
	status = vm_deallocate(mach_task_self(), (vm_address_t)pathRef, pathLen);
#ifdef	DEBUG
	if (status != KERN_SUCCESS) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("_notifyviafd vm_deallocate() failed: %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
#endif	/* DEBUG */

	if (mySession == NULL) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		return KERN_SUCCESS;
	}

	/* do common sanity checks, get socket */
	*sc_status = __SCDynamicStoreNotifyFileDescriptor(mySession->store, identifier, &sock);

	/* check status of __SCDynamicStoreNotifyFileDescriptor() */
	if (*sc_status != kSCStatusOK) {
		return KERN_SUCCESS;
	}

	/* establish the connection, get ready for a read() */
	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) == -1) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("_notifyviafd connect() failed: %s"), strerror(errno));
		(void) close(sock);
		storePrivate->notifyStatus = NotifierNotRegistered;
		storePrivate->notifyFile   = -1;
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	(void) unlink(un.sun_path);

	bufSiz = sizeof(storePrivate->notifyFileIdentifier);
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSiz, sizeof(bufSiz)) == -1) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("_notifyviafd setsockopt() failed: %s"), strerror(errno));
		(void) close(sock);
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	nbioYes = 1;
	if (ioctl(sock, FIONBIO, &nbioYes) == -1) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("_notifyviafd ioctl(,FIONBIO,) failed: %s"), strerror(errno));
		(void) close(sock);
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	/* set notifier active */
	storePrivate->notifyStatus         = Using_NotifierInformViaFD;
	storePrivate->notifyFile           = sock;
	storePrivate->notifyFileIdentifier = identifier;

	return KERN_SUCCESS;
}
