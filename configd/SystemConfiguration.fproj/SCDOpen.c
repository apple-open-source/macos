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

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

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
	CFIndex				keyCnt;
	int				oldThreadState;
	int				sc_status;
	kern_return_t			status;
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)cf;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreDeallocate:"));

	(void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldThreadState);

	/* Remove notification keys */
	if ((keyCnt = CFSetGetCount(storePrivate->keys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex	i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(storePrivate->keys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
			(void) SCDynamicStoreRemoveWatchedKey(store,
							      CFArrayGetValueAtIndex(keysToRemove, i),
							      FALSE);
		}
		CFRelease(keysToRemove);
	}

	/* Remove regex notification keys */
	if ((keyCnt = CFSetGetCount(storePrivate->reKeys)) > 0) {
		void		**watchedKeys;
		CFArrayRef	keysToRemove;
		CFIndex	i;

		watchedKeys = CFAllocatorAllocate(NULL, keyCnt * sizeof(CFStringRef), 0);
		CFSetGetValues(storePrivate->reKeys, watchedKeys);
		keysToRemove = CFArrayCreate(NULL, watchedKeys, keyCnt, &kCFTypeArrayCallBacks);
		CFAllocatorDeallocate(NULL, watchedKeys);
		for (i=0; i<keyCnt; i++) {
		       (void) SCDynamicStoreRemoveWatchedKey(store,
							     CFArrayGetValueAtIndex(keysToRemove, i),
							     TRUE);
		}
		CFRelease(keysToRemove);
	}

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
	CFRelease(storePrivate->reKeys);

	return;
}


static CFTypeID __kSCDynamicStoreTypeID = _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCDynamicStoreClass = {
	0,				// version
	"SCDynamicStore",			// className
	NULL,				// init
	NULL,				// copy
	__SCDynamicStoreDeallocate,		// dealloc
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


SCDynamicStoreRef
__SCDynamicStoreCreatePrivate(CFAllocatorRef		allocator,
			     const CFStringRef		name,
			     SCDynamicStoreCallBack	callout,
			     SCDynamicStoreContext	*context)
{
	SCDynamicStorePrivateRef	store;
	UInt32				size;

	/* initialize runtime */
	pthread_once(&initialized, __SCDynamicStoreInitialize);

	/* allocate session */
	size  = sizeof(SCDynamicStorePrivate) - sizeof(CFRuntimeBase);
	store = (SCDynamicStorePrivateRef)_CFRuntimeCreateInstance(allocator,
								   __kSCDynamicStoreTypeID,
								   size,
								   NULL);
	if (!store) {
		return NULL;
	}

	/* server side of the "configd" session */
	store->server = MACH_PORT_NULL;

	/* flags */
	store->locked = FALSE;

	/* SCDKeys being watched */
	store->keys   = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	store->reKeys = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

	/* Notification status */
	store->notifyStatus		= NotifierNotRegistered;

	/* "client" information associated with SCDynamicStoreCreateRunLoopSource() */
	store->rlsRefs				= 0;
	store->rls				= NULL;
	store->rlsFunction			= callout;
	store->rlsContext.info			= NULL;
	store->rlsContext.retain		= NULL;
	store->rlsContext.release		= NULL;
	store->rlsContext.copyDescription	= NULL;
	if (context) {
		bcopy(context, &store->rlsContext, sizeof(SCDynamicStoreContext));
		if (context->retain) {
			store->rlsContext.info = (void *)context->retain(context->info);
		}
	}

	/* "client" information associated with SCDynamicStoreNotifyCallback() */
	store->callbackFunction		= NULL;
	store->callbackArgument		= NULL;
	store->callbackPort		= NULL;
	store->callbackRunLoop		= NULL;
	store->callbackRunLoopSource	= NULL;

	/* "server" information associated with SCDynamicStoreNotifyMachPort(); */
	store->notifyPort		= MACH_PORT_NULL;
	store->notifyPortIdentifier	= 0;

	/* "server" information associated with SCDynamicStoreNotifyFileDescriptor(); */
	store->notifyFile		= -1;
	store->notifyFileIdentifier	= 0;

	/* "server" information associated with SCDynamicStoreNotifySignal(); */
	store->notifySignal		= 0;
	store->notifySignalTask		= TASK_NULL;

	return (SCDynamicStoreRef)store;
}


SCDynamicStoreRef
SCDynamicStoreCreate(CFAllocatorRef		allocator,
		     CFStringRef		name,
		     SCDynamicStoreCallBack	callout,
		     SCDynamicStoreContext	*context)
{
	SCDynamicStoreRef		store;
	SCDynamicStorePrivateRef	storePrivate;
	kern_return_t			status;
	mach_port_t			bootstrap_port;
	mach_port_t			server;
	CFDataRef			xmlName;		/* serialized name */
	xmlData_t			myNameRef;
	CFIndex				myNameLen;
	int				sc_status;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreCreate:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  name = %@"), name);

	/*
	 * allocate and initialize a new session
	 */
	store        = __SCDynamicStoreCreatePrivate(allocator, name, callout, context);
	storePrivate = (SCDynamicStorePrivateRef)store;

	status = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("task_get_bootstrap_port(): %s"), mach_error_string(status));
		CFRelease(store);
		_SCErrorSet(status);
		return NULL;
	}

	status = bootstrap_look_up(bootstrap_port, SCD_SERVER, &server);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			CFRelease(store);
			_SCErrorSet(status);
			return NULL;
			break;
		default :
#ifdef	DEBUG
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("bootstrap_status: %s"), mach_error_string(status));
#endif	/* DEBUG */
			CFRelease(store);
			_SCErrorSet(status);
			return NULL;
	}

	/* serialize the name */
	xmlName = CFPropertyListCreateXMLData(NULL, name);
	myNameRef = (xmlData_t)CFDataGetBytePtr(xmlName);
	myNameLen = CFDataGetLength(xmlName);

	/* open a new session with the server */
	status = configopen(server, myNameRef, myNameLen, &storePrivate->server, (int *)&sc_status);

	/* clean up */
	CFRelease(xmlName);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configopen(): %s"), mach_error_string(status));
		CFRelease(store);
		_SCErrorSet(status);
		return NULL;
	}

	if (sc_status != kSCStatusOK) {
		CFRelease(store);
		_SCErrorSet(sc_status);
		return FALSE;
	}

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  server port = %d"), storePrivate->server);
	return store;
}


CFTypeID
SCDynamicStoreGetTypeID(void) {
	return __kSCDynamicStoreTypeID;
}
