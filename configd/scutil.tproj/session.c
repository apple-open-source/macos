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

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "scutil.h"
#include "notify.h"

void
do_open(int argc, char **argv)
{
	if (store)	CFRelease(store);

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil"), storeCallback, NULL);
	if (!store) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
		return;
	}

	return;
}


void
do_close(int argc, char **argv)
{
	if (notifyRls) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), notifyRls, kCFRunLoopDefaultMode);
		CFRelease(notifyRls);
		notifyRls = NULL;
	}

	if (store) {
		CFRelease(store);
		store = NULL;
	}
	return;
}


void
do_lock(int argc, char **argv)
{
	if (!SCDynamicStoreLock(store)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	return;
}


void
do_unlock(int argc, char **argv)
{
	if (!SCDynamicStoreUnlock(store)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	return;
}
