/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 * June 24, 2001		Allan Nathanson <ajn@apple.com>
 * - update to public SystemConfiguration.framework APIs
 *
 * November 10, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>


#define USE_FLAT_FILES	"UseFlatFiles"

SCDynamicStoreRef	store		= NULL;
CFRunLoopSourceRef	rls		= NULL;

CFMutableDictionaryRef	currentPrefs;		/* current prefs */
CFMutableDictionaryRef	newPrefs;		/* new prefs */
CFMutableArrayRef	unchangedPrefsKeys;	/* new prefs keys which match current */
CFMutableArrayRef	removedPrefsKeys;	/* old prefs keys to be removed */

Boolean			_verbose	= FALSE;


static void
updateCache(const void *key, const void *value, void *context)
{
	CFStringRef		configKey	= (CFStringRef)key;
	CFPropertyListRef	configData	= (CFPropertyListRef)value;
	CFPropertyListRef	cacheData;
	CFIndex			i;

	cacheData = CFDictionaryGetValue(currentPrefs, configKey);
	if (cacheData) {
		/* key exists */
		if (CFEqual(cacheData, configData)) {
			/*
			 * if the old & new property list values have
			 * not changed then we don't need to update
			 * the preference.
			 */
			CFArrayAppendValue(unchangedPrefsKeys, configKey);
		}
	}

	/* in any case, this key should not be removed */
	i = CFArrayGetFirstIndexOfValue(removedPrefsKeys,
					CFRangeMake(0, CFArrayGetCount(removedPrefsKeys)),
					configKey);
	if (i != kCFNotFound) {
		CFArrayRemoveValueAtIndex(removedPrefsKeys, i);
	}

	return;
}


static void
flatten(SCPreferencesRef	pSession,
	CFStringRef		key,
	CFDictionaryRef		base)
{
	CFDictionaryRef		subset;
	CFStringRef		link;
	CFMutableDictionaryRef	myDict;
	CFStringRef		myKey;
	CFIndex			i;
	CFIndex			nKeys;
	const void		**keys;
	const void		**vals;

	if (!CFDictionaryGetValueIfPresent(base, kSCResvLink, (const void **)&link)) {
		/* if this dictionary is not linked */
		subset = base;
	} else {
		/* if __LINK__ key is present */
		subset = SCPreferencesPathGetValue(pSession, link);
		if (!subset) {
			/* if error with link */
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCPreferencesPathGetValue(,%@,) failed: %s"),
			      link,
			      SCErrorString(SCError()));
			return;
		}
	}

	if (CFDictionaryContainsKey(subset, kSCResvInactive)) {
		/* if __INACTIVE__ key is present */
		return;
	}

	myKey = CFStringCreateWithFormat(NULL,
					 NULL,
					 CFSTR("%@%@"),
					 kSCDynamicStoreDomainSetup,
					 key);

	myDict = (CFMutableDictionaryRef)CFDictionaryGetValue(newPrefs, myKey);
	if (myDict) {
		myDict = CFDictionaryCreateMutableCopy(NULL,
						       0,
						       (CFDictionaryRef)myDict);
	} else {
		myDict = CFDictionaryCreateMutable(NULL,
						   0,
						   &kCFTypeDictionaryKeyCallBacks,
						   &kCFTypeDictionaryValueCallBacks);
	}

	nKeys = CFDictionaryGetCount(subset);
	if (nKeys > 0) {
		keys  = CFAllocatorAllocate(NULL, nKeys * sizeof(CFStringRef)      , 0);
		vals  = CFAllocatorAllocate(NULL, nKeys * sizeof(CFPropertyListRef), 0);
		CFDictionaryGetKeysAndValues(subset, keys, vals);
		for (i=0; i<nKeys; i++) {
			if (CFGetTypeID((CFTypeRef)vals[i]) != CFDictionaryGetTypeID()) {
				/* add this key/value to the current dictionary */
				CFDictionarySetValue(myDict, keys[i], vals[i]);
			} else {
				CFStringRef	subKey;

				/* flatten [sub]dictionaries */
				subKey = CFStringCreateWithFormat(NULL,
								  NULL,
								  CFSTR("%@%s%@"),
								  key,
								  CFEqual(key, CFSTR("/")) ? "" : "/",
								  keys[i]);
				flatten(pSession, subKey, vals[i]);
				CFRelease(subKey);
			}
		}
		CFAllocatorDeallocate(NULL, keys);
		CFAllocatorDeallocate(NULL, vals);
	}

	if (CFDictionaryGetCount(myDict) > 0) {
		/* add this dictionary to the new preferences */
		CFDictionarySetValue(newPrefs, myKey, myDict);
	}

	CFRelease(myDict);
	CFRelease(myKey);

	return;
}


static void
updateConfiguration(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
	CFStringRef		current		= NULL;
	CFDateRef		date		= NULL;
	CFMutableDictionaryRef	dict		= NULL;
	CFDictionaryRef		global		= NULL;
	CFIndex			i;
	Boolean			noPrefs		= TRUE;
	CFStringRef		pattern;
	CFMutableArrayRef	patterns;
	SCPreferencesRef	pSession	= NULL;
	CFDictionaryRef		set		= NULL;

	SCLog(_verbose, LOG_DEBUG, CFSTR("updating configuration"));

	/*
	 * initialize old preferences, new preferences, an array
	 * of keys which have not changed, and an array of keys
	 * to be removed (cleaned up).
	 */

	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	pattern  = CFStringCreateWithFormat(NULL,
					    NULL,
					    CFSTR("^%@.*"),
					    kSCDynamicStoreDomainSetup);
	CFArrayAppendValue(patterns, pattern);
	dict = (CFMutableDictionaryRef)SCDynamicStoreCopyMultiple(store, NULL, patterns);
	CFRelease(patterns);
	CFRelease(pattern);
	if (dict) {
		currentPrefs = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFRelease(dict);
	} else {
		currentPrefs = CFDictionaryCreateMutable(NULL,
							 0,
							 &kCFTypeDictionaryKeyCallBacks,
							 &kCFTypeDictionaryValueCallBacks);
	}

	unchangedPrefsKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	i = CFDictionaryGetCount(currentPrefs);
	if (i > 0) {
		const void	**keys;
		CFArrayRef	array;

		keys = CFAllocatorAllocate(NULL, i * sizeof(CFStringRef), 0);
		CFDictionaryGetKeysAndValues(currentPrefs, keys, NULL);
		array = CFArrayCreate(NULL, keys, i, &kCFTypeArrayCallBacks);
		removedPrefsKeys = CFArrayCreateMutableCopy(NULL, 0, array);
		CFRelease(array);
		CFAllocatorDeallocate(NULL, keys);
	} else {
		removedPrefsKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	/*
	 * The "newPrefs" dictionary will contain the new / updated
	 * configuration which will be written to the configuration cache.
	 */
	newPrefs = CFDictionaryCreateMutable(NULL,
						 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);

	/*
	 * create status dictionary associated with current configuration
	 * information including:
	 *   - current set "name" to cache
	 *   - time stamp indicating when the cache preferences were
	 *     last updated.
	 */
	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	date = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());

	/*
	 * load preferences
	 */
	pSession = SCPreferencesCreate(NULL, CFSTR("PreferencesMonitor.bundle"), NULL);
	if (pSession) {
		CFArrayRef	prefKeys = SCPreferencesCopyKeyList(pSession);

		if (prefKeys) {
			if (CFArrayGetCount(prefKeys) > 0) {
				noPrefs = FALSE;
			}
			CFRelease(prefKeys);
		}
	}
	if (noPrefs == TRUE) {
		/* no preferences, indicate that we should use flat files */
		CFStringRef	key;

		SCLog(TRUE, LOG_NOTICE, CFSTR("updateConfiguration(): no preferences."));

		key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@" USE_FLAT_FILES), kSCDynamicStoreDomainSetup);
		CFDictionarySetValue(newPrefs, key, date);
		CFRelease(key);

		/* leave any Setup: cache keys */
		CFArrayRemoveAllValues(removedPrefsKeys);

		goto done;
	}

	/*
	 * get "global" system preferences
	 */
	(CFPropertyListRef)global = SCPreferencesGetValue(pSession, kSCPrefSystem);
	if (!global) {
		/* if no global preferences are defined */
		goto getSet;
	}

	if (!isA_CFDictionary(global)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("updateConfiguration(): %@ is not a dictionary."),
		      kSCPrefSystem);
		goto done;
	}

	/* flatten property list */
	flatten(pSession, CFSTR("/"), global);

    getSet :

	/*
	 * get current set name
	 */
	(CFPropertyListRef)current = SCPreferencesGetValue(pSession, kSCPrefCurrentSet);
	if (!current) {
		/* if current set not defined */
		goto done;
	}

	if (!isA_CFString(current)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("updateConfiguration(): %@ is not a string."),
		      kSCPrefCurrentSet);
		goto done;
	}

	/*
	 * get current set
	 */
	(CFPropertyListRef)set = SCPreferencesPathGetValue(pSession, current);
	if (!set) {
		/* if error with path */
		SCLog(TRUE, LOG_ERR,
		      CFSTR("%@ value (%@) not valid"),
		      kSCPrefCurrentSet,
		      current);
		goto done;
	}

	if (!isA_CFDictionary(set)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("updateConfiguration(): %@ is not a dictionary."),
		      current);
		goto done;
	}

	/* flatten property list */
	flatten(pSession, CFSTR("/"), set);

	CFDictionarySetValue(dict, kSCDynamicStorePropSetupCurrentSet, current);

    done :

	/* add last updated time stamp */
	CFDictionarySetValue(dict, kSCDynamicStorePropSetupLastUpdated, date);

	/* add Setup: key */
	CFDictionarySetValue(newPrefs, kSCDynamicStoreDomainSetup, dict);

	/* compare current and new preferences */
	CFDictionaryApplyFunction(newPrefs, updateCache, NULL);

	/* remove those keys which have not changed from the update */
	for (i=0; i<CFArrayGetCount(unchangedPrefsKeys); i++) {
		CFStringRef	key;

		key = CFArrayGetValueAtIndex(unchangedPrefsKeys, i);
		CFDictionaryRemoveValue(newPrefs, key);
	}

	/* Update the dynamic store */
	if (!SCDynamicStoreSetMultiple(store, newPrefs, removedPrefsKeys, NULL)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreSetMultiple() failed: %s"),
		      SCErrorString(SCError()));
	}

	CFRelease(currentPrefs);
	CFRelease(newPrefs);
	CFRelease(unchangedPrefsKeys);
	CFRelease(removedPrefsKeys);
	if (dict)	CFRelease(dict);
	if (date)	CFRelease(date);
	if (pSession)	CFRelease(pSession);
	return;
}


void
prime()
{
	SCLog(_verbose, LOG_DEBUG, CFSTR("prime() called"));

	/* load the initial configuration from the database */
	updateConfiguration(store, NULL, NULL);

	return;
}


void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
	CFStringRef		key;
	CFMutableArrayRef	keys;
	Boolean			ok;

	if (bundleVerbose) {
		_verbose = TRUE;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* open a "configd" session to allow cache updates */
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("Configuraton Preferences Monitor plug-in"),
				     updateConfiguration,
				     NULL);
	if (!store) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreCreate() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	/*
	 * register for change notifications.
	 */
	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	key  = SCDynamicStoreKeyCreatePreferences(NULL, NULL, kSCPreferencesKeyApply);
	CFArrayAppendValue(keys, key);
	CFRelease(key);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreSetNotificationKeys() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!rls) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	return;

    error :

	if (store)	CFRelease(store);

	return;
}


#ifdef  MAIN
int
main(int argc, char **argv)
{
	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	prime();
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
