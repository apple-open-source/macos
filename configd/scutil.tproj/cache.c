/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <sys/types.h>

#include "scutil.h"
#include "cache.h"


static CFComparisonResult
sort_keys(const void *p1, const void *p2, void *context) {
	CFStringRef key1 = (CFStringRef)p1;
	CFStringRef key2 = (CFStringRef)p2;
	return CFStringCompare(key1, key2, 0);
}


void
do_list(int argc, char **argv)
{
	int			i;
	CFStringRef		pattern;
	CFArrayRef		list;
	CFIndex			listCnt;
	CFMutableArrayRef	sortedList;

	pattern = CFStringCreateWithCString(NULL,
					    (argc >= 1) ? argv[0] : ".*",
					    kCFStringEncodingMacRoman);

	list = SCDynamicStoreCopyKeyList(store, pattern);
	CFRelease(pattern);
	if (!list) {
		if (SCError() != kSCStatusOK) {
			SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		} else {
			SCPrint(TRUE, stdout, CFSTR("  no keys.\n"));
		}
		return;
	}

	listCnt = CFArrayGetCount(list);
	sortedList = CFArrayCreateMutableCopy(NULL, listCnt, list);
	CFRelease(list);
	CFArraySortValues(sortedList,
			  CFRangeMake(0, listCnt),
			  sort_keys,
			  NULL);

	if (listCnt > 0) {
		for (i = 0; i < listCnt; i++) {
			SCPrint(TRUE,
				stdout,
				CFSTR("  subKey [%d] = %@\n"),
				i,
				CFArrayGetValueAtIndex(sortedList, i));
		}
	} else {
		SCPrint(TRUE, stdout, CFSTR("  no keys.\n"));
	}
	CFRelease(sortedList);

	return;
}


void
do_add(int argc, char **argv)
{
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);

	if (argc < 2) {
		if (!SCDynamicStoreAddValue(store, key, value)) {
			SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		}
	} else {
		if (!SCDynamicStoreAddTemporaryValue(store, key, value)) {
			SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		}
	}

	CFRelease(key);
	return;
}


void
do_get(int argc, char **argv)
{
	CFStringRef		key;
	CFPropertyListRef	newValue;

	key      = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	newValue = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (!newValue) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	if (value != NULL) {
		CFRelease(value);		/* we have new information, release the old */
	}
	value = newValue;

	return;
}


void
do_set(int argc, char **argv)
{
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	if (!SCDynamicStoreSetValue(store, key, value)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	CFRelease(key);
	return;
}


void
do_show(int argc, char **argv)
{
	CFStringRef		key;
	CFPropertyListRef	newValue;

	key = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);

	if (argc == 1) {
		newValue = SCDynamicStoreCopyValue(store, key);
	} else {
		CFArrayRef	patterns;

		patterns = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
		newValue = SCDynamicStoreCopyMultiple(store, NULL, patterns);
		CFRelease(patterns);
	}

	CFRelease(key);
	if (!newValue) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	SCPrint(TRUE, stdout, CFSTR("%@\n"), newValue);
	CFRelease(newValue);
	return;
}


void
do_remove(int argc, char **argv)
{
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	if (!SCDynamicStoreRemoveValue(store, key)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	CFRelease(key);
	return;
}


void
do_notify(int argc, char **argv)
{
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	if (!SCDynamicStoreNotifyValue(store, key)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	CFRelease(key);
	return;
}


void
do_touch(int argc, char **argv)
{
	CFStringRef	key;

	key    = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	if (!SCDynamicStoreTouchValue(store, key)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	CFRelease(key);
	return;
}
