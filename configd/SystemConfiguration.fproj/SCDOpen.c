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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */

static CFStringRef
__SCDynamicStoreCopyDescription(CFTypeRef cf) {
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef	result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCDynamicStore %p [%p]> {\n"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCDynamicStoreDeallocate(CFTypeRef cf)
{
	int				oldThreadState;
	int				sc_status;
	kern_return_t			status;
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)cf;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreDeallocate:"));

	(void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldThreadState);

	/* Remove/cancel any outstanding notification requests. */
	(void) SCDynamicStoreNotifyCancel(store);

	if (storePrivate->server && storePrivate->locked) {
		(void) SCDynamicStoreUnlock(store);	/* release the lock */
	}

	if (storePrivate->server != MACH_PORT_NULL) {
		status = configclose(storePrivate->server, (int *)&sc_status);
		if (status != KERN_SUCCESS) {
			if (status != MACH_SEND_INVALID_DEST)
				SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configclose(): %s"), mach_error_string(status));
		}

		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
	}

	(void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldThreadState);
	pthread_testcancel();

	/* release any callback context info */
	if (storePrivate->rlsContext.release) {
		storePrivate->rlsContext.release(storePrivate->rlsContext.info);
	}

	/* release any keys being watched */
	CFRelease(storePrivate->keys);
	CFRelease(storePrivate->patterns);

	return;
}


static CFTypeID __kSCDynamicStoreTypeID = _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCDynamicStoreClass = {
	0,				// version
	"SCDynamicStore",		// className
	NULL,				// init
	NULL,				// copy
	__SCDynamicStoreDeallocate,	// dealloc
	NULL,				// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
	__SCDynamicStoreCopyDescription	// copyDebugDesc
};


static pthread_once_t initialized	= PTHREAD_ONCE_INIT;

static void
__SCDynamicStoreInitialize(void) {
	__kSCDynamicStoreTypeID = _CFRuntimeRegisterClass(&__SCDynamicStoreClass);
	return;
}


SCDynamicStorePrivateRef
__SCDynamicStoreCreatePrivate(CFAllocatorRef		allocator,
			     const CFStringRef		name,
			     SCDynamicStoreCallBack	callout,
			     SCDynamicStoreContext	*context)
{
	uint32_t			size;
	SCDynamicStorePrivateRef	storePrivate;

	/* initialize runtime */
	pthread_once(&initialized, __SCDynamicStoreInitialize);

	/* allocate session */
	size  = sizeof(SCDynamicStorePrivate) - sizeof(CFRuntimeBase);
	storePrivate = (SCDynamicStorePrivateRef)_CFRuntimeCreateInstance(allocator,
									  __kSCDynamicStoreTypeID,
									  size,
									  NULL);
	if (!storePrivate) {
		return NULL;
	}

	/* server side of the "configd" session */
	storePrivate->server = MACH_PORT_NULL;

	/* flags */
	storePrivate->locked = FALSE;

	/* Notification status */
	storePrivate->notifyStatus			= NotifierNotRegistered;

	/* "client" information associated with SCDynamicStoreCreateRunLoopSource() */
	storePrivate->rlsRefs				= 0;
	storePrivate->rls				= NULL;
	storePrivate->rlsFunction			= callout;
	storePrivate->rlsContext.info			= NULL;
	storePrivate->rlsContext.retain			= NULL;
	storePrivate->rlsContext.release		= NULL;
	storePrivate->rlsContext.copyDescription	= NULL;
	if (context) {
		bcopy(context, &storePrivate->rlsContext, sizeof(SCDynamicStoreContext));
		if (context->retain) {
			storePrivate->rlsContext.info = (void *)context->retain(context->info);
		}
	}

	/* "client" information associated with SCDynamicStoreNotifyCallback() */
	storePrivate->callbackFunction			= NULL;
	storePrivate->callbackArgument			= NULL;
	storePrivate->callbackPort			= NULL;
	storePrivate->callbackRunLoop			= NULL;
	storePrivate->callbackRunLoopSource		= NULL;

	/* "server" information associated with SCDynamicStoreSetNotificationKeys() */
	storePrivate->keys				= CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	storePrivate->patterns				= CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

	/* "server" information associated with SCDynamicStoreNotifyMachPort(); */
	storePrivate->notifyPort			= MACH_PORT_NULL;
	storePrivate->notifyPortIdentifier		= 0;

	/* "server" information associated with SCDynamicStoreNotifyFileDescriptor(); */
	storePrivate->notifyFile			= -1;
	storePrivate->notifyFileIdentifier		= 0;

	/* "server" information associated with SCDynamicStoreNotifySignal(); */
	storePrivate->notifySignal			= 0;
	storePrivate->notifySignalTask			= TASK_NULL;

	return storePrivate;
}


SCDynamicStoreRef
SCDynamicStoreCreate(CFAllocatorRef		allocator,
		     CFStringRef		name,
		     SCDynamicStoreCallBack	callout,
		     SCDynamicStoreContext	*context)
{
	SCDynamicStorePrivateRef	storePrivate;
	kern_return_t			status;
	mach_port_t			bootstrap_port;
	CFBundleRef			bundle;
	CFStringRef			bundleID	= NULL;
	mach_port_t			server;
	char				*server_name;
	CFDataRef			utfName;		/* serialized name */
	xmlData_t			myNameRef;
	CFIndex				myNameLen;
	int				sc_status;

	if (_sc_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreCreate:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  name = %@"), name);
	}

	/*
	 * allocate and initialize a new session
	 */
	storePrivate = __SCDynamicStoreCreatePrivate(allocator, name, callout, context);

	status = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("task_get_bootstrap_port(): %s"), mach_error_string(status));
		CFRelease(storePrivate);
		_SCErrorSet(status);
		return NULL;
	}

	server_name = getenv("SCD_SERVER");
	if (!server_name) {
		server_name = SCD_SERVER;
	}

	status = bootstrap_look_up(bootstrap_port, server_name, &server);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			CFRelease(storePrivate);
			_SCErrorSet(status);
			return NULL;
			break;
		default :
#ifdef	DEBUG
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("bootstrap_look_up() failed: status=%d"), status);
#endif	/* DEBUG */
			CFRelease(storePrivate);
			_SCErrorSet(status);
			return NULL;
	}

	/* serialize the name */
	bundle = CFBundleGetMainBundle();
	if (bundle) {
		bundleID = CFBundleGetIdentifier(bundle);
		if (bundleID) {
			CFRetain(bundleID);
		} else {
			CFURLRef	url;

			url = CFBundleCopyExecutableURL(bundle);
			if (url) {
				bundleID = CFURLCopyPath(url);
				CFRelease(url);
			}
		}
	}

	if (bundleID) {
		CFStringRef	fullName;

		if (CFEqual(bundleID, CFSTR("/"))) {
			CFRelease(bundleID);
			bundleID = CFStringCreateWithFormat(NULL, NULL, CFSTR("(%d)"), getpid());
		}

		fullName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@:%@"), bundleID, name);
		name = fullName;
		CFRelease(bundleID);
	} else {
		CFRetain(name);
	}

	if (!_SCSerializeString(name, &utfName, (void **)&myNameRef, &myNameLen)) {
		CFRelease(name);
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}
	CFRelease(name);

	/* open a new session with the server */
	status = configopen(server, myNameRef, myNameLen, &storePrivate->server, (int *)&sc_status);

	/* clean up */
	CFRelease(utfName);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configopen(): %s"), mach_error_string(status));
		CFRelease(storePrivate);
		_SCErrorSet(status);
		return NULL;
	}

	if (sc_status != kSCStatusOK) {
		CFRelease(storePrivate);
		_SCErrorSet(sc_status);
		return NULL;
	}

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  server port = %d"), storePrivate->server);
	return (SCDynamicStoreRef)storePrivate;
}


CFTypeID
SCDynamicStoreGetTypeID(void) {
	pthread_once(&initialized, __SCDynamicStoreInitialize);	/* initialize runtime */
	return __kSCDynamicStoreTypeID;
}
