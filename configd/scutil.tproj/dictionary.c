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

#include "scutil.h"


//#include <stdlib.h>
//#include <limits.h>


void
do_dictInit(int argc, char **argv)
{
	CFMutableDictionaryRef	dict;

	if (data != NULL) {
		SCDHandleRelease(data);
	}

	data = SCDHandleInit();
	dict = CFDictionaryCreateMutable(NULL
					 ,0
					 ,&kCFTypeDictionaryKeyCallBacks
					 ,&kCFTypeDictionaryValueCallBacks
					 );
	SCDHandleSetData(data, dict);
	CFRelease(dict);

	return;
}


void
do_dictShow(int argc, char **argv)
{
	int			instance;
	CFPropertyListRef	store;

	if (data == NULL) {
		SCDLog(LOG_INFO, CFSTR("d.show: dictionary must be initialized."));
		return;
	}

	instance = SCDHandleGetInstance(data);
	store    = SCDHandleGetData(data);

	SCDLog(LOG_NOTICE, CFSTR("dict (instance = %d) = \n\t%@"), instance, store);

	return;
}


void
do_dictSetKey(int argc, char **argv)
{
	CFPropertyListRef	store;
	CFStringRef		key;
	CFPropertyListRef	value     = NULL;
	CFMutableArrayRef	array     = NULL;
	boolean_t		doArray   = FALSE;
	boolean_t		doBoolean = FALSE;
	boolean_t		doNumeric = FALSE;

	if (data == NULL) {
		SCDLog(LOG_INFO, CFSTR("d.add: dictionary must be initialized."));
		return;
	}

	store = SCDHandleGetData(data);
	if (CFGetTypeID(store) != CFDictionaryGetTypeID()) {
		SCDLog(LOG_INFO, CFSTR("d.add: data (fetched from configuration server) is not a dictionary"));
		return;
	}


	key = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
	argv++; argc--;

	while (argc > 0) {
		if (strcmp(argv[0], "*") == 0) {
			/* if array requested */
			doArray = TRUE;
		} else if (strcmp(argv[0], "-") == 0) {
			/* if string values requested */
		} else if (strcmp(argv[0], "?") == 0) {
			/* if boolean values requested */
			doBoolean = TRUE;
		} else if (strcmp(argv[0], "#") == 0) {
			/* if numeric values requested */
			doNumeric = TRUE;
		} else {
			/* it's not a special flag */
			break;
		}
		argv++; argc--;
	}

	if (argc > 1) {
		doArray = TRUE;
	} else if (!doArray && (argc == 0)) {
		SCDLog(LOG_INFO, CFSTR("d.add: no values"));
		return;
	}

	if (doArray) {
		array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	while (argc > 0) {
		if (doBoolean) {
			if         ((strcasecmp(argv[0], "true") == 0) ||
				    (strcasecmp(argv[0], "t"   ) == 0) ||
				    (strcasecmp(argv[0], "yes" ) == 0) ||
				    (strcasecmp(argv[0], "y"   ) == 0) ||
				    (strcmp    (argv[0], "1"   ) == 0)) {
				value = CFRetain(kCFBooleanTrue);
			} else if ((strcasecmp(argv[0], "false") == 0) ||
				   (strcasecmp(argv[0], "f"    ) == 0) ||
				   (strcasecmp(argv[0], "no"   ) == 0) ||
				   (strcasecmp(argv[0], "n"    ) == 0) ||
				   (strcmp    (argv[0], "0"    ) == 0)) {
				value = CFRetain(kCFBooleanFalse);
			} else {
				SCDLog(LOG_INFO, CFSTR("d.add: invalid data"));
				if (doArray) {
					CFRelease(array);
				}
				return;
			}
		} else if (doNumeric) {
			int	intValue;

			if (sscanf(argv[0], "%d", &intValue) == 1) {
				value = CFNumberCreate(NULL, kCFNumberIntType, &intValue);
			} else {
				SCDLog(LOG_INFO, CFSTR("d.add: invalid data"));
				if (doArray) {
					CFRelease(array);
				}
				return;
			}
		} else {
			value = (CFPropertyListRef)CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
		}

		if (doArray) {
			CFArrayAppendValue(array, value);
		}

		argv++; argc--;
	}

	if (doArray) {
		value = array;
	}

	CFDictionarySetValue((CFMutableDictionaryRef)store, key, value);
	CFRelease(value);
	CFRelease(key);

	return;
}


void
do_dictRemoveKey(int argc, char **argv)
{
	CFPropertyListRef	store;
	CFStringRef		key;

	if (data == NULL) {
		SCDLog(LOG_INFO, CFSTR("d.remove: dictionary must be initialized."));
		return;
	}

	store = SCDHandleGetData(data);
	if (CFGetTypeID(store) == CFDictionaryGetTypeID()) {
		key = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);
		CFDictionaryRemoveValue((CFMutableDictionaryRef)store, key);
		CFRelease(key);
	} else {
		SCDLog(LOG_INFO, CFSTR("d.add: data (fetched from configuration server) is not a dictionary"));
	}

	return;
}
