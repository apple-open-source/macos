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

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SCD.h>
#include "config.h"		/* MiG generated file */
#include "SCDPrivate.h"


static CFComparisonResult
sort_keys(const void *p1, const void *p2, void *context) {
	CFStringRef key1 = (CFStringRef)p1;
	CFStringRef key2 = (CFStringRef)p2;
	return CFStringCompare(key1, key2, 0);
}


SCDStatus
SCDNotifierGetChanges(SCDSessionRef session, CFArrayRef *notifierKeys)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	CFDataRef		xmlData;	/* data (XML serialized) */
	xmlDataOut_t		xmlDataRef;	/* serialized data */
	int			xmlDataLen;
	SCDStatus		scd_status;
	CFArrayRef		allKeys;
	CFStringRef		xmlError;
	CFIndex			keyCnt;
	CFMutableArrayRef	sortedKeys;

	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierGetChanges:"));

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	/* send the key & fetch the associated data from the server */
	status = notifychanges(sessionPrivate->server,
			       &xmlDataRef,
			       &xmlDataLen,
			       (int *)&scd_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("notifychanges(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	if (scd_status != SCD_OK) {
		status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
		if (status != KERN_SUCCESS) {
			SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		*notifierKeys = NULL;
		return scd_status;
	}

	/* un-serialize the list of keys which have changed */
	xmlData = CFDataCreate(NULL, xmlDataRef, xmlDataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	allKeys = CFPropertyListCreateFromXMLData(NULL,
						  xmlData,
						  kCFPropertyListImmutable,
						  &xmlError);
	CFRelease(xmlData);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() list: %s"), xmlError);
		return SCD_FAILED;
	}

	keyCnt = CFArrayGetCount(allKeys);
	sortedKeys = CFArrayCreateMutableCopy(NULL, keyCnt, allKeys);
	CFRelease(allKeys);
	CFArraySortValues(sortedKeys,
			  CFRangeMake(0, keyCnt),
			  sort_keys,
			  NULL);

	*notifierKeys = sortedKeys;
	return scd_status;
}
