/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
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
 * April 2, 2004		Allan Nathanson <ajn@apple.com>
 * - use SCPreference notification APIs
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


static SCPreferencesRef		prefs		= NULL;
static SCDynamicStoreRef	store		= NULL;

static CFMutableDictionaryRef	currentPrefs;		/* current prefs */
static CFMutableDictionaryRef	newPrefs;		/* new prefs */
static CFMutableArrayRef	unchangedPrefsKeys;	/* new prefs keys which match current */
static CFMutableArrayRef	removedPrefsKeys;	/* old prefs keys to be removed */

static Boolean			_verbose	= FALSE;


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
flatten(SCPreferencesRef	prefs,
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
		subset = SCPreferencesPathGetValue(prefs, link);
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
		for (i = 0; i < nKeys; i++) {
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
				flatten(prefs, subKey, vals[i]);
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
updateConfiguration(SCPreferencesRef		prefs,
		    SCPreferencesNotification   notificationType,
		    void			*info)
{
	CFStringRef		current		= NULL;
	CFDateRef		date		= NULL;
	CFMutableDictionaryRef	dict		= NULL;
	CFDictionaryRef		global		= NULL;
	CFIndex			i;
	CFArrayRef		keys;
	CFIndex			n;
	CFStringRef		pattern;
	CFMutableArrayRef	patterns;
	CFDictionaryRef		set		= NULL;

	if ((notificationType & kSCPreferencesNotificationApply) != kSCPreferencesNotificationApply) {
		return;
	}

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
		const void	**currentKeys;
		CFArrayRef	array;

		currentKeys = CFAllocatorAllocate(NULL, i * sizeof(CFStringRef), 0);
		CFDictionaryGetKeysAndValues(currentPrefs, currentKeys, NULL);
		array = CFArrayCreate(NULL, currentKeys, i, &kCFTypeArrayCallBacks);
		removedPrefsKeys = CFArrayCreateMutableCopy(NULL, 0, array);
		CFRelease(array);
		CFAllocatorDeallocate(NULL, currentKeys);
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
	keys = SCPreferencesCopyKeyList(prefs);
	if ((keys == NULL) || (CFArrayGetCount(keys) == 0)) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("updateConfiguration(): no preferences."));
		goto done;
	}

	/*
	 * get "global" system preferences
	 */
	global = SCPreferencesGetValue(prefs, kSCPrefSystem);
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
	flatten(prefs, CFSTR("/"), global);

    getSet :

	/*
	 * get current set name
	 */
	current = SCPreferencesGetValue(prefs, kSCPrefCurrentSet);
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
	set = SCPreferencesPathGetValue(prefs, current);
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
	flatten(prefs, CFSTR("/"), set);

	CFDictionarySetValue(dict, kSCDynamicStorePropSetupCurrentSet, current);

    done :

	/* add last updated time stamp */
	CFDictionarySetValue(dict, kSCDynamicStorePropSetupLastUpdated, date);

	/* add Setup: key */
	CFDictionarySetValue(newPrefs, kSCDynamicStoreDomainSetup, dict);

	/* compare current and new preferences */
	CFDictionaryApplyFunction(newPrefs, updateCache, NULL);

	/* remove those keys which have not changed from the update */
	n = CFArrayGetCount(unchangedPrefsKeys);
	for (i = 0; i < n; i++) {
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

	/* finished with current prefs, wait for changes */
	SCPreferencesSynchronize(prefs);

	CFRelease(currentPrefs);
	CFRelease(newPrefs);
	CFRelease(unchangedPrefsKeys);
	CFRelease(removedPrefsKeys);
	if (dict)	CFRelease(dict);
	if (date)	CFRelease(date);
	if (keys)	CFRelease(keys);
	return;
}


__private_extern__
void
stop_PreferencesMonitor(CFRunLoopSourceRef stopRls)
{
	// cleanup

	if (prefs != NULL) {
		if (!SCPreferencesUnscheduleFromRunLoop(prefs,
							CFRunLoopGetCurrent(),
							kCFRunLoopDefaultMode)) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCPreferencesUnscheduleFromRunLoop() failed: %s"),
			      SCErrorString(SCError()));
		}
		CFRelease(prefs);
		prefs = NULL;
	}

	if (store != NULL) {
		CFRelease(store);
		store = NULL;
	}

	CFRunLoopSourceSignal(stopRls);
	return;
}


__private_extern__
void
prime_PreferencesMonitor()
{
	SCLog(_verbose, LOG_DEBUG, CFSTR("prime() called"));

	/* load the initial configuration from the database */
	updateConfiguration(prefs, kSCPreferencesNotificationApply, (void *)store);

	return;
}


__private_extern__
void
load_PreferencesMonitor(CFBundleRef bundle, Boolean bundleVerbose)
{
	if (bundleVerbose) {
		_verbose = TRUE;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* open a SCDynamicStore session to allow cache updates */
	store = SCDynamicStoreCreate(NULL, CFSTR("PreferencesMonitor.bundle"), NULL, NULL);
	if (store == NULL) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreCreate() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	/* open a SCPreferences session */
	prefs = SCPreferencesCreate(NULL, CFSTR("PreferencesMonitor.bundle"), NULL);
	if (prefs == NULL) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCPreferencesCreate() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	if (!SCPreferencesSetCallback(prefs, updateConfiguration, NULL)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCPreferencesSetCallBack() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	/*
	 * register for change notifications.
	 */
	if (!SCPreferencesScheduleWithRunLoop(prefs, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCPreferencesScheduleWithRunLoop() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	return;

    error :

	if (store)	CFRelease(store);
	if (prefs)	CFRelease(prefs);

	return;
}


#ifdef  MAIN
int
main(int argc, char **argv)
{
	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load_PreferencesMonitor(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	prime_PreferencesMonitor();
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
