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

#include "configd.h"
#include "session.h"

SCDStatus
_SCDNotifierGetChanges(SCDSessionRef session, CFArrayRef *notifierKeys)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	CFStringRef		sessionKey;
	CFDictionaryRef		info;
	CFMutableDictionaryRef	newInfo;

	SCDLog(LOG_DEBUG, CFSTR("_SCDNotifierGetChanges:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), sessionPrivate->server);
	info = CFDictionaryGetValue(sessionData, sessionKey);
	if ((info == NULL) ||
	    (CFDictionaryContainsKey(info, kSCDChangedKeys) == FALSE)) {
		CFRelease(sessionKey);
		*notifierKeys = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);;
		return SCD_OK;
	}
	newInfo = CFDictionaryCreateMutableCopy(NULL, 0, info);

	*notifierKeys = CFDictionaryGetValue(newInfo, kSCDChangedKeys);
	CFRetain(*notifierKeys);

	CFDictionaryRemoveValue(newInfo, kSCDChangedKeys);
	if (CFDictionaryGetCount(newInfo) > 0) {
		CFDictionarySetValue(sessionData, sessionKey, newInfo);
	} else {
		CFDictionaryRemoveValue(sessionData, sessionKey);
	}
	CFRelease(newInfo);
	CFRelease(sessionKey);

	return SCD_OK;
}


kern_return_t
_notifychanges(mach_port_t			server,
	       xmlDataOut_t			*listRef,	/* raw XML bytes */
	       mach_msg_type_number_t		*listLen,
	       int				*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFArrayRef		notifierKeys;	/* array of CFStringRef's */
	CFDataRef		xmlList;	/* list (XML serialized) */

	SCDLog(LOG_DEBUG, CFSTR("List notification keys which have changed."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	*scd_status = _SCDNotifierGetChanges(mySession->session, &notifierKeys);
	if (*scd_status != SCD_OK) {
		*listRef = NULL;
		*listLen = 0;
		return KERN_SUCCESS;
	}

	/*
	 * serialize the array, copy it into an allocated buffer which will be
	 * released when it is returned as part of a Mach message.
	 */
	xmlList = CFPropertyListCreateXMLData(NULL, notifierKeys);
	CFRelease(notifierKeys);
	*listLen = CFDataGetLength(xmlList);
	status = vm_allocate(mach_task_self(), (void *)listRef, *listLen, TRUE);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_allocate(): %s"), mach_error_string(status));
		CFRelease(xmlList);
		*listRef = NULL;
		*listLen = 0;
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	bcopy((char *)CFDataGetBytePtr(xmlList), *listRef, *listLen);
	CFRelease(xmlList);

	return KERN_SUCCESS;
}
