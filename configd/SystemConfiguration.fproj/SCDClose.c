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

#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"


SCDStatus
SCDClose(SCDSessionRef *session)
{
	SCDSessionPrivateRef	sessionPrivate;
	int			oldThreadState;
	kern_return_t		status;
	SCDStatus		scd_status;
	CFIndex			keyCnt;

	SCDLog(LOG_DEBUG, CFSTR("SCDClose:"));

	if ((session == NULL) || (*session == NULL)) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}
	sessionPrivate = (SCDSessionPrivateRef)*session;

	(void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldThreadState);

	scd_status = (sessionPrivate->server == MACH_PORT_NULL) ? SCD_NOSESSION : SCD_OK;

	/* Remove notification keys */
	if ((keyCnt = CFSetGetCount(sessionPrivate->keys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex	i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(sessionPrivate->keys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
			if (scd_status == SCD_OK) {
				scd_status = SCDNotifierRemove(*session,
							       CFArrayGetValueAtIndex(keysToRemove, i),
							       0);
			}
		}
		CFRelease(keysToRemove);
	}

	/* Remove regex notification keys */
	if ((keyCnt = CFSetGetCount(sessionPrivate->reKeys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex	i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(sessionPrivate->reKeys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
			if (scd_status == SCD_OK) {
				scd_status = SCDNotifierRemove(*session,
							       CFArrayGetValueAtIndex(keysToRemove, i),
							       kSCDRegexKey);
			}
		}
		CFRelease(keysToRemove);
	}

	/* Remove/cancel any outstanding notification requests. */
	(void) SCDNotifierCancel(*session);

	if (SCDOptionGet(*session, kSCDOptionIsLocked) && (scd_status == SCD_OK)) {
		scd_status = SCDUnlock(*session);	/* release the lock */
	}

	if (scd_status == SCD_OK) {
		status = configclose(sessionPrivate->server, (int *)&scd_status);
		if (status != KERN_SUCCESS) {
			if (status != MACH_SEND_INVALID_DEST)
				SCDLog(LOG_DEBUG, CFSTR("configclose(): %s"), mach_error_string(status));
			scd_status = SCD_NOSERVER;
		}

		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
	}

	CFAllocatorDeallocate(NULL, sessionPrivate);
	*session = NULL;

	(void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldThreadState);
	pthread_testcancel();

	return scd_status;
}
