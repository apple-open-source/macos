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
 * - initial revision
 */

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */


static __inline__ void
my_CFSetApplyFunction(CFSetRef			theSet,
		      CFSetApplierFunction	applier,
		      void			*context)
{
	CFAllocatorRef	myAllocator;
	CFSetRef	mySet;

	myAllocator = CFGetAllocator(theSet);
	mySet       = CFSetCreateCopy(myAllocator, theSet);
	CFSetApplyFunction(mySet, applier, context);
	CFRelease(mySet);
	return;
}


/*
 * "context" argument for removeOldKey() and addNewKey()
 */
typedef struct {
	SCDynamicStoreRef       store;
	CFArrayRef		newKeys;	/* for removeOldKey */
	Boolean			isRegex;
	Boolean			ok;
} updateKeysContext, *updateKeysContextRef;


static void
removeOldKey(const void *value, void *context)
{
	CFStringRef			oldKey		= (CFStringRef)value;
	updateKeysContextRef		myContextRef	= (updateKeysContextRef)context;

	if (!myContextRef->ok) {
		return;
	}

	if (!myContextRef->newKeys ||
	    !CFArrayContainsValue(myContextRef->newKeys,
				  CFRangeMake(0, CFArrayGetCount(myContextRef->newKeys)),
				  oldKey)) {
		/* the old notification key is not being retained, remove it */
		myContextRef->ok = SCDynamicStoreRemoveWatchedKey(myContextRef->store,
								  oldKey,
								  myContextRef->isRegex);
	}

	return;
}


static void
addNewKey(const void *value, void *context)
{
	CFStringRef			newKey		= (CFStringRef)value;
	updateKeysContextRef		myContextRef	= (updateKeysContextRef)context;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)myContextRef->store;

	if (!myContextRef->ok) {
		return;
	}

	if (myContextRef->isRegex) {
		if (!CFSetContainsValue(storePrivate->reKeys, newKey)) {
			/* add pattern to this sessions notifier list */
			myContextRef->ok = SCDynamicStoreAddWatchedKey(myContextRef->store, newKey, TRUE);
		}
	} else {
		if (!CFSetContainsValue(storePrivate->keys, newKey)) {
			/* add key to this sessions notifier list */
			myContextRef->ok = SCDynamicStoreAddWatchedKey(myContextRef->store, newKey, FALSE);
		}
	}

	return;
}

Boolean
SCDynamicStoreSetNotificationKeys(SCDynamicStoreRef	store,
				  CFArrayRef		keys,
				  CFArrayRef		patterns)
{
	updateKeysContext		myContext;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreSetNotificationKeys:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  keys     = %@"), keys);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  patterns = %@"), patterns);

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return FALSE;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		/* sorry, you must have an open session to play */
		_SCErrorSet(kSCStatusNoStoreServer);
		return FALSE;
	}

	myContext.ok      = TRUE;
	myContext.store   = store;

	/* remove any previously registered keys */
	myContext.newKeys = keys;
	myContext.isRegex = FALSE;
	my_CFSetApplyFunction(storePrivate->keys, removeOldKey, &myContext);

	/* register any new keys */
	if (keys) {
		CFArrayApplyFunction(keys,
				     CFRangeMake(0, CFArrayGetCount(keys)),
				     addNewKey,
				     &myContext);
	}

	/* remove any previously registered patterns */
	myContext.newKeys = patterns;
	myContext.isRegex = TRUE;
	my_CFSetApplyFunction(storePrivate->reKeys, removeOldKey, &myContext);

	/* register any new patterns */
	if (patterns) {
		CFArrayApplyFunction(patterns,
				     CFRangeMake(0, CFArrayGetCount(patterns)),
				     addNewKey,
				     &myContext);
	}

	return myContext.ok;
}
