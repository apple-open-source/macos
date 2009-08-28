/*
 * Copyright (c) 2000-2004, 2006, 2008 Apple Inc. All rights reserved.
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
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "configd.h"
#include "session.h"
#include "pattern.h"


__private_extern__
int
__SCDynamicStoreRemoveWatchedKey(SCDynamicStoreRef store, CFStringRef key, Boolean isRegex, Boolean internal)
{
	CFNumberRef			sessionNum;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;

	if ((store == NULL) || (storePrivate->server == MACH_PORT_NULL)) {
		return kSCStatusNoStoreSession;	/* you must have an open session to play */
	}

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace,
			CFSTR("%s : %5d : %s : %@\n"),
			internal ? "*watch-" : "watch- ",
			storePrivate->server,
			isRegex  ? "pattern" : "key",
			key);
	}

	/*
	 * remove key from this sessions notifier list after checking that
	 * it was previously defined.
	 */
	if (isRegex) {
		if (!CFSetContainsValue(storePrivate->patterns, key))
			return kSCStatusNoKey;		/* sorry, key does not exist in notifier list */

		/* remove key from this sessions notifier list */
		CFSetRemoveValue(storePrivate->patterns, key);

		/* remove this session as a pattern watcher */
		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &storePrivate->server);
		patternRemoveSession(key, sessionNum);
		CFRelease(sessionNum);
	} else {
		if (!CFSetContainsValue(storePrivate->keys, key))
			return kSCStatusNoKey;		/* sorry, key does not exist in notifier list */

		/* remove key from this sessions notifier list */
		CFSetRemoveValue(storePrivate->keys, key);

		/*
		 * We are watching a specific key. As such, update the
		 * store to mark our interest in any changes.
		 */
		sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &storePrivate->server);
		_removeWatcher(sessionNum, key);
		CFRelease(sessionNum);
	}

	return kSCStatusOK;
}


__private_extern__
kern_return_t
_notifyremove(mach_port_t		server,
	      xmlData_t			keyRef,		/* raw XML bytes */
	      mach_msg_type_number_t	keyLen,
	      int			isRegex,
	      int			*sc_status
)
{
	CFStringRef		key		= NULL;		/* key  (un-serialized) */
	serverSessionRef	mySession;

	/* un-serialize the key */
	if (!_SCUnserializeString(&key, NULL, (void *)keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
		goto done;
	}

	if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	mySession = getSession(server);
	if (mySession == NULL) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		goto done;
	}

	*sc_status = __SCDynamicStoreRemoveWatchedKey(mySession->store,
						      key,
						      isRegex != 0,
						      FALSE);

    done :

	if (key)	CFRelease(key);
	return KERN_SUCCESS;
}

