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
 * June 2, 2000			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "configd.h"


CFMutableDictionaryRef	sessionData		= NULL;

CFMutableDictionaryRef	storeData		= NULL;
CFMutableDictionaryRef	storeData_s		= NULL;

CFMutableSetRef		changedKeys		= NULL;
CFMutableSetRef		changedKeys_s		= NULL;

CFMutableSetRef		deferredRemovals	= NULL;
CFMutableSetRef		deferredRemovals_s	= NULL;

CFMutableSetRef		removedSessionKeys	= NULL;
CFMutableSetRef		removedSessionKeys_s	= NULL;

CFMutableSetRef		needsNotification	= NULL;

int			storeLocked		= 0;		/* > 0 if dynamic store locked */


void
_swapLockedStoreData()
{
	void	*temp;

	temp                 = storeData;
	storeData            = storeData_s;
	storeData_s          = temp;

	temp                 = changedKeys;
	changedKeys          = changedKeys_s;
	changedKeys_s        = temp;

	temp                 = deferredRemovals;
	deferredRemovals     = deferredRemovals_s;
	deferredRemovals_s   = temp;

	temp                 = removedSessionKeys;
	removedSessionKeys   = removedSessionKeys_s;
	removedSessionKeys_s = temp;

	return;
}


void
_addWatcher(CFNumberRef sessionNum, CFStringRef watchedKey)
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
	if (dict) {
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
	if (i == -1) {
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

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  _addWatcher: %@, %@"), sessionNum, watchedKey);

	return;
}


/*
 * _addRegexWatcherByKey()
 *
 * This is a CFDictionaryApplierFunction which will iterate over each key
 * defined in the "storeData" dictionary. The arguments are the dictionary
 * key, it's associated store dictionary, and a context structure which
 * includes the following:
 *
 *   1. the session which has just added a regex notification request
 *   2. the compiled regular expression associated with the above key.
 *
 * If a (real) dictionary key is found which matches the provided regular
 * expression then we mark that key as being watched by the session.
 */
void
_addRegexWatcherByKey(const void *key, void *val, void *context)
{
	CFStringRef	storeStr  = key;
	CFDictionaryRef	info      = val;
	mach_port_t	sessionID = ((addContextRef)context)->store->server;
	regex_t		*preg     = ((addContextRef)context)->preg;
	int		storeKeyLen;
	char		*storeKey;
	CFNumberRef	sessionNum;
	int		reError;
	char		reErrBuf[256];
	int		reErrStrLen;

	if (CFDictionaryContainsKey(info, kSCDData) == FALSE) {
		/* if no data (yet) */
		return;
	}

	/* convert store key to C string */
	storeKeyLen = CFStringGetLength(storeStr) + 1;
	storeKey    = CFAllocatorAllocate(NULL, storeKeyLen, 0);
	if (!CFStringGetCString(storeStr, storeKey, storeKeyLen, kCFStringEncodingMacRoman)) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("CFStringGetCString: could not convert store key to C string"));
		CFAllocatorDeallocate(NULL, storeKey);
		return;
	}

	/* compare store key to new notification keys regular expression pattern */
	reError = regexec(preg, storeKey, 0, NULL, 0);
	switch (reError) {
		case 0 :
			/* we've got a match */
			sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &sessionID);
			_addWatcher(sessionNum, storeStr);
			CFRelease(sessionNum);
			break;
		case REG_NOMATCH :
			/* no match */
			break;
		default :
			reErrStrLen = regerror(reError, preg, reErrBuf, sizeof(reErrBuf));
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("regexec(): %s"), reErrBuf);
			break;
	}
	CFAllocatorDeallocate(NULL, storeKey);
}


/*
 * _addRegexWatchersBySession()
 *
 * This is a CFDictionaryApplierFunction which will iterate over each session
 * defined in the "sessionData" dictionary. The arguments are the session
 * key, it's associated session dictionary, , and the store key being added.
 *
 * If an active session includes any regular expression keys which match the
 * key being added to the "storeData" dictionary then we mark this key as being
 * watched by the session.
 */
void
_addRegexWatchersBySession(const void *key, void *val, void *context)
{
	CFStringRef	sessionKey = key;
	CFDictionaryRef	info       = val;
	CFStringRef	addedKey   = context;
	CFIndex		newKeyLen;
	char		*newKeyStr;
	CFArrayRef	rKeys;
	CFArrayRef	rData;
	CFIndex		i;

	if (info == NULL) {
		/* if no dictionary for this session */
		return;
	}

	rKeys = CFDictionaryGetValue(info, kSCDRegexKeys);
	if (rKeys == NULL) {
		/* if no regex keys for this session */
		return;
	}
	rData = CFDictionaryGetValue(info, kSCDRegexData);

	/* convert new key to C string */
	newKeyLen = CFStringGetLength(addedKey) + 1;
	newKeyStr = CFAllocatorAllocate(NULL, newKeyLen, 0);
	if (!CFStringGetCString(addedKey, newKeyStr, newKeyLen, kCFStringEncodingMacRoman)) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("CFStringGetCString: could not convert new key to C string"));
		CFAllocatorDeallocate(NULL, newKeyStr);
		return;
	}

	/* iterate over the regex keys looking for an pattern which matches the new key */
	for (i=0; i<CFArrayGetCount(rKeys); i++) {
		CFDataRef	regexData = CFArrayGetValueAtIndex(rData, i);
		regex_t		*preg     = (regex_t *)CFDataGetBytePtr(regexData);
		int		reError;
		char		reErrBuf[256];
		int		reErrStrLen;
		SInt32		sessionInt;
		CFNumberRef	sessionNum;

		/* check if this key matches the regular expression */
		reError = regexec(preg, newKeyStr, 0, NULL, 0);
		switch (reError) {
			case 0 :
				/* we've got a match, add a reference */
				sessionInt = CFStringGetIntValue(sessionKey);
				sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &sessionInt);
				_addWatcher(sessionNum, addedKey);
				CFRelease(sessionNum);
				break;
			case REG_NOMATCH :
				/* no match */
				break;
			default :
				reErrStrLen = regerror(reError, preg, reErrBuf, sizeof(reErrBuf));
				SCLog(_configd_verbose, LOG_DEBUG, CFSTR("regexec(): %s"), reErrBuf);
				break;
		}

	}
	CFAllocatorDeallocate(NULL, newKeyStr);

	return;
}


void
_removeWatcher(CFNumberRef sessionNum, CFStringRef watchedKey)
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
	if ((dict == NULL) || (CFDictionaryContainsKey(dict, kSCDWatchers) == FALSE)) {
		/* key doesn't exist (isn't this really fatal?) */
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  _removeWatcher: %@, %@, key not watched"), sessionNum, watchedKey);
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
	if (i == -1) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  _removeWatcher: %@, %@, session not watching"), sessionNum, watchedKey);
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

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  _removeWatcher: %@, %@"), sessionNum, watchedKey);

	return;
}


/*
 * _removeRegexWatcherByKey()
 *
 * This is a CFDictionaryApplierFunction which will iterate over each key
 * defined in the "storeData" dictionary. The arguments are the dictionary
 * key, it's associated store dictionary, and a context structure which
 * includes the following:
 *
 *   1. the session which has just removed a regex notification request
 *   2. the compiled regular expression associated with the above key.
 *
 * If a key is found and it matches the provided regular expression then
 * it will its "being watched" status will be cleared.
 */
void
_removeRegexWatcherByKey(const void *key, void *val, void *context)
{
	CFStringRef	storeStr  = key;
	CFDictionaryRef	info      = val;
	mach_port_t	sessionID = ((removeContextRef)context)->store->server;
	regex_t		*preg     = ((removeContextRef)context)->preg;
	CFNumberRef	sessionNum;
	CFArrayRef	watchers;
	int		storeKeyLen;
	char		*storeKey;
	int		reError;
	char		reErrBuf[256];
	int		reErrStrLen;

	if ((info == NULL) || (CFDictionaryContainsKey(info, kSCDWatchers) == FALSE)) {
		/* no dictionary or no watchers */
		return;
	}

	sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &sessionID);

	watchers = CFDictionaryGetValue(info, kSCDWatchers);
	if (CFArrayContainsValue(watchers,
				 CFRangeMake(0, CFArrayGetCount(watchers)),
				 sessionNum) == FALSE) {
		/* this session is not watching this key */
		CFRelease(sessionNum);
		return;
	}

	/* convert key to C string */
	storeKeyLen = CFStringGetLength(storeStr) + 1;
	storeKey    = CFAllocatorAllocate(NULL, storeKeyLen, 0);
	if (!CFStringGetCString(storeStr, storeKey, storeKeyLen, kCFStringEncodingMacRoman)) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("CFStringGetCString: could not convert key to C string"));
		CFAllocatorDeallocate(NULL, storeKey);
		CFRelease(sessionNum);
		return;
	}

	/* check if this key matches the regular expression */
	reError = regexec(preg, storeKey, 0, NULL, 0);
	switch (reError) {
		case 0 :
			/* we've got a match */
			_removeWatcher(sessionNum, storeStr);
			break;
		case REG_NOMATCH :
			/* no match */
			break;
		default :
			reErrStrLen = regerror(reError, preg, reErrBuf, sizeof(reErrBuf));
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("regexec(): %s"), reErrBuf);
			break;
	}
	CFAllocatorDeallocate(NULL, storeKey);
	CFRelease(sessionNum);
}


/*
 * _removeRegexWatchersBySession()
 *
 * This is a CFDictionaryApplierFunction which will iterate over each session
 * defined in the "sessionData" dictionary. The arguments are the session
 * key, it's associated session dictionary, and the store key being removed.
 *
 * If an active session includes any regular expression keys which match the
 * key being removed from the "storeData" dictionary then we clear this keys
 * reference of being watched.
 */
void
_removeRegexWatchersBySession(const void *key, void *val, void *context)
{
	CFStringRef	sessionKey = key;
	CFDictionaryRef	info       = val;
	CFStringRef	removedKey = context;
	CFIndex		oldKeyLen;
	char		*oldKeyStr;
	CFArrayRef	rKeys;
	CFArrayRef	rData;
	CFIndex		i;

	if (info == NULL) {
		/* if no dictionary for this session */
		return;
	}

	rKeys = CFDictionaryGetValue(info, kSCDRegexKeys);
	if (rKeys == NULL) {
		/* if no regex keys for this session */
		return;
	}
	rData = CFDictionaryGetValue(info, kSCDRegexData);

	/* convert new key to C string */
	oldKeyLen = CFStringGetLength(removedKey) + 1;
	oldKeyStr = CFAllocatorAllocate(NULL, oldKeyLen, 0);
	if (!CFStringGetCString(removedKey, oldKeyStr, oldKeyLen, kCFStringEncodingMacRoman)) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("CFStringGetCString: could not convert old key to C string"));
		CFAllocatorDeallocate(NULL, oldKeyStr);
		return;
	}

	/* iterate over the regex keys looking for an pattern which matches the old key */
	for (i=0; i<CFArrayGetCount(rKeys); i++) {
		CFDataRef	regexData = CFArrayGetValueAtIndex(rData, i);
		regex_t		*preg     = (regex_t *)CFDataGetBytePtr(regexData);
		int		reError;
		char		reErrBuf[256];
		int		reErrStrLen;
		SInt32		sessionInt;
		CFNumberRef	sessionNum;

		/* check if this key matches the regular expression */
		reError = regexec(preg, oldKeyStr, 0, NULL, 0);
		switch (reError) {
			case 0 :
				/* we've got a match, remove a reference */
				sessionInt = CFStringGetIntValue(sessionKey);
				sessionNum = CFNumberCreate(NULL, kCFNumberIntType, &sessionInt);
				_removeWatcher(sessionNum, removedKey);
				CFRelease(sessionNum);
				break;
			case REG_NOMATCH :
				/* no match */
				break;
			default :
				reErrStrLen = regerror(reError, preg, reErrBuf, sizeof(reErrBuf));
				SCLog(_configd_verbose, LOG_DEBUG, CFSTR("regexec(): %s"), reErrBuf);
				break;
		}

	}
	CFAllocatorDeallocate(NULL, oldKeyStr);

	return;
}
