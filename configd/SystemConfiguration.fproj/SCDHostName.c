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

#include <SystemConfiguration/SystemConfiguration.h>


CFStringRef
SCDKeyCreateHostName()
{
	return SCDKeyCreate(CFSTR("%@/%@"),
			    kSCCacheDomainSetup,
			    kSCCompSystem);
}


SCDStatus
SCDHostNameGet(CFStringRef *name, CFStringEncoding *nameEncoding)
{
	CFDictionaryRef	dict;
	SCDHandleRef	handle	= NULL;
	CFStringRef	key;
	SCDSessionRef	session	= NULL;
	SCDStatus	status;

	if (name == NULL) {
		return SCD_FAILED;
	}

	/* get current user */
	status = SCDOpen(&session, CFSTR("SCDHostNameGet"));
	if (status != SCD_OK) {
		goto done;
	}

	key = SCDKeyCreateHostName();
	status = SCDGet(session, key, &handle);
	CFRelease(key);
	if (status != SCD_OK) {
		goto done;
	}

	dict = SCDHandleGetData(handle);

	*name = CFDictionaryGetValue(dict, kSCPropSystemComputerName);
	if (*name == NULL) {
		goto done;
	}
	CFRetain(*name);

	if (nameEncoding) {
		CFNumberRef	num;

		num = CFDictionaryGetValue(dict,
					   kSCPropSystemComputerNameEncoding);
		if (num) {
			CFNumberGetValue(num, kCFNumberIntType, nameEncoding);
		} else {
			*nameEncoding = CFStringGetSystemEncoding();
		}
	}

    done :

	if (handle)	SCDHandleRelease(handle);
	if (session)	(void) SCDClose(&session);
	return status;
}
