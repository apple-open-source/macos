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
SCDKeyCreateConsoleUser()
{
	return SCDKeyCreate(CFSTR("%@/%@/%@"),
			    CFSTR("state:"),	// FIXME!!! (should be kSCCacheDomainState)
			    kSCCompUsers,
			    kSCEntUsersConsoleUser);
}


SCDStatus
SCDConsoleUserGet(char *user, int userlen, uid_t *uid, gid_t *gid)
{
	CFDictionaryRef	dict;
	SCDHandleRef	handle	= NULL;
	CFStringRef	key;
	SCDSessionRef	session	= NULL;
	SCDStatus	status;

	/* get current user */
	status = SCDOpen(&session, CFSTR("SCDConsoleUserGet"));
	if (status != SCD_OK) {
		goto done;
	}

	key = SCDKeyCreateConsoleUser();
	status = SCDGet(session, key, &handle);
	CFRelease(key);
	if (status != SCD_OK) {
		goto done;
	}

	dict = SCDHandleGetData(handle);

	if (user && (userlen > 0)) {
		CFStringRef	consoleUser;

		bzero(user, userlen);
		if (CFDictionaryGetValueIfPresent(dict,
						  kSCPropUsersConsoleUserName,
						  (void **)&consoleUser)) {
			CFIndex		len;
			CFRange		range;

			range = CFRangeMake(0, CFStringGetLength(consoleUser));
			(void)CFStringGetBytes(consoleUser,
					       range,
					       kCFStringEncodingMacRoman,
					       0,
					       FALSE,
					       user,
					       userlen,
					       &len);
		}
	}

	if (uid) {
		CFNumberRef	num;
		SInt32		val;

		if (CFDictionaryGetValueIfPresent(dict,
						  kSCPropUsersConsoleUserUID,
						  (void **)&num)) {
			if (CFNumberGetValue(num, kCFNumberSInt32Type, &val)) {
				*uid = (uid_t)val;
			}
		}
	}

	if (gid) {
		CFNumberRef	num;
		SInt32		val;

		if (CFDictionaryGetValueIfPresent(dict,
						  kSCPropUsersConsoleUserGID,
						  (void **)&num)) {
			if (CFNumberGetValue(num, kCFNumberSInt32Type, &val)) {
				*gid = (gid_t)val;
			}
		}
	}

    done :

	if (handle)	SCDHandleRelease(handle);
	if (session)	(void) SCDClose(&session);
	return status;
}


SCDStatus
SCDConsoleUserSet(const char *user, uid_t uid, gid_t gid)
{
	CFStringRef		consoleUser;
	CFMutableDictionaryRef	dict	= NULL;
	SCDHandleRef		handle	= NULL;
	CFStringRef		key	= SCDKeyCreateConsoleUser();
	CFNumberRef		num;
	SCDSessionRef		session	= NULL;
	SCDStatus		status;

	status = SCDOpen(&session, CFSTR("SCDConsoleUserSet"));
	if (status != SCD_OK) {
		goto done;
	}

	if (user == NULL) {
		(void)SCDRemove(session, key);
		goto done;
	}

	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

	consoleUser = CFStringCreateWithCString(NULL, user, kCFStringEncodingMacRoman);
	CFDictionarySetValue(dict, kSCPropUsersConsoleUserName, consoleUser);
	CFRelease(consoleUser);

	num = CFNumberCreate(NULL, kCFNumberSInt32Type, (SInt32 *)&uid);
	CFDictionarySetValue(dict, kSCPropUsersConsoleUserUID, num);
	CFRelease(num);

	num = CFNumberCreate(NULL, kCFNumberSInt32Type, (SInt32 *)&gid);
	CFDictionarySetValue(dict, kSCPropUsersConsoleUserGID, num);
	CFRelease(num);

	handle = SCDHandleInit();
	SCDHandleSetData(handle, dict);

	status = SCDLock(session);
	if (status != SCD_OK) {
		goto done;
	}
	(void)SCDRemove(session, key);
	(void)SCDAdd   (session, key, handle);
	status = SCDUnlock(session);

    done :

	if (dict)	CFRelease(dict);
	if (handle)	SCDHandleRelease(handle);
	if (key)	CFRelease(key);
	if (session)	(void) SCDClose(&session);
	return status;
}
