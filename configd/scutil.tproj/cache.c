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

#include "scutil.h"

void
do_list(int argc, char **argv)
{
	CFStringRef		key;
	int			regexOptions = 0;
	SCDStatus		status;
	CFArrayRef		list;
	CFIndex			listCnt;
	int			i;

	key = CFStringCreateWithCString(NULL,
					(argc >= 1) ? argv[0] : "",
					kCFStringEncodingMacRoman);

	if (argc == 2)
		regexOptions = kSCDRegexKey;

	status = SCDList(session, key, regexOptions, &list);
	CFRelease(key);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDList: %s"), SCDError(status));
		return;
	}

	listCnt = CFArrayGetCount(list);
	if (listCnt > 0) {
		for (i=0; i<listCnt; i++) {
			SCDLog(LOG_NOTICE, CFSTR("  subKey [%d] = %@"), i, CFArrayGetValueAtIndex(list, i));
		}
	} else {
		SCDLog(LOG_NOTICE, CFSTR("  no subKey's"));
	}
	CFRelease(list);

	return;
}


void
do_add(int argc, char **argv)
{
	CFStringRef	key;
	SCDStatus	status;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);

	if (argc < 2) {
		status = SCDAdd(session, key, data);
		if (status != SCD_OK) {
			SCDLog(LOG_INFO, CFSTR("SCDAdd: %s"), SCDError(status));
		}
	} else {
		status = SCDAddSession(session, key, data);
		if (status != SCD_OK) {
			SCDLog(LOG_INFO, CFSTR("SCDAddSession: %s"), SCDError(status));
		}
	}

	CFRelease(key);
	return;
}


void
do_get(int argc, char **argv)
{
	SCDStatus	status;
	CFStringRef	key;
	SCDHandleRef	newData = NULL;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	status = SCDGet(session, key, &newData);
	CFRelease(key);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDGet: %s"), SCDError(status));
		if (newData != NULL) {
			SCDHandleRelease(newData);	/* toss the handle from SCDGet() */
		}
		return;
	}

	if (data != NULL) {
		SCDHandleRelease(data);		/* we got a new handle from SCDGet() */
	}
	data = newData;

	return;
}


void
do_set(int argc, char **argv)
{
	SCDStatus	status;
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	status = SCDSet(session, key, data);
	CFRelease(key);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDSet: %s"), SCDError(status));
	}
	return;
}


void
do_remove(int argc, char **argv)
{
	SCDStatus	status;
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	status = SCDRemove(session, key);
	CFRelease(key);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDRemove: %s"), SCDError(status));
	}
	return;
}


void
do_touch(int argc, char **argv)
{
	SCDStatus	status;
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	status = SCDTouch(session, key);
	CFRelease(key);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDTouch: %s"), SCDError(status));
	}
	return;
}
