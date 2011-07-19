/*
 * Copyright (c) 2000-2006, 2008-2011 Apple Inc. All rights reserved.
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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <TargetConditionals.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */


static CFStringRef	_sc_bundleID	= NULL;
static pthread_mutex_t	_sc_lock	= PTHREAD_MUTEX_INITIALIZER;
static mach_port_t	_sc_server	= MACH_PORT_NULL;


static const char	*notifyType[] = {
	"",
	"wait",
	"inform w/callback",
	"inform w/mach port",
	"inform w/fd",
	"inform w/signal",
	"inform w/runLoop",
	"inform w/dispatch"
};


static CFStringRef
__SCDynamicStoreCopyDescription(CFTypeRef cf) {
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCDynamicStore %p [%p]> {"), cf, allocator);
	if (storePrivate->server != MACH_PORT_NULL) {
		CFStringAppendFormat(result, NULL, CFSTR("server port = %p"), storePrivate->server);
	} else {
		CFStringAppendFormat(result, NULL, CFSTR("server not (no longer) available"));
	}
	if (storePrivate->locked) {
		CFStringAppendFormat(result, NULL, CFSTR(", locked"));
	}
	if (storePrivate->disconnectFunction != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", disconnect = %p"), storePrivate->disconnectFunction);
	}
	switch (storePrivate->notifyStatus) {
		case Using_NotifierWait :
			CFStringAppendFormat(result, NULL, CFSTR(", waiting for a notification"));
			break;
		case Using_NotifierInformViaMachPort :
			CFStringAppendFormat(result, NULL, CFSTR(", mach port notifications"));
			break;
		case Using_NotifierInformViaFD :
			CFStringAppendFormat(result, NULL, CFSTR(", FD notifications"));
			break;
		case Using_NotifierInformViaSignal :
			CFStringAppendFormat(result, NULL, CFSTR(", BSD signal notifications"));
			break;
		case Using_NotifierInformViaRunLoop :
		case Using_NotifierInformViaCallback :
			if (storePrivate->notifyStatus == Using_NotifierInformViaRunLoop) {
				CFStringAppendFormat(result, NULL, CFSTR(", runloop notifications"));
				CFStringAppendFormat(result, NULL, CFSTR(" {callout = %p"), storePrivate->rlsFunction);
				CFStringAppendFormat(result, NULL, CFSTR(", info = %p"), storePrivate->rlsContext.info);
				CFStringAppendFormat(result, NULL, CFSTR(", rls = %p"), storePrivate->rls);
			} else {
				CFStringAppendFormat(result, NULL, CFSTR(", mach port/callback notifications"));
				CFStringAppendFormat(result, NULL, CFSTR(" {callout = %p"), storePrivate->callbackFunction);
				CFStringAppendFormat(result, NULL, CFSTR(", info = %p"), storePrivate->callbackArgument);
			}
			if (storePrivate->callbackRLS != NULL) {
				CFStringAppendFormat(result, NULL, CFSTR(", notify rls = %@" ), storePrivate->callbackRLS);
			}
			CFStringAppendFormat(result, NULL, CFSTR("}"));
			break;
		default :
			CFStringAppendFormat(result, NULL, CFSTR(", notification delivery not requested%s"),
					     storePrivate->rlsFunction ? " (yet)" : "");
			break;
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCDynamicStoreDeallocate(CFTypeRef cf)
{
	int				oldThreadState;
	int				sc_status;
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)cf;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	(void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldThreadState);

	/* Remove/cancel any outstanding notification requests. */
	(void) SCDynamicStoreNotifyCancel(store);

	if ((storePrivate->server != MACH_PORT_NULL) && storePrivate->locked) {
		(void) SCDynamicStoreUnlock(store);	/* release the lock */
	}

	if (storePrivate->server != MACH_PORT_NULL) {
		__MACH_PORT_DEBUG(TRUE, "*** __SCDynamicStoreDeallocate", storePrivate->server);
		(void) configclose(storePrivate->server, (int *)&sc_status);
		__MACH_PORT_DEBUG(TRUE, "*** __SCDynamicStoreDeallocate (after configclose)", storePrivate->server);

		/*
		 * the above call to configclose() should result in the SCDynamicStore
		 * server code deallocating it's receive right.  That, in turn, should
		 * result in our send becoming a dead name.  We could explicitly remove
		 * the dead name right with a call to mach_port_mod_refs() but, to be
		 * sure, we use mach_port_deallocate() since that will get rid of a
		 * send, send_once, or dead name right.
		 */
		(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
	}

	(void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldThreadState);
	pthread_testcancel();

	/* release any callback context info */
	if (storePrivate->rlsContext.release != NULL) {
		(*storePrivate->rlsContext.release)(storePrivate->rlsContext.info);
	}

	/* release any keys being watched */
	if (storePrivate->keys != NULL) CFRelease(storePrivate->keys);
	if (storePrivate->patterns != NULL) CFRelease(storePrivate->patterns);

	/* release any client info */
	if (storePrivate->name != NULL) CFRelease(storePrivate->name);
	if (storePrivate->options != NULL) CFRelease(storePrivate->options);

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


static void
childForkHandler()
{
	/* the process has forked (and we are the child process) */

	_sc_server = MACH_PORT_NULL;
	return;
}


static pthread_once_t initialized	= PTHREAD_ONCE_INIT;

static void
__SCDynamicStoreInitialize(void)
{
	CFBundleRef	bundle;

	/* register with CoreFoundation */
	__kSCDynamicStoreTypeID = _CFRuntimeRegisterClass(&__SCDynamicStoreClass);

	/* add handler to cleanup after fork() */
	(void) pthread_atfork(NULL, NULL, childForkHandler);

	/* get the application/executable/bundle name */
	bundle = CFBundleGetMainBundle();
	if (bundle != NULL) {
		_sc_bundleID = CFBundleGetIdentifier(bundle);
		if (_sc_bundleID != NULL) {
			CFRetain(_sc_bundleID);
		} else {
			CFURLRef	url;

			url = CFBundleCopyExecutableURL(bundle);
			if (url != NULL) {
				_sc_bundleID = CFURLCopyPath(url);
				CFRelease(url);
			}
		}

		if (_sc_bundleID != NULL) {
			if (CFEqual(_sc_bundleID, CFSTR("/"))) {
				CFRelease(_sc_bundleID);
				_sc_bundleID = CFStringCreateWithFormat(NULL, NULL, CFSTR("(%d)"), getpid());
			}
		}
	}

	return;
}


static mach_port_t
__SCDynamicStoreServerPort(kern_return_t *status)
{
	mach_port_t	server	= MACH_PORT_NULL;
	char		*server_name;

	server_name = getenv("SCD_SERVER");
	if (!server_name) {
		server_name = SCD_SERVER;
	}

#ifdef	BOOTSTRAP_PRIVILEGED_SERVER
	*status = bootstrap_look_up2(bootstrap_port,
				     server_name,
				     &server,
				     0,
				     BOOTSTRAP_PRIVILEGED_SERVER);
#else	// BOOTSTRAP_PRIVILEGED_SERVER
	*status = bootstrap_look_up(bootstrap_port, server_name, &server);
#endif	// BOOTSTRAP_PRIVILEGED_SERVER

	switch (*status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			return server;
		case BOOTSTRAP_NOT_PRIVILEGED :
			/* the service is not privileged */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			break;
		default :
#ifdef	DEBUG
			SCLog(_sc_verbose, LOG_DEBUG,
			      CFSTR("SCDynamicStoreCreate[WithOptions] bootstrap_look_up() failed: status=%s"),
			      bootstrap_strerror(*status));
#endif	/* DEBUG */
			break;
	}

	return MACH_PORT_NULL;
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
	if (storePrivate == NULL) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	/* client side of the "configd" session */
	storePrivate->name				= NULL;
	storePrivate->options				= NULL;

	/* server side of the "configd" session */
	storePrivate->server				= MACH_PORT_NULL;

	/* flags */
	storePrivate->locked				= FALSE;
	storePrivate->useSessionKeys			= FALSE;

	/* Notification status */
	storePrivate->notifyStatus			= NotifierNotRegistered;

	/* "client" information associated with SCDynamicStoreCreateRunLoopSource() */
	storePrivate->rlList				= NULL;
	storePrivate->rls				= NULL;
	storePrivate->rlsFunction			= callout;
	storePrivate->rlsContext.info			= NULL;
	storePrivate->rlsContext.retain			= NULL;
	storePrivate->rlsContext.release		= NULL;
	storePrivate->rlsContext.copyDescription	= NULL;
	if (context) {
		bcopy(context, &storePrivate->rlsContext, sizeof(SCDynamicStoreContext));
		if (context->retain != NULL) {
			storePrivate->rlsContext.info = (void *)(*context->retain)(context->info);
		}
	}

	/* "client" information associated with SCDynamicStoreNotifyCallback() */
	storePrivate->callbackFunction			= NULL;
	storePrivate->callbackArgument			= NULL;
	storePrivate->callbackPort			= NULL;
	storePrivate->callbackRLS			= NULL;

	/* "client" information associated with SCDynamicStoreSetDispatchQueue() */
	storePrivate->dispatchQueue			= NULL;
	storePrivate->callbackSource			= NULL;
	storePrivate->callbackQueue			= NULL;

	/* "client" information associated with SCDynamicStoreSetDisconnectCallBack() */
	storePrivate->disconnectFunction		= NULL;
	storePrivate->disconnectForceCallBack		= FALSE;

	/* "server" information associated with SCDynamicStoreSetNotificationKeys() */
	storePrivate->keys				= NULL;
	storePrivate->patterns				= NULL;

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


static Boolean
__SCDynamicStoreAddSession(SCDynamicStorePrivateRef storePrivate)
{
	CFDataRef	myName;			/* serialized name */
	xmlData_t	myNameRef;
	CFIndex		myNameLen;
	CFDataRef	myOptions	= NULL;	/* serialized options */
	xmlData_t	myOptionsRef	= NULL;
	CFIndex		myOptionsLen	= 0;
	int		sc_status	= kSCStatusFailed;
	mach_port_t	server;
	kern_return_t	status		= KERN_SUCCESS;

	if (!_SCSerializeString(storePrivate->name, &myName, (void **)&myNameRef, &myNameLen)) {
		goto done;
	}

	/* serialize the options */
	if (storePrivate->options != NULL) {
		if (!_SCSerialize(storePrivate->options, &myOptions, (void **)&myOptionsRef, &myOptionsLen)) {
			CFRelease(myName);
			goto done;
		}
	}

	/* open a new session with the server */
	server = _sc_server;
	while (TRUE) {
		if (server != MACH_PORT_NULL) {
			status = configopen(server,
					    myNameRef,
					    myNameLen,
					    myOptionsRef,
					    myOptionsLen,
					    &storePrivate->server,
					    (int *)&sc_status);
			if (status == KERN_SUCCESS) {
				break;
			}

			// our [cached] server port is not valid
			if ((status != MACH_SEND_INVALID_DEST) && (status != MIG_SERVER_DIED)) {
				// if we got an unexpected error, don't retry
				sc_status = status;
				break;
			}
		}

		pthread_mutex_lock(&_sc_lock);
		if (_sc_server != MACH_PORT_NULL) {
			if (server == _sc_server) {
				// if the server we tried returned the error
				(void)mach_port_deallocate(mach_task_self(), _sc_server);
				_sc_server = __SCDynamicStoreServerPort(&sc_status);
			} else {
				// another thread has refreshed the SCDynamicStore server port
			}
		} else {
			_sc_server = __SCDynamicStoreServerPort(&sc_status);
		}
		server = _sc_server;
		pthread_mutex_unlock(&_sc_lock);

		if (server == MACH_PORT_NULL) {
			// if SCDynamicStore server not available
			break;
		}
	}
	__MACH_PORT_DEBUG(TRUE, "*** SCDynamicStoreAddSession", storePrivate->server);

	// clean up
	CFRelease(myName);
	if (myOptions != NULL)	CFRelease(myOptions);

    done :

	switch (sc_status) {
		case kSCStatusOK :
			return TRUE;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			SCLog(TRUE,
			      (status == KERN_SUCCESS) ? LOG_DEBUG : LOG_ERR,
			      CFSTR("SCDynamicStore server not available"));
			break;
		default :
			SCLog(TRUE,
			      (status == KERN_SUCCESS) ? LOG_DEBUG : LOG_ERR,
			      CFSTR("SCDynamicStoreCreateAddSession configopen(): %s"),
			      SCErrorString(sc_status));
			break;
	}

	_SCErrorSet(sc_status);
	return FALSE;
}


__private_extern__
Boolean
__SCDynamicStoreReconnect(SCDynamicStoreRef store)
{
	Boolean				ok;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	ok = __SCDynamicStoreAddSession(storePrivate);
	return ok;
}


static void
pushDisconnect(SCDynamicStoreRef store)
{
	void					*context_info;
	void					(*context_release)(const void *);
	SCDynamicStoreDisconnectCallBack	disconnectFunction;
	SCDynamicStorePrivateRef		storePrivate	= (SCDynamicStorePrivateRef)store;

	disconnectFunction = storePrivate->disconnectFunction;
	if (disconnectFunction == NULL) {
		// if no reconnect callout, push empty notification
		storePrivate->disconnectForceCallBack = TRUE;
		return;
	}

	if (storePrivate->rlsContext.retain != NULL) {
		context_info	= (void *)storePrivate->rlsContext.retain(storePrivate->rlsContext.info);
		context_release	= storePrivate->rlsContext.release;
	} else {
		context_info	= storePrivate->rlsContext.info;
		context_release	= NULL;
	}
	(*disconnectFunction)(store, context_info);
	if (context_release) {
		context_release(context_info);
	}

	return;
}


__private_extern__
Boolean
__SCDynamicStoreReconnectNotifications(SCDynamicStoreRef store)
{
	dispatch_queue_t			dispatchQueue	= NULL;
	__SCDynamicStoreNotificationStatus	notifyStatus;
	Boolean					ok		= TRUE;
	CFArrayRef				rlList		= NULL;
	SCDynamicStorePrivateRef		storePrivate	= (SCDynamicStorePrivateRef)store;

	// save old SCDynamicStore [notification] state
	notifyStatus = storePrivate->notifyStatus;

	// before tearing down our [old] notifications, make sure we've
	// retained any information that will be lost when we cancel the
	// current no-longer-valid handler
	switch (notifyStatus) {
		case Using_NotifierInformViaRunLoop :
			if (storePrivate->rlList != NULL) {
				rlList = CFArrayCreateCopy(NULL, storePrivate->rlList);
			}
		case Using_NotifierInformViaDispatch :
			dispatchQueue = storePrivate->dispatchQueue;
			if (dispatchQueue != NULL) dispatch_retain(dispatchQueue);
			break;
		default :
			break;
	}

#ifdef	NOTNOW
	// invalidate the run loop source(s)
	if (storePrivate->callbackRLS != NULL) {
		CFRunLoopSourceInvalidate(storePrivate->callbackRLS);
		CFRelease(storePrivate->callbackRLS);
		storePrivate->callbackRLS = NULL;
	}

	// invalidate port
	if (storePrivate->callbackPort != NULL) {
		__MACH_PORT_DEBUG(TRUE, "*** __SCDynamicStoreReconnectNotifications w/MACH_NOTIFY_NO_SENDERS", CFMachPortGetPort(storePrivate->callbackPort));
		CFMachPortInvalidate(storePrivate->callbackPort);
		CFRelease(storePrivate->callbackPort);
		storePrivate->callbackPort = NULL;
	}
#endif	// NOTNOW

	// cancel [old] notifications
	SCDynamicStoreNotifyCancel(store);

	// set notification keys & patterns
	if ((storePrivate->keys != NULL) || (storePrivate->patterns)) {
		ok = SCDynamicStoreSetNotificationKeys(store,
						       storePrivate->keys,
						       storePrivate->patterns);
		if (!ok) {
			SCLog((SCError() != BOOTSTRAP_UNKNOWN_SERVICE),
			      LOG_ERR,
			      CFSTR("__SCDynamicStoreReconnectNotifications: SCDynamicStoreSetNotificationKeys() failed"));
			goto done;
		}
	}

	switch (notifyStatus) {
		case Using_NotifierInformViaRunLoop : {
			CFIndex			i;
			CFIndex			n;
			CFRunLoopSourceRef	rls;

			rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
			if (rls == NULL) {
				SCLog((SCError() != BOOTSTRAP_UNKNOWN_SERVICE),
				      LOG_ERR,
				      CFSTR("__SCDynamicStoreReconnectNotifications: SCDynamicStoreCreateRunLoopSource() failed"));
				ok = FALSE;
				break;
			}

			n = (rlList != NULL) ? CFArrayGetCount(rlList) : 0;
			for (i = 0; i < n; i += 3) {
				CFRunLoopRef	rl	= (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
				CFStringRef	rlMode	= (CFStringRef) CFArrayGetValueAtIndex(rlList, i+2);

				CFRunLoopAddSource(rl, rls, rlMode);
			}

			CFRelease(rls);
			break;
		}
		case Using_NotifierInformViaDispatch :
			ok = SCDynamicStoreSetDispatchQueue(store, dispatchQueue);
			if (!ok) {
				SCLog((SCError() != BOOTSTRAP_UNKNOWN_SERVICE),
				      LOG_ERR,
				      CFSTR("__SCDynamicStoreReconnectNotifications: SCDynamicStoreSetDispatchQueue() failed"));
				goto done;
			}
			break;

		default :
			_SCErrorSet(kSCStatusFailed);
			ok = FALSE;
			break;
	}

    done :

	// cleanup
	switch (notifyStatus) {
		case Using_NotifierInformViaRunLoop :
			if (rlList != NULL) CFRelease(rlList);
			break;
		case Using_NotifierInformViaDispatch :
			if (dispatchQueue != NULL) dispatch_release(dispatchQueue);
			break;
		default :
			break;
	}

	if (!ok) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStore server %s, notification (%s) not restored"),
		      (SCError() == BOOTSTRAP_UNKNOWN_SERVICE) ? "shutdown" : "failed",
		      notifyType[notifyStatus]);
	}

	// inform the client
	pushDisconnect(store);

	return ok;
}


const CFStringRef	kSCDynamicStoreUseSessionKeys	= CFSTR("UseSessionKeys");	/* CFBoolean */


SCDynamicStoreRef
SCDynamicStoreCreateWithOptions(CFAllocatorRef		allocator,
				CFStringRef		name,
				CFDictionaryRef		storeOptions,
				SCDynamicStoreCallBack	callout,
				SCDynamicStoreContext	*context)
{
	Boolean				ok;
	SCDynamicStorePrivateRef	storePrivate;

	// allocate and initialize a new session
	storePrivate = __SCDynamicStoreCreatePrivate(allocator, name, callout, context);
	if (storePrivate == NULL) {
		return NULL;
	}

	// set "name"
	if (_sc_bundleID != NULL) {
		storePrivate->name = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@:%@"), _sc_bundleID, name);
	} else {
		storePrivate->name = CFRetain(name);
	}

	// set "options"
	storePrivate->options = (storeOptions != NULL) ? CFRetain(storeOptions) : NULL;

	// establish SCDynamicStore session
	ok = __SCDynamicStoreAddSession(storePrivate);
	if (!ok) {
		CFRelease(storePrivate);
		storePrivate = NULL;
	}

	return (SCDynamicStoreRef)storePrivate;
}


SCDynamicStoreRef
SCDynamicStoreCreate(CFAllocatorRef		allocator,
		     CFStringRef		name,
		     SCDynamicStoreCallBack	callout,
		     SCDynamicStoreContext	*context)
{
	return SCDynamicStoreCreateWithOptions(allocator, name, NULL, callout, context);
}


CFTypeID
SCDynamicStoreGetTypeID(void) {
	pthread_once(&initialized, __SCDynamicStoreInitialize);	/* initialize runtime */
	return __kSCDynamicStoreTypeID;
}

Boolean
SCDynamicStoreSetDisconnectCallBack(SCDynamicStoreRef			store,
				    SCDynamicStoreDisconnectCallBack	callout)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	if (store == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return FALSE;
	}

	storePrivate->disconnectFunction = callout;
	return TRUE;
}
