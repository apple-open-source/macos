/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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
 * June 2, 2000			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <unistd.h>

#include "configd.h"
#include "configd_server.h"
#include "session.h"
#include "scdtest.h"


__private_extern__ CFMutableDictionaryRef	storeData		= NULL;

__private_extern__ CFMutableDictionaryRef	patternData		= NULL;

__private_extern__ CFMutableSetRef		changedKeys		= NULL;

__private_extern__ CFMutableSetRef		deferredRemovals	= NULL;

__private_extern__ CFMutableSetRef		removedSessionKeys	= NULL;

__private_extern__ CFMutableSetRef		needsNotification	= NULL;


#if TARGET_OS_OSX

static void
install_console_user_write_restriction(void)
{
	CFDictionaryRef	controls;
	CFStringRef	key;

	/*
	 * rdar://64659598 Only entitled processes should be able to call SCDynamicStoreSetConsoleInformation
	 *
	 * <key>write-protect</key>
	 * <true/> [value is immaterial]
	 */
	key = kSCDAccessControls_writeProtect;
	controls = CFDictionaryCreate(NULL,
				      (const void * *)&key,
				      (const void * *)&kCFBooleanTrue,
				      1,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	key = SCDynamicStoreKeyCreateConsoleUser(NULL);
	_storeKeySetAccessControls(key, controls);
	CFRelease(key);

	CFRelease(controls);
}

#endif /* TARGET_OS_OSX */

#define _SET_KEY_VALUE_ELEMENT(kl, vl, c, k, v) {			\
	long _limit = sizeof(kl) / sizeof(kl[0]);			\
									\
	if ((c) >= _limit) {						\
		SC_log(LOG_ERR,						\
		       "%s:%d _SET_KEY_VALUE_ELEMENT (%ld >= %ld)",	\
		       __func__, __LINE__, (long)c, (long)_limit);	\
	}								\
	else {								\
	      kl[c] = k;						\
	      vl[c] = v;						\
	      c++;							\
	}								\
}

static void
install_device_name_read_restrictions(void)
{
#define N_KEYS		5
	CFDictionaryRef	controls;
	CFIndex		count;
	CFStringRef	entitlement;
	CFStringRef	key;
	CFStringRef	keys[N_KEYS];
	CFArrayRef	read_allow = NULL;
	CFArrayRef	read_deny;
	CFTypeRef	values[N_KEYS];

	/* deny App Clips */
	count = 0;
	entitlement = CFSTR(APP_CLIP_ENTITLEMENT);
	read_deny = CFArrayCreate(NULL,
				  (const void * *)&entitlement,
				  1,
				  &kCFTypeArrayCallBacks);
	_SET_KEY_VALUE_ELEMENT(keys, values, count,
			       kSCDAccessControls_readDeny,
			       read_deny);

	/* deny Background Asset Extensions */
	_SET_KEY_VALUE_ELEMENT(keys, values, count,
			       kSCDAccessControls_readDenyBackground,
			       kCFBooleanTrue);
#if TARGET_OS_IPHONE
	/* allow entitlement */
	entitlement = CFSTR(DEVICE_NAME_PUBLIC_ENTITLEMENT);
	read_allow = CFArrayCreate(NULL,
				   (const void * *)&entitlement,
				   1,
				   &kCFTypeArrayCallBacks);
	_SET_KEY_VALUE_ELEMENT(keys, values, count,
			       kSCDAccessControls_readAllow,
			       read_allow);

	/* allow System processes */
	_SET_KEY_VALUE_ELEMENT(keys, values, count,
			       kSCDAccessControls_readAllowSystem,
			       kCFBooleanTrue);
#endif /* TARGET_OS_IPHONE */
	controls = CFDictionaryCreate(NULL,
				      (const void * *)keys,
				      (const void * *)values,
				      count,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	if (read_allow != NULL) {
		CFRelease(read_allow);
	}
	CFRelease(read_deny);

	/* set access control for ComputerName, LocalHostName, and HostName */
	key = SCDynamicStoreKeyCreateComputerName(NULL);
	_storeKeySetAccessControls(key, controls);
	CFRelease(key);
	key = SCDynamicStoreKeyCreateHostNames(NULL);
	_storeKeySetAccessControls(key, controls);
	CFRelease(key);

	CFRelease(controls);
}


static CFDictionaryRef
create_entitlement_controls(CFStringRef entitlement, CFStringRef key)
{
	CFArrayRef	list;
	CFDictionaryRef	controls;

	list = CFArrayCreate(NULL,
			     (const void * *)&entitlement, 1,
			     &kCFTypeArrayCallBacks);
	controls = CFDictionaryCreate(NULL,
				      (const void * *)&key,
				      (const void * *)&list,
				      1,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	CFRelease(list);
	return (controls);
}

static CFDictionaryRef
create_boolean_controls(CFStringRef key)
{
	CFDictionaryRef	controls;

	controls = CFDictionaryCreate(NULL,
				      (const void * *)&key,
				      (const void * *)&kCFBooleanTrue,
				      1,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	return (controls);
}

static void
install_test_restrictions(void)
{
	CFDictionaryRef	controls;

	/*
	 * read-deny
	 */
	controls = create_entitlement_controls(SCDTEST_READ_DENY1_ENTITLEMENT,
					       kSCDAccessControls_readDeny);
	_storeKeySetAccessControls(SCDTEST_READ_DENY1_KEY, controls);
	CFRelease(controls);

	controls = create_entitlement_controls(SCDTEST_READ_DENY2_ENTITLEMENT,
					       kSCDAccessControls_readDeny);
	_storeKeySetAccessControls(SCDTEST_READ_DENY2_KEY, controls);
	CFRelease(controls);

	/*
	 * read-allow
	 */
	controls = create_entitlement_controls(SCDTEST_READ_ALLOW1_ENTITLEMENT,
					       kSCDAccessControls_readAllow);
	_storeKeySetAccessControls(SCDTEST_READ_ALLOW1_KEY, controls);
	CFRelease(controls);
	controls = create_entitlement_controls(SCDTEST_READ_ALLOW2_ENTITLEMENT,
					       kSCDAccessControls_readAllow);
	_storeKeySetAccessControls(SCDTEST_READ_ALLOW2_KEY, controls);
	CFRelease(controls);

	/*
	 * write-protect
	 */
	controls = create_boolean_controls(kSCDAccessControls_writeProtect);
	_storeKeySetAccessControls(SCDTEST_WRITE_PROTECT1_KEY, controls);
	_storeKeySetAccessControls(SCDTEST_WRITE_PROTECT2_KEY, controls);
	CFRelease(controls);
}

__private_extern__
void
__SCDynamicStoreInit(void)
{
	storeData          = CFDictionaryCreateMutable(NULL,
						       0,
						       &kCFTypeDictionaryKeyCallBacks,
						       &kCFTypeDictionaryValueCallBacks);
	patternData        = CFDictionaryCreateMutable(NULL,
						       0,
						       &kCFTypeDictionaryKeyCallBacks,
						       &kCFTypeDictionaryValueCallBacks);
	changedKeys        = CFSetCreateMutable(NULL,
						0,
						&kCFTypeSetCallBacks);
	deferredRemovals   = CFSetCreateMutable(NULL,
						0,
						&kCFTypeSetCallBacks);
	removedSessionKeys = CFSetCreateMutable(NULL,
						0,
						&kCFTypeSetCallBacks);

#if TARGET_OS_OSX
	install_console_user_write_restriction();
#endif /* TARGET_OS_OSX */
	install_device_name_read_restrictions();

	if (_SC_isAppleInternal()) {
		install_test_restrictions();
	}
	return;
}


__private_extern__
void
_storeAddWatcher(CFNumberRef sessionNum, CFStringRef watchedKey)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	CFArrayRef		watchers;
	CFMutableArrayRef	newWatchers;
	CFArrayRef		watcherRefs;
	CFMutableArrayRef	newWatcherRefs;
	CFIndex			i;
	int			refCnt;
	CFNumberRef		refNum;

	/*
	 * Get the dictionary associated with this key out of the store
	 */
	dict = CFDictionaryGetValue(storeData, watchedKey);
	if (dict != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	/*
	 * Get the set of watchers out of the keys dictionary
	 */
	watchers    = CFDictionaryGetValue(newDict, kSCDWatchers);
	watcherRefs = CFDictionaryGetValue(newDict, kSCDWatcherRefs);
	if (watchers) {
		newWatchers    = CFArrayCreateMutableCopy(NULL, 0, watchers);
		newWatcherRefs = CFArrayCreateMutableCopy(NULL, 0, watcherRefs);
	} else {
		newWatchers    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		newWatcherRefs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	/*
	 * Add my session to the set of watchers
	 */
	i = CFArrayGetFirstIndexOfValue(newWatchers,
					CFRangeMake(0, CFArrayGetCount(newWatchers)),
					sessionNum);
	if (i == kCFNotFound) {
		/* if this is the first instance of this session watching this key */
		CFArrayAppendValue(newWatchers, sessionNum);
		refCnt = 1;
		refNum = CFNumberCreate(NULL, kCFNumberIntType, &refCnt);
		CFArrayAppendValue(newWatcherRefs, refNum);
		CFRelease(refNum);
	} else {
		/* if this is another instance of this session watching this key */
		refNum = CFArrayGetValueAtIndex(newWatcherRefs, i);
		CFNumberGetValue(refNum, kCFNumberIntType, &refCnt);
		refCnt++;
		refNum = CFNumberCreate(NULL, kCFNumberIntType, &refCnt);
		CFArraySetValueAtIndex(newWatcherRefs, i, refNum);
		CFRelease(refNum);
	}

	/*
	 * Update the keys dictionary
	 */
	CFDictionarySetValue(newDict, kSCDWatchers, newWatchers);
	CFRelease(newWatchers);
	CFDictionarySetValue(newDict, kSCDWatcherRefs, newWatcherRefs);
	CFRelease(newWatcherRefs);

	/*
	 * Update the store for this key
	 */
	CFDictionarySetValue(storeData, watchedKey, newDict);
	CFRelease(newDict);

#ifdef	DEBUG
	SC_log(LOG_DEBUG, "  _storeAddWatcher: %@, %@", sessionNum, watchedKey);
#endif	/* DEBUG */

	return;
}


__private_extern__
void
_storeRemoveWatcher(CFNumberRef sessionNum, CFStringRef watchedKey)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	CFArrayRef		watchers;
	CFMutableArrayRef	newWatchers;
	CFArrayRef		watcherRefs;
	CFMutableArrayRef	newWatcherRefs;
	CFIndex			i;
	int			refCnt;
	CFNumberRef		refNum;

	/*
	 * Get the dictionary associated with this key out of the store
	 */
	dict = CFDictionaryGetValue(storeData, watchedKey);
	if ((dict == NULL) || !CFDictionaryContainsKey(dict, kSCDWatchers)) {
		/* key doesn't exist (isn't this really fatal?) */
#ifdef	DEBUG
		SC_log(LOG_DEBUG, "  _storeRemoveWatcher: %@, %@, key not watched", sessionNum, watchedKey);
#endif	/* DEBUG */
		return;
	}
	newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);

	/*
	 * Get the set of watchers out of the keys dictionary and
	 * remove this session from the list.
	 */
	watchers       = CFDictionaryGetValue(newDict, kSCDWatchers);
	newWatchers    = CFArrayCreateMutableCopy(NULL, 0, watchers);

	watcherRefs    = CFDictionaryGetValue(newDict, kSCDWatcherRefs);
	newWatcherRefs = CFArrayCreateMutableCopy(NULL, 0, watcherRefs);

	/* locate the session reference */
	i = CFArrayGetFirstIndexOfValue(newWatchers,
					CFRangeMake(0, CFArrayGetCount(newWatchers)),
					sessionNum);
	if (i == kCFNotFound) {
#ifdef	DEBUG
		SC_log(LOG_DEBUG, "  _storeRemoveWatcher: %@, %@, session not watching", sessionNum, watchedKey);
#endif	/* DEBUG */
		CFRelease(newDict);
		CFRelease(newWatchers);
		CFRelease(newWatcherRefs);
		return;
	}

	/* remove one session reference */
	refNum = CFArrayGetValueAtIndex(newWatcherRefs, i);
	CFNumberGetValue(refNum, kCFNumberIntType, &refCnt);
	if (--refCnt > 0) {
		refNum = CFNumberCreate(NULL, kCFNumberIntType, &refCnt);
		CFArraySetValueAtIndex(newWatcherRefs, i, refNum);
		CFRelease(refNum);
	} else {
		/* if this was the last reference */
		CFArrayRemoveValueAtIndex(newWatchers, i);
		CFArrayRemoveValueAtIndex(newWatcherRefs, i);
	}

	if (CFArrayGetCount(newWatchers) > 0) {
		/* if this key is still being "watched" */
		CFDictionarySetValue(newDict, kSCDWatchers, newWatchers);
		CFDictionarySetValue(newDict, kSCDWatcherRefs, newWatcherRefs);
	} else {
		/* no watchers left, remove the empty set */
		CFDictionaryRemoveValue(newDict, kSCDWatchers);
		CFDictionaryRemoveValue(newDict, kSCDWatcherRefs);
	}
	CFRelease(newWatchers);
	CFRelease(newWatcherRefs);

	if (CFDictionaryGetCount(newDict) > 0) {
		/* if this key is still active */
		CFDictionarySetValue(storeData, watchedKey, newDict);
	} else {
		/* no information left, remove the empty dictionary */
		CFDictionaryRemoveValue(storeData, watchedKey);
	}
	CFRelease(newDict);

#ifdef	DEBUG
	SC_log(LOG_DEBUG, "  _storeRemoveWatcher: %@, %@", sessionNum, watchedKey);
#endif	/* DEBUG */

	return;
}


__private_extern__
CFDictionaryRef
_storeKeyGetAccessControls(CFStringRef key)
{
	CFDictionaryRef		controls	= NULL;
	CFDictionaryRef		dict		= NULL;

	if (CFDictionaryGetValueIfPresent(storeData, key, (const void **)&dict) &&
	    isA_CFDictionary(dict) &&
	    CFDictionaryGetValueIfPresent(dict, kSCDAccessControls, (const void **)&controls) &&
	    isA_CFDictionary(controls)) {
		return controls;
	}

	return NULL;
}


__private_extern__
void
_storeKeySetAccessControls(CFStringRef key, CFDictionaryRef controls)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;

	// get the dictionary associated with this key out of the store
	dict = CFDictionaryGetValue(storeData, key);
	if (dict != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	// save "controls" associated with this key
	CFDictionarySetValue(newDict, kSCDAccessControls, controls);

	// update the store for this key
	CFDictionarySetValue(storeData, key, newDict);
	CFRelease(newDict);

	return;
}


#define N_QUICK	64


__private_extern__
void
pushNotifications(void)
{
	CFIndex				notifyCnt;
	int				server;
	const void *			sessionsToNotify_q[N_QUICK];
	const void **			sessionsToNotify	= sessionsToNotify_q;
	SCDynamicStorePrivateRef	storePrivate;
	serverSessionRef		theSession;

	if (needsNotification == NULL)
		return;		/* if no sessions need to be kicked */

	notifyCnt = CFSetGetCount(needsNotification);
	if (notifyCnt > (CFIndex)(sizeof(sessionsToNotify_q) / sizeof(CFNumberRef)))
		sessionsToNotify = CFAllocatorAllocate(NULL, notifyCnt * sizeof(CFNumberRef), 0);
	CFSetGetValues(needsNotification, sessionsToNotify);
	while (--notifyCnt >= 0) {
		(void) CFNumberGetValue(sessionsToNotify[notifyCnt],
					kCFNumberIntType,
					&server);
		theSession = getSession(server);
		assert(theSession != NULL);

		storePrivate = (SCDynamicStorePrivateRef)theSession->store;

		/*
		 * deliver [CFRunLoop/dispatch] notifications to client sessions
		 */
		if ((storePrivate->notifyStatus == Using_NotifierInformViaMachPort) &&
		    (storePrivate->notifyPort != MACH_PORT_NULL)) {
			/*
			 * Post notification as mach message
			 */
			SC_trace("-->port : %5u : port = %u",
				 storePrivate->server,
				 storePrivate->notifyPort);

			/* use a random (and non-zero) identifier */
			while (storePrivate->notifyPortIdentifier == 0) {
				storePrivate->notifyPortIdentifier = (mach_msg_id_t)random();
			}

			_SC_sendMachMessage(storePrivate->notifyPort, storePrivate->notifyPortIdentifier);
		}

		if ((storePrivate->notifyStatus == Using_NotifierInformViaFD) &&
		    (storePrivate->notifyFile >= 0)) {
			ssize_t		written;

			SC_trace("-->fd   : %5u : fd = %d, msgid = %d",
				 storePrivate->server,
				 storePrivate->notifyFile,
				 storePrivate->notifyFileIdentifier);

			written = write(storePrivate->notifyFile,
					&storePrivate->notifyFileIdentifier,
					sizeof(storePrivate->notifyFileIdentifier));
			if (written == -1) {
				if (errno == EWOULDBLOCK) {
#ifdef	DEBUG
					SC_log(LOG_DEBUG, "sorry, only one outstanding notification per session");
#endif	/* DEBUG */
				} else {
#ifdef	DEBUG
					SC_log(LOG_DEBUG, "could not send notification, write() failed: %s",
					      strerror(errno));
#endif	/* DEBUG */
					storePrivate->notifyFile = -1;
				}
			} else if (written != sizeof(storePrivate->notifyFileIdentifier)) {
#ifdef	DEBUG
				SC_log(LOG_DEBUG, "could not send notification, incomplete write()");
#endif	/* DEBUG */
				storePrivate->notifyFile = -1;
			}
		}
	}
	if (sessionsToNotify != sessionsToNotify_q) CFAllocatorDeallocate(NULL, sessionsToNotify);

	/*
	 * this list of notifications have been posted, wait for some more.
	 */
	CFRelease(needsNotification);
	needsNotification = NULL;

	return;
}
