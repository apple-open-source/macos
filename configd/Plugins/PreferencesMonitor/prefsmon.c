/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
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


#include <TargetConditionals.h>
#include <sys/types.h>
#include <unistd.h>

#include <SystemConfiguration/SystemConfiguration.h>
#define __SC_CFRELEASE_NEEDED	1
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCNetworkCategory.h>
#include <SystemConfiguration/SCNetworkCategoryManager.h>
#include "SCNetworkConfigurationInternal.h"
#include "CategoryManagerServer.h"
#include "plugin_shared.h"
#include "prefsmon_log.h"

/* globals */
static SCPreferencesRef		S_prefs			= NULL;
static SCDynamicStoreRef	S_store			= NULL;

/* InterfaceNamer[.plugin] monitoring globals */
static CFMutableArrayRef	excluded_interfaces	= NULL;		// of SCNetworkInterfaceRef
static CFMutableArrayRef	excluded_names		= NULL;		// of CFStringRef (BSD name)
static Boolean			haveConfiguration	= FALSE;
static CFStringRef		namerKey		= NULL;
static CFMutableArrayRef	preconfigured_interfaces= NULL;		// of SCNetworkInterfaceRef
static CFMutableArrayRef	preconfigured_names	= NULL;		// of CFStringRef (BSD name)

/* KernelEventMonitor[.plugin] monitoring globals */
static CFStringRef		interfacesKey		= NULL;

/* SCDynamicStore (Setup:) */
static CFMutableDictionaryRef	currentPrefs;		/* current prefs */
static CFMutableDictionaryRef	newPrefs;		/* new prefs */
static CFMutableArrayRef	unchangedPrefsKeys;	/* new prefs keys which match current */
static CFMutableArrayRef	removedPrefsKeys;	/* old prefs keys to be removed */

/* Category Information */
static CFArrayRef		S_category_info;

static Boolean			rofs			= FALSE;

#define MY_PLUGIN_NAME		"PreferencesMonitor"
#define	MY_PLUGIN_ID		CFSTR("com.apple.SystemConfiguration." MY_PLUGIN_NAME)


static void
updateConfiguration(SCPreferencesRef		prefs,
		    SCPreferencesNotification   notificationType,
		    void			*info);

static void
savePastConfiguration(SCPreferencesRef prefs, CFStringRef old_model)
{
	CFDictionaryRef	system;

	// save "/System" (e.g. host names)
	system = SCPreferencesGetValue(prefs, kSCPrefSystem);
	if (system != NULL) {
		CFRetain(system);
	}

	// save the [previous devices] configuration
	__SCNetworkConfigurationSaveModel(prefs, old_model);

	if (system != NULL) {
		// and retain "/System" (e.g. host names)
		SCPreferencesSetValue(prefs, kSCPrefSystem, system);
		CFRelease(system);
	}

	return;
}


static Boolean
establishNewPreferences(SCPreferencesRef prefs)
{
	SCNetworkSetRef	current		= NULL;
	CFStringRef	new_model;
	Boolean		ok		= FALSE;
	CFStringRef	old_model;
	int		sc_status	= kSCStatusFailed;
	SCNetworkSetRef	set		= NULL;
	Boolean		updated		= FALSE;

	while (TRUE) {
		ok = SCPreferencesLock(prefs, TRUE);
		if (ok) {
			break;
		}

		sc_status = SCError();
		if (sc_status == kSCStatusStale) {
			SCPreferencesSynchronize(prefs);
		} else {
			SC_log(LOG_NOTICE, "Could not acquire network configuration lock: %s",
			       SCErrorString(sc_status));
			return FALSE;
		}
	}

	// check if we need to regenerate the configuration for a new model
	old_model = SCPreferencesGetValue(prefs, MODEL);
	new_model = _SC_hw_model(FALSE);
	if ((old_model != NULL) && !_SC_CFEqual(old_model, new_model)) {
		SC_log(LOG_NOTICE, "Hardware model changed\n"
				   "  created on \"%@\"\n"
				   "  now on     \"%@\"",
		       old_model,
		       new_model);

		// save (and clean) the configuration that was created for "other" hardware
		savePastConfiguration(prefs, old_model);
	}

	current = SCNetworkSetCopyCurrent(prefs);
	if (current != NULL) {
		set = current;
	}

	if (set == NULL) {
		set = _SCNetworkSetCreateDefault(prefs);
		if (set == NULL) {
			ok = FALSE;
			sc_status = SCError();
			goto done;
		}
	}

	ok = SCNetworkSetEstablishDefaultConfiguration(set);
	if (!ok) {
		sc_status = SCError();
		goto done;
	}

    done :

	if (ok) {
		ok = SCPreferencesCommitChanges(prefs);
		if (ok) {
			SC_log(LOG_NOTICE, "New network configuration saved");
			updated = TRUE;
		} else {
			sc_status = SCError();
			if (sc_status == EROFS) {
				/* a read-only fileysstem is OK */
				ok = TRUE;

				/* ... but we don't want to synchronize */
				rofs = TRUE;
			}
		}

		/* apply (committed or temporary/read-only) changes */
		(void) SCPreferencesApplyChanges(prefs);
	} else if ((current == NULL) && (set != NULL)) {
		(void) SCNetworkSetRemove(set);
	}

	if (!ok) {
		if (sc_status == kSCStatusOK) {
			SC_log(LOG_NOTICE, "Network configuration not updated");
		} else {
			SC_log(LOG_NOTICE, "Could not establish network configuration: %s",
			       SCErrorString(sc_status));
		}
	}

	(void)SCPreferencesUnlock(prefs);
	if (set != NULL) CFRelease(set);
	return updated;
}


static void
watchSCDynamicStore(SCDynamicStoreRef store)
{
	CFMutableArrayRef	keys;
	Boolean			ok;
	CFRunLoopSourceRef	rls;

	/*
	 * watch for KernelEventMonitor[.bundle] changes (the list of
	 * active network interfaces)
	 */
	interfacesKey = SCDynamicStoreKeyCreateNetworkInterface(NULL,
								kSCDynamicStoreDomainState);

	/*
	 * watch for InterfaceNamer[.bundle] changes (quiet, timeout,
	 * and the list of pre-configured interfaces)
	 */
	namerKey = SCDynamicStoreKeyCreate(NULL,
					   CFSTR("%@" "InterfaceNamer"),
					   kSCDynamicStoreDomainPlugin);

	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (rls == NULL) {
		SC_log(LOG_NOTICE, "SCDynamicStoreCreateRunLoopSource() failed: %s", SCErrorString(SCError()));
		haveConfiguration = TRUE;
		return;
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRelease(rls);

	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(keys, interfacesKey);
	CFArrayAppendValue(keys, namerKey);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
		SC_log(LOG_NOTICE, "SCDynamicStoreSetNotificationKeys() failed: %s", SCErrorString(SCError()));
		haveConfiguration = TRUE;
	}

	return;
}


static Boolean
findInterfaces(CFArrayRef interfaces, CFMutableArrayRef *matched_interfaces, CFMutableArrayRef *matched_names)
{
	CFIndex		n;
	CFIndex		nx	= 0;
	Boolean		updated	= FALSE;

	// start clean
	if (*matched_interfaces != NULL) {
		CFRelease(*matched_interfaces);
		*matched_interfaces = NULL;
	}
	if (*matched_names != NULL) {
		nx = CFArrayGetCount(*matched_names);
		CFRelease(*matched_names);
		*matched_names = NULL;
	}

	n = (interfaces != NULL) ? CFArrayGetCount(interfaces) : 0;
	for (CFIndex i = 0; i < n; i++) {
		CFStringRef		bsdName	 = CFArrayGetValueAtIndex(interfaces, i);
		SCNetworkInterfaceRef	interface;

		for (int retry = 0; retry < 10; retry++) {
			if (retry != 0) {
				// add short delay (before retry)
				usleep(20 * 1000);	// 20ms
			}

			interface = _SCNetworkInterfaceCreateWithBSDName(NULL, bsdName, kIncludeNoVirtualInterfaces);
			if (interface == NULL) {
				SC_log(LOG_ERR, "could not create network interface for %@", bsdName);
			} else if (_SCNetworkInterfaceGetIOPath(interface) == NULL) {
				SC_log(LOG_ERR, "could not get IOPath for %@", bsdName);
				CFRelease(interface);
				interface = NULL;
			}

			if (interface == NULL) {
				// if SCNetworkInterface not [currently] available
				continue;
			}

			// keep track of the interface name (quicker than having to iterate the list
			// of SCNetworkInterfaces, extract the name, and compare).
			if (*matched_names == NULL) {
				*matched_names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}
			CFArrayAppendValue(*matched_names, bsdName);

			if (*matched_interfaces == NULL) {
				*matched_interfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}
			CFArrayAppendValue(*matched_interfaces, interface);
			CFRelease(interface);

			updated = TRUE;
			break;
		}
	}

	// check if all interfaces were detached
	n = (*matched_names != NULL) ? CFArrayGetCount(*matched_names) : 0;
	if ((nx > 0) && (n == 0)) {
		updated = TRUE;
	}

	return updated;
}


static void
storeCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
#pragma unused(info)
	CFDictionaryRef	dict;
	Boolean		quiet		= FALSE;
	Boolean		timeout		= FALSE;
	Boolean		updated		= FALSE;

	/*
	 * Capture/process InterfaceNamer[.bundle] info
	 * 1. check if IORegistry "quiet", "timeout"
	 * 2. update list of excluded interfaces (e.g. those requiring that
	 *    the attached host be trusted)
	 * 3. update list of named pre-configured interfaces
	 */
	dict = SCDynamicStoreCopyValue(store, namerKey);
	if (dict != NULL) {
		if (isA_CFDictionary(dict)) {
			CFArrayRef	excluded;
			CFArrayRef	preconfigured;

			if (CFDictionaryContainsKey(dict, kInterfaceNamerKey_Quiet)) {
				quiet = TRUE;
			}
			if (CFDictionaryContainsKey(dict, kInterfaceNamerKey_Timeout)) {
				timeout = TRUE;
			}

			excluded = CFDictionaryGetValue(dict, kInterfaceNamerKey_ExcludedInterfaces);
			excluded = isA_CFArray(excluded);
			if (!_SC_CFEqual(excluded, excluded_names)) {
				Boolean		excluded_updated;

				excluded_updated = findInterfaces(excluded, &excluded_interfaces, &excluded_names);
				if (excluded_updated) {
					CFStringRef	interfaces	= CFSTR("<empty>");

					// report [updated] pre-configured interfaces
					if (excluded_names != NULL) {
						interfaces = CFStringCreateByCombiningStrings(NULL, excluded_names, CFSTR(","));
					} else {
						CFRetain(interfaces);
					}
					SC_log(LOG_INFO, "excluded interface list changed: %@", interfaces);
					CFRelease(interfaces);

					updated = TRUE;
				}
			}

			preconfigured = CFDictionaryGetValue(dict, kInterfaceNamerKey_PreConfiguredInterfaces);
			preconfigured = isA_CFArray(preconfigured);
			if (!_SC_CFEqual(preconfigured, preconfigured_names)) {
				Boolean		preconfigured_updated;

				preconfigured_updated = findInterfaces(preconfigured, &preconfigured_interfaces, &preconfigured_names);
				if (preconfigured_updated) {
					CFStringRef	interfaces	= CFSTR("<empty>");

					// report [updated] pre-configured interfaces
					if (preconfigured_names != NULL) {
						interfaces = CFStringCreateByCombiningStrings(NULL, preconfigured_names, CFSTR(","));
					} else {
						CFRetain(interfaces);
					}
					SC_log(LOG_INFO, "pre-configured interface list changed: %@", interfaces);
					CFRelease(interfaces);

					updated = TRUE;
				}
			}
		}

		CFRelease(dict);
	}

	if (!haveConfiguration && (quiet || timeout)) {
		static int	logged	= 0;

		if (quiet
#if	!TARGET_OS_IPHONE
		    || timeout
#endif	/* !TARGET_OS_IPHONE */
		    ) {
			haveConfiguration = TRUE;
		}

		(void) establishNewPreferences(S_prefs);

		if (timeout && (logged++ == 0)) {
			SC_log(LOG_ERR, "Network configuration creation timed out waiting for IORegistry");
		}
	}

	if (updated && (changedKeys != NULL)) {
		// if pre-configured interface list changed
		updateConfiguration(S_prefs, kSCPreferencesNotificationApply,
				    NULL);
	}

	return;
}


static void
updateCache(const void *key, const void *value, void *context)
{
#pragma unused(context)
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
			SC_log(LOG_NOTICE, "SCPreferencesPathGetValue(,%@,) failed: %s",
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
			if (isA_CFDictionary(vals[i])) {
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
			} else {
				/* add this key/value to the current dictionary */
				CFDictionarySetValue(myDict, keys[i], vals[i]);
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
excludeConfigurations(SCPreferencesRef prefs)
{
	Boolean		ok;
	CFRange		range;
	CFArrayRef	services;
	SCNetworkSetRef	set;

	range = CFRangeMake(0,
			    (excluded_names != NULL) ? CFArrayGetCount(excluded_names) : 0);
	if (range.length == 0) {
		// if no [excluded] interfaces
		return;
	}

	set = SCNetworkSetCopyCurrent(prefs);
	if (set == NULL) {
		// if no current set
		return;
	}

	/*
	 * Check for (and remove) any network services associated with
	 * an excluded interface from the prefs.
	 */
	services = SCNetworkSetCopyServices(set);
	if (services != NULL) {
		CFIndex		n;

		n = CFArrayGetCount(services);
		for (CFIndex i = 0; i < n; i++) {
			CFStringRef		bsdName;
			SCNetworkInterfaceRef	interface;
			SCNetworkServiceRef	service;

			service = CFArrayGetValueAtIndex(services, i);

			interface = SCNetworkServiceGetInterface(service);
			if (interface == NULL) {
				// if no interface
				continue;
			}

			bsdName = SCNetworkInterfaceGetBSDName(interface);
			if (bsdName == NULL) {
				// if no interface name
				continue;
			}

			if (!CFArrayContainsValue(excluded_names, range, bsdName)) {
				// if not excluded
				continue;
			}

			// remove [excluded] network service from the prefs
			SC_log(LOG_NOTICE, "excluding network service for %@", bsdName);
			ok = SCNetworkSetRemoveService(set, service);
			if (!ok) {
				SC_log(LOG_ERR, "SCNetworkSetRemoveService() failed: %s",
				       SCErrorString(SCError()));
			}
		}

		CFRelease(services);
	}

	CFRelease(set);
	return;
}


static void
updatePreConfiguredConfiguration(SCPreferencesRef prefs)
{
	Boolean		ok;
	CFRange		range;
	CFArrayRef	services;
	SCNetworkSetRef	set;
	Boolean		updated	= FALSE;

	range = CFRangeMake(0,
			    (preconfigured_names != NULL) ? CFArrayGetCount(preconfigured_names) : 0);
	if (range.length == 0) {
		// if no [pre-configured] interfaces
		return;
	}

	set = SCNetworkSetCopyCurrent(prefs);
	if (set == NULL) {
		// if no current set
		return;
	}

	/*
	 * Check for (and remove) any network services associated with
	 * a pre-configured interface from the prefs.
	 */
	services = SCNetworkServiceCopyAll(prefs);
	if (services != NULL) {
		CFIndex		n;

		n = CFArrayGetCount(services);
		for (CFIndex i = 0; i < n; i++) {
			CFStringRef		bsdName;
			SCNetworkInterfaceRef	interface;
			SCNetworkServiceRef	service;

			service = CFArrayGetValueAtIndex(services, i);

			interface = SCNetworkServiceGetInterface(service);
			if (interface == NULL) {
				// if no interface
				continue;
			}

			bsdName = SCNetworkInterfaceGetBSDName(interface);
			if (bsdName == NULL) {
				// if no interface name
				continue;
			}

			if (!CFArrayContainsValue(preconfigured_names, range, bsdName)) {
				// if not preconfigured
				continue;
			}

			// remove [preconfigured] network service from the prefs
			SC_log(LOG_NOTICE, "removing network service for %@", bsdName);
			ok = SCNetworkServiceRemove(service);
			if (!ok) {
				SC_log(LOG_ERR, "SCNetworkServiceRemove() failed: %s",
				       SCErrorString(SCError()));
			}
			updated = TRUE;
		}

		CFRelease(services);
	}

	if (updated) {
		// commit the updated prefs ... but don't apply
		ok = SCPreferencesCommitChanges(prefs);
		if (!ok) {
			if (SCError() != EROFS) {
				SC_log(LOG_ERR, "SCPreferencesCommitChanges() failed: %s",
				       SCErrorString(SCError()));
			}
		}
	}

	/*
	 * Now, add a new network service for each pre-configured interface
	 */
	for (CFIndex i = 0; i < range.length; i++) {
		CFStringRef		bsdName;
		SCNetworkInterfaceRef	interface	= CFArrayGetValueAtIndex(preconfigured_interfaces, i);
		SCNetworkServiceRef	service;

		bsdName = SCNetworkInterfaceGetBSDName(interface);

		// create network service
		service = _SCNetworkServiceCreatePreconfigured(prefs, interface);
		if (service == NULL) {
			continue;
		}

		// add network service to the current set
		ok = SCNetworkSetAddService(set, service);
		if (!ok) {
			SC_log(LOG_ERR, "could not add service for \"%@\": %s",
			       bsdName,
			       SCErrorString(SCError()));
			SCNetworkServiceRemove(service);
			CFRelease(service);
			continue;
		}

		SC_log(LOG_INFO, "network service %@ added for \"%@\"",
		       SCNetworkServiceGetServiceID(service),
		       bsdName);

		CFRelease(service);
	}

	CFRelease(set);
	return;
}


static void
updateSCDynamicStore(SCDynamicStoreRef store, SCPreferencesRef prefs)
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
		SC_log(LOG_NOTICE, "updateConfiguration(): no preferences");
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
		SC_log(LOG_NOTICE, "updateConfiguration(): %@ is not a dictionary",
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
		SC_log(LOG_NOTICE, "updateConfiguration(): %@ is not a string",
		       kSCPrefCurrentSet);
		goto done;
	}

	/*
	 * get current set
	 */
	set = SCPreferencesPathGetValue(prefs, current);
	if (!set) {
		/* if error with path */
		SC_log(LOG_NOTICE, "%@ value (%@) not valid",
		       kSCPrefCurrentSet,
		       current);
		goto done;
	}

	if (!isA_CFDictionary(set)) {
		SC_log(LOG_NOTICE, "updateConfiguration(): %@ is not a dictionary",
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
#ifndef MAIN
	if (!SCDynamicStoreSetMultiple(store, newPrefs, removedPrefsKeys, NULL)) {
		SC_log(LOG_NOTICE, "SCDynamicStoreSetMultiple() failed: %s", SCErrorString(SCError()));
	}
#else	// !MAIN
	SC_log(LOG_DEBUG, "SCDynamicStore\nset: %@\nremove: %@",
	       newPrefs,
	       removedPrefsKeys);
#endif	// !MAIN

	CFRelease(currentPrefs);
	CFRelease(newPrefs);
	CFRelease(unchangedPrefsKeys);
	CFRelease(removedPrefsKeys);
	if (dict)	CFRelease(dict);
	if (date)	CFRelease(date);
	if (keys)	CFRelease(keys);
	return;
}

#if	TARGET_OS_OSX
#include "preboot.h"
static void
updatePrebootVolume(void)
{
	(void)syncNetworkConfigurationToPrebootVolume();
}

static Boolean
haveCurrentSet(SCPreferencesRef prefs)
{
	SCNetworkSetRef	current;
	Boolean		have_current;

	current = SCNetworkSetCopyCurrent(prefs);
	if (current != NULL) {
		CFRelease(current);
		have_current = TRUE;
	}
	else {
		have_current = FALSE;
	}
	return (have_current);
}

#endif /* TARGET_OS_OSX*/

typedef struct {
	SCPreferencesRef	prefs;
	SCNetworkSetRef		set;
	CFArrayRef		services;
} categoryContext, *categoryContextRef;

static void
categoryContextInit(categoryContextRef context)
{
	bzero(context, sizeof(*context));
}

static Boolean
categoryContextPopulate(categoryContextRef context,SCPreferencesRef prefs)
{
	context->set = SCNetworkSetCopyCurrent(prefs);
	if (context->set == NULL) {
		SC_log(LOG_NOTICE, "No default set");
		return (FALSE);
	}
	context->services = SCNetworkSetCopyServices(context->set);
	context->prefs = prefs;
	CFRetain(prefs);
	return (TRUE);
}

static void
categoryContextFree(categoryContextRef context)
{
	__SC_CFRELEASE(context->prefs);
	__SC_CFRELEASE(context->services);
	__SC_CFRELEASE(context->set);
	return;
}

static Boolean
ifNameListMatch(CFStringRef * iflist, CFIndex count, CFStringRef name)
{
	for (CFIndex i = 0; i < count; i++) {
		if (CFEqual(iflist[i], name)) {
			return (TRUE);
		}
	}
	return (FALSE);
}

static void
removeServicesForInterfaces(categoryContextRef context,
			    CFStringRef * iflist, CFIndex iflist_count)
{
	CFIndex		count;

	if (context->services == NULL) {
		SC_log(LOG_NOTICE, "%s: no services", __func__);
		return;
	}
	count = CFArrayGetCount(context->services);
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef		name;
		SCNetworkInterfaceRef	netif;
		Boolean			removed;
		SCNetworkServiceRef	service;

		service = (SCNetworkServiceRef)
			CFArrayGetValueAtIndex(context->services, i);
		netif = SCNetworkServiceGetInterface(service);
		if (netif == NULL) {
			continue;
		}
		name = SCNetworkInterfaceGetBSDName(netif);
		if (name == NULL) {
			continue;
		}
		if (!ifNameListMatch(iflist, iflist_count, name)) {
			continue;
		}
		removed = SCNetworkSetRemoveService(context->set, service);
		SC_log(LOG_NOTICE, "%s: remove service %@ (%@): %s",
		       __func__, service, name,
		       removed ? "SUCCESS" : "FAILED");
	}
	return;
}

static void
establishServiceForInterface(categoryContextRef context,
			     CFStringRef ifname)
{
	SCNetworkInterfaceRef	netif;
	SCNetworkServiceRef	service;
	CFStringRef		serviceID;

	SC_log(LOG_NOTICE, "%s: %@",
	       __func__, ifname);
	netif = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname,
					     kIncludeAllVirtualInterfaces);
	if (netif == NULL) {
		SC_log(LOG_NOTICE, "%s: can't create netif for %@",
		       __func__, ifname);
		return;
	}
	service = SCNetworkServiceCreate(context->prefs, netif);
	serviceID = _SC_copyInterfaceUUID(ifname);
	if (serviceID != NULL) {
		if (!_SCNetworkServiceSetServiceID(service, serviceID)) {
			SC_log(LOG_NOTICE,
			       "%s: failed to set serviceID for %@",
			       __func__, ifname);
		}
		CFRelease(serviceID);
	}
	if (!SCNetworkServiceEstablishDefaultConfiguration(service)) {
		SC_log(LOG_NOTICE,
		       "%s: %@ failed to establish default, %s",
		       __func__, ifname, SCErrorString(SCError()));
	}
	else if (!SCNetworkSetAddService(context->set, service)) {
		SC_log(LOG_NOTICE,
		       "%s: can't add service for %@ to set, %s",
		       __func__, ifname, SCErrorString(SCError()));
	}
	CFRelease(service);
	CFRelease(netif);
}

static void
ensureDefaultServiceExistsForInterface(categoryContextRef context,
				       CFStringRef ifname)
{
	CFIndex			count;
	SCNetworkServiceRef	first_service = NULL;

	if (context->services == NULL) {
		SC_log(LOG_NOTICE, "%s: no services", __func__);
		return;
	}
	count = CFArrayGetCount(context->services);
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef		name;
		SCNetworkInterfaceRef	netif;
		SCNetworkServiceRef	service;

		service = (SCNetworkServiceRef)
			CFArrayGetValueAtIndex(context->services, i);
		netif = SCNetworkServiceGetInterface(service);
		if (netif == NULL) {
			continue;
		}
		name = SCNetworkInterfaceGetBSDName(netif);
		if (name == NULL) {
			continue;
		}
		if (!CFEqual(ifname, name)) {
			continue;
		}
		if (first_service == NULL) {
			first_service = service;
			SC_log(LOG_NOTICE, "%s: found service %@ (%@)",
			       __func__, service, name);
		}
		else {
			/* remove all but the first service */
			Boolean removed;

			removed = SCNetworkSetRemoveService(context->set,
							    service);
			SC_log(LOG_NOTICE, "%s: remove service %@ (%@): %s",
			       __func__, service, name,
			       removed ? "SUCCESS" : "FAILED");
		}
	}
	if (first_service == NULL) {
		/* XXX: should be persisted instead of being dynamic */
		establishServiceForInterface(context, ifname);
	}
	else {
		/* ensure that is set to defaults */
		SC_log(LOG_NOTICE, "%s: TBD: ensure defaults for %@",
		       __func__, ifname);
	}
	return;
}

static Boolean
stringlist_contains_string(CFStringRef * list, CFIndex count, CFStringRef name)
{
	Boolean		present = FALSE;

	for (CFIndex i = 0; i < count; i++) {
		if (CFEqual(list[i], name)) {
			present = TRUE;
			break;
		}
	}
	return (present);
}

static void
insertCategoryServices(categoryContextRef context,
		       SCNetworkCategoryRef category, CFStringRef value,
		       CFArrayRef services, CFStringRef ifname)
{
	CFIndex		count;
	CFStringRef	iflist[CFArrayGetCount(services)];
	CFIndex		iflist_count = 0;

	count = CFArrayGetCount(services);
	if (ifname != NULL) {
		iflist[0] = ifname;
		iflist_count = 1;
	}
	else {
		for (CFIndex i = 0; i < count; i++) {
			CFStringRef		name;
			SCNetworkInterfaceRef	netif;
			SCNetworkServiceRef	service;
			
			service = (SCNetworkServiceRef)
				CFArrayGetValueAtIndex(services, i);
			SC_log(LOG_NOTICE, "%s: service %@", __func__, service);
			netif = SCNetworkServiceGetInterface(service);
			if (netif == NULL) {
				SC_log(LOG_NOTICE, "%s: no netif", __func__);
				continue;
			}
			name = SCNetworkInterfaceGetBSDName(netif);
			if (name == NULL) {
				SC_log(LOG_NOTICE, "%s: no name %@", __func__, netif);
				continue;
			}
			if (!stringlist_contains_string(iflist, iflist_count,
							name)) {
				iflist[iflist_count++] = name;
				SC_log(LOG_NOTICE, "%s: added %@, count %d", __func__,
				       name, (int)iflist_count);
			}
		}
	}
	/* remove services for interface(s) */
	removeServicesForInterfaces(context, iflist, iflist_count);
	
	/* add services */
	for (CFIndex i = 0; i < count; i++) {
		CFDictionaryRef		qos;
		SCNetworkServiceRef	service;
			
		service = (SCNetworkServiceRef)
			CFArrayGetValueAtIndex(services, i);
		qos = SCNetworkCategoryGetServiceQoSMarkingPolicy(category,
								  value,
								  service);
		if (!SCNetworkSetAddService(context->set, service)) {
			SC_log(LOG_NOTICE,
			       "%s: can't add service %@ to set, %s",
			       __func__, service, SCErrorString(SCError()));
		}
		else {
			SCNetworkInterfaceRef	netif;

			SC_log(LOG_NOTICE,
			       "%s: added service %@ to set",
			       __func__, service);
			netif = SCNetworkServiceGetInterface(service);
			if (netif != NULL && qos != NULL) {
				Boolean 	ok;

				ok = SCNetworkInterfaceSetQoSMarkingPolicy(netif,
									   qos);
				SC_log(LOG_NOTICE,
				       "%s: %sset QoSMarkingPolicy on %@",
				       __func__, !ok ? "FAILED to " : "",
				       netif);
			}
		}
	}
	return;
}

static void
handleCategoryInfo(categoryContextRef context, CFDictionaryRef dict)
{
	SCNetworkCategoryRef	category = NULL;
	CFStringRef		categoryID;
	CFStringRef		ifname;
	Boolean			keep_configured = FALSE;
	CFArrayRef		services = NULL;
	CFStringRef		value;

	categoryID = CategoryInformationGetCategory(dict);
	if (categoryID == NULL) {
		SC_log(LOG_NOTICE, "%s: no category", __func__);
		return;
	}
	category = SCNetworkCategoryCreate(context->prefs, categoryID);
	if (category == NULL) {
		SC_log(LOG_NOTICE, "%s: failed to allocate category",
		       __func__);
		return;
	}
	value = CategoryInformationGetValue(dict);
	if (value != NULL) {
		services = SCNetworkCategoryCopyServices(category, value);
	}
	keep_configured = ((CategoryInformationGetFlags(dict)
			    & kSCNetworkCategoryManagerFlagsKeepConfigured)
			   != 0);
	ifname = CategoryInformationGetInterfaceName(dict);
	if (value == NULL || services == NULL) {
		if (ifname != NULL && keep_configured) {
			ensureDefaultServiceExistsForInterface(context, ifname);
		}
	}
	else {
		insertCategoryServices(context, category, value,
				       services, ifname);
	}
	__SC_CFRELEASE(category);
	__SC_CFRELEASE(services);
	return;
}

static void
updateCategoryServices(SCPreferencesRef prefs)
{
	categoryContext	context;
	CFIndex		count;

	if (S_category_info == NULL) {
		return;
	}
	categoryContextInit(&context);
	if (!categoryContextPopulate(&context, prefs)) {
		return;
	}
	count = CFArrayGetCount(S_category_info);
	for (CFIndex i = 0; i < count; i++) {
		CFDictionaryRef		dict;

		dict = CFArrayGetValueAtIndex(S_category_info, i);
		handleCategoryInfo(&context, dict);
	}
	categoryContextFree(&context);
	return;
}

static void
updateConfiguration(SCPreferencesRef		prefs,
		    SCPreferencesNotification   notificationType,
		    void			*info)
{
#pragma unused(info)
	if ((notificationType & kSCPreferencesNotificationCommit) != 0) {
		if (!rofs) {
			SCPreferencesSynchronize(prefs);
		}
#if	TARGET_OS_OSX
		/* if network configuration available, disable template creation */
		if (haveCurrentSet(prefs)) {
			haveConfiguration = TRUE;
		}
		/* copy configuration to preboot volume */
		updatePrebootVolume();
#endif	/* TARGET_OS_OSX */
	}

	if ((notificationType & kSCPreferencesNotificationApply) == 0) {
		goto done;
	}

	SC_log(LOG_INFO, "updating configuration");

	/* adjust configuration for category-based services */
	updateCategoryServices(prefs);

	/* add any [Apple] pre-configured network services */
	updatePreConfiguredConfiguration(prefs);

	/* remove any excluded network services */
	excludeConfigurations(prefs);

	/* update SCDynamicStore (Setup:) */
	updateSCDynamicStore(S_store, prefs);

	/* finished with current prefs, wait for changes */
	if (!rofs) {
		SCPreferencesSynchronize(prefs);
	}

    done :

	return;
}

static void
categoryInformationChanged(void * _not_used)
{
#pragma unused(_not_used)
	Boolean		changed = FALSE;
	CFArrayRef	info;

	info = CategoryManagerServerInformationCopy();
	SC_log(LOG_NOTICE, "%s: info %@", __func__, info);
	if (!_SC_CFEqual(S_category_info, info)) {
		changed = TRUE;
	}
	__SC_CFRELEASE(S_category_info);
	S_category_info = info;
	if (changed) {
		updateConfiguration(S_prefs,
				    kSCPreferencesNotificationApply,
				    NULL);
		CategoryManagerServerInformationAck(info);
	}
	return;
}

static Boolean
startCategoryManagerServer(void)
{
	CFRunLoopSourceContext 		context;
	CFRunLoopSourceRef		rls;
	Boolean				started;

	memset(&context, 0, sizeof(context));
	context.perform = categoryInformationChanged;
	rls = CFRunLoopSourceCreate(NULL, 0, &context);
	started = CategoryManagerServerStart(CFRunLoopGetCurrent(), rls);
	if (!started) {
		SC_log(LOG_ERR, "CategoryManagerServerStart failed");
	}
	else {
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls,
				   kCFRunLoopDefaultMode);
	}
	CFRelease(rls);
	return (started);
}

__private_extern__
void
prime_PreferencesMonitor(void)
{
	SC_log(LOG_DEBUG, "prime() called");

	if (startCategoryManagerServer()) {
		SC_log(LOG_NOTICE, "CategoryManagerServer started");
	}
	
	/* load the initial configuration from the database */
	updateConfiguration(S_prefs, kSCPreferencesNotificationApply, NULL);
	return;
}


#ifndef	MAIN
#define	PREFERENCES_MONITOR_PLIST	NULL
#else	// !MAIN
#define	PREFERENCES_MONITOR_PLIST	CFSTR("/tmp/preferences.plist")
#endif	// !MAIN


__private_extern__
void
load_PreferencesMonitor(CFBundleRef bundle, Boolean bundleVerbose)
{
#pragma unused(bundleVerbose)
	CFStringRef		option_keys[]	= { kSCPreferencesOptionAllowModelConflict,
						    kSCPreferencesOptionAvoidDeadlock };
	CFPropertyListRef	option_vals[]	= { kCFBooleanTrue,
						    kCFBooleanFalse };
	CFDictionaryRef		options;

	SC_log(LOG_DEBUG, "load() called");
	SC_log(LOG_DEBUG, "  bundle ID = %@", CFBundleGetIdentifier(bundle));

	/* open a SCDynamicStore session to allow cache updates */
	S_store = SCDynamicStoreCreate(NULL,
				       CFSTR("PreferencesMonitor.bundle"),
				       storeCallback,
				       NULL);
	if (S_store == NULL) {
		SC_log(LOG_NOTICE, "SCDynamicStoreCreate() failed: %s", SCErrorString(SCError()));
		goto error;
	}

	/* open a SCPreferences session */
	options = CFDictionaryCreate(NULL,
				     (const void **)option_keys,
				     (const void **)option_vals,
				     sizeof(option_keys) / sizeof(option_keys[0]),
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
	S_prefs = SCPreferencesCreateWithOptions(NULL,
						 MY_PLUGIN_ID,
						 PREFERENCES_MONITOR_PLIST,
						 NULL,	// authorization
						 options);
	CFRelease(options);
	if (S_prefs != NULL) {
		Boolean		need_update = FALSE;
		CFStringRef 	new_model;
		CFStringRef	old_model;

		// check if we need to update the configuration
		__SCNetworkConfigurationUpgrade(&S_prefs, NULL, TRUE);

		// check if we need to regenerate the configuration for a new model
		old_model = SCPreferencesGetValue(S_prefs, MODEL);
		new_model = _SC_hw_model(FALSE);
		if ((old_model != NULL) && !_SC_CFEqual(old_model, new_model)) {
			SC_log(LOG_NOTICE, "Hardware model changed\n"
					   "  created on \"%@\"\n"
					   "  now on     \"%@\"",
			       old_model,
			       new_model);

			// save (and clean) the configuration that was created for "other" hardware
			savePastConfiguration(S_prefs, old_model);

			// ... and we'll update the configuration later (when the IORegistry quiesces)
			need_update = TRUE;
		}

		if (!need_update) {
			SCNetworkSetRef current;

			current = SCNetworkSetCopyCurrent(S_prefs);
			if (current != NULL) {
				/* network configuration available, disable template creation */
				haveConfiguration = TRUE;
				CFRelease(current);
			}
		}
	} else {
		SC_log(LOG_NOTICE, "SCPreferencesCreate() failed: %s", SCErrorString(SCError()));
		goto error;
	}

	/*
	 * register for change notifications.
	 */
	if (!SCPreferencesSetCallback(S_prefs, updateConfiguration, NULL)) {
		SC_log(LOG_NOTICE, "SCPreferencesSetCallBack() failed: %s", SCErrorString(SCError()));
		goto error;
	}

	if (!SCPreferencesScheduleWithRunLoop(S_prefs, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
		SC_log(LOG_NOTICE, "SCPreferencesScheduleWithRunLoop() failed: %s", SCErrorString(SCError()));
		goto error;
	}

	/*
	 * watch InterfaceNamer and KernelEventMonitor changes to know when
	 * the IORegistry has quiesced (to create the initial configuration
	 * template), to track any pre-configured interfaces, and to ensure
	 * that we create a network service for any active interfaces.
	 */
	watchSCDynamicStore(S_store);
	storeCallback(S_store, NULL, NULL);

	return;

    error :

	if (S_store != NULL)	CFRelease(S_store);
	if (S_prefs != NULL)	CFRelease(S_prefs);
	haveConfiguration = TRUE;

	return;
}


#ifdef  MAIN
int
main(int argc, char * const argv[])
{
	_sc_log     = kSCLogDestinationFile;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load_PreferencesMonitor(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	prime_PreferencesMonitor();
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
