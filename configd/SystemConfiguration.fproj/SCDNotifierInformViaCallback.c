/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
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
 * March 31, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */

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
#ifdef	DEBUG
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  notifier port closed, disabling notifier"));
#endif	/* DEBUG */
	} else if (cbFunc == NULL) {
		/* there is no (longer) a callback function, disable additional callbacks */
#ifdef	DEBUG
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  no callback function, disabling notifier"));
#endif	/* DEBUG */
	} else {
#ifdef	DEBUG
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  executing notification function"));
#endif	/* DEBUG */
		if ((*cbFunc)(store, cbArg)) {
			/*
			 * callback function returned success.
			 */
			return;
		} else {
#ifdef	DEBUG
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  callback returned error, disabling notifier"));
#endif	/* DEBUG */
		}
	}

#ifdef	DEBUG
	if (port != storePrivate->callbackPort) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("informCallback, why is port != callbackPort?"));
	}
#endif	/* DEBUG */

	/* invalidate the run loop source */
	CFRunLoopSourceInvalidate(storePrivate->callbackRLS);
	CFRelease(storePrivate->callbackRLS);
	storePrivate->callbackRLS = NULL;

	/* invalidate port */
	CFMachPortInvalidate(storePrivate->callbackPort);
	CFRelease(storePrivate->callbackPort);
	storePrivate->callbackPort     		= NULL;

	/* disable notifier */
	storePrivate->notifyStatus     		= NotifierNotRegistered;
	storePrivate->callbackArgument 		= NULL;
	storePrivate->callbackFunction 		= NULL;

	return;
}


static CFStringRef
notifyMPCopyDescription(const void *info)
{
	SCDynamicStoreRef	store	= (SCDynamicStoreRef)info;

	return CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("<SCDynamicStore notification MP> {store = %p}"),
					store);
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
						  , notifyMPCopyDescription
						  };

	if (store == NULL) {
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
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreNotifyCallback mach_port_request_notification(): %s"), mach_error_string(status));
		CFMachPortInvalidate(storePrivate->callbackPort);
		CFRelease(storePrivate->callbackPort);
		_SCErrorSet(status);
		return FALSE;
	}

#ifdef	DEBUG
	if (oldNotify != MACH_PORT_NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreNotifyCallback(): why is oldNotify != MACH_PORT_NULL?"));
	}
#endif	/* DEBUG */

	/* Requesting notification via mach port */
	status = notifyviaport(storePrivate->server,
			       port,
			       0,
			       (int *)&sc_status);

	if (status != KERN_SUCCESS) {
#ifdef	DEBUG
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreNotifyCallback notifyviaport(): %s"), mach_error_string(status));
#endif	/* DEBUG */
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
	storePrivate->callbackArgument	= arg;
	storePrivate->callbackFunction	= func;
	storePrivate->callbackRLS	= CFMachPortCreateRunLoopSource(NULL, storePrivate->callbackPort, 0);
	CFRunLoopAddSource(runLoop, storePrivate->callbackRLS, kCFRunLoopDefaultMode);

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
#ifdef	DEBUG
		SCLog(_sc_verbose, LOG_INFO, CFSTR("  rlsCallback(), notifier port closed"));
#endif	/* DEBUG */

#ifdef	DEBUG
		if (port != storePrivate->callbackPort) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("rlsCallback(), why is port != callbackPort?"));
		}
#endif	/* DEBUG */

		/* invalidate the run loop source(s) */
		CFRunLoopSourceInvalidate(storePrivate->callbackRLS);
		CFRelease(storePrivate->callbackRLS);
		storePrivate->callbackRLS = NULL;

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
#ifdef	DEBUG
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  invalidate = %d"), port);
#endif	/* DEBUG */
	(void)mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
}


static void
rlsSchedule(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

#ifdef	DEBUG
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("schedule notifications for mode %@"), mode);
#endif	/* DEBUG */

	if (storePrivate->rlsRefs++ == 0) {
		CFMachPortContext	context = { 0
						  , (void *)store
						  , CFRetain
						  , CFRelease
						  , notifyMPCopyDescription
						  };
		mach_port_t		oldNotify;
		mach_port_t		port;
		int			sc_status;
		kern_return_t		status;

#ifdef	DEBUG
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  activate callback runloop source"));
#endif	/* DEBUG */

		/* Allocating port (for server response) */
		status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
		if (status != KERN_SUCCESS) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("mach_port_allocate(): %s"), mach_error_string(status));
			return;
		}

		status = mach_port_insert_right(mach_task_self(),
						port,
						port,
						MACH_MSG_TYPE_MAKE_SEND);
		if (status != KERN_SUCCESS) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("mach_port_insert_right(): %s"), mach_error_string(status));
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
			SCLog(TRUE, LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
			(void) mach_port_destroy(mach_task_self(), port);
			return;
		}

#ifdef	DEBUG
		if (oldNotify != MACH_PORT_NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("rlsSchedule(): why is oldNotify != MACH_PORT_NULL?"));
		}
#endif	/* DEBUG */

		status = notifyviaport(storePrivate->server, port, 0, (int *)&sc_status);
		if (status != KERN_SUCCESS) {
#ifdef	DEBUG
			if (status != MACH_SEND_INVALID_DEST)
				SCLog(_sc_verbose, LOG_DEBUG, CFSTR("notifyviaport(): %s"), mach_error_string(status));
#endif	/* DEBUG */
			(void) mach_port_destroy(mach_task_self(), port);
			port = MACH_PORT_NULL;
			(void) mach_port_destroy(mach_task_self(), storePrivate->server);
			storePrivate->server = MACH_PORT_NULL;
			return;
		}

		storePrivate->callbackPort = CFMachPortCreateWithPort(NULL, port, rlsCallback, &context, NULL);
		CFMachPortSetInvalidationCallBack(storePrivate->callbackPort, rlsPortInvalidate);
		storePrivate->callbackRLS = CFMachPortCreateRunLoopSource(NULL, storePrivate->callbackPort, 0);
	}

	if (storePrivate->callbackRLS != NULL) {
		CFRunLoopAddSource(rl, storePrivate->callbackRLS, mode);
	}

	return;
}


static void
rlsCancel(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

#ifdef	DEBUG
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("cancel notifications for mode %@"), mode);
#endif	/* DEBUG */

	if (storePrivate->callbackRLS != NULL) {
		CFRunLoopRemoveSource(rl, storePrivate->callbackRLS, mode);
	}

	if (--storePrivate->rlsRefs == 0) {
		int		sc_status;
		kern_return_t	status;

#ifdef	DEBUG
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  cancel callback runloop source"));
#endif	/* DEBUG */

		if (storePrivate->callbackRLS != NULL) {
			/* invalidate & remove the run loop source */
			CFRunLoopSourceInvalidate(storePrivate->callbackRLS);
			CFRelease(storePrivate->callbackRLS);
			storePrivate->callbackRLS = NULL;
		}

		if (storePrivate->callbackPort != NULL) {
			/* invalidate port */
			CFMachPortInvalidate(storePrivate->callbackPort);
			CFRelease(storePrivate->callbackPort);
			storePrivate->callbackPort = NULL;
		}

		if (storePrivate->server != MACH_PORT_NULL) {
			status = notifycancel(storePrivate->server, (int *)&sc_status);
			if (status != KERN_SUCCESS) {
#ifdef	DEBUG
				if (status != MACH_SEND_INVALID_DEST)
					SCLog(_sc_verbose, LOG_INFO, CFSTR("notifycancel(): %s"), mach_error_string(status));
#endif	/* DEBUG */
				(void) mach_port_destroy(mach_task_self(), storePrivate->server);
				storePrivate->server = MACH_PORT_NULL;
				return;
			}
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

#ifdef	DEBUG
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  executing notification function"));
#endif	/* DEBUG */

	changedKeys = SCDynamicStoreCopyNotifiedKeys(store);
	if (changedKeys == NULL) {
		/* if no changes or something happened to the server */
		return;
	}

	if (CFArrayGetCount(changedKeys) == 0) {
		goto done;
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

    done :

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


static CFStringRef
rlsCopyDescription(const void *info)
{
	CFMutableStringRef	result;
	SCDynamicStoreRef	store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	result = CFStringCreateMutable(NULL, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCDynamicStore RLS> {"));
	CFStringAppendFormat(result, NULL, CFSTR("store = %p"), store);
	if (storePrivate->notifyStatus == Using_NotifierInformViaRunLoop) {
		CFStringRef	description	= NULL;

		CFStringAppendFormat(result, NULL, CFSTR(", callout = %p"), storePrivate->rlsFunction);

		if ((storePrivate->rlsContext.info != NULL) && (storePrivate->rlsContext.copyDescription != NULL)) {
			description = (*storePrivate->rlsContext.copyDescription)(storePrivate->rlsContext.info);
		}
		if (description == NULL) {
			description = CFStringCreateWithFormat(NULL, NULL, CFSTR("<SCDynamicStore context %p>"), storePrivate->rlsContext.info);
		}
		if (description == NULL) {
			description = CFRetain(CFSTR("<no description>"));
		}
		CFStringAppendFormat(result, NULL, CFSTR(", context = %@"), description);
		CFRelease(description);
	} else {
		CFStringAppendFormat(result, NULL, CFSTR(", callout = %p"), storePrivate->callbackFunction);
		CFStringAppendFormat(result, NULL, CFSTR(", info = %p"), storePrivate->callbackArgument);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


CFRunLoopSourceRef
SCDynamicStoreCreateRunLoopSource(CFAllocatorRef	allocator,
				  SCDynamicStoreRef	store,
				  CFIndex		order)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	if (store == NULL) {
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

	if (storePrivate->rls != NULL) {
		CFRetain(storePrivate->rls);
	} else {
		CFRunLoopSourceContext	context = { 0			// version
						  , (void *)store	// info
						  , rlsRetain		// retain
						  , rlsRelease		// release
						  , rlsCopyDescription	// copyDescription
						  , CFEqual		// equal
						  , CFHash		// hash
						  , rlsSchedule		// schedule
						  , rlsCancel		// cancel
						  , rlsPerform		// perform
						  };

		storePrivate->rls = CFRunLoopSourceCreate(allocator, order, &context);
	}

	if (storePrivate->rls == NULL) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	return storePrivate->rls;
}
