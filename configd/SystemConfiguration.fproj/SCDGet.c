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


SCDStatus
SCDGet(SCDSessionRef session, CFStringRef key, SCDHandleRef *handle)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	CFDataRef		xmlKey;		/* key (XML serialized) */
	xmlData_t		myKeyRef;	/* key (serialized) */
	CFIndex			myKeyLen;
	xmlDataOut_t		xmlDataRef;	/* data (serialized) */
	CFIndex			xmlDataLen;
	CFDataRef		xmlData;	/* data (XML serialized) */
	CFPropertyListRef	data;		/* data (un-serialized) */
	int			newInstance;
	SCDStatus		scd_status;
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("SCDGet:"));
	SCDLog(LOG_DEBUG, CFSTR("  key      = %@"), key);

	if (key == NULL) {
		return SCD_INVALIDARGUMENT;	/* no key specified */
	}

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	/* serialize the key */
	xmlKey = CFPropertyListCreateXMLData(NULL, key);
	myKeyRef = (xmlData_t)CFDataGetBytePtr(xmlKey);
	myKeyLen = CFDataGetLength(xmlKey);

	/* send the key & fetch the associated data from the server */
	status = configget(sessionPrivate->server,
			   myKeyRef,
			   myKeyLen,
			   &xmlDataRef,
			   (int *)&xmlDataLen,
			   &newInstance,
			   (int *)&scd_status);

	/* clean up */
	CFRelease(xmlKey);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("configget(): %s"), mach_error_string(status));
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
		*handle = NULL;
		return scd_status;
	}

	/* un-serialize the data, return a handle associated with the key */
	*handle = SCDHandleInit();
	xmlData = CFDataCreate(NULL, xmlDataRef, xmlDataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	data = CFPropertyListCreateFromXMLData(NULL,
					       xmlData,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlData);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() data: %s"), xmlError);
		SCDHandleRelease(*handle);
		return SCD_FAILED;
	}
	SCDHandleSetData(*handle, data);
	CFRelease(data);

	_SCDHandleSetInstance(*handle, newInstance);

	SCDLog(LOG_DEBUG, CFSTR("  data     = %@"), SCDHandleGetData(*handle));
	SCDLog(LOG_DEBUG, CFSTR("  instance = %d"), SCDHandleGetInstance(*handle));

	return scd_status;
}
