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

#include <sys/types.h>
#include <regex.h>

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"


SCDStatus
SCDNotifierRemove(SCDSessionRef session, CFStringRef key, int regexOptions)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	CFDataRef		xmlKey;		/* serialized key */
	xmlData_t		myKeyRef;
	CFIndex			myKeyLen;
	SCDStatus		scd_status;

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierRemove:"));
	SCDLog(LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCDLog(LOG_DEBUG, CFSTR("  regexOptions = %0o"), regexOptions);

	if (key == NULL) {
		return SCD_INVALIDARGUMENT;	/* no key specified */
	}

	if (session == NULL) {
		return SCD_NOSESSION;		/* you can't do anything without a session */
	}

	/*
	 * remove key from this sessions notifier list after checking that
	 * it was previously defined.
	 */
	if (regexOptions & kSCDRegexKey) {
		if (!CFSetContainsValue(sessionPrivate->reKeys, key))
			return SCD_NOKEY;		/* sorry, key does not exist in notifier list */
		CFSetRemoveValue(sessionPrivate->reKeys, key);	/* remove key from this sessions notifier list */
	} else {
		if (!CFSetContainsValue(sessionPrivate->keys, key))
			return SCD_NOKEY;		/* sorry, key does not exist in notifier list */
		CFSetRemoveValue(sessionPrivate->keys, key);	/* remove key from this sessions notifier list */
	}

	if (sessionPrivate->server == MACH_PORT_NULL) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}

	/* serialize the key */
	xmlKey = CFPropertyListCreateXMLData(NULL, key);
	myKeyRef = (xmlData_t)CFDataGetBytePtr(xmlKey);
	myKeyLen = CFDataGetLength(xmlKey);

	/* send the key to the server */
	status = notifyremove(sessionPrivate->server,
			      myKeyRef,
			      myKeyLen,
			      regexOptions,
			      (int *)&scd_status);

	/* clean up */
	CFRelease(xmlKey);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifyremove(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	return scd_status;
}
