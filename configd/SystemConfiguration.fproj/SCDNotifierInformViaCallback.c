/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 * March 31, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */

#include "v1Compatibility.h"

static void
informCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	mach_msg_empty_rcv_t		*buf		= msg;
	mach_msg_id_t			msgid		= buf->header.msgh_id;
	SCDynamicStoreCallBack_v1	cbFunc		= storePrivate->callbackFunction;
	void				*cbArg		= storePrivate->callbackArgument;

	if (msgid == MACH_NOTIFY_NO_SENDERS) {
		/* the server died, disable additional callbacks */
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  notifier port closed, disabling notifier"));
	} else if (cbFunc == NULL) {
		/* there is no (longer) a callback function, disable additional callbacks */
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  no callback function, disabling notifier"));
	} else {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  executing notifiction function"));
		if ((*cbFunc)(store, cbArg)) {
			/*
			 * callback function returned success.
			 */
			return;
		} else {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  callback returned error, disabling notifier"));
		}
	}

#ifdef	DEBUG
	if (port != storePrivate->callbackPort) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("informCallback, why is port != callbackPort?"));
	}
#endif	/* DEBUG */

	/* remove the run loop source */
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
			      storePrivate->callbackRunLoopSource,
			      kCFRunLoopDefaultMode);
	CFRelease(storePrivate->callbackRunLoopSource);

	/* invalidate port */
	CFMachPortInvalidate(storePrivate->callbackPort);
	CFRelease(storePrivate->callbackPort);

	/* disable notifier */
	storePrivate->notifyStatus     		= NotifierNotRegistered;
	storePrivate->callbackArgument 		= NULL;
	storePrivate->callbackFunction 		= NULL;
	storePrivate->callbackPort     		= NULL;
	storePrivate->callbackRunLoop		= NULL;
	storePrivate->callbackRunLoopSource	= NULL;

	return;
}


Boolean
SCDynamicStoreNotifyCallback(SCDynamicStoreRef		store,
			     CFRunLoopRef		runLoop,
			     SCDynamicStoreCallBack_v1	func,
			     void			*arg)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	mach_port_t			port;
	mach_port_t			oldNotify;
	int				sc_status;
	CFMachPortContext		context = { 0
						  , (void *)store
						  , CFRetain
						  , CFRelease
						  , NULL
						  };

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreNotifyCallback:"));

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return FALSE;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		/* sorry, you must have an open session to play */
		_SCErrorSet(kSCStatusNoStoreServer);
		return FALSE;
	}

	if (storePrivate->notifyStatus != NotifierNotRegistered) {
		/* sorry, you can only have one notification registered at once */
		_SCErrorSet(kSCStatusNotifierActive);
		return FALSE;
	}

	/* Allocating port (for server response) */
	storePrivate->callbackPort = CFMachPortCreate(NULL,
						      informCallback,
						      &context,
						      NULL);

	/* Request a notification when/if the server dies */
	port = CFMachPortGetPort(storePrivate->callbackPort);
	status = mach_port_request_notification(mach_task_self(),
						port,
						MACH_NOTIFY_NO_SENDERS,
						1,
						port,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&oldNotify);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
		CFMachPortInvalidate(storePrivate->callbackPort);
		CFRelease(storePrivate->callbackPort);
		_SCErrorSet(status);
		return FALSE;
	}

	if (oldNotify != MACH_PORT_NULL) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("SCDynamicStoreNotifyCallback(): why is oldNotify != MACH_PORT_NULL?"));
	}

	/* Requesting notification via mach port */
	status = notifyviaport(storePrivate->server,
			       port,
			       0,
			       (int *)&sc_status);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("notifyviaport(): %s"), mach_error_string(status));
		CFMachPortInvalidate(storePrivate->callbackPort);
		CFRelease(storePrivate->callbackPort);
		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return FALSE;
	}

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	/* set notifier active */
	storePrivate->notifyStatus		= Using_NotifierInformViaCallback;

	/* Creating/adding a run loop source for the port */
	storePrivate->callbackArgument		= arg;
	storePrivate->callbackFunction		= func;
	storePrivate->callbackRunLoop		= runLoop;
	storePrivate->callbackRunLoopSource =
		CFMachPortCreateRunLoopSource(NULL, storePrivate->callbackPort, 0);

	CFRunLoopAddSource(storePrivate->callbackRunLoop,
			   storePrivate->callbackRunLoopSource,
			   kCFRunLoopDefaultMode);

	return TRUE;
}


static void
rlsCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	mach_msg_empty_rcv_t		*buf		= msg;
	mach_msg_id_t			msgid		= buf->header.msgh_id;
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	if (msgid == MACH_NOTIFY_NO_SENDERS) {
		/* the server died, disable additional callbacks */
		SCLog(_sc_verbose, LOG_INFO, CFSTR("  rlsCallback(), notifier port closed"));

#ifdef	DEBUG
		if (port != storePrivate->callbackPort) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("rlsCallback(), why is port != callbackPort?"));
		}
#endif	/* DEBUG */

		/* remove the run loop source(s) */
		CFRunLoopSourceInvalidate(storePrivate->callbackRunLoopSource);
		CFRelease(storePrivate->callbackRunLoopSource);
		storePrivate->callbackRunLoopSource = NULL;

		/* invalidate port */
		CFMachPortInvalidate(storePrivate->callbackPort);
		CFRelease(storePrivate->callbackPort);
		storePrivate->callbackPort = NULL;

		return;
	}

	/* signal the real runloop source */
	CFRunLoopSourceSignal(storePrivate->rls);
	return;
}


static void
rlsPortInvalidate(CFMachPortRef mp, void *info) {
	mach_port_t	port	= CFMachPortGetPort(mp);

	// A simple deallocate won't get rid of all the references we've accumulated
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  invalidate = %d"), port);
	mach_port_destroy(mach_task_self(), port);
}


static void
rlsSchedule(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("schedule notifications for mode %@"), mode);

	if (storePrivate->rlsRefs++ == 0) {
		CFMachPortContext	context = { 0
						  , (void *)store
						  , CFRetain
						  , CFRelease
						  , NULL
						  };
		mach_port_t		oldNotify;
		mach_port_t		port;
		int			sc_status;
		kern_return_t		status;

		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  activate callback runloop source"));

		/* Allocating port (for server response) */
		status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("mach_port_allocate(): %s"), mach_error_string(status));
			return;
		}
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  port = %d"), port);

		status = mach_port_insert_right(mach_task_self(),
						port,
						port,
						MACH_MSG_TYPE_MAKE_SEND);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("mach_port_insert_right(): %s"), mach_error_string(status));
			(void) mach_port_destroy(mach_task_self(), port);
			return;
		}

		/* Request a notification when/if the server dies */
		status = mach_port_request_notification(mach_task_self(),
							port,
							MACH_NOTIFY_NO_SENDERS,
							1,
							port,
							MACH_MSG_TYPE_MAKE_SEND_ONCE,
							&oldNotify);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
			(void) mach_port_destroy(mach_task_self(), port);
			return;
		}

		if (oldNotify != MACH_PORT_NULL) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("rlsSchedule(): why is oldNotify != MACH_PORT_NULL?"));
		}

		status = notifyviaport(storePrivate->server, port, 0, (int *)&sc_status);
		if (status != KERN_SUCCESS) {
			if (status != MACH_SEND_INVALID_DEST)
				SCLog(_sc_verbose, LOG_DEBUG, CFSTR("notifyviaport(): %s"), mach_error_string(status));
			(void) mach_port_destroy(mach_task_self(), port);
			port = MACH_PORT_NULL;
			(void) mach_port_destroy(mach_task_self(), storePrivate->server);
			storePrivate->server = MACH_PORT_NULL;
			return;
		}

		storePrivate->callbackPort = CFMachPortCreateWithPort(NULL, port, rlsCallback, &context, NULL);
		CFMachPortSetInvalidationCallBack(storePrivate->callbackPort, rlsPortInvalidate);
		storePrivate->callbackRunLoopSource = CFMachPortCreateRunLoopSource(NULL, storePrivate->callbackPort, 0);
	}

	CFRunLoopAddSource(rl, storePrivate->callbackRunLoopSource, mode);
	return;
}


static void
rlsCancel(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("cancel notifications for mode %@"), mode);

	CFRunLoopRemoveSource(rl, storePrivate->callbackRunLoopSource, mode);

	if (--storePrivate->rlsRefs == 0) {
		int		sc_status;
		kern_return_t	status;

		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  cancel callback runloop source"));

		/* remove the run loop source */
		CFRelease(storePrivate->callbackRunLoopSource);
		storePrivate->callbackRunLoopSource = NULL;

		/* invalidate port */
		CFMachPortInvalidate(storePrivate->callbackPort);
		CFRelease(storePrivate->callbackPort);
		storePrivate->callbackPort = NULL;

		status = notifycancel(storePrivate->server, (int *)&sc_status);
		if (status != KERN_SUCCESS) {
			if (status != MACH_SEND_INVALID_DEST)
				SCLog(_sc_verbose, LOG_INFO, CFSTR("notifycancel(): %s"), mach_error_string(status));
			(void) mach_port_destroy(mach_task_self(), storePrivate->server);
			storePrivate->server = MACH_PORT_NULL;
			return;
		}
	}
	return;
}

static void
rlsPerform(void *info)
{
	CFArrayRef			changedKeys;
	void				*context_info;
	void				(*context_release)(const void *);
	SCDynamicStoreCallBack		rlsFunction;
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  executing notifiction function"));

	changedKeys = SCDynamicStoreCopyNotifiedKeys(store);
	if (!changedKeys) {
		/* something happened to the server */
		return;
	}

	rlsFunction = storePrivate->rlsFunction;

	if (NULL != storePrivate->rlsContext.retain) {
		context_info	= (void *)storePrivate->rlsContext.retain(storePrivate->rlsContext.info);
		context_release	= storePrivate->rlsContext.release;
	} else {
		context_info	= storePrivate->rlsContext.info;
		context_release	= NULL;
	}
	(*rlsFunction)(store, changedKeys, context_info);
	if (context_release) {
		context_release(context_info);
	}

	CFRelease(changedKeys);
	return;
}


static CFTypeRef
rlsRetain(CFTypeRef cf)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)cf;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	if (storePrivate->notifyStatus != Using_NotifierInformViaRunLoop) {
		/* mark RLS active */
		storePrivate->notifyStatus = Using_NotifierInformViaRunLoop;
		/* keep a reference to the store */
		CFRetain(store);
	}

	return cf;
}

static void
rlsRelease(CFTypeRef cf)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)cf;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	/* mark RLS inactive */
	storePrivate->notifyStatus = NotifierNotRegistered;
	storePrivate->rls = NULL;

	/* release our reference to the store */
	CFRelease(store);

	return;
}


CFRunLoopSourceRef
SCDynamicStoreCreateRunLoopSource(CFAllocatorRef	allocator,
				  SCDynamicStoreRef	store,
				  CFIndex		order)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreCreateRunLoopSource:"));

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		/* sorry, you must have an open session to play */
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;
	}

	switch (storePrivate->notifyStatus) {
		case NotifierNotRegistered :
		case Using_NotifierInformViaRunLoop :
			/* OK to enable runloop notification */
			break;
		default :
			/* sorry, you can only have one notification registered at once */
			_SCErrorSet(kSCStatusNotifierActive);
			return NULL;
	}

	if (storePrivate->rls) {
		CFRetain(storePrivate->rls);
	} else {
		CFRunLoopSourceContext	context = { 0			// version
						  , (void *)store	// info
						  , rlsRetain		// retain
						  , rlsRelease		// release
						  , CFCopyDescription	// copyDescription
						  , CFEqual		// equal
						  , CFHash		// hash
						  , rlsSchedule		// schedule
						  , rlsCancel		// cancel
						  , rlsPerform		// perform
						  };

		storePrivate->rls = CFRunLoopSourceCreate(allocator, order, &context);
	}

	if (!storePrivate->rls) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	return storePrivate->rls;
}
