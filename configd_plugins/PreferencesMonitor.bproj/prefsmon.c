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


#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#define USE_FLAT_FILES	"UseFlatFiles"

SCDSessionRef		session		= NULL;
CFMutableSetRef		curCacheKeys	= NULL;	/* previous cache keys */
CFMutableSetRef		newCacheKeys	= NULL;	/* cache keys which reflect current config */
CFIndex			keyCnt;


static void
updateCache(const void *key, const void *value, void *context)
{
	CFStringRef		configKey	= (CFStringRef)key;
	CFPropertyListRef	configData	= (CFDictionaryRef)value;
	SCDStatus		scd_status;
	SCDHandleRef		handle;
	CFPropertyListRef	cacheData;

	scd_status = SCDGet(session, configKey, &handle);
	switch (scd_status) {
		case SCD_OK :
			/* key exists, compare old & new dictionaries */
			cacheData = SCDHandleGetData(handle);
			if (!CFEqual(cacheData, configData)) {
				/* data has changed */
				SCDHandleSetData(handle, configData);
				scd_status = SCDSet(session, configKey, handle);
				if (scd_status != SCD_OK) {
					SCDLog(LOG_ERR, CFSTR("SCDSet(): %s"), SCDError(scd_status));
				}
			}
			SCDHandleRelease(handle);
			break;
		case SCD_NOKEY :
			/* key does not exist, this is a new interface or domain */
			handle = SCDHandleInit();
			SCDHandleSetData(handle, configData);
			scd_status = SCDAdd(session, configKey, handle);
			SCDHandleRelease(handle);
			if (scd_status != SCD_OK) {
				SCDLog(LOG_ERR, CFSTR("SCDSet(): %s"), SCDError(scd_status));
			}
			break;
		default :
			/* some other error */
			SCDLog(LOG_ERR, CFSTR("SCDGet(): %s"), SCDError(scd_status));
			break;
	}

	CFSetRemoveValue(curCacheKeys, configKey);
	CFSetAddValue   (newCacheKeys, configKey);

	return;
}


void
removeCacheKey(const void *value, void *context)
{
	SCDStatus	scd_status;
	CFStringRef	configKey	= (CFStringRef)value;

	scd_status = SCDRemove(session, configKey);
	if ((scd_status != SCD_OK) && (scd_status != SCD_NOKEY)) {
		SCDLog(LOG_ERR, CFSTR("SCDRemove() failed: %s"), SCDError(scd_status));
	}

	return;
}


static void
flatten(SCPSessionRef		pSession,
	CFStringRef		key,
	CFDictionaryRef		base,
	CFMutableDictionaryRef	newPreferences)
{
	CFDictionaryRef		subset;
	CFStringRef		link;
	CFMutableDictionaryRef	myDict;
	CFStringRef		myKey;
	CFIndex			i;
	CFIndex			nKeys;
	void			**keys;
	void			**vals;

	if (!CFDictionaryGetValueIfPresent(base, kSCResvLink, (void **)&link)) {
		/* if this dictionary is not linked */
		subset = base;
	} else {
		/* if __LINK__ key is present */
		SCPStatus	scp_status;

		scp_status = SCPPathGetValue(pSession, link, &subset);
		if (scp_status != SCP_OK) {
			/* if error with link */
			SCDLog(LOG_ERR,
			       CFSTR("SCPPathGetValue(,%@,) failed: %s"),
			       link,
			       SCPError(scp_status));
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
					 kSCCacheDomainSetup,
					 key);

	myDict = (CFMutableDictionaryRef)CFDictionaryGetValue(newPreferences, myKey);
	if (myDict == NULL) {
		myDict = CFDictionaryCreateMutable(NULL,
						   0,
						   &kCFTypeDictionaryKeyCallBacks,
						   &kCFTypeDictionaryValueCallBacks);
	} else {
		myDict = CFDictionaryCreateMutableCopy(NULL,
						       0,
						       (CFDictionaryRef)myDict);
	}

	nKeys = CFDictionaryGetCount(subset);
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
			flatten(pSession, subKey, vals[i], newPreferences);
			CFRelease(subKey);
		}
	}

	CFAllocatorDeallocate(NULL, keys);
	CFAllocatorDeallocate(NULL, vals);

	if (CFDictionaryGetCount(myDict) > 0) {
		/* add this dictionary to the new preferences */
		CFDictionarySetValue(newPreferences, myKey, myDict);
	}

	CFRelease(myDict);
	CFRelease(myKey);

	return;
}


static boolean_t
updateConfiguration(SCDSessionRef session, void *arg)
{
	CFArrayRef		changedKeys;
	boolean_t		cleanupKeys	= TRUE;
	CFStringRef		current		= NULL;
	CFArrayRef		currentKeys;
	CFDateRef		date;
	CFMutableDictionaryRef	dict;
	CFDictionaryRef		global		= NULL;
	boolean_t		haveLock	= FALSE;
	CFIndex			i;
	CFMutableDictionaryRef	newPreferences	= NULL;	/* new configuration preferences */
	SCPSessionRef		pSession	= NULL;
	SCPStatus		scp_status;
	CFDictionaryRef		set		= NULL;
	SCDStatus		scd_status;

	SCDLog(LOG_DEBUG, CFSTR("updating configuration"));

	/*
	 * Fetched the changed keys
	 */
	scd_status = SCDNotifierGetChanges(session, &changedKeys);
	if (scd_status == SCD_OK) {
		CFRelease(changedKeys);
	} else {
		SCDLog(LOG_ERR, CFSTR("SCDNotifierGetChanges() failed: %s"), SCDError(scd_status));
		/* XXX need to do something more with this FATAL error XXXX */
	}

	/*
	 * initialize old/new configuration, ensure that we clean up any
	 * existing cache keys
	 */
	curCacheKeys = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	newCacheKeys = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

	scd_status = SCDList(session, kSCCacheDomainSetup, 0, &currentKeys);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDList() failed: %s"), SCDError(scd_status));
		goto error;
	}

	for (i=0; i<CFArrayGetCount(currentKeys); i++) {
		CFSetAddValue(curCacheKeys, CFArrayGetValueAtIndex(currentKeys, i));
	}
	CFRelease(currentKeys);

	/*
	 * The "newPreferences" dictionary will contain the new / updated
	 * configuration which will be written to the configuration cache.
	 */
	newPreferences = CFDictionaryCreateMutable(NULL,
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
	scp_status = SCPOpen(&pSession, CFSTR("PreferencesMonitor.bundle"), NULL, 0);
	if (scp_status != SCP_OK) {
		/* no preferences, indicate that we should use flat files */
		CFStringRef	key;

		SCDLog(LOG_NOTICE, CFSTR("updateConfiguration(): no preferences."));

		key = SCDKeyCreate(CFSTR("%@" USE_FLAT_FILES), kSCCacheDomainSetup);
		CFDictionarySetValue(newPreferences, key, date);
		CFRelease(key);

		/* leave any Setup: cache keys */
		cleanupKeys = FALSE;

		goto done;
	}

	/* get "global" system preferences */
	scp_status = SCPGet(pSession, kSCPrefSystem, (CFPropertyListRef *)&global);
	switch (scp_status) {
		case SCP_OK :
			break;
		case SCP_NOKEY :
			/* if current set not defined */
			goto getSet;
		default :
			SCDLog(LOG_NOTICE,
			       CFSTR("SCPGet(,%@,) failed: %s"),
			       kSCPrefSystem,
			       SCPError(scp_status));
			goto error;
	}

	if ((global == NULL) || (CFGetTypeID(global) != CFDictionaryGetTypeID())) {
		SCDLog(LOG_NOTICE,
		       CFSTR("updateConfiguration(): %@ is not a dictionary."),
		       kSCPrefSystem);
		goto done;
	}

	/* flatten property list */
	flatten(pSession, CFSTR("/"), global, newPreferences);

    getSet :

	/* get current set name */
	scp_status = SCPGet(pSession, kSCPrefCurrentSet, (CFPropertyListRef *)&current);
	switch (scp_status) {
		case SCP_OK :
			break;
		case SCP_NOKEY :
			/* if current set not defined */
			goto done;
		default :
			SCDLog(LOG_NOTICE,
			       CFSTR("SCPGet(,%@,) failed: %s"),
			       kSCPrefCurrentSet,
			       SCPError(scp_status));
			goto error;
	}

	if ((current == NULL) || (CFGetTypeID(current) != CFStringGetTypeID())) {
		SCDLog(LOG_NOTICE,
		       CFSTR("updateConfiguration(): %@ is not a string."),
		       kSCPrefCurrentSet);
		goto done;
	}

	/* get current set */
	scp_status = SCPPathGetValue(pSession, current, &set);
	switch (scp_status) {
		case SCP_OK :
			break;
		case SCP_NOKEY :
			/* if error with path */
			SCDLog(LOG_ERR,
			       CFSTR("%@ value (%@) not valid"),
			       kSCPrefCurrentSet,
			       current);
			goto done;
		default :
			SCDLog(LOG_ERR,
			       CFSTR("SCPPathGetValue(,%@,) failed: %s"),
			       current,
			       SCPError(scp_status));
			goto error;
	}
	if ((set == NULL) || (CFGetTypeID(set) != CFDictionaryGetTypeID())) {
		goto done;
	}

	/* flatten property list */
	flatten(pSession, CFSTR("/"), set, newPreferences);

	CFDictionarySetValue(dict, kSCCachePropSetupCurrentSet, current);

    done :

	/* add last updated time stamp */
	CFDictionarySetValue(dict, kSCCachePropSetupLastUpdated, date);
	CFRelease(date);

	/* add Setup: key */
	CFDictionarySetValue(newPreferences, kSCCacheDomainSetup, dict);
	CFRelease(dict);

	scd_status = SCDLock(session);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDLock() failed: %s"), SCDError(scd_status));
		goto error;
	}
	haveLock = TRUE;

	/*
	 * Compare (and update as needed) any keys present in the cache which are
	 * present in the current configuration. New keys will also be added.
	 */
	CFDictionaryApplyFunction(newPreferences, updateCache, NULL);

	/*
	 * Lastly, remove keys which no longer exist in the current configuration.
	 */
	if (cleanupKeys) {
		CFSetApplyFunction(curCacheKeys, removeCacheKey, NULL);
	}

    error :

	if (haveLock) {
		scd_status = SCDUnlock(session);
		if (scd_status != SCD_OK) {
			SCDLog(LOG_ERR, CFSTR("SCDUnlock() failed: %s"), SCDError(scd_status));
		}
	}

	CFRelease(curCacheKeys);
	CFRelease(newCacheKeys);
	if (pSession)		(void)SCPClose(&pSession);
	if (newPreferences) 	CFRelease(newPreferences);
	return TRUE;
}


void
prime()
{
	SCDLog(LOG_DEBUG, CFSTR("prime() called"));

	/* load the initial configuration from the database */
	updateConfiguration(session, NULL);

	return;
}


void
start(const char *bundleName, const char *bundleDir)
{
	SCDStatus	scd_status;
	CFStringRef	notifyKey;

	SCDLog(LOG_DEBUG, CFSTR("start() called"));
	SCDLog(LOG_DEBUG, CFSTR("  bundle name      = %s"), bundleName);
	SCDLog(LOG_DEBUG, CFSTR("  bundle directory = %s"), bundleDir);

	/* open a "configd" session to allow cache updates */
	scd_status = SCDOpen(&session, CFSTR("Configuraton Preferences Monitor plug-in"));
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDOpen() failed: %s"), SCDError(scd_status));
		goto error;
	}

	/*
	 * register for change notifications.
	 */
	notifyKey = SCPNotificationKeyCreate(NULL, kSCPKeyApply);
	scd_status = SCDNotifierAdd(session, notifyKey, 0);
	CFRelease(notifyKey);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDNotifierAdd() failed: %s"), SCDError(scd_status));
		goto error;
	}

	scd_status = SCDNotifierInformViaCallback(session, updateConfiguration, NULL);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDNotifierInformViaCallback() failed: %s"), SCDError(scd_status));
		goto error;
	}

	return;

    error :

	if (session)	(void) SCDClose(&session);

	return;
}
