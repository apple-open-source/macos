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
 * March 31, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <unistd.h>

#include "configd.h"
#include "session.h"


int
__SCDynamicStoreNotifyCancel(SCDynamicStoreRef store)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreNotifyCancel:"));

	if (!store) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	/*
	 * cleanup any mach port based notifications.
	 */
	if (storePrivate->notifyPort != MACH_PORT_NULL) {
		(void) mach_port_destroy(mach_task_self(), storePrivate->notifyPort);
		storePrivate->notifyPort = MACH_PORT_NULL;
	}

	/*
	 * cleanup any file based notifications.
	 */
	if (storePrivate->notifyFile >= 0) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  closing (notification) fd %d"), storePrivate->notifyFile);
		(void) close(storePrivate->notifyFile);
		storePrivate->notifyFile = -1;
	}

	/*
	 * cleanup any signal notifications.
	 */
	if (storePrivate->notifySignal > 0) {
		(void) mach_port_destroy(mach_task_self(), storePrivate->notifySignalTask);
		storePrivate->notifySignal     = 0;
		storePrivate->notifySignalTask = TASK_NULL;
	}

	/* remove this session from the to-be-notified list */
	if (needsNotification) {
		CFNumberRef	num;

		num = CFNumberCreate(NULL, kCFNumberIntType, &storePrivate->server);
		CFSetRemoveValue(needsNotification, num);
		CFRelease(num);

		if (CFSetGetCount(needsNotification) == 0) {
			CFRelease(needsNotification);
			needsNotification = NULL;
		}
	}

	/* set notifier inactive */
	storePrivate->notifyStatus = NotifierNotRegistered;

	return kSCStatusOK;
}


kern_return_t
_notifycancel(mach_port_t	server,
	      int		*sc_status)
{
	serverSessionRef	mySession = getSession(server);

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Cancel requested notifications."));
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  server = %d"), server);

	*sc_status = __SCDynamicStoreNotifyCancel(mySession->store);
	if (*sc_status != kSCStatusOK) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  __SCDynamicStoreNotifyCancel(): %s"), SCErrorString(*sc_status));
	}

	return KERN_SUCCESS;
}
