/*
 * Copyright (c) 2000-2022 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sysdir.h>
#include <sysexits.h>
#include <unistd.h>

#include "configd.h"
#include "configd_server.h"
#include <SystemConfiguration/SCDPlugin.h>
#include "SystemConfigurationInternal.h"


/*
 * path components, extensions, entry points, ...
 */
#define	BUNDLE_DIRECTORY	"/SystemConfiguration"	/* [/System/Library]/... */
#define	BUNDLE_DIR_EXTENSION	".bundle"


#define PLUGIN_ALL(p)		CFSTR(p),
#if	!TARGET_OS_IPHONE
#define PLUGIN_MACOSX(p)	CFSTR(p),
#define PLUGIN_IOS(p)
#else	// !TARGET_OS_IPHONE
#define PLUGIN_MACOSX(p)
#define PLUGIN_IOS(p)		CFSTR(p),
#endif	// !TARGET_OS_IPHONE

/*
 * plugin_allow_list[]
 * - the platform dependent list of allowed bundleIDs
 */
static const CFStringRef	plugin_allow_list[]	= {
	PLUGIN_MACOSX("com.apple.SystemConfiguration.ApplicationFirewall")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.InterfaceNamer")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.KernelEventMonitor")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.PreferencesMonitor")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.IPConfiguration")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.IPMonitor")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.EAPOLController")
	PLUGIN_MACOSX("com.apple.SystemConfiguration.ISPreference")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.LinkConfiguration")
	PLUGIN_MACOSX("com.apple.SystemConfiguration.PPPController")
	PLUGIN_ALL   ("com.apple.SystemConfiguration.QoSMarking")
	PLUGIN_MACOSX("com.apple.print.notification")
};
#define	N_PLUGIN_ALLOW_LIST	(sizeof(plugin_allow_list) / sizeof(plugin_allow_list[0]))


typedef struct {
	CFBundleRef				bundle;
	Boolean					loaded;
	Boolean					builtin;
	Boolean					enabled;
	Boolean					forced;
	Boolean					verbose;
	SCDynamicStoreBundleLoadFunction	*load;
	SCDynamicStoreBundleStartFunction	*start;
	SCDynamicStoreBundlePrimeFunction	*prime;
	SCDynamicStoreBundleStopFunction	*stop;
} *bundleInfoRef;


// all plugins to be exec'd
//   val = bundleInfoRef
static CFArrayRef		plugins			= NULL;

// exiting bundles
static CFMutableDictionaryRef	exiting			= NULL;


extern SCDynamicStoreBundleLoadFunction		load_IPMonitor;
extern SCDynamicStoreBundlePrimeFunction	prime_IPMonitor;
#if	!TARGET_OS_SIMULATOR
extern SCDynamicStoreBundleLoadFunction		load_InterfaceNamer;
extern SCDynamicStoreBundleLoadFunction		load_KernelEventMonitor;
extern SCDynamicStoreBundlePrimeFunction	prime_KernelEventMonitor;
extern SCDynamicStoreBundleLoadFunction		load_LinkConfiguration;
extern SCDynamicStoreBundleLoadFunction		load_PreferencesMonitor;
extern SCDynamicStoreBundlePrimeFunction	prime_PreferencesMonitor;
extern SCDynamicStoreBundleLoadFunction		load_QoSMarking;
#endif	// !TARGET_OS_SIMULATOR


typedef struct {
	const CFStringRef			bundleID;
	SCDynamicStoreBundleLoadFunction	*load;
	SCDynamicStoreBundleStartFunction	*start;
	SCDynamicStoreBundlePrimeFunction	*prime;
	SCDynamicStoreBundleStopFunction	*stop;
} builtin, *builtinRef;


static const builtin plugins_builtin[] = {
	{
		CFSTR("com.apple.SystemConfiguration.IPMonitor"),
		load_IPMonitor,
		NULL,
		prime_IPMonitor,
		NULL
	},
#if	!TARGET_OS_SIMULATOR
	{
		CFSTR("com.apple.SystemConfiguration.InterfaceNamer"),
		load_InterfaceNamer,
		NULL,
		NULL,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.KernelEventMonitor"),
		load_KernelEventMonitor,
		NULL,
		prime_KernelEventMonitor,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.LinkConfiguration"),
		load_LinkConfiguration,
		NULL,
		NULL,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.PreferencesMonitor"),
		load_PreferencesMonitor,
		NULL,
		prime_PreferencesMonitor,
		NULL
	},
	{
		CFSTR("com.apple.SystemConfiguration.QoSMarking"),
		load_QoSMarking,
		NULL,
		NULL,
		NULL
	},
#endif	// !TARGET_OS_SIMULATOR
};


static Boolean
pluginIsAllowed(CFStringRef bundleID)
{
	for (size_t i = 0; i < N_PLUGIN_ALLOW_LIST; i++) {
		if (CFEqual(bundleID, plugin_allow_list[i])) {
			return (TRUE);
		}
	}
	return (FALSE);
}

static void
addPlugin(CFMutableDictionaryRef plugins, CFBundleRef bundle, Boolean forceEnabled)
{
	CFStringRef		bundleID;
	CFDictionaryRef		bundleDict;
	bundleInfoRef		bundleInfo;

	bundleID = CFBundleGetIdentifier(bundle);
	if (bundleID == NULL) {
		SC_log(LOG_NOTICE, "%s: ignoring %@ (no bundle ID)",
		       __func__, bundle);
		return;
	}
	if (!pluginIsAllowed(bundleID)) {
		SC_log(LOG_NOTICE,
		       "%s: %@ not in allow list, ignoring",
		       __func__, bundleID);
		return;
	}
	bundleInfo = CFAllocatorAllocate(NULL, sizeof(*bundleInfo), 0);
	bundleInfo->bundle	= (CFBundleRef)CFRetain(bundle);
	bundleInfo->loaded	= FALSE;
	bundleInfo->builtin	= FALSE;
	bundleInfo->enabled	= TRUE;
	bundleInfo->forced	= forceEnabled;
	bundleInfo->verbose	= FALSE;
	bundleInfo->load	= NULL;
	bundleInfo->start	= NULL;
	bundleInfo->prime	= NULL;
	bundleInfo->stop	= NULL;

	bundleDict = CFBundleGetInfoDictionary(bundle);
	if (isA_CFDictionary(bundleDict)) {
		CFBooleanRef	bVal;

		bVal = CFDictionaryGetValue(bundleDict, kSCBundleIsBuiltinKey);
		if (isA_CFBoolean(bVal)) {
			bundleInfo->builtin = CFBooleanGetValue(bVal);
		}

		bVal = CFDictionaryGetValue(bundleDict, kSCBundleEnabledKey);
		if (isA_CFBoolean(bVal)) {
			bundleInfo->enabled = CFBooleanGetValue(bVal);
		}

		bVal = CFDictionaryGetValue(bundleDict, kSCBundleVerboseKey);
		if (isA_CFBoolean(bVal)) {
			bundleInfo->verbose = CFBooleanGetValue(bVal);
		}
	}

	CFDictionaryAddValue(plugins, bundleID, bundleInfo);
	SC_log(LOG_NOTICE, "%s: %@", __func__, bundleID);
	return;
}


static CF_RETURNS_RETAINED CFStringRef
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


static const char *
getBundleDirNameAndPath(CFBundleRef bundle, char *buf, size_t buf_len)
{
	char		*cp;
	size_t		len;
	Boolean		ok;
	CFURLRef	url;

	url = CFBundleCopyBundleURL(bundle);
	if (url == NULL) {
		return NULL;
	}

	ok = CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *)buf, buf_len);
	CFRelease(url);
	if (!ok) {
		return NULL;
	}

	cp = strrchr(buf, '/');
	if (cp != NULL) {
		cp++;
	} else {
		cp = buf;
	}

	/* check if this directory entry is a valid bundle name */
	len = strlen(cp);
	if (len <= sizeof(BUNDLE_DIR_EXTENSION)) {
		/* if entry name isn't long enough */
		return NULL;
	}

	len -= sizeof(BUNDLE_DIR_EXTENSION) - 1;
	if (strcmp(&cp[len], BUNDLE_DIR_EXTENSION) != 0) {
		/* if entry name doesn't end with ".bundle" */
		return NULL;
	}

	return cp;
}


#pragma mark -
#pragma mark load


static void
loadBundle(const void *value, void *context) {
	CFStringRef	bundleID;
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;
	CFIndex		*nLoaded	= (CFIndex *)context;
	CFStringRef	shortID;

	bundleID = CFBundleGetIdentifier(bundleInfo->bundle);
	shortID = shortBundleIdentifier(bundleID);

	if (!bundleInfo->enabled && !bundleInfo->forced) {
		// sorry, this bundle has not been enabled
		SC_log(LOG_INFO, "skipped %@ (disabled)", bundleID);
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
		SC_log(LOG_INFO, "adding  %@", bundleID);

		for (size_t i = 0; i < sizeof(plugins_builtin)/sizeof(plugins_builtin[0]); i++) {
			if (CFEqual(bundleID, plugins_builtin[i].bundleID)) {
				bundleInfo->load  = plugins_builtin[i].load;
				bundleInfo->start = plugins_builtin[i].start;
				bundleInfo->prime = plugins_builtin[i].prime;
				bundleInfo->stop  = plugins_builtin[i].stop;
				break;
			}
		}

		if ((bundleInfo->load  == NULL) &&
		    (bundleInfo->start == NULL) &&
		    (bundleInfo->prime == NULL) &&
		    (bundleInfo->stop  == NULL)) {
			SC_log(LOG_NOTICE, "%@ add failed", bundleID);
			goto done;
		}
	} else {
		CFErrorRef	error	= NULL;

		SC_log(LOG_INFO, "loading %@", bundleID);

		if (!CFBundleLoadExecutableAndReturnError(bundleInfo->bundle, &error)) {
			CFDictionaryRef	user_info;

			SC_log(LOG_NOTICE, "%@ load failed", bundleID);
			user_info = CFErrorCopyUserInfo(error);
			if (user_info != NULL) {
				CFStringRef	link_error_string;

				link_error_string = CFDictionaryGetValue(user_info,
									 CFSTR("NSDebugDescription"));
				if (link_error_string != NULL) {
					SC_log(LOG_NOTICE, "%@", link_error_string);
				}
				CFRelease(user_info);
			}
			CFRelease(error);
			goto done;
		}

		// get bundle entry points
		bundleInfo->load  = (SCDynamicStoreBundleLoadFunction *) getBundleSymbol(bundleInfo->bundle,
											 CFSTR("load" ),
											 shortID);
		bundleInfo->start = (SCDynamicStoreBundleStartFunction *)getBundleSymbol(bundleInfo->bundle,
											 CFSTR("start"),
											 shortID);
		bundleInfo->prime = (SCDynamicStoreBundlePrimeFunction *)getBundleSymbol(bundleInfo->bundle,
											 CFSTR("prime"),
											 shortID);
		bundleInfo->stop  = (SCDynamicStoreBundleStopFunction *) getBundleSymbol(bundleInfo->bundle,
											 CFSTR("stop" ),
											 shortID);
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
callLoadFunction(const void *value, void *context)
{
#pragma unused(context)
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;

	if (!bundleInfo->loaded) {
		return;
	}

	if (bundleInfo->load == NULL) {
		// if no load() function
		return;
	}

	SC_log(LOG_DEBUG, "calling load() for %@",
	       CFBundleGetIdentifier(bundleInfo->bundle));

	(*bundleInfo->load)(bundleInfo->bundle, bundleInfo->verbose);

	return;
}


#pragma mark -
#pragma mark start


void
callStartFunction(const void *value, void *context)
{
#pragma unused(context)
	const char	*bundleDirName;
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;
	char		bundleName[MAXNAMLEN + 1];
	char		bundlePath[MAXPATHLEN];
	size_t		len;

	if (!bundleInfo->loaded) {
		return;
	}

	if (bundleInfo->start == NULL) {
		// if no start() function
		return;
	}

	/* copy the bundle's path */
	bundleDirName = getBundleDirNameAndPath(bundleInfo->bundle, bundlePath, sizeof(bundlePath));
	if (bundleDirName == NULL) {
		// if we have a problem with the bundle's path
		return;
	}

	/* copy (just) the bundle's name */
	if (strlcpy(bundleName, bundleDirName, sizeof(bundleName)) > sizeof(bundleName)) {
		// if we have a problem with the bundle's name
		return;
	}
	len = strlen(bundleName) - (sizeof(BUNDLE_DIR_EXTENSION) - 1);
	bundleName[len] = '\0';

	SC_log(LOG_DEBUG, "calling start() for %@",
	       CFBundleGetIdentifier(bundleInfo->bundle));

	(*bundleInfo->start)(bundleName, bundlePath);

	return;
}


#pragma mark -
#pragma mark prime


void
callPrimeFunction(const void *value, void *context)
{
#pragma unused(context)
	bundleInfoRef	bundleInfo	= (bundleInfoRef)value;

	if (!bundleInfo->loaded) {
		return;
	}

	if (bundleInfo->prime == NULL) {
		// if no prime() function
		return;
	}

	SC_log(LOG_DEBUG, "calling prime() for %@",
	       CFBundleGetIdentifier(bundleInfo->bundle));

	(*bundleInfo->prime)();

	return;
}


#pragma mark -
#pragma mark stop


static void
stopComplete(void *info)
{
	CFBundleRef		bundle		= (CFBundleRef)info;
	CFStringRef		bundleID	= CFBundleGetIdentifier(bundle);
	CFRunLoopSourceRef	stopRls;

	SC_log(LOG_INFO, "** %@ complete (%f)", bundleID, CFAbsoluteTimeGetCurrent());

	stopRls = (CFRunLoopSourceRef)CFDictionaryGetValue(exiting, bundle);
	if (stopRls == NULL) {
		return;
	}

	CFRunLoopSourceInvalidate(stopRls);

	CFDictionaryRemoveValue(exiting, bundle);

	if (CFDictionaryGetCount(exiting) == 0) {
		// if all of the plugins are happy
		SC_log(LOG_INFO, "server shutdown complete (%f)", CFAbsoluteTimeGetCurrent());
		exit (EX_OK);
	}

	return;
}


static void __attribute__((noreturn))
stopDelayed(CFRunLoopTimerRef timer, void *info)
{
#pragma unused(timer)
#pragma unused(info)
	const void	**keys;
	CFIndex		i;
	CFIndex		n;

	SC_log(LOG_INFO, "server shutdown was delayed, unresponsive plugins:");

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
		SC_log(LOG_NOTICE, "** %@", bundleID);
	}
	CFAllocatorDeallocate(NULL, keys);

	exit (EX_OK);
}

static CFStringRef
stopRLSCopyDescription(const void *info)
{
	CFBundleRef	bundle	= (CFBundleRef)info;

	return CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("<stopRLS %p> {bundleID = %@}"),
					info,
					CFBundleGetIdentifier(bundle));
}


static void
stopBundle(const void *value, void *context)
{
#pragma unused(context)
	bundleInfoRef			bundleInfo	= (bundleInfoRef)value;
	CFRunLoopSourceRef		stopRls;
	CFRunLoopSourceContext		stopContext	= { 0				// version
							  , bundleInfo->bundle		// info
							  , CFRetain			// retain
							  , CFRelease			// release
							  , stopRLSCopyDescription	// copyDescription
							  , CFEqual			// equal
							  , CFHash			// hash
							  , NULL			// schedule
							  , NULL			// cancel
							  , stopComplete		// perform
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
	(*bundleInfo->stop)(stopRls);
	CFRelease(stopRls);

	return;
}


static void
stopBundles(void)
{
	/*
	 * If defined, call each bundles stop() function.  This function is
	 * called when configd has been asked to shut down (via a SIGTERM).  The
	 * function should signal the provided run loop source when it is "ready"
	 * for the shut down to proceeed.
	 */
	SC_log(LOG_DEBUG, "calling bundle stop() functions");
	CFArrayApplyFunction(plugins,
			     CFRangeMake(0, CFArrayGetCount(plugins)),
			     stopBundle,
			     NULL);

	if (CFDictionaryGetCount(exiting) == 0) {
		// if all of the plugins are happy
		SC_log(LOG_INFO, "server shutdown complete (%f)", CFAbsoluteTimeGetCurrent());
		exit (EX_OK);
	} else {
		CFRunLoopTimerRef	timer;

		/*
		 * launchd will only wait 20 seconds before sending us a
		 * SIGKILL and because we want to know what's stuck before
		 * that time so set our own "we're not waiting any longer"
		 * timeout for 15 seconds.
		 */
		timer = CFRunLoopTimerCreate(NULL,				/* allocator */
					     CFAbsoluteTimeGetCurrent() + 15.0,	/* fireDate (in 15 seconds) */
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


#pragma mark -
#pragma mark term


__private_extern__
Boolean
plugin_term(int *status)
{
	if (CFArrayGetCount(plugins) == 0) {
		// if no plugins
		*status = EX_OK;
		return FALSE;	// don't delay shutdown
	}

	if (exiting != NULL) {
		// if shutdown already active
		return TRUE;
	}

	SC_log(LOG_INFO, "starting server shutdown (%f)", CFAbsoluteTimeGetCurrent());

	exiting = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);

	stopBundles();
	return TRUE;
}


#pragma mark -
#pragma mark initialization


/*
 * ALT_CFRelease()
 *
 * An alternate CFRelease() that we can use to fake out the
 * static analyzer.
 */
static __inline__ void
ALT_CFRelease(CFTypeRef cf)
{
	CFRelease(cf);
}


static CFDictionaryRef
pluginsCopy(void *arg)
{
	CFMutableDictionaryRef	plugins;

	plugins = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    NULL);

	if (arg == NULL) {
		char					path[MAXPATHLEN];
		sysdir_search_path_enumeration_state	state;

		/*
		 * identify and load all bundles
		 */
		state = sysdir_start_search_path_enumeration(SYSDIR_DIRECTORY_LIBRARY,
							     SYSDIR_DOMAIN_MASK_SYSTEM);
		while ((state = sysdir_get_next_search_path_enumeration(state, path))) {
			CFArrayRef	bundles;
			CFURLRef	url;

#if	TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
			const char	*path_sim_prefix;

			path_sim_prefix = getenv("IPHONE_SIMULATOR_ROOT");
			if ((path_sim_prefix != NULL) && (strcmp(path_sim_prefix, ".") != 0)) {
				char	path_sim[MAXPATHLEN];

				strlcpy(path_sim, path_sim_prefix, sizeof(path_sim));
				strlcat(path_sim, path, sizeof(path_sim));
				strlcpy(path, path_sim, sizeof(path));
			} else {
				path[0] = '\0';
			}
#endif	// TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST

			/* load any available bundle */
			strlcat(path, BUNDLE_DIRECTORY, sizeof(path));
			SC_log(LOG_DEBUG, "searching for plugins in \"%s\"", path);
			url = CFURLCreateFromFileSystemRepresentation(NULL,
								      (UInt8 *)path,
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
					addPlugin(plugins, bundle, FALSE);

					// The CFBundleCreateBundlesFromDirectory() API has
					// a known/outstanding bug in that it over-retains the
					// returned bundles.  Since we do not expect this to
					// be fixed we release the extra references.
					//
					//   See <rdar://problems/4912137&6078752> for more info.
					//
					// Also, we use the hack below to keep the static
					// analyzer happy.
					ALT_CFRelease(bundle);
				}
				CFRelease(bundles);
			}
		}
	} else {
		CFBundleRef	bundle;
		CFURLRef	url;

		/*
		 * load (only) the specified bundle
		 */
		url = CFURLCreateFromFileSystemRepresentation(NULL,
							      (UInt8 *)arg,
							      strlen((char *)arg),
							      TRUE);
		bundle = CFBundleCreate(NULL, url);
		if (bundle != NULL) {
			addPlugin(plugins, bundle, TRUE);
			CFRelease(bundle);
		}
		CFRelease(url);
	}

	return plugins;
}


CFArrayRef
pluginsSelect(CFDictionaryRef loaded)
{
	CFMutableArrayRef	selected;

	selected = CFArrayCreateMutable(NULL, 0, NULL);

	for (size_t i = 0; i < N_PLUGIN_ALLOW_LIST; i++) {
		CFStringRef	bundleID;
		bundleInfoRef	bundleInfo;

		bundleID = plugin_allow_list[i];
		bundleInfo = (bundleInfoRef)CFDictionaryGetValue(loaded, bundleID);
		if (bundleInfo != NULL) {
			Boolean		bundleExclude;

			bundleExclude = CFSetContainsValue(_plugins_exclude, bundleID);
			if (!bundleExclude) {
				CFStringRef	shortID;

				shortID = shortBundleIdentifier(bundleID);
				if (shortID != NULL) {
					bundleExclude = CFSetContainsValue(_plugins_exclude, shortID);
					CFRelease(shortID);
				}
			}

			if (bundleExclude) {
				// sorry, this bundle has been excluded
				SC_log(LOG_INFO, "skipped %@ (excluded)", bundleID);
				continue;
			}

			CFArrayAppendValue(selected, bundleInfo);
		}
	}

	return selected;
}

__private_extern__
void
plugin_exec(void *arg)
{
	CFDictionaryRef	available;
	CFIndex		nLoaded		= 0;

	/* allow plug-ins to exec child/helper processes */
	_SCDPluginExecInit();

	/* collect, select, and order the available plugin bundles */
	available = pluginsCopy(arg);
	plugins = pluginsSelect(available);
	CFRelease(available);

	/*
	 * load each bundle.
	 */
	SC_log(LOG_DEBUG, "loading bundles");
	CFArrayApplyFunction(plugins,
			     CFRangeMake(0, CFArrayGetCount(plugins)),
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
	SC_log(LOG_DEBUG, "calling bundle load() functions");
	CFArrayApplyFunction(plugins,
			     CFRangeMake(0, CFArrayGetCount(plugins)),
			     callLoadFunction,
			     NULL);

	if (nLoaded == 0) {
		// if no bundles loaded
		return;
	}

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
	SC_log(LOG_DEBUG, "calling bundle start() functions");
	CFArrayApplyFunction(plugins,
			     CFRangeMake(0, CFArrayGetCount(plugins)),
			     callStartFunction,
			     NULL);

	/*
	 * If defined, call each bundles prime() function.  This function is
	 * called after the bundle has been loaded and its load() and start()
	 * functions have been called.  It should initialize any configuration
	 * information and/or state in the store.
	 */
	SC_log(LOG_DEBUG, "calling bundle prime() functions");
	CFArrayApplyFunction(plugins,
			     CFRangeMake(0, CFArrayGetCount(plugins)),
			     callPrimeFunction,
			     NULL);

	/*
	 * At this point, the assumption is that each loaded plugin will have
	 * established CFMachPort, CFSocket, and CFRunLoopTimer input sources
	 * to handle any events and registered these sources with this threads
	 * run loop and we're ready to go.
	 *
	 * Note: it is also assumed that any plugin needing to wait and/or block
	 * will do so only a private thread (or asynchronously on a non-main
	 * dispatch queue).
	 */

	return;
}
