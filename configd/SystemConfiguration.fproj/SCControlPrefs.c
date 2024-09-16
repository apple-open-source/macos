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
	union {
		CFRunLoopRef		runloop;
		dispatch_queue_t	queue;
		void *			ptr;
	};
	Boolean				use_queue;
} WorkScheduler, *WorkSchedulerRef;

typedef struct __SCControlPrefs {

	// base CFType information
	CFRuntimeBase		cfBase;

	// prefs
	const char		*prefsPlist;
	SCPreferencesRef	prefs;
	SCPreferencesRef	prefs_managed;

	// callback
	_SCControlPrefsCallBack	callback;
	WorkScheduler		scheduler;

} _SCControlPrefs;

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
	__SCControlPrefsCopyDescription,// copyDebugDesc
#ifdef CF_RECLAIM_AVAILABLE
	NULL,
#endif
#ifdef CF_REFCOUNT_AVAILABLE
	NULL
#endif
};

static CFTypeID		__kSCControlPrefsTypeID;


static CFStringRef
__SCControlPrefsCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	_SCControlPrefsRef		control		= (_SCControlPrefsRef)cf;
	CFMutableStringRef		result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCControlPrefs %p [%p]> {"), control, allocator);
	CFStringAppendFormat(result, NULL, CFSTR(" prefsPlist = %s"), control->prefsPlist);
	if (control->prefs != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", prefs = %p"), control->prefs);
	}
	if (control->prefs_managed != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", prefs_managed = %p"), control->prefs_managed);
	}
	if (control->callback != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", callback = %p"), control->callback);
	}
	if (control->scheduler.ptr != NULL) {
		CFStringAppendFormat(result, NULL,
				     CFSTR(", %s = %p"),
				     control->scheduler.use_queue
				     ? "queue" : "runloop",
				     control->scheduler.ptr);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCControlPrefsDeallocate(CFTypeRef cf)
{
	_SCControlPrefsRef	control	= (_SCControlPrefsRef)cf;

	/* release resources */
	if (control->callback != NULL && control->scheduler.ptr != NULL) {
		if (control->scheduler.use_queue) {
			(void )SCPreferencesSetDispatchQueue(control->prefs, NULL);
		}
		else {
			(void) SCPreferencesSetCallback(control->prefs, NULL, NULL);
			(void) SCPreferencesUnscheduleFromRunLoop(control->prefs,
								  control->scheduler.runloop,
								  kCFRunLoopCommonModes);
			CFRelease(control->scheduler.runloop);
		}
	}
	if (control->prefsPlist != NULL) free((void *)control->prefsPlist);
	if (control->prefs != NULL) CFRelease(control->prefs);
	if (control->prefs_managed != NULL) CFRelease(control->prefs_managed);

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
	_SCControlPrefsRef	control		= (_SCControlPrefsRef)info;

	/* get the current value */
	if (control->callback != NULL) {
		(*control->callback)(control);
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
enable_prefs_observer(_SCControlPrefsRef control)
{
	dispatch_block_t	handler;
	dispatch_queue_t	queue;

	if (control->scheduler.use_queue) {
		queue = control->scheduler.queue;
		handler = ^{
			prefs_changed(control);
		};
	}
	else {
		CFRunLoopSourceContext	context	= {
			.version = 0,
			.info    = (void *)control,
			.retain  = CFRetain,
			.release = CFRelease,
			.perform = prefs_changed
		};
		CFRunLoopRef		runloop = control->scheduler.runloop;
		CFRunLoopSourceRef	source;

		source = CFRunLoopSourceCreate(NULL, 0, &context);
		CFRunLoopAddSource(runloop, source, kCFRunLoopCommonModes);
		queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
		handler = ^{
			CFRunLoopSourceSignal(source);
			CFRunLoopWakeUp(runloop);
		};
	}
	_scprefs_observer_watch(scprefs_observer_type_global,
				control->prefsPlist,
				queue,
				handler);
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
get_managed_prefs(_SCControlPrefsRef control)
{
	if (control->prefs_managed == NULL) {
		CFStringRef	prefsID;

		prefsID = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s%s"),
						   kManagedPrefsDirStr,
						   control->prefsPlist);
		control->prefs_managed = make_prefs(prefsID);
		CFRelease(prefsID);
	}

	return control->prefs_managed;
}
#endif	// TARGET_OS_IPHONE


static SCPreferencesRef
get_prefs(_SCControlPrefsRef control)
{
	if (control->prefs == NULL) {
		CFStringRef	prefsID;

		prefsID = CFStringCreateWithCString(NULL,
						    control->prefsPlist,
						    kCFStringEncodingUTF8);
		control->prefs = make_prefs(prefsID);
		CFRelease(prefsID);
	}

	return control->prefs;
}


static _SCControlPrefsRef
_SCControlPrefsCreateCommon(const char		*prefsPlist,
			    WorkSchedulerRef	scheduler,
			    _SCControlPrefsCallBack	callback)
{
	_SCControlPrefsRef		control;
	SCPreferencesRef		prefs;
	uint32_t			size;

	/* initialize runtime */
	__SCControlPrefsInitialize();

	/* allocate target */
	size         = sizeof(_SCControlPrefs) - sizeof(CFRuntimeBase);
	control = (_SCControlPrefsRef)_CFRuntimeCreateInstance(NULL,
							       __kSCControlPrefsTypeID,
							       size,
							       NULL);
	if (control == NULL) {
		return NULL;
	}

	/* initialize non-zero/NULL members */
	control->prefsPlist = strdup(prefsPlist);

	prefs = get_prefs(control);
	if (prefs == NULL) {
		goto done;
	}
	if (scheduler->ptr != NULL) {
		SCPreferencesContext	context	= { .info = control };
		Boolean			ok;

		control->scheduler = *scheduler;
		control->callback = callback;
		ok = SCPreferencesSetCallback(prefs, prefs_changed_sc, &context);
		if (!ok) {
			SC_log(LOG_NOTICE,
			       "SCPreferencesSetCallBack() failed: %s",
			       SCErrorString(SCError()));
			goto done;
		}
		if (scheduler->use_queue) {
			ok = SCPreferencesSetDispatchQueue(prefs, scheduler->queue);
			if (!ok) {
				SC_log(LOG_NOTICE,
				       "SCPreferencesSetDisaptchQueue() failed: %s",
				       SCErrorString(SCError()));
				(void) SCPreferencesSetCallback(prefs, NULL, NULL);
			}
		}
		else {
			CFRetain(scheduler->runloop);
			ok = SCPreferencesScheduleWithRunLoop(prefs, scheduler->runloop,
							      kCFRunLoopCommonModes);
			if (!ok) {
				SC_log(LOG_NOTICE,
				       "SCPreferencesScheduleWithRunLoop() failed: %s",
				       SCErrorString(SCError()));
				(void) SCPreferencesSetCallback(prefs, NULL, NULL);
			}
		}
#if	TARGET_OS_IPHONE
		enable_prefs_observer((_SCControlPrefsRef)control);
#endif	// TARGET_OS_IPHONE
	}

    done :

	return (_SCControlPrefsRef)control;
}


_SCControlPrefsRef
_SCControlPrefsCreate(const char		*prefsPlist,
		      CFRunLoopRef		runloop,
		      _SCControlPrefsCallBack	callback)
{
	WorkScheduler	scheduler = {
		.runloop = runloop,
		.use_queue = FALSE
	};
	return _SCControlPrefsCreateCommon(prefsPlist, &scheduler, callback);
}

_SCControlPrefsRef
_SCControlPrefsCreateWithQueue(const char		*prefsPlist,
			       dispatch_queue_t		queue,
			       _SCControlPrefsCallBack	callback)
{
	WorkScheduler	scheduler = {
		.queue = queue,
		.use_queue = TRUE
	};
	return _SCControlPrefsCreateCommon(prefsPlist, &scheduler, callback);
}

Boolean
_SCControlPrefsGetBoolean(_SCControlPrefsRef	control,
			  CFStringRef		key)
{
	CFBooleanRef			bVal;
	Boolean				done = FALSE;
	Boolean				enabled		= FALSE;
	SCPreferencesRef		prefs;

#if	TARGET_OS_IPHONE
	prefs = get_managed_prefs(control);
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
		prefs = get_prefs(control);
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
	Boolean			ok		= FALSE;
	SCPreferencesRef	prefs;

	prefs = get_prefs(control);
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

#ifdef TEST_SCCONTROL_PREFS

/*
 * How to run the test harness
 * 1) run either dispatch or runloop variant
 * dispatch:  	./sccontrolprefs com.apple.IPMonitor.control.plist
 * runloop:	./sccontrolprefs -r com.apple.IPMonitor.control.plist
 * 2) scutil --log IPMonitor on | off
 * 3) changed_callback() should output "Changed"
 */

#include <getopt.h>

static void
usage(const char * progname)
{
	fprintf(stderr,
		"usage: %s <prefs>", progname);
	exit(1);
}

static void
changed_callback(_SCControlPrefsRef control)
{
	printf("Changed\n");
}

int
main(int argc, char * argv[])
{
	int 			ch;
	_SCControlPrefsRef	control;
	const char *		prefs_id;
	const char *		progname = argv[0];
	Boolean			use_queue = TRUE;

	while ((ch = getopt(argc, argv, "r")) != -1) {
		switch (ch) {
		case 'r':
			use_queue = FALSE;
			break;
		default:
			usage(progname);
			break;
		}
	}
	if (optind == argc) {
		usage(progname);
	}
	prefs_id = argv[optind];
	if (use_queue) {
		dispatch_queue_t	queue;

		printf("Notifications using dispatch queue\n");
		queue = dispatch_queue_create("sccontrolprefs", NULL);
		control = _SCControlPrefsCreateWithQueue(prefs_id, queue,
							 changed_callback);
		if (control == NULL) {
			fprintf(stderr,
				"_SCControlPrefsCreateWithQueue failed\n");
			exit(2);
		}
		SCPrint(TRUE, stdout, CFSTR("control %@\n"), control);
		dispatch_main();
	}
	else {
		printf("Notifications using runloop\n");
		control = _SCControlPrefsCreate(prefs_id,
						CFRunLoopGetCurrent(),
						changed_callback);
		if (control == NULL) {
			fprintf(stderr,
				"_SCControlPrefsCreate failed\n");
			exit(2);
		}
		SCPrint(TRUE, stdout, CFSTR("control %@\n"), control);
		CFRunLoopRun();
	}
	exit(0);
	return (0);
}

#endif /* TEST_SCCONTROL_PREFS */
