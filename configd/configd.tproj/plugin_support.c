/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * June 11, 2001		Allan Nathanson <ajn@apple.com>
 * - start using CFBundle code
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * May 26, 2000			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <mach-o/dyld.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <unistd.h>
#include <NSSystemDirectories.h>

#include "configd.h"
#include <SystemConfiguration/SCDPlugin.h>
void	_SCDPluginExecInit();


/*
 * path components, extensions, entry points, ...
 */
#define	BUNDLE_DIRECTORY	"/SystemConfiguration"	/* [/System/Library]/... */
#define	BUNDLE_DIR_EXTENSION	".bundle"


static	CFMutableArrayRef	allBundles	= NULL;


/* exception handling functions */
typedef kern_return_t (*cer_func_t)		(mach_port_t		exception_port,
						 mach_port_t		thread,
						 mach_port_t		task,
						 exception_type_t	exception,
						 exception_data_t	code,
						 mach_msg_type_number_t	codeCnt);

typedef kern_return_t (*cer_state_func_t)	(mach_port_t		exception_port,
						 exception_type_t	exception,
						 exception_data_t	code,
						 mach_msg_type_number_t	codeCnt,
						 int			*flavor,
						 thread_state_t		old_state,
						 mach_msg_type_number_t	old_stateCnt,
						 thread_state_t		new_state,
						 mach_msg_type_number_t	*new_stateCnt);

typedef kern_return_t (*cer_identity_func_t)	(mach_port_t		exception_port,
						 mach_port_t		thread,
						 mach_port_t		task,
						 exception_type_t	exception,
						 exception_data_t	code,
						 mach_msg_type_number_t	codeCnt,
						 int			*flavor,
						 thread_state_t		old_state,
						 mach_msg_type_number_t	old_stateCnt,
						 thread_state_t		new_state,
						 mach_msg_type_number_t	*new_stateCnt);

static cer_func_t		catch_exception_raise_func          = NULL;
static cer_state_func_t		catch_exception_raise_state_func    = NULL;
static cer_identity_func_t	catch_exception_raise_identity_func = NULL;

kern_return_t
catch_exception_raise(mach_port_t		exception_port,
		      mach_port_t		thread,
		      mach_port_t		task,
		      exception_type_t		exception,
		      exception_data_t		code,
		      mach_msg_type_number_t	codeCnt)
{

	if (catch_exception_raise_func == NULL) {
		/* The user hasn't defined catch_exception_raise in their binary */
		abort();
	}
	return (*catch_exception_raise_func)(exception_port,
					     thread,
					     task,
					     exception,
					     code,
					     codeCnt);
}


kern_return_t
catch_exception_raise_state(mach_port_t			exception_port,
			    exception_type_t		exception,
			    exception_data_t		code,
			    mach_msg_type_number_t	codeCnt,
			    int				*flavor,
			    thread_state_t		old_state,
			    mach_msg_type_number_t	old_stateCnt,
			    thread_state_t		new_state,
			    mach_msg_type_number_t	*new_stateCnt)
{
	if (catch_exception_raise_state_func == 0) {
		/* The user hasn't defined catch_exception_raise_state in their binary */
		abort();
	}
	return (*catch_exception_raise_state_func)(exception_port,
						   exception,
						   code,
						   codeCnt,
						   flavor,
						   old_state,
						   old_stateCnt,
						   new_state,
						   new_stateCnt);
}


kern_return_t
catch_exception_raise_state_identity(mach_port_t		exception_port,
				     mach_port_t		thread,
				     mach_port_t		task,
				     exception_type_t		exception,
				     exception_data_t		code,
				     mach_msg_type_number_t	codeCnt,
				     int			*flavor,
				     thread_state_t		old_state,
				     mach_msg_type_number_t	old_stateCnt,
				     thread_state_t		new_state,
				     mach_msg_type_number_t	*new_stateCnt)
{
	if (catch_exception_raise_identity_func == 0) {
		/* The user hasn't defined catch_exception_raise_identify in their binary */
		abort();
	}
	return (*catch_exception_raise_identity_func)(exception_port,
						      thread,
						      task,
						      exception,
						      code,
						      codeCnt,
						      flavor,
						      old_state,
						      old_stateCnt,
						      new_state,
						      new_stateCnt);
}


static CFStringRef
shortBundleIdentifier(CFStringRef bundleID)
{
	CFIndex         len	= CFStringGetLength(bundleID);
	CFRange         range;
	CFStringRef	shortID	= NULL;

	if (CFStringFindWithOptions(bundleID,
				    CFSTR("."),
				    CFRangeMake(0, len),
				    kCFCompareBackwards,
				    &range)) {
		range.location = range.location + range.length;
		range.length   = len - range.location;
		shortID = CFStringCreateWithSubstring(NULL, bundleID, range);
	}

	return shortID;
}


static void
loadBundle(const void *value, void *context) {
	CFBundleRef				bundle		= (CFBundleRef)value;
	CFStringRef				bundleID	= CFBundleGetIdentifier(bundle);
	Boolean					bundleExclude	= FALSE;
	Boolean					bundleVerbose	= FALSE;
	CFDictionaryRef				dict;
	void					*func;
	SCDynamicStoreBundleLoadFunction	load;
	Boolean					loaded;
	CFIndex					*nLoaded	= (CFIndex *)context;

	SCLog(TRUE, LOG_DEBUG, CFSTR("loading %@"), bundleID);

	bundleExclude = CFSetContainsValue(_plugins_exclude, bundleID);
	if (!bundleExclude) {
		CFStringRef	shortID	= shortBundleIdentifier(bundleID);

		if (shortID) {
			bundleExclude = CFSetContainsValue(_plugins_exclude, shortID);
			CFRelease(shortID);
		}
	}

	if (bundleExclude) {
		SCLog(TRUE,
		      LOG_DEBUG,
		      CFSTR("%@ load skipped"),
		      bundleID);
		return;
	}

	loaded = CFBundleLoadExecutable(bundle);
	if (!loaded) {
		SCLog(TRUE,
		      LOG_NOTICE,
		      CFSTR("%@ load failed"),
		      bundleID);
		return;
	}

	if (!CFBundleIsExecutableLoaded(bundle)) {
		SCLog(TRUE,
		      LOG_NOTICE,
		      CFSTR("%@ executable not loaded"),
		      bundleID);
		return;
	}

	/* bump the count of loaded bundles */
	*nLoaded = *nLoaded + 1;

	/* identify any exception handling functions */

	func = CFBundleGetFunctionPointerForName(bundle, CFSTR("catch_exception_raise"));
	if (func) {
		catch_exception_raise_func = func;
	}

	func = CFBundleGetFunctionPointerForName(bundle, CFSTR("catch_exception_raise_state"));
	if (func) {
		catch_exception_raise_state_func = func;
	}

	func = CFBundleGetFunctionPointerForName(bundle, CFSTR("catch_exception_raise_identity"));
	if (func) {
		catch_exception_raise_identity_func = func;
	}

	/* if defined, call the bundles load() function */

	load = CFBundleGetFunctionPointerForName(bundle, CFSTR("load"));
	if (!load) {
		return;
	}

	bundleVerbose = CFSetContainsValue(_plugins_verbose, bundleID);
	if (!bundleVerbose) {
		CFStringRef	shortID	= shortBundleIdentifier(bundleID);

		if (shortID) {
			bundleVerbose = CFSetContainsValue(_plugins_verbose, shortID);
			CFRelease(shortID);
		}
	}

	if (!bundleVerbose) {
		dict = CFBundleGetInfoDictionary(bundle);
		if (isA_CFDictionary(dict)) {
			CFBooleanRef	bVal;

			bVal = CFDictionaryGetValue(dict, kSCBundleVerbose);
			if (isA_CFBoolean(bVal) && CFBooleanGetValue(bVal)) {
				bundleVerbose = TRUE;
			}
		}
	}

	(*load)(bundle, bundleVerbose);
	return;
}


static void
startBundle(const void *value, void *context) {
	CFBundleRef				bundle		= (CFBundleRef)value;
	CFURLRef				bundleURL;
	char					bundleName[MAXNAMLEN + 1];
	char					bundlePath[MAXPATHLEN];
	char					*cp;
	CFDictionaryRef				dict;
	int					len;
	Boolean					ok;
	SCDynamicStoreBundleStartFunction	start;

	if (!CFBundleIsExecutableLoaded(bundle)) {
		return;
	}

	start = CFBundleGetFunctionPointerForName(bundle, CFSTR("start"));
	if (!start) {
		return;
	}

	dict = isA_CFDictionary(CFBundleGetInfoDictionary(bundle));
	if (!dict) {
		return;
	}

	bundleURL = CFBundleCopyBundleURL(bundle);
	if (!bundleURL) {
		return;
	}

	ok = CFURLGetFileSystemRepresentation(bundleURL,
					      TRUE,
					      (UInt8 *)&bundlePath,
					      sizeof(bundlePath));
	CFRelease(bundleURL);
	if (!ok) {
		return;
	}

	cp = strrchr(bundlePath, '/');
	if (cp) {
		cp++;
	} else {
		cp = bundlePath;
	}

	/* check if this directory entry is a valid bundle name */
	len = strlen(cp);
	if (len <= (int)sizeof(BUNDLE_DIR_EXTENSION)) {
		/* if entry name isn't long enough */
		return;
	}

	len -= sizeof(BUNDLE_DIR_EXTENSION) - 1;
	if (strcmp(&cp[len], BUNDLE_DIR_EXTENSION) != 0) {
		/* if entry name doesn end with ".bundle" */
		return;
	}

	/* get (just) the bundle's name */
	bundleName[0] = '\0';
	(void) strncat(bundleName, cp, len);

	(*start)(bundleName, bundlePath);
	return;
}


static void
primeBundle(const void *value, void *context) {
	CFBundleRef				bundle		= (CFBundleRef)value;
	SCDynamicStoreBundlePrimeFunction	prime;

	if (!CFBundleIsExecutableLoaded(bundle)) {
		return;
	}

	prime = CFBundleGetFunctionPointerForName(bundle, CFSTR("prime"));
	if (!prime) {
		return;
	}

	(*prime)();
	return;
}


#ifdef	DEBUG

static void
timerCallback(CFRunLoopTimerRef timer, void *info)
{
	SCLog(_configd_verbose,
	      LOG_INFO,
	      CFSTR("the CFRunLoop is waiting for something to happen...."));
	return;
}

#endif	/* DEBUG */


static void
sortBundles(CFMutableArrayRef orig)
{
	CFMutableArrayRef   new;

	new = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	while (CFArrayGetCount(orig) > 0) {
		int	i;
		Boolean	inserted	= FALSE;
		int	nOrig		= CFArrayGetCount(orig);

		for (i = 0; i < nOrig; i++) {
			CFBundleRef	bundle1	  = (CFBundleRef)CFArrayGetValueAtIndex(orig, i);
			CFStringRef	bundleID1 = CFBundleGetIdentifier(bundle1);
			int		count;
			CFDictionaryRef	dict;
			int		j;
			int		nRequires;
			CFArrayRef	requires  = NULL;

			dict = isA_CFDictionary(CFBundleGetInfoDictionary(bundle1));
			if (dict) {
				requires = CFDictionaryGetValue(dict, kSCBundleRequires);
				requires = isA_CFArray(requires);
			}
			if (bundleID1 == NULL || requires == NULL) {
				CFArrayInsertValueAtIndex(new, 0, bundle1);
				CFArrayRemoveValueAtIndex(orig, i);
				inserted = TRUE;
				break;
			}
			count = nRequires = CFArrayGetCount(requires);
			for (j = 0; j < nRequires; j++) {
				int		k;
				int		nNew;
				CFStringRef	r	= CFArrayGetValueAtIndex(requires, j);

				nNew = CFArrayGetCount(new);
				for (k = 0; k < nNew; k++) {
					CFBundleRef	bundle2	  = (CFBundleRef)CFArrayGetValueAtIndex(new, k);
					CFStringRef	bundleID2 = CFBundleGetIdentifier(bundle2);

					if (bundleID2 && CFEqual(bundleID2, r)) {
						count--;
					}
				}
			}
			if (count == 0) {
				/* all dependencies are met, append */
				CFArrayAppendValue(new, bundle1);
				CFArrayRemoveValueAtIndex(orig, i);
				inserted = TRUE;
				break;
			}
		}

		if (inserted == FALSE) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("Bundles have circular dependency!!!"));
			break;
		}
	}
	if (CFArrayGetCount(orig) > 0) {
		/* we have a circular dependency, append remaining items on new array */
		CFArrayAppendArray(new, orig, CFRangeMake(0, CFArrayGetCount(orig)));
	}
	else {
		/* new one is a sorted version of original */
	}

	CFArrayRemoveAllValues(orig);
	CFArrayAppendArray(orig, new, CFRangeMake(0, CFArrayGetCount(new)));
	CFRelease(new);
	return;
}


__private_extern__
void *
plugin_exec(void *arg)
{
	CFIndex			nLoaded		= 0;

	/* keep track of bundles */
	allBundles = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/* allow plug-ins to exec child/helper processes */
	_SCDPluginExecInit();

	if (arg == NULL) {
		char				path[MAXPATHLEN];
		NSSearchPathEnumerationState	state;

		/*
		 * identify and load all bundles
		 */
		state = NSStartSearchPathEnumeration(NSLibraryDirectory,
						     NSLocalDomainMask|NSSystemDomainMask);
		while ((state = NSGetNextSearchPathEnumeration(state, path))) {
			CFArrayRef	bundles;
			CFURLRef	url;

			/* load any available bundle */
			strcat(path, BUNDLE_DIRECTORY);
			SCLog(_configd_verbose, LOG_DEBUG, CFSTR("searching for bundles in \".\""));
			url = CFURLCreateFromFileSystemRepresentation(NULL,
								      path,
								      strlen(path),
								      TRUE);
			bundles = CFBundleCreateBundlesFromDirectory(NULL, url, CFSTR(".bundle"));
			CFRelease(url);

			if (bundles) {
				CFArrayAppendArray(allBundles,
						   bundles,
						   CFRangeMake(0, CFArrayGetCount(bundles)));
				CFRelease(bundles);
			}
		}

		sortBundles(allBundles);
	} else {
		CFBundleRef	bundle;
		CFURLRef	url;

		/*
		 * load (only) the specified bundle
		 */
		url = CFURLCreateFromFileSystemRepresentation(NULL,
							      (char *)arg,
							      strlen((char *)arg),
							      TRUE);
		bundle = CFBundleCreate(NULL, url);
		if (bundle) {
			CFArrayAppendValue(allBundles, bundle);
			CFRelease(bundle);
		}
		CFRelease(url);
	}

	/*
	 * load each bundle and, if defined, call its load() function.  This
	 * function (or the start() function) should initialize any variables,
	 * open any sessions with "configd", and register any needed notifications.
	 *
	 * Note: Establishing initial information in the store should be
	 *       deferred until the prime() initialization function so that
	 *       any bundles which want to receive a notification that the
	 *       data has changed will have an opportunity to install a
	 *       notification handler.
	 */
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("calling bundle load() functions"));
	CFArrayApplyFunction(allBundles,
			     CFRangeMake(0, CFArrayGetCount(allBundles)),
			     loadBundle,
			     &nLoaded);

	/*
	 * If defined, call each bundles start() function.  This function is
	 * called after the bundle has been loaded and its load() function has
	 * been called.  It should initialize any variables, open any sessions
	 * with "configd", and register any needed notifications.
	 *
	 * Note: Establishing initial information in the store should be
	 *       deferred until the prime() initialization function so that
	 *       any bundles which want to receive a notification that the
	 *       data has changed will have an opportunity to install a
	 *       notification handler.
	 */
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("calling bundle start() functions"));
	CFArrayApplyFunction(allBundles,
			     CFRangeMake(0, CFArrayGetCount(allBundles)),
			     startBundle,
			     NULL);

	/*
	 * If defined, call each bundles prime() function.  This function is
	 * called after the bundle has been loaded and its load() and start()
	 * functions have been called.  It should initialize any configuration
	 * information and/or state in the store.
	 */
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("calling bundle prime() functions"));
	CFArrayApplyFunction(allBundles,
			     CFRangeMake(0, CFArrayGetCount(allBundles)),
			     primeBundle,
			     NULL);

#ifdef	DEBUG
	if (arg == NULL && (nLoaded > 0)) {
		CFRunLoopTimerRef	timer;

		/* allocate a periodic event (to help show we're not blocking) */
		timer = CFRunLoopTimerCreate(NULL,				/* allocator */
					     CFAbsoluteTimeGetCurrent() + 1.0,	/* fireDate */
					     60.0,				/* interval */
					     0,					/* flags */
					     0,					/* order */
					     timerCallback,			/* callout */
					     NULL);				/* context */
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
		CFRelease(timer);
	}
#endif	/* DEBUG */

	/*
	 * The assumption is that each loaded plugin will establish CFMachPortRef,
	 * CFSocketRef, and CFRunLoopTimerRef input sources to handle any events
	 * and register these sources with this threads run loop. If the plugin
	 * needs to wait and/or block at any time it should do so only in its a
	 * private thread.
	 */
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("starting plugin CFRunLoop"));
	CFRunLoopRun();
	SCLog(_configd_verbose, LOG_INFO, CFSTR("what, no more work for the \"configd\" bundles?"));
	return NULL;
}


__private_extern__
void
plugin_init()
{
	pthread_attr_t	tattr;
	pthread_t	tid;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Starting thread for plug-ins..."));
	pthread_attr_init(&tattr);
	pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
//      pthread_attr_setstacksize(&tattr, 96 * 1024); // each thread gets a 96K stack
	pthread_create(&tid, &tattr, plugin_exec, NULL);
	pthread_attr_destroy(&tattr);
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  thread id=0x%08x"), tid);

	return;
}
