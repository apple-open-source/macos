/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "session.h"
#include "pattern.h"


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


__private_extern__
int
__SCDynamicStoreAddWatchedKey(SCDynamicStoreRef store, CFStringRef key, Boolean isRegex, Boolean internal)
{
	int				sc_status	= kSCStatusOK;
	CFNumberRef			sessionNum	= NULL;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreAddWatchedKey:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  key     = %@"), key);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  isRegex = %s"), isRegex ? "TRUE" : "FALSE");
	}

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("%s : %5d : %s : %@\n"),
			internal ? "*watch+" : "watch+ ",
			storePrivate->server,
			isRegex  ? "pattern" : "key",
			key);
	}

	sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &storePrivate->server);

	if (isRegex) {
		if (CFSetContainsValue(storePrivate->patterns, key)) {
			/* sorry, pattern already exists in notifier list */
			sc_status = kSCStatusKeyExists;
			goto done;
		}

		/*
		 * add this session as a pattern watcher
		 */
		if (!patternAddSession(key, sessionNum)) {
			sc_status = kSCStatusInvalidArgument;
			goto done;
		}

		/* add pattern to this sessions notifier list */
		CFSetAddValue(storePrivate->patterns, key);
	} else {
		if (CFSetContainsValue(storePrivate->keys, key)) {
			/* sorry, key already exists in notifier list */
			sc_status = kSCStatusKeyExists;
			goto done;
		}

		/*
		 * We are watching a specific key. As such, update the
		 * store to mark our interest in any changes.
		 */
		_addWatcher(sessionNum, key);

		/* add key to this sessions notifier list */
		CFSetAddValue(storePrivate->keys, key);
	}

    done :

	if (sessionNum)	CFRelease(sessionNum);
	return sc_status;
}


__private_extern__
kern_return_t
_notifyadd(mach_port_t 			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   int				isRegex,
	   int				*sc_status
)
{
	serverSessionRef	mySession = getSession(server);
	CFStringRef		key;		/* key  (un-serialized) */

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("Add notification key for this session."));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  server = %d"), server);
	}

	/* un-serialize the key */
	if (!_SCUnserializeString(&key, NULL, (void *)keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
		CFRelease(key);
		return KERN_SUCCESS;
	}

	if (!mySession) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		CFRelease(key);
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreAddWatchedKey(mySession->store, key, isRegex != 0, FALSE);
	CFRelease(key);

	return KERN_SUCCESS;
}


/*
 * "context" argument for removeOldKey() and addNewKey()
 */
typedef struct {
	SCDynamicStoreRef       store;
	CFSetRef		oldKeys;	/* for addNewKey */
	CFArrayRef		newKeys;	/* for removeOldKey */
	Boolean			isRegex;
	int			sc_status;
} updateKeysContext, *updateKeysContextRef;


static void
removeOldKey(const void *value, void *context)
{
	CFStringRef			oldKey		= (CFStringRef)value;
	updateKeysContextRef		myContextRef	= (updateKeysContextRef)context;

	if (myContextRef->sc_status != kSCStatusOK) {
		return;
	}

	if (!myContextRef->newKeys ||
	    !CFArrayContainsValue(myContextRef->newKeys,
				  CFRangeMake(0, CFArrayGetCount(myContextRef->newKeys)),
				  oldKey)) {
		/* the old notification key is not being retained, remove it */
		myContextRef->sc_status = __SCDynamicStoreRemoveWatchedKey(myContextRef->store,
									   oldKey,
									   myContextRef->isRegex,
									   TRUE);
	}

	return;
}


static void
addNewKey(const void *value, void *context)
{
	CFStringRef			newKey		= (CFStringRef)value;
	updateKeysContextRef		myContextRef	= (updateKeysContextRef)context;

	if (myContextRef->sc_status != kSCStatusOK) {
		return;
	}

	if (!myContextRef->oldKeys ||
	    !CFSetContainsValue(myContextRef->oldKeys, newKey)) {
		/* if this is a new notification key */
		myContextRef->sc_status = __SCDynamicStoreAddWatchedKey(myContextRef->store,
									newKey,
									myContextRef->isRegex,
									TRUE);
	}

	return;
}


__private_extern__
int
__SCDynamicStoreSetNotificationKeys(SCDynamicStoreRef store, CFArrayRef keys, CFArrayRef patterns)
{
	updateKeysContext		myContext;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreSetNotificationKeys:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  keys     = %@"), keys);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  patterns = %@"), patterns);
	}

	if (!store || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("watch   : %5d : %d keys, %d patterns\n"),
			storePrivate->server,
			keys     ? CFArrayGetCount(keys)     : 0,
			patterns ? CFArrayGetCount(patterns) : 0);
	}

	myContext.store     = store;
	myContext.sc_status = kSCStatusOK;

	/* remove any previously registered keys, register any new keys */
	myContext.oldKeys = CFSetCreateCopy(NULL, storePrivate->keys);
	myContext.newKeys = keys;
	myContext.isRegex = FALSE;
	my_CFSetApplyFunction(storePrivate->keys, removeOldKey, &myContext);
	if (keys) {
		CFArrayApplyFunction(keys,
				     CFRangeMake(0, CFArrayGetCount(keys)),
				     addNewKey,
				     &myContext);
	}
	CFRelease(myContext.oldKeys);

	/* remove any previously registered patterns, register any new patterns */
	myContext.oldKeys = CFSetCreateCopy(NULL, storePrivate->patterns);
	myContext.newKeys = patterns;
	myContext.isRegex = TRUE;
	my_CFSetApplyFunction(storePrivate->patterns, removeOldKey, &myContext);
	if (patterns) {
		CFArrayApplyFunction(patterns,
				     CFRangeMake(0, CFArrayGetCount(patterns)),
				     addNewKey,
				     &myContext);
	}
	CFRelease(myContext.oldKeys);

	return myContext.sc_status;
}


__private_extern__
kern_return_t
_notifyset(mach_port_t 			server,
	   xmlData_t			keysRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keysLen,
	   xmlData_t			patternsRef,		/* raw XML bytes */
	   mach_msg_type_number_t	patternsLen,
	   int				*sc_status
)
{
	serverSessionRef	mySession	= getSession(server);
	CFArrayRef		keys		= NULL;	/* key (un-serialized) */
	CFArrayRef		patterns	= NULL;	/* patterns (un-serialized) */

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("Add notification key for this session."));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  server = %d"), server);
	}

	*sc_status = kSCStatusOK;

	if (keysRef && (keysLen > 0)) {
		/* un-serialize the keys */
		if (!_SCUnserialize((CFPropertyListRef *)&keys, NULL, (void *)keysRef, keysLen)) {
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(keys)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (patternsRef && (patternsLen > 0)) {
		/* un-serialize the patterns */
		if (!_SCUnserialize((CFPropertyListRef *)&patterns, NULL, (void *)patternsRef, patternsLen)) {
			*sc_status = kSCStatusFailed;
		} else if (!isA_CFArray(patterns)) {
			*sc_status = kSCStatusInvalidArgument;
		}
	}

	if (!mySession) {
		/* you must have an open session to play */
		*sc_status = kSCStatusNoStoreSession;
	}

	if (*sc_status == kSCStatusOK) {
		*sc_status = __SCDynamicStoreSetNotificationKeys(mySession->store, keys, patterns);
	}

	if (keys)	CFRelease(keys);
	if (patterns)	CFRelease(patterns);

	return KERN_SUCCESS;
}
