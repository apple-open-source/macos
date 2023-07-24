/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
 * - APIs for accessing control preferences and being notified
 *   when they change
 */

/*
 * Modification History
 *
 * Jun 10, 2021			Allan Nathanson (ajn@apple.com)
 * - created
 */


#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/scprefs_observer.h>
#include <pthread.h>
#include "SCControlPrefs.h"


typedef struct {

	// base CFType information
	CFRuntimeBase		cfBase;

	// prefs
	const char		*prefsPlist;
	SCPreferencesRef	prefs;
	SCPreferencesRef	prefs_managed;

	// callback
	_SCControlPrefsCallBack	callback;
	CFRunLoopRef		runloop;

} _SCControlPrefsPrivate, *_SCControlPrefsPrivateRef;

static CFStringRef	__SCControlPrefsCopyDescription	(CFTypeRef cf);
static void		__SCControlPrefsDeallocate	(CFTypeRef cf);

static const CFRuntimeClass __SCControlPrefsClass = {
	0,				// version
	"SCControlPrefs",		// className
	NULL,				// init
	NULL,				// copy
	__SCControlPrefsDeallocate,	// dealloc
	NULL,				// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
	__SCControlPrefsCopyDescription	// copyDebugDesc
};

static CFTypeID		__kSCControlPrefsTypeID;


static CFStringRef
__SCControlPrefsCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	_SCControlPrefsRef		control		= (_SCControlPrefsRef)cf;
	_SCControlPrefsPrivateRef	controlPrivate	= (_SCControlPrefsPrivateRef)control	;
	CFMutableStringRef		result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCControlPrefs %p [%p]> {"), control, allocator);
	CFStringAppendFormat(result, NULL, CFSTR(" prefsPlist = %s"), controlPrivate->prefsPlist);
	if (controlPrivate->prefs != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", prefs = %p"), controlPrivate->prefs);
	}
	if (controlPrivate->prefs_managed != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", prefs_managed = %p"), controlPrivate->prefs_managed);
	}
	if (controlPrivate->callback != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", callback = %p"), controlPrivate->callback);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCControlPrefsDeallocate(CFTypeRef cf)
{
	_SCControlPrefsPrivateRef	controlPrivate	= (_SCControlPrefsPrivateRef)cf;

	/* release resources */
	if ((controlPrivate->callback != NULL) && (controlPrivate->runloop != NULL)) {
		(void) SCPreferencesSetCallback(controlPrivate->prefs, NULL, NULL);
		(void) SCPreferencesUnscheduleFromRunLoop(controlPrivate->prefs,
							  controlPrivate->runloop,
							  kCFRunLoopCommonModes);
		CFRelease(controlPrivate->runloop);
	}
	if (controlPrivate->prefsPlist != NULL) free((void *)controlPrivate->prefsPlist);
	if (controlPrivate->prefs != NULL) CFRelease(controlPrivate->prefs);
	if (controlPrivate->prefs_managed != NULL) CFRelease(controlPrivate->prefs_managed);

	return;
}


static void
__SCControlPrefsInitialize(void)
{
	static dispatch_once_t  initialized;

	dispatch_once(&initialized, ^{
		__kSCControlPrefsTypeID = _CFRuntimeRegisterClass(&__SCControlPrefsClass);
	});
	
	return;
}


static void
prefs_changed(void *info)
{
	_SCControlPrefsRef		control		= (_SCControlPrefsRef)info;
	_SCControlPrefsPrivateRef	controlPrivate	= (_SCControlPrefsPrivateRef)control;

	/* get the current value */
	if (controlPrivate->callback != NULL) {
		(*controlPrivate->callback)(control);
	}

	return;
}


static void
prefs_changed_sc(SCPreferencesRef		prefs,
		 SCPreferencesNotification	type,
		 void				*info)
{
#pragma unused(prefs)
#pragma unused(type)
	prefs_changed(info);
	return;
}


#if	TARGET_OS_IPHONE
static void
enable_prefs_observer(_SCControlPrefsRef control, CFRunLoopRef runloop)
{
	CFRunLoopSourceContext 		context	= {
						    .version = 0,
						    .info    = (void *)control,
						    .retain  = CFRetain,
						    .release = CFRelease,
						    .perform = prefs_changed
						  };
	_SCControlPrefsPrivateRef	controlPrivate	= (_SCControlPrefsPrivateRef)control;
	CFRunLoopSourceRef		source;

	source = CFRunLoopSourceCreate(NULL, 0, &context);
	CFRunLoopAddSource(runloop, source, kCFRunLoopCommonModes);
	_scprefs_observer_watch(scprefs_observer_type_global,
				controlPrivate->prefsPlist,
				dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
				^{
		if (source != NULL) {
			CFRunLoopSourceSignal(source);
			if (runloop != NULL) {
				CFRunLoopWakeUp(runloop);
			}
		};
	});

	return;
}
#endif	/* TARGET_OS_IPHONE */


static SCPreferencesRef
make_prefs(CFStringRef prefsID)
{
	CFMutableDictionaryRef	options;
	SCPreferencesRef	prefs;

	options = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(options, kSCPreferencesOptionRemoveWhenEmpty, kCFBooleanTrue);
	prefs = SCPreferencesCreateWithOptions(NULL,
					       CFSTR("_SCControlPrefs"),
					       prefsID,
					       NULL,
					       options);
	CFRelease(options);

	return prefs;
}


#define kManagedPrefsDirStr			"/Library/Managed Preferences/mobile/"

#if	TARGET_OS_IPHONE
static SCPreferencesRef
get_managed_prefs(_SCControlPrefsPrivateRef controlPrivate)
{
	if (controlPrivate->prefs_managed == NULL) {
		CFStringRef	prefsID;

		prefsID = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s%s"),
						   kManagedPrefsDirStr,
						   controlPrivate->prefsPlist);
		controlPrivate->prefs_managed = make_prefs(prefsID);
		CFRelease(prefsID);
	}

	return controlPrivate->prefs_managed;
}
#endif	// TARGET_OS_IPHONE


static SCPreferencesRef
get_prefs(_SCControlPrefsPrivateRef controlPrivate)
{
	if (controlPrivate->prefs == NULL) {
		CFStringRef	prefsID;

		prefsID = CFStringCreateWithCString(NULL,
						    controlPrivate->prefsPlist,
						    kCFStringEncodingUTF8);
		controlPrivate->prefs = make_prefs(prefsID);
		CFRelease(prefsID);
	}

	return controlPrivate->prefs;
}


_SCControlPrefsRef
_SCControlPrefsCreate(const char		*prefsPlist,
		      CFRunLoopRef		runloop,
		      _SCControlPrefsCallBack	callback)
{
	_SCControlPrefsPrivateRef	controlPrivate;
	SCPreferencesRef		prefs;
	uint32_t			size;

	/* initialize runtime */
	__SCControlPrefsInitialize();

	/* allocate target */
	size         = sizeof(_SCControlPrefsPrivate) - sizeof(CFRuntimeBase);
	controlPrivate = (_SCControlPrefsPrivateRef)_CFRuntimeCreateInstance(NULL,
									     __kSCControlPrefsTypeID,
									     size,
									     NULL);
	if (controlPrivate == NULL) {
		return NULL;
	}

	/* initialize non-zero/NULL members */
	controlPrivate->prefsPlist = strdup(prefsPlist);

	prefs = get_prefs(controlPrivate);
	if ((prefs != NULL) && (runloop != NULL) && (callback != NULL)) {
		SCPreferencesContext	context	= { .info = controlPrivate };
		Boolean			ok;

		controlPrivate->callback = callback;
		controlPrivate->runloop  = runloop;
		CFRetain(controlPrivate->runloop);
		ok = SCPreferencesSetCallback(prefs, prefs_changed_sc, &context);
		if (!ok) {
			SC_log(LOG_NOTICE, "SCPreferencesSetCallBack() failed: %s", SCErrorString(SCError()));
			goto done;
		}

		ok = SCPreferencesScheduleWithRunLoop(prefs, runloop, kCFRunLoopCommonModes);
		if (!ok) {
			SC_log(LOG_NOTICE, "SCPreferencesScheduleWithRunLoop() failed: %s", SCErrorString(SCError()));
			(void) SCPreferencesSetCallback(prefs, NULL, NULL);
		}

#if	TARGET_OS_IPHONE
		enable_prefs_observer((_SCControlPrefsRef)controlPrivate, runloop);
#endif	// TARGET_OS_IPHONE
	}

    done :

	return (_SCControlPrefsRef)controlPrivate;
}


Boolean
_SCControlPrefsGetBoolean(_SCControlPrefsRef	control,
			  CFStringRef		key)
{
	CFBooleanRef			bVal;
	_SCControlPrefsPrivateRef	controlPrivate	= (_SCControlPrefsPrivateRef)control;
	Boolean				done = FALSE;
	Boolean				enabled		= FALSE;
	SCPreferencesRef		prefs;

#if	TARGET_OS_IPHONE
	prefs = get_managed_prefs(controlPrivate);
	if (prefs != NULL) {
		bVal = SCPreferencesGetValue(prefs, key);
		if (isA_CFBoolean(bVal) != NULL) {
			enabled = CFBooleanGetValue(bVal);
			done = TRUE;
		}
		SCPreferencesSynchronize(prefs);
	}
#endif	/* TARGET_OS_IPHONE */

	if (!done) {
		prefs = get_prefs(controlPrivate);
		if (prefs != NULL) {
			bVal = SCPreferencesGetValue(prefs, key);
			if (isA_CFBoolean(bVal) != NULL) {
				enabled = CFBooleanGetValue(bVal);
			}
			SCPreferencesSynchronize(prefs);
		}
	}
	return enabled;
}


Boolean
_SCControlPrefsSetBoolean(_SCControlPrefsRef	control,
			  CFStringRef		key,
			  Boolean		enabled)
{
	_SCControlPrefsPrivateRef	controlPrivate	= (_SCControlPrefsPrivateRef)control;
	Boolean				ok		= FALSE;
	SCPreferencesRef		prefs;

	prefs = get_prefs(controlPrivate);
	if (prefs != NULL) {
		if (enabled) {
			SCPreferencesSetValue(prefs, key, kCFBooleanTrue);
		} else {
			SCPreferencesRemoveValue(prefs, key);
		}
		ok = SCPreferencesCommitChanges(prefs);
		SCPreferencesSynchronize(prefs);
	}

	return ok;
}
