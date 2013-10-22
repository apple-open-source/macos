/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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
 * EAPOLControlPrefs.c
 * - definitions for accessing EAPOL preferences
 */

/* 
 * Modification History
 *
 * January 9, 2013	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/scprefs_observer.h>
#include <TargetConditionals.h>
#include "EAPOLControlPrefs.h"
#include "symbol_scope.h"
#include "myCFUtil.h"

/*
 * kEAPOLClientPrefsID
 * - identifies the eapolclient preferences file that contains
 *   LogFlags and other control variables
 */
#define kEAPOLClientPrefsID		CFSTR("com.apple.eapolclient.plist")


/*
 * kEAPOLControlPrefsID, kEAPOLControlPrefsIDStr
 * - identifies the managed preferences ID used to apply control
 *   settings on iOS via configuration profile
 */

#define kEAPOLControlPrefsIDStr		"com.apple.eapol.control.plist"
#define kEAPOLControlPrefsID		CFSTR(kEAPOLControlPrefsIDStr)

/*
 * kLogFlags
 * - indicates how verbose the eapolclient logging will be
 */
#define kLogFlags			CFSTR("LogFlags")

STATIC SCPreferencesRef			S_prefs;
STATIC EAPOLControlPrefsCallBack	S_callback;

STATIC SCPreferencesRef
EAPOLControlPrefsGet(void)
{
    if (S_prefs == NULL) {
	EAPOLControlPrefsInit(NULL, NULL);
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

#if TARGET_OS_EMBEDDED
/*
 * kEAPOLControlManangedPrefsID
 * - identifies the location of the managed preferences file
 */
#define kManagedPrefsDirStr		"/Library/Managed Preferences/mobile/"
#define kEAPOLControlManagedPrefsID	CFSTR(kManagedPrefsDirStr	\
					      kEAPOLControlPrefsIDStr)
STATIC SCPreferencesRef		S_managed_prefs;

STATIC SCPreferencesRef
EAPOLControlManagedPrefsGet(void)
{
    if (S_managed_prefs == NULL) {
	S_managed_prefs
	    = SCPreferencesCreate(NULL, CFSTR("EAPOLControlPrefs"),
				  kEAPOLControlManagedPrefsID);
    }
    return (S_managed_prefs);
}

STATIC void
enable_prefs_observer(CFRunLoopRef runloop)
{
    CFRunLoopSourceContext 	context;
    dispatch_block_t		handler;
    CFRunLoopSourceRef		source;
    dispatch_queue_t		queue;

    bzero(&context, sizeof(context));
    context.perform = prefs_changed;
    source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopAddSource(runloop, source, kCFRunLoopCommonModes);
    queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    handler = ^{
	if (source != NULL) {
	    CFRunLoopSourceSignal(source);
	    if (runloop != NULL) {
		CFRunLoopWakeUp(runloop);
	    }
	};
    };
    (void)_scprefs_observer_watch(scprefs_observer_type_global,
				  kEAPOLControlPrefsIDStr,
				  queue, handler);
    return;
}

#else /* TARGET_OS_EMBEDDED */

STATIC void
enable_prefs_observer(CFRunLoopRef runloop)
{
    return;
}

#endif /* TARGET_OS_EMBEDDED */

EXTERN void
EAPOLControlPrefsSynchronize(void)
{
    if (S_prefs != NULL) {
	SCPreferencesSynchronize(S_prefs);
    }
#if TARGET_OS_EMBEDDED
    if (S_managed_prefs != NULL) {
	SCPreferencesSynchronize(S_managed_prefs);
    }
#endif /* TARGET_OS_EMBEDDED */
    return;
}

STATIC void
EAPOLControlPrefsChanged(SCPreferencesRef prefs, SCPreferencesNotification type,
			 void * info)
{
    prefs_changed(NULL);
    return;
}

EXTERN SCPreferencesRef
EAPOLControlPrefsInit(CFRunLoopRef runloop, EAPOLControlPrefsCallBack callback)
{
    S_prefs = SCPreferencesCreate(NULL, CFSTR("EAPOLControlPrefs"),
				  kEAPOLClientPrefsID);
    if (runloop != NULL && callback != NULL) {
	S_callback = callback;
	SCPreferencesSetCallback(S_prefs, EAPOLControlPrefsChanged, NULL);
	SCPreferencesScheduleWithRunLoop(S_prefs, runloop,
					 kCFRunLoopCommonModes);
	enable_prefs_observer(runloop);
    }
    return (S_prefs);
}

STATIC Boolean
EAPOLControlPrefsSave(void)
{
    Boolean		saved = FALSE;

    if (S_prefs != NULL) {
	saved = SCPreferencesCommitChanges(S_prefs);
	SCPreferencesSynchronize(S_prefs);
    }
    return (saved);
}

STATIC CFNumberRef
prefs_get_number(CFStringRef key)
{
    CFNumberRef		num = NULL;

#if TARGET_OS_EMBEDDED
    num = SCPreferencesGetValue(EAPOLControlManagedPrefsGet(), key);
    num = isA_CFNumber(num);
#endif /* TARGET_OS_EMBEDDED */
    if (num == NULL) {
	num = SCPreferencesGetValue(EAPOLControlPrefsGet(), key);
	num = isA_CFNumber(num);
    }
    return (num);
}

STATIC void
prefs_set_number(CFStringRef key, CFNumberRef num)
{
    SCPreferencesRef	prefs = EAPOLControlPrefsGet();

    if (prefs != NULL) {
	if (isA_CFNumber(num) == NULL) {
	    SCPreferencesRemoveValue(prefs, key);
	}
	else {
	    SCPreferencesSetValue(prefs, key, num);
	}
    }
    return;
}

/**
 ** Get
 **/
EXTERN uint32_t
EAPOLControlPrefsGetLogFlags(void)
{
    CFNumberRef	num;
    uint32_t	ret_value = 0;

    num = prefs_get_number(kLogFlags);
    if (num != NULL) {
	CFNumberGetValue(num, kCFNumberSInt32Type, &ret_value);
    }
    return (ret_value);
}

/**
 ** Set
 **/
EXTERN Boolean
EAPOLControlPrefsSetLogFlags(uint32_t flags)
{
    if (flags == 0) {
	prefs_set_number(kLogFlags, NULL);
    }
    else {
	CFNumberRef	num;

	num = CFNumberCreate(NULL, kCFNumberSInt32Type, &flags);
	prefs_set_number(kLogFlags, num);
	my_CFRelease(&num);
    }
    return (EAPOLControlPrefsSave());
}
