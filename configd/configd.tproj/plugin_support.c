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

#include <mach-o/dyld.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <unistd.h>
#include <NSSystemDirectories.h>

#include "configd.h"


/*
 * Information maintained for each to-be-kicked registration.
 */
typedef struct {
	/*
	 * bundle paths
	 */
	char				bundle[MAXNAMLEN + 1];	/* bundle name */
	char				path  [MAXPATHLEN];	/* bundle path */

	/*
	 * entry points for initialization code.
	 */
	SCDBundleStartRoutine_t		start;	/* address of start() routine */
	SCDBundlePrimeRoutine_t		prime;	/* address of prime() routine */

} plugin, *pluginRef;

CFMutableArrayRef	plugins;


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


static boolean_t
bundleLoad(pluginRef info)
{
	int				len;
	NSObjectFileImage		image;
	NSObjectFileImageReturnCode	status;
	NSModule			module;
	NSSymbol			symbol;
	unsigned long			options;
	char				*bundleExe;	/* full path of bundle executable */
	struct stat			sb;

	/*
	 * allocate enough space for the bundle directory path, a "/" separator,
	 * the bundle name, and the (optional) "_debug" extension.
	 */

	len =  strlen(info->path);		/* path */
	len += sizeof(BUNDLE_NEW_SUBDIR) - 1;	/* "/" or "/Contents/MacOS/" */
	len += strlen(info->bundle);		/* bundle name */
	len += sizeof(BUNDLE_DEBUG_EXTENSION);	/* "_debug" (and NUL) */
	bundleExe = CFAllocatorAllocate(NULL, len, 0);

	/* check for the (old layout) bundle executable path */
	strcpy(bundleExe, info->path);
	strcat(bundleExe, BUNDLE_OLD_SUBDIR);
	strcat(bundleExe, info->bundle);
	if (stat(bundleExe, &sb) == 0) {
		goto load;
	}

	/* check for the "_debug" version */
	strcat(bundleExe, BUNDLE_DEBUG_EXTENSION);
	if (stat(bundleExe, &sb) == 0) {
		goto load;
	}

	/* check for the (new layout) bundle executable path */
	strcpy(bundleExe, info->path);
	strcat(bundleExe, BUNDLE_NEW_SUBDIR);
	strcat(bundleExe, info->bundle);
	if (stat(bundleExe, &sb) == 0) {
		goto load;
	}

	/* check for the "_debug" version */
	strcat(bundleExe, BUNDLE_DEBUG_EXTENSION);
	if (stat(bundleExe, &sb) == 0) {
		goto load;
	}

	SCDLog(LOG_ERR,
	       CFSTR("bundleLoad() failed, no executable for %s in %s"),
	       info->bundle,
	       info->path);
	CFAllocatorDeallocate(NULL, bundleExe);
	return FALSE;

    load :

	/* load the bundle */
	SCDLog(LOG_DEBUG, CFSTR("loading %s"), bundleExe);
	status = NSCreateObjectFileImageFromFile(bundleExe, &image);
	if (status != NSObjectFileImageSuccess) {
		char	*err;

		switch (status) {
			case NSObjectFileImageFailure :
				err = "NSObjectFileImageFailure";
				break;
			case NSObjectFileImageInappropriateFile :
				err = "NSObjectFileImageInappropriateFile";
				break;
			case NSObjectFileImageArch :
				err = "NSObjectFileImageArch";
				break;
			case NSObjectFileImageFormat :
				err = "NSObjectFileImageFormat";
				break;
			case NSObjectFileImageAccess :
				err = "NSObjectFileImageAccess";
				break;
			default :
				err = "Unknown";
				break;
		}
		SCDLog(LOG_ERR, CFSTR("NSCreateObjectFileImageFromFile() failed"));
		SCDLog(LOG_ERR, CFSTR("  executable path = %s"), bundleExe);
		SCDLog(LOG_ERR, CFSTR("  error status    = %s"), err);
		CFAllocatorDeallocate(NULL, bundleExe);
		return FALSE;
	}

	options =  NSLINKMODULE_OPTION_BINDNOW;
	options |= NSLINKMODULE_OPTION_PRIVATE;
	options |= NSLINKMODULE_OPTION_RETURN_ON_ERROR;
	module = NSLinkModule(image, bundleExe, options);

	if (module == NULL) {
		NSLinkEditErrors	c;
		int			errorNumber;
		const char		*fileName;
		const char		*errorString;

		SCDLog(LOG_ERR, CFSTR("NSLinkModule() failed"));
		SCDLog(LOG_ERR, CFSTR("  executable path = %s"), bundleExe);

		/* collect and report the details */
		NSLinkEditError(&c, &errorNumber, &fileName, &errorString);
		SCDLog(LOG_ERR, CFSTR("  NSLinkEditErrors = %d"), (int)c);
		SCDLog(LOG_ERR, CFSTR("  errorNumber      = %d"), errorNumber);
		if((fileName != NULL) && (*fileName != '\0'))
			SCDLog(LOG_ERR, CFSTR("  fileName         = %s"), fileName);
		if((errorString != NULL) && (*errorString != '\0'))
			SCDLog(LOG_ERR, CFSTR("  errorString      = %s"), errorString);

		CFAllocatorDeallocate(NULL, bundleExe);
		return FALSE;
	}

	CFAllocatorDeallocate(NULL, bundleExe);

	/* identify the initialization functions */

	symbol = NSLookupSymbolInModule(module, BUNDLE_ENTRY_POINT);
	if (symbol) {
		info->start = NSAddressOfSymbol(symbol);
	}

	symbol = NSLookupSymbolInModule(module, BUNDLE_ENTRY_POINT2);
	if (symbol) {
		info->prime = NSAddressOfSymbol(symbol);
	}

	if ((info->start == NULL) && (info->prime == NULL)) {
		SCDLog(LOG_DEBUG, CFSTR("  no entry points"));
		return FALSE;
	}

	/* identify any exception handling functions */

	symbol = NSLookupSymbolInModule(module, "_catch_exception_raise");
	if (symbol) {
		catch_exception_raise_func = NSAddressOfSymbol(symbol);
	}

	symbol = NSLookupSymbolInModule(module, "_catch_exception_raise_state");
	if (symbol) {
		catch_exception_raise_state_func = NSAddressOfSymbol(symbol);
	}

	symbol = NSLookupSymbolInModule(module, "_catch_exception_raise_identity");
	if (symbol) {
		catch_exception_raise_identity_func = NSAddressOfSymbol(symbol);
	}

	return TRUE;
}


static void
bundleStart(const void *value, void *context)
{
	CFDataRef	data = (CFDataRef)value;
	pluginRef	info;

	info = (pluginRef)CFDataGetBytePtr(data);
	if (info->start) {
		(*info->start)(info->bundle, info->path);
	}
}


static void
bundlePrime(const void *value, void *context)
{
	CFDataRef	data = (CFDataRef)value;
	pluginRef	info;

	info = (pluginRef)CFDataGetBytePtr(data);
	if (info->prime) {
		(*info->prime)(info->bundle, info->path);
	}
}


static void
loadOne(const char *bundleDir, const char *bundleName)
{
	CFMutableDataRef	info;
	pluginRef		pluginInfo;
	int			len;

	/* check if this directory entry is a valid bundle name */
	len = strlen(bundleName);
	if (len <= sizeof(BUNDLE_DIR_EXTENSION)) {
		/* if entry name isn't long enough */
		return;
	}

	len -= sizeof(BUNDLE_DIR_EXTENSION) - 1;
	if (strcmp(&bundleName[len], BUNDLE_DIR_EXTENSION) != 0) {
		/* if entry name doesn end with ".bundle" */
		return;
	}

	info = CFDataCreateMutable(NULL, sizeof(plugin));
	pluginInfo = (pluginRef)CFDataGetBytePtr(info);
	pluginInfo->start = NULL;
	pluginInfo->prime = NULL;

	/* get (just) the bundle's name */
	pluginInfo->bundle[0] = '\0';
	(void) strncat(pluginInfo->bundle, bundleName, len);

	/* get the bundle directory path */
	(void) sprintf(pluginInfo->path, "%s/%s", bundleDir, bundleName);

	/* load the bundle */
	if (bundleLoad(pluginInfo)) {
		SCDLog(LOG_INFO, CFSTR("%s loaded"), bundleName);
		CFArrayAppendValue(plugins, info);
	} else {
		SCDLog(LOG_ERR,  CFSTR("load of \"%s\" failed"), bundleName);
	}
	CFRelease(info);

	return;
}


static void
loadAll(const char *bundleDir)
{
	DIR			*dirp;
	struct dirent		*dp;

	dirp = opendir(bundleDir);
	if (dirp == NULL) {
		/* if no plugin directory */
		return;
	}

	while ((dp = readdir(dirp)) != NULL) {
		loadOne(bundleDir, dp->d_name);
	}

	closedir(dirp);
	return;
}


void
timerCallback(CFRunLoopTimerRef timer, void *info)
{
	SCDLog(LOG_INFO, CFSTR("the CFRunLoop is waiting for something to happen...."));
	return;
}


void *
plugin_exec(void *arg)
{
	NSSearchPathEnumerationState    state;
	char                            path[MAXPATHLEN];

	/* keep track of loaded plugins */
	plugins = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (arg == NULL) {
		/*
		 * identify and load all plugins
		 */
		state = NSStartSearchPathEnumeration(NSLibraryDirectory,
						     NSLocalDomainMask|NSSystemDomainMask);
		while ((state = NSGetNextSearchPathEnumeration(state, path))) {
			/* load any available plugins */
			strcat(path, BUNDLE_DIRECTORY);
			SCDLog(LOG_DEBUG, CFSTR("searching for plugins in \"%s\""), path);
			loadAll(path);
		}

		if (SCDOptionGet(NULL, kSCDOptionDebug)) {
			SCDLog(LOG_DEBUG, CFSTR("searching for plugins in \".\""));
			loadAll(".");
		}
	} else {
		/*
		 * load the plugin specified on the command line
		 */
		char	*bn, *bd;

		if ((bn = strrchr((char *)arg, '/')) != NULL) {
			int	len;

			/* plug-in directory */
			len = bn - (char *)arg;
			if (len == 0)
				len++;		/* if plugin is in the root directory */

			bd = CFAllocatorAllocate(NULL, len + 1, 0);
			bd[0] = '\0';
			(void) strncat(bd, (char *)arg, len);

			/* plug-in name */
			bn++;		/* name starts just after trailing path separator */
		} else {
			/* plug-in (in current) directory */
			bd = CFAllocatorAllocate(NULL, sizeof("."), 0);
			(void) strcpy(bd, ".");

			/* plug-in name */
			bn = (char *)arg;	/* no path separators */
		}

		loadOne(bd, bn);

		CFAllocatorDeallocate(NULL, bd);

		/* allocate a periodic event (to help show we're not blocking) */
		if (CFArrayGetCount(plugins)) {
			CFRunLoopTimerRef	timer;

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
	}

	/*
	 * execute each plugins start() function which should initialize any
	 * variables, open any sessions with "configd", and register any needed
	 * notifications. Establishing initial information in the cache should
	 * be deferred until the prime() initialization function so that any
	 * plug-ins which want to receive a notification that the data has
	 * changed will have an opportunity to install a notification handler.
	 */
	SCDLog(LOG_DEBUG, CFSTR("calling plugin start() functions"));
	CFArrayApplyFunction(plugins,
			     CFRangeMake(0, CFArrayGetCount(plugins)),
			     bundleStart,
			     NULL);

	/*
	 * execute each plugins prime() function which should initialize any
	 * configuration information and/or state in the cache.
	 */
	SCDLog(LOG_DEBUG, CFSTR("calling plugin prime() functions"));
	CFArrayApplyFunction(plugins,
			     CFRangeMake(0, CFArrayGetCount(plugins)),
			     bundlePrime,
			     NULL);

	/*
	 * all plugins have been loaded and started.
	 */
	CFRelease(plugins);

	if (!SCDOptionGet(NULL, kSCDOptionDebug) && (arg == NULL)) {
	    /* synchronize with parent process */
	    kill(getppid(), SIGTERM);
	}

	/*
	 * The assumption is that each loaded plugin will establish CFMachPortRef,
	 * CFSocketRef, and CFRunLoopTimerRef input sources to handle any events
	 * and register these sources with this threads run loop. If the plugin
	 * needs to wait and/or block at any time it should do so only in its a
	 * private thread.
	 */
	SCDLog(LOG_DEBUG, CFSTR("starting plugin CFRunLoop"));
	CFRunLoopRun();
	SCDLog(LOG_INFO, CFSTR("what, no more work for the \"configd\" plugins?"));
	return NULL;
}


void
plugin_init()
{
	pthread_attr_t	tattr;
	pthread_t	tid;

	SCDLog(LOG_DEBUG, CFSTR("Starting thread for plug-ins..."));
	pthread_attr_init(&tattr);
	pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
//      pthread_attr_setstacksize(&tattr, 96 * 1024); // each thread gets a 96K stack
	pthread_create(&tid, &tattr, plugin_exec, NULL);
	pthread_attr_destroy(&tattr);
	SCDLog(LOG_DEBUG, CFSTR("  thread id=0x%08x"), tid);

	return;
}
