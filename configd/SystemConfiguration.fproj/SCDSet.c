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
SCDSet(SCDSessionRef session, CFStringRef key, SCDHandleRef handle)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	kern_return_t		status;
	CFDataRef		xmlKey;		/* serialized key */
	xmlData_t		myKeyRef;
	CFIndex			myKeyLen;
	CFDataRef		xmlData;	/* serialized data */
	xmlData_t		myDataRef;
	CFIndex			myDataLen;
	SCDStatus		scd_status;
	int			newInstance;

	SCDLog(LOG_DEBUG, CFSTR("SCDSet:"));
	SCDLog(LOG_DEBUG, CFSTR("  key          = %@"), key);
	SCDLog(LOG_DEBUG, CFSTR("  data         = %@"), SCDHandleGetData(handle));
	SCDLog(LOG_DEBUG, CFSTR("  instance     = %d"), SCDHandleGetInstance(handle));

	if (key == NULL) {
		return SCD_INVALIDARGUMENT;	/* no key specified */
	}

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;		/* you can't do anything with a closed session */
	}

	/* serialize the key and data */
	xmlKey = CFPropertyListCreateXMLData(NULL, key);
	myKeyRef = (xmlData_t)CFDataGetBytePtr(xmlKey);
	myKeyLen = CFDataGetLength(xmlKey);

	xmlData = CFPropertyListCreateXMLData(NULL, SCDHandleGetData(handle));
	myDataRef = (xmlData_t)CFDataGetBytePtr(xmlData);
	myDataLen = CFDataGetLength(xmlData);

	/* send the key & data to the server, get new instance id */
	status = configset(sessionPrivate->server,
			   myKeyRef,
			   myKeyLen,
			   myDataRef,
			   myDataLen,
			   SCDHandleGetInstance(handle),
			   &newInstance,
			   (int *)&scd_status);

	/* clean up */
	CFRelease(xmlKey);
	CFRelease(xmlData);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCDLog(LOG_DEBUG, CFSTR("configset(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), sessionPrivate->server);
		sessionPrivate->server = MACH_PORT_NULL;
		return SCD_NOSERVER;
	}

	if (scd_status == SCD_OK) {
		_SCDHandleSetInstance(handle, newInstance);
	}

	SCDLog(LOG_DEBUG, CFSTR("  new instance = %d"), SCDHandleGetInstance(handle));

	return scd_status;
}
