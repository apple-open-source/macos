/*
 * Copyright (c) 2013-2024 Apple Inc. All rights reserved.
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
 * IPConfigurationControlPrefs.c
 * - definitions for accessing IPConfiguration preferences
 */

/* 
 * Modification History
 *
 * March 26, 2013	Dieter Siegmund (dieter@apple)
 * - created (from EAPOLControlPrefs.c)
 */

#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/scprefs_observer.h>
#include <TargetConditionals.h>
#include "IPConfigurationControlPrefs.h"
#include "symbol_scope.h"
#include "cfutil.h"

/*
 * kIPConfigurationControlPrefsID, kIPConfigurationControlPrefsIDStr
 * - identifies the IPConfiguration preferences file that contains
 *   Verbose and other control variables
 * - also identifies the managed preferences ID used to apply control
 *   settings on iOS via configuration profile
 */

#define kIPConfigurationControlPrefsIDStr \
    "com.apple.IPConfiguration.control.plist"
#define kIPConfigurationControlPrefsID	\
    CFSTR(kIPConfigurationControlPrefsIDStr)

/*
 * kVerbose
 * - indicates whether verbose logging is enabled or not
 */
#define kVerbose			CFSTR("Verbose")	/* boolean */

/*
 * kAWDReportInterfaceTypes
 * - indicates whether to submit AWD report for particular interface
 *   types
 * - a string with one of "All", "Cellular", or "None"
 */
#define kAWDReportInterfaceTypes	CFSTR("AWDReportInterfaceTypes") /* string */

/*
 * kCellularCLAT46AutoEnable
 * - indicates whether CLAT46 should be auto-enabled for cellular interfaces
 */
#define kCellularCLAT46AutoEnable	CFSTR("CellularCLAT46AutoEnable")	/* boolean */

/*
 * kIPv6LinkLocalModifierExpires
 * - indicates IPv6 link-local modifiers should expire
 */
#define kIPv6LinkLocalModifierExpires	CFSTR("IPv6LinkLocalModifierExpires")	/* boolean */

/*
 * kDHCPDUIDType
 * - specifies the DHCP DUID type to use
 */
#define kDHCPDUIDType			CFSTR("DHCPDUIDType")	/* int */


/*
 * kHideBSSID
 * - indicates whether the BSSID should be hidden or not
 */
#define kHideBSSID			CFSTR("HideBSSID")	/* boolean */


STATIC SCPreferencesRef				S_prefs;
STATIC IPConfigurationControlPrefsCallBack	S_callback;

STATIC SCPreferencesRef
IPConfigurationControlPrefsGet(void)
{
    if (S_prefs == NULL) {
	IPConfigurationControlPrefsInit(NULL, NULL);
    }
    return (S_prefs);
}

STATIC void
prefs_changed(__unused void * arg)
{
    if (S_callback != NULL) {
	(*S_callback)(S_prefs);
    }
    return;
}

#if TARGET_OS_IPHONE
/*
 * kIPConfigurationControlManangedPrefsID
 * - identifies the location of the managed preferences file
 */
#define kManagedPrefsDirStr		"/Library/Managed Preferences/mobile/"
#define kIPConfigurationControlManagedPrefsID \
    CFSTR(kManagedPrefsDirStr kIPConfigurationControlPrefsIDStr)
STATIC SCPreferencesRef		S_managed_prefs;

STATIC SCPreferencesRef
IPConfigurationControlManagedPrefsGet(void)
{
    if (S_managed_prefs == NULL) {
	S_managed_prefs
	    = SCPreferencesCreate(NULL, CFSTR("IPConfigurationControlPrefs"),
				  kIPConfigurationControlManagedPrefsID);
    }
    return (S_managed_prefs);
}

STATIC void
enable_prefs_observer(dispatch_queue_t queue)
{
    dispatch_block_t		handler;

    handler = ^{
	    prefs_changed(NULL);
    };
    (void)_scprefs_observer_watch(scprefs_observer_type_global,
				  kIPConfigurationControlPrefsIDStr,
				  queue, handler);
    return;
}

#else /* TARGET_OS_IPHONE */

STATIC void
enable_prefs_observer(dispatch_queue_t queue)
{
    return;
}

#endif /* TARGET_OS_IPHONE */

PRIVATE_EXTERN void
IPConfigurationControlPrefsSynchronize(void)
{
    if (S_prefs != NULL) {
	SCPreferencesSynchronize(S_prefs);
    }
#if TARGET_OS_IPHONE
    if (S_managed_prefs != NULL) {
	SCPreferencesSynchronize(S_managed_prefs);
    }
#endif /* TARGET_OS_IPHONE */
    return;
}

STATIC void
IPConfigurationControlPrefsChanged(SCPreferencesRef prefs,
				   SCPreferencesNotification type,
				   void * info)
{
    prefs_changed(NULL);
    return;
}

PRIVATE_EXTERN SCPreferencesRef
IPConfigurationControlPrefsInit(dispatch_queue_t queue,
				IPConfigurationControlPrefsCallBack callback)
{
    S_prefs = SCPreferencesCreate(NULL, CFSTR("IPConfigurationControlPrefs"),
				  kIPConfigurationControlPrefsID);
    if (queue != NULL && callback != NULL) {
	S_callback = callback;
	SCPreferencesSetCallback(S_prefs,
				 IPConfigurationControlPrefsChanged, NULL);
	SCPreferencesSetDispatchQueue(S_prefs, queue);
	enable_prefs_observer(queue);
    }
    return (S_prefs);
}

STATIC Boolean
IPConfigurationControlPrefsSave(void)
{
    Boolean		saved = FALSE;

    if (S_prefs != NULL) {
	saved = SCPreferencesCommitChanges(S_prefs);
	SCPreferencesSynchronize(S_prefs);
    }
    return (saved);
}

STATIC CFBooleanRef
prefs_get_boolean(CFStringRef key)
{
    CFBooleanRef	b = NULL;

#if TARGET_OS_IPHONE
    b = SCPreferencesGetValue(IPConfigurationControlManagedPrefsGet(), key);
    b = isA_CFBoolean(b);
#endif /* TARGET_OS_IPHONE */
    if (b == NULL) {
	b = SCPreferencesGetValue(IPConfigurationControlPrefsGet(), key);
	b = isA_CFBoolean(b);
    }
    return (b);
}

STATIC void
prefs_set_boolean(CFStringRef key, CFBooleanRef b)
{
    SCPreferencesRef	prefs = IPConfigurationControlPrefsGet();

    if (prefs != NULL) {
	if (isA_CFBoolean(b) == NULL) {
	    SCPreferencesRemoveValue(prefs, key);
	}
	else {
	    SCPreferencesSetValue(prefs, key, b);
	}
    }
    return;
}

STATIC CFNumberRef
prefs_get_number(CFStringRef key)
{
    CFNumberRef	n = NULL;

#if TARGET_OS_IPHONE
    n = SCPreferencesGetValue(IPConfigurationControlManagedPrefsGet(), key);
    n = isA_CFNumber(n);
#endif /* TARGET_OS_IPHONE */
    if (n == NULL) {
	n = SCPreferencesGetValue(IPConfigurationControlPrefsGet(), key);
	n = isA_CFNumber(n);
    }
    return (n);
}

STATIC void
prefs_set_number(CFStringRef key, CFNumberRef n)
{
    SCPreferencesRef	prefs = IPConfigurationControlPrefsGet();

    if (prefs != NULL) {
	if (isA_CFNumber(n) == NULL) {
	    SCPreferencesRemoveValue(prefs, key);
	}
	else {
	    SCPreferencesSetValue(prefs, key, n);
	}
    }
    return;
}

STATIC CFStringRef
prefs_get_string(CFStringRef key)
{
    CFStringRef	str = NULL;

#if TARGET_OS_IPHONE
    str = SCPreferencesGetValue(IPConfigurationControlManagedPrefsGet(), key);
    str = isA_CFString(str);
#endif /* TARGET_OS_IPHONE */
    if (str == NULL) {
	str = SCPreferencesGetValue(IPConfigurationControlPrefsGet(), key);
	str = isA_CFString(str);
    }
    return (str);
}

STATIC void
prefs_set_string(CFStringRef key, CFStringRef str)
{
    SCPreferencesRef	prefs = IPConfigurationControlPrefsGet();

    if (prefs != NULL) {
	if (isA_CFString(str) == NULL) {
	    SCPreferencesRemoveValue(prefs, key);
	}
	else {
	    SCPreferencesSetValue(prefs, key, str);
	}
    }
    return;
}

STATIC void
prefs_set_boolean_value(CFStringRef key, Boolean val)
{
	prefs_set_boolean(key, val ? kCFBooleanTrue : kCFBooleanFalse);
}

typedef struct {
    IPConfigurationInterfaceTypes	type;
    CFStringRef				str;
} if_type_table_entry, *if_type_table_entry_ref;

STATIC if_type_table_entry interface_types[] = {
    { kIPConfigurationInterfaceTypesNone, CFSTR("None") },
    { kIPConfigurationInterfaceTypesCellular, CFSTR("Cellular") },
    { kIPConfigurationInterfaceTypesAll, CFSTR("All") },
};
#define INTERFACE_TYPES_COUNT 	(sizeof(interface_types) / sizeof(interface_types[0]))
#define DEFAULT_INTERFACE_TYPES	kIPConfigurationTypesCellular

PRIVATE_EXTERN IPConfigurationInterfaceTypes
IPConfigurationInterfaceTypesFromString(CFStringRef str)
{
    if (str != NULL) {
	int			i;
	if_type_table_entry_ref	scan;

	for (i = 0, scan = interface_types;
	     i < INTERFACE_TYPES_COUNT;
	     i++, scan++) {
	    if (CFEqual(scan->str, str)) {
		return (scan->type);
	    }
	}
    }
    return (kIPConfigurationInterfaceTypesUnspecified);
}

PRIVATE_EXTERN CFStringRef
IPConfigurationInterfaceTypesToString(IPConfigurationInterfaceTypes types)
{
    int				i;
    if_type_table_entry_ref	scan;

    for (i = 0, scan = interface_types;
	 i < INTERFACE_TYPES_COUNT;
	 i++, scan++) {
	if (types == scan->type) {
	    return (scan->str);
	}
    }
    return (NULL);
}

/**
 ** Get
 **/
PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsGetVerbose(Boolean default_val)
{
	CFBooleanRef	val;
	Boolean		verbose = default_val;

	val = prefs_get_boolean(kVerbose);
	if (val != NULL) {
		verbose = CFBooleanGetValue(val);
	}
	return (verbose);
}

PRIVATE_EXTERN IPConfigurationInterfaceTypes
IPConfigurationControlPrefsGetAWDReportInterfaceTypes(void)
{
	CFStringRef	types;

	types = prefs_get_string(kAWDReportInterfaceTypes);
	return (IPConfigurationInterfaceTypesFromString(types));
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsGetCellularCLAT46AutoEnable(Boolean default_val)
{
	Boolean		enabled = default_val;
	CFBooleanRef	val;

	val = prefs_get_boolean(kCellularCLAT46AutoEnable);
	if (val != NULL) {
		enabled = CFBooleanGetValue(val);
	}
	return (enabled);
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsGetIPv6LinkLocalModifierExpires(Boolean default_val)
{
	Boolean		expires = default_val;
	CFBooleanRef	val;

	val = prefs_get_boolean(kIPv6LinkLocalModifierExpires);
	if (val != NULL) {
		expires = CFBooleanGetValue(val);
	}
	return (expires);
}

PRIVATE_EXTERN DHCPDUIDType
IPConfigurationControlPrefsGetDHCPDUIDType(void)
{
	int		type = kDHCPDUIDTypeNone;
	CFNumberRef	val;

	val = prefs_get_number(kDHCPDUIDType);
	if (val != NULL) {
		CFNumberGetValue(val, kCFNumberIntType, &type);
	}
	return (type);
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsGetHideBSSID(Boolean default_val,
					Boolean * ret_was_set)
{
	Boolean		hide = default_val;
	CFBooleanRef	val;
	Boolean		was_set = FALSE;

	val = prefs_get_boolean(kHideBSSID);
	if (val != NULL) {
		hide = CFBooleanGetValue(val);
		was_set = TRUE;
	}
	if (ret_was_set != NULL) {
		*ret_was_set = was_set;
	}
	return (hide);
}

/**
 ** Set
 **/
PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetVerbose(Boolean verbose)
{
	prefs_set_boolean_value(kVerbose, verbose);
	return (IPConfigurationControlPrefsSave());
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetAWDReportInterfaceTypes(IPConfigurationInterfaceTypes
						      types)
{
    CFStringRef		str;

    str = IPConfigurationInterfaceTypesToString(types);
    prefs_set_string(kAWDReportInterfaceTypes, str);
    return (IPConfigurationControlPrefsSave());
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetCellularCLAT46AutoEnable(Boolean enable)
{
	prefs_set_boolean_value(kCellularCLAT46AutoEnable, enable);
	return (IPConfigurationControlPrefsSave());
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetIPv6LinkLocalModifierExpires(Boolean expires)
{
	prefs_set_boolean_value(kIPv6LinkLocalModifierExpires, expires);
	return (IPConfigurationControlPrefsSave());
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetDHCPDUIDType(DHCPDUIDType type)
{
	if (type == kDHCPDUIDTypeNone) {
		/* no specified type */
		prefs_set_number(kDHCPDUIDType, NULL);
	}
	else {
		CFNumberRef	n;
		int		t = type;

		n = CFNumberCreate(NULL, kCFNumberIntType, &t);
		prefs_set_number(kDHCPDUIDType, n);
		CFRelease(n);
	}
	return (IPConfigurationControlPrefsSave());
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetHideBSSID(Boolean hide)
{
	prefs_set_boolean_value(kHideBSSID, hide);
	return (IPConfigurationControlPrefsSave());
}

PRIVATE_EXTERN Boolean
IPConfigurationControlPrefsSetHideBSSIDDefault(void)
{
    SCPreferencesRef	prefs = IPConfigurationControlPrefsGet();

    if (prefs != NULL) {
	    SCPreferencesRemoveValue(prefs, kHideBSSID);
    }
    return (IPConfigurationControlPrefsSave());
}
