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
_SCDGet(SCDSessionRef session, CFStringRef key, SCDHandleRef *handle)
{
	SCDSessionPrivateRef	sessionPrivate = (SCDSessionPrivateRef)session;
	CFDictionaryRef		dict;
	CFNumberRef		num;
	int			dictInstance;

	SCDLog(LOG_DEBUG, CFSTR("_SCDGet:"));
	SCDLog(LOG_DEBUG, CFSTR("  key      = %@"), key);

	if ((session == NULL) || (sessionPrivate->server == MACH_PORT_NULL)) {
		return SCD_NOSESSION;
	}

	dict = CFDictionaryGetValue(cacheData, key);
	if ((dict == NULL) || (CFDictionaryContainsKey(dict, kSCDData) == FALSE)) {
		/* key doesn't exist (or data never defined) */
		return SCD_NOKEY;
	}

	/* Create a new handle associated with the cached data */
	*handle = SCDHandleInit();

	/* Return the data associated with the key */
	SCDHandleSetData(*handle, CFDictionaryGetValue(dict, kSCDData));

	/* Return the instance number associated with the key */
	num = CFDictionaryGetValue(dict, kSCDInstance);
	(void) CFNumberGetValue(num, kCFNumberIntType, &dictInstance);
	_SCDHandleSetInstance(*handle, dictInstance);

	SCDLog(LOG_DEBUG, CFSTR("  data     = %@"), SCDHandleGetData(*handle));
	SCDLog(LOG_DEBUG, CFSTR("  instance = %d"), SCDHandleGetInstance(*handle));

	return SCD_OK;
}


kern_return_t
_configget(mach_port_t			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   xmlDataOut_t			*dataRef,	/* raw XML bytes */
	   mach_msg_type_number_t	*dataLen,
	   int				*newInstance,
	   int				*scd_status
)
{
	kern_return_t		status;
	serverSessionRef	mySession = getSession(server);
	CFDataRef		xmlKey;		/* key  (XML serialized) */
	CFStringRef		key;		/* key  (un-serialized) */
	CFDataRef		xmlData;	/* data (XML serialized) */
	SCDHandleRef		handle;
	CFStringRef		xmlError;

	SCDLog(LOG_DEBUG, CFSTR("Get key from configuration database."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	/* un-serialize the key */
	xmlKey = CFDataCreate(NULL, keyRef, keyLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)keyRef, keyLen);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	key = CFPropertyListCreateFromXMLData(NULL,
					      xmlKey,
					      kCFPropertyListImmutable,
					      &xmlError);
	CFRelease(xmlKey);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() key: %s"), xmlError);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	*scd_status = _SCDGet(mySession->session, key, &handle);
	CFRelease(key);
	if (*scd_status != SCD_OK) {
		*dataRef = NULL;
		*dataLen = 0;
		return KERN_SUCCESS;
	}

	/*
	 * serialize the data, copy it into an allocated buffer which will be
	 * released when it is returned as part of a Mach message.
	 */
	xmlData = CFPropertyListCreateXMLData(NULL, SCDHandleGetData(handle));
	*dataLen = CFDataGetLength(xmlData);
	status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_allocate(): %s"), mach_error_string(status));
		*scd_status = SCD_FAILED;
		CFRelease(xmlData);
		*dataRef = NULL;
		*dataLen = 0;
		return KERN_SUCCESS;
	}

	bcopy((char *)CFDataGetBytePtr(xmlData), *dataRef, *dataLen);
	CFRelease(xmlData);

	/*
	 * return the instance number associated with the returned data.
	 */
	*newInstance = SCDHandleGetInstance(handle);

	SCDHandleRelease(handle);

	return KERN_SUCCESS;
}
