/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
 * October 30, 2003		Allan Nathanson <ajn@apple.com>
 * - add plugin "stop()" function support
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
#include <sysexits.h>
#include <unistd.h>
#include <NSSystemDirectories.h>

#include "configd.h"
#include "configd_server.h"
#include <SystemConfiguration/SCDPlugin.h>
void	_SCDPluginExecInit();


/*
 * path components, extensions, entry points, ...
 */
#define	BUNDLE_DIRECTORY	"/SystemConfiguration"	/* [/System/Library]/... */
#define	BUNDLE_DIR_EXTENSION	".bundle"


typedef struct {
	CFBundleRef				bundle;
	Boolean					loaded;
	Boolean					builtin;
	Boolean					verbose;
	SCDynamicStoreBundleLoadFunction	load;
	SCDynamicStoreBundleStartFunction	start;
	SCDynamicStoreBundlePrimeFunction	prime;
	SCDynamicStoreBundleStopFunction	stop;
} *bundleInfoRef;


// all loaded bundles
static CFMutableArrayRef	allBundles	= NULL;

// exiting bundles
static CFMutableDictionaryRef	exiting		= NULL;

// plugin CFRunLoopRef
static CFRunLoopRef		plugin_runLoop	= NULL;


#ifdef	ppc
//extern SCDynamicStoreBundleLoadFunction	load_ATconfig;
//extern SCDynamicStoreBundleStopFunction	stop_ATconfig;
#endif	/* ppc */
extern SCDynamicStoreBundleLoadFunction		load_IPMonitor;
extern SCDynamicStoreBundlePrimeFunction	prime_IPMonitor;
extern SCDynamicStoreBundleLoadFunction		load_InterfaceNamer;
extern SCDynamicStoreBundleLoadFunction		load_KernelEventMonitor;
extern SCDynamicStoreBundlePrimeFunction	prime_KernelEventMonitor;
extern SCDynamicStoreBundleLoadFunction		load_Kicker;
extern SCDynamicStoreBundleLoadFunction		load_LinkConfiguration;
extern SCDynamicStoreBundleLoadFunction		load_PreferencesMonitor;
extern SCDynamicStoreBundlePrimeFunction	prime_PreferencesMonitor;
extern SCDynamicStoreBundleStopFunction		stop_PreferencesMonitor;


typedef struct {
	const CFStringRef	bundleID;
	const void		*load;		// SCDynamicStoreBundleLoadFunction
	const void		*start;		// SCDynamicStoreBundleStartFunction
	const void		*prime;		// SCDynamicStoreBundlePrimeFunction
	const void		*stop;		// SCDynamicStoreBundleStopFunction
} builtin, *builtinRef;


static const builtin builtin_plugins[] = {
#ifdef	ppc
//	{
//		CFSTR("com.apple.SystemConfiguration.ATconfig"),
//		&load_ATconfig,
//		NULL,
//		NULL,
//		&stop_ATconfig
//	},
#endif	/* ppc */
	{
		CFSTR("com.apple.SystemConfiguration.IPMonitor"),
		&load_IPMonitor,
		NULL,
		&prime_IPMonitor,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.InterfaceNamer"),
		&load_InterfaceNamer,
		NULL,
		NULL,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.KernelEventMonitor"),
		&load_KernelEventMonitor,
		NULL,
		&prime_KernelEventMonitor,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.Kicker"),
		&load_Kicker,
		NULL,
		NULL,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.LinkConfiguration"),
		&load_LinkConfiguration,
		NULL,
		NULL,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.PreferencesMonitor"),
		&load_PreferencesMonitor,
		NULL,
		&prime_PreferencesMonitor,
		&stop_PreferencesMonitor
	}
};


static void
addBundle(CFBundleRef bundle)
{
	CFDictionaryRef		bundleDict;
	bundleInfoRef		bundleInfo;
	
	bundleInfo = CFAllocatorAllocate(NULL, sizeof(*bundleInfo), 0);
	bundleInfo->bundle	= (CFBundleRef)CFRetain(bundle);
	bundleInfo->loaded	= FALSE;
	bundleInfo->builtin	= FALSE;
	bundleInfo->verbose	= FALSE;
	bundleInfo->load	= NULL;
	bundleInfo->start	= NULL;
	bundleInfo->prime	= NULL;
	bundleInfo->stop	= NULL;
	
	bundleDict = CFBundleGetInfoDictionary(bundle);
	if (isA_CFDictionary(bundleDict)) {
		CFBooleanRef	bVal;

		bVal = CFDictionaryGetValue(bundleDict, kSCBundleIsBuiltinKey);
		if (isA_CFBoolean(bVal) && CFBooleanGetValue(bVal)) {
			bundleInfo->builtin = TRUE;
		}

		bVal = CFDictionaryGetValue(bundleDict, kSCBundleVerboseKey);
		if (isA_CFBoolean(bVal) && CFBooleanGetValue(bVal)) {
			bundleInfo->verbose = TRUE;
		}
	}

	CFArrayAppendValue(allBundles, bundleInfo);
	return;
}


static CFStringRef
shortBundleIdentifier(CFStringRef bundleID)
{
	CFIndex		len	= CFStringGetLength(bundleID);
	CFRange		range;
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


static void *
getBundleSymbol(CFBundleRef bundle, CFStringRef functionName, CFStringRef shortID)
{
	void	*func;

	// search for load(), start(), prime(), stop(), ...
	func = CFBundleGetFunctionPointerForName(bundle, functionName);
	if (func != NULL) {
		return func;
	}

	if (shortID != NULL) {
		CFStringRef	altFunctionName;

		// search for load_XXX(), ...
		altFunctionName = CFStringCreateWithFormat(NULL,
						    NULL,
						    CFSTR("%@_%@"),
						    functionName,
						    shortID);
		func = CFBundleGetFunctionPointerForName(bundle, altFunctionName);
		CFRelease(altFunctionName);
	}

	return func;
}


static void
loadBundle(const void *value, void *context) {
	CFStringRef	bundleID;
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;
	Boolean		bundleExclude;
	CFIndex		*nLoaded	= (CFIndex *)context;
	CFStringRef	shortID;

	bundleID = CFBundleGetIdentifier(bundleInfo->bundle);
	if (bundleID == NULL) {
		// sorry, no bundles without a bundle identifier
		SCLog(TRUE, LOG_DEBUG, CFSTR("skipped %@"), bundleInfo->bundle);
		return;
	}

	shortID = shortBundleIdentifier(bundleID);

	bundleExclude = CFSetContainsValue(_plugins_exclude, bundleID);
	if (bundleExclude) {
		if (shortID != NULL) {
			bundleExclude = CFSetContainsValue(_plugins_exclude, shortID);
		}
	}

	if (bundleExclude) {
		// sorry, this bundle has been excluded
		SCLog(TRUE, LOG_DEBUG, CFSTR("excluded %@"), bundleID);
		goto done;
	}

	if (!bundleInfo->verbose) {
		bundleInfo->verbose = CFSetContainsValue(_plugins_verbose, bundleID);
		if (!bundleInfo->verbose) {
			if (shortID != NULL) {
				bundleInfo->verbose = CFSetContainsValue(_plugins_verbose, shortID);
			}
		}
	}

	if (bundleInfo->builtin) {
		int	i;

		SCLog(TRUE, LOG_DEBUG, CFSTR("adding  %@"), bundleID);

		for (i = 0; i < sizeof(builtin_plugins)/sizeof(builtin_plugins[0]); i++) {
			if (CFEqual(bundleID, builtin_plugins[i].bundleID)) {
				bundleInfo->load  = builtin_plugins[i].load;
				bundleInfo->start = builtin_plugins[i].start;
				bundleInfo->prime = builtin_plugins[i].prime;
				bundleInfo->stop  = builtin_plugins[i].stop;
				break;
			}
		}
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("loading %@"), bundleID);

		if (!CFBundleLoadExecutable(bundleInfo->bundle)) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("%@ load failed"), bundleID);
			goto done;
		}

		// get bundle entry points
		bundleInfo->load  = getBundleSymbol(bundleInfo->bundle, CFSTR("load" ), shortID);
		bundleInfo->start = getBundleSymbol(bundleInfo->bundle, CFSTR("start"), shortID);
		bundleInfo->prime = getBundleSymbol(bundleInfo->bundle, CFSTR("prime"), shortID);
		bundleInfo->stop  = getBundleSymbol(bundleInfo->bundle, CFSTR("stop" ), shortID);
	}

	/* mark this bundle as having been loaded */
	bundleInfo->loaded = TRUE;

	/* bump the count of loaded bundles */
	*nLoaded = *nLoaded + 1;

    done :

	if (shortID != NULL)	CFRelease(shortID);
	return;
}


void
callLoadFunction(const void *value, void *context) {
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;

	if (!bundleInfo->loaded) {
		return;
	}

	if (bundleInfo->load == NULL) {
		// if no load() function
		return;
	}

	(*bundleInfo->load)(bundleInfo->bundle, bundleInfo->verbose);
	return;
}


void
callStartFunction(const void *value, void *context) {
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;
	CFURLRef	bundleURL;
	char		bundleName[MAXNAMLEN + 1];
	char		bundlePath[MAXPATHLEN];
	char		*cp;
	int		len;
	Boolean		ok;

	if (!bundleInfo->loaded) {
		return;
	}

	if (bundleInfo->start == NULL) {
		// if no start() function
		return;
	}

	bundleURL = CFBundleCopyBundleURL(bundleInfo->bundle);
	if (bundleURL == NULL) {
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

	(*bundleInfo->start)(bundleName, bundlePath);
	return;
}


void
callPrimeFunction(const void *value, void *context) {
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;

	if (!bundleInfo->loaded) {
		return;
	}

	if (bundleInfo->prime == NULL) {
		// if no prime() function
		return;
	}

	(*bundleInfo->prime)();
	return;
}


static void
stopComplete(void *info)
{
	CFBundleRef		bundle		= (CFBundleRef)info;
	CFStringRef		bundleID	= CFBundleGetIdentifier(bundle);
	CFRunLoopSourceRef	stopRls;

	SCLog(TRUE, LOG_DEBUG, CFSTR("** %@ complete (%f)"), bundleID, CFAbsoluteTimeGetCurrent());

	stopRls = (CFRunLoopSourceRef)CFDictionaryGetValue(exiting, bundle);
	CFRunLoopSourceInvalidate(stopRls);

	CFDictionaryRemoveValue(exiting, bundle);

	if (CFDictionaryGetCount(exiting) == 0) {
		int	status;

		// if all of the plugins are happy
		status = server_shutdown();
		SCLog(TRUE, LOG_DEBUG, CFSTR("server shutdown complete (%f)"), CFAbsoluteTimeGetCurrent());
		exit (status);
	}

	return;
}


static void
stopDelayed(CFRunLoopTimerRef timer, void *info)
{
	const void	**keys;
	CFIndex		i;
	CFIndex		n;
	int		status;

	SCLog(TRUE, LOG_ERR, CFSTR("server shutdown was delayed, unresponsive plugins:"));

	/*
	 * we've asked our plugins to shutdown but someone
	 * isn't listening.
	 */
	n = CFDictionaryGetCount(exiting);
	keys = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
	CFDictionaryGetKeysAndValues(exiting, keys, NULL);
	for (i = 0; i < n; i++) {
		CFBundleRef	bundle;
		CFStringRef	bundleID;

		bundle   = (CFBundleRef)keys[i];
		bundleID = CFBundleGetIdentifier(bundle);
		SCLog(TRUE, LOG_ERR, CFSTR("** %@"), bundleID);
	}
	CFAllocatorDeallocate(NULL, keys);

	status = server_shutdown();
	exit (status);
}

static void
stopBundle(const void *value, void *context) {
	bundleInfoRef			bundleInfo	= (bundleInfoRef)value;
	CFRunLoopSourceRef		stopRls;
	CFRunLoopSourceContext		stopContext	= { 0			// version
							  , bundleInfo->bundle	// info
							  , CFRetain		// retain
							  , CFRelease		// release
							  , CFCopyDescription	// copyDescription
							  , CFEqual		// equal
							  , CFHash		// hash
							  , NULL		// schedule
							  , NULL		// cancel
							  , stopComplete	// perform
							  };

	if (!bundleInfo->loaded) {
		return;
	}

	if (bundleInfo->stop == NULL) {
		// if no stop() function
		return;
	}

	stopRls = CFRunLoopSourceCreate(NULL, 0, &stopContext);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), stopRls, kCFRunLoopDefaultMode);
	CFDictionaryAddValue(exiting, bundleInfo->bundle, stopRls);
	CFRelease(stopRls);

	(*bundleInfo->stop)(stopRls);

	return;
}


static void
stopBundles()
{
	/*
	 * If defined, call each bundles stop() function.  This function is
	 * called when configd has been asked to shut down (via a SIGTERM).  The
	 * function should signal the provided run loop source when it is "ready"
	 * for the shut down to proceeed.
	 */
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("calling bundle stop() functions"));
	CFArrayApplyFunction(allBundles,
			     CFRangeMake(0, CFArrayGetCount(allBundles)),
			     stopBundle,
			     NULL);

	if (CFDictionaryGetCount(exiting) == 0) {
		int	status;

		// if all of the plugins are happy
		status = server_shutdown();
		SCLog(TRUE, LOG_DEBUG, CFSTR("server shutdown complete (%f)"), CFAbsoluteTimeGetCurrent());
		exit (status);
	} else {
		CFRunLoopTimerRef	timer;

		/* sorry, we're not going to wait longer than 20 seconds */
		timer = CFRunLoopTimerCreate(NULL,				/* allocator */
					     CFAbsoluteTimeGetCurrent() + 20.0,	/* fireDate (in 20 seconds) */
					     0.0,				/* interval (== one-shot) */
					     0,					/* flags */
					     0,					/* order */
					     stopDelayed,			/* callout */
					     NULL);				/* context */
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
		CFRelease(timer);
	}

	return;
}


__private_extern__
Boolean
plugin_term(int *status)
{
	CFRunLoopSourceRef	stopRls;
	CFRunLoopSourceContext	stopContext = { 0		// version
					      , NULL		// info
					      , NULL		// retain
					      , NULL		// release
					      , NULL		// copyDescription
					      , NULL		// equal
					      , NULL		// hash
					      , NULL		// schedule
					      , NULL		// cancel
					      , stopBundles	// perform
					      };

	if (plugin_runLoop == NULL) {
		// if no plugins
		*status = EX_OK;
		return FALSE;	// don't delay shutdown
	}

	if (exiting != NULL) {
		// if shutdown already active
		return TRUE;
	}

	SCLog(TRUE, LOG_DEBUG, CFSTR("starting server shutdown (%f)"), CFAbsoluteTimeGetCurrent());

	exiting = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

	stopRls = CFRunLoopSourceCreate(NULL, 0, &stopContext);
	CFRunLoopAddSource(plugin_runLoop, stopRls, kCFRunLoopDefaultMode);
	CFRunLoopSourceSignal(stopRls);
	CFRelease(stopRls);
	CFRunLoopWakeUp(plugin_runLoop);

	return TRUE;
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
	CFMutableArrayRef	new;

	new = CFArrayCreateMutable(NULL, 0, NULL);
	while (CFArrayGetCount(orig) > 0) {
		int	i;
		Boolean	inserted	= FALSE;
		int	nOrig		= CFArrayGetCount(orig);

		for (i = 0; i < nOrig; i++) {
			bundleInfoRef	bundleInfo1	= (bundleInfoRef)CFArrayGetValueAtIndex(orig, i);
			CFStringRef	bundleID1	= CFBundleGetIdentifier(bundleInfo1->bundle);
			int		count;
			CFDictionaryRef	dict;
			int		j;
			int		nRequires;
			CFArrayRef	requires  = NULL;

			dict = isA_CFDictionary(CFBundleGetInfoDictionary(bundleInfo1->bundle));
			if (dict) {
				requires = CFDictionaryGetValue(dict, kSCBundleRequiresKey);
				requires = isA_CFArray(requires);
			}
			if (bundleID1 == NULL || requires == NULL) {
				CFArrayInsertValueAtIndex(new, 0, bundleInfo1);
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
					bundleInfoRef	bundleInfo2	= (bundleInfoRef)CFArrayGetValueAtIndex(new, k);
					CFStringRef	bundleID2	= CFBundleGetIdentifier(bundleInfo2->bundle);

					if (bundleID2 && CFEqual(bundleID2, r)) {
						count--;
					}
				}
			}
			if (count == 0) {
				/* all dependencies are met, append */
				CFArrayAppendValue(new, bundleInfo1);
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
	CFIndex		nLoaded		= 0;

	/* keep track of bundles */
	allBundles = CFArrayCreateMutable(NULL, 0, NULL);

	/* allow plug-ins to exec child/helper processes */
	_SCDPluginExecInit();

	if (arg == NULL) {
		char				path[MAXPATHLEN];
		NSSearchPathEnumerationState	state;

		/*
		 * identify and load all bundles
		 */
		state = NSStartSearchPathEnumeration(NSLibraryDirectory,
						     NSSystemDomainMask);
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

			if (bundles != NULL) {
				CFIndex	i;
				CFIndex	n;

				n = CFArrayGetCount(bundles);
				for (i = 0; i < n; i++) {
					CFBundleRef	bundle;
					
					bundle = (CFBundleRef)CFArrayGetValueAtIndex(bundles, i);
					addBundle(bundle);
				}
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
		if (bundle != NULL) {
			addBundle(bundle);
			CFRelease(bundle);
		}
		CFRelease(url);
	}

	/*
	 * load each bundle.
	 */
	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("loading bundles"));
	CFArrayApplyFunction(allBundles,
			     CFRangeMake(0, CFArrayGetCount(allBundles)),
			     loadBundle,
			     &nLoaded);

	/*
	 * If defined, call each bundles load() function.  This function (or
	 * the start() function) should initialize any variables, open any
	 * sessions with "configd", and register any needed notifications.
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
			     callLoadFunction,
			     NULL);

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
			     callStartFunction,
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
			     callPrimeFunction,
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
	plugin_runLoop = CFRunLoopGetCurrent();
	CFRunLoopRun();

	SCLog(_configd_verbose, LOG_INFO, CFSTR("No more work for the \"configd\" plugins"));
	plugin_runLoop = NULL;
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
