/*
 * Copyright (c) 2000-2005, 2008, 2009 Apple Inc. All rights reserved.
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

#include <Availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#if	!TARGET_OS_IPHONE
#include <dispatch/dispatch.h>
#endif	// !TARGET_OS_IPHONE
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
	if (storePrivate->callbackRLS != NULL) {
		CFRunLoopSourceInvalidate(storePrivate->callbackRLS);
		CFRelease(storePrivate->callbackRLS);
		storePrivate->callbackRLS = NULL;
	}

	/* invalidate port */
	if (storePrivate->callbackPort != NULL) {
		__MACH_PORT_DEBUG(TRUE, "*** informCallback", CFMachPortGetPort(storePrivate->callbackPort));
		CFMachPortInvalidate(storePrivate->callbackPort);
		CFRelease(storePrivate->callbackPort);
		storePrivate->callbackPort = NULL;
	}

	/* disable notifier */
	storePrivate->notifyStatus     	= NotifierNotRegistered;
	storePrivate->callbackArgument 	= NULL;
	storePrivate->callbackFunction 	= NULL;

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
	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
	if (status != KERN_SUCCESS) {
		SCLog(TRUE, LOG_ERR, CFSTR("mach_port_allocate(): %s"), mach_error_string(status));
		_SCErrorSet(status);
		return FALSE;
	}

	status = mach_port_insert_right(mach_task_self(),
					port,
					port,
					MACH_MSG_TYPE_MAKE_SEND);
	if (status != KERN_SUCCESS) {
		/*
		 * We can't insert a send right into our own port!  This should
		 * only happen if someone stomped on OUR port (so let's leave
		 * the port alone).
		 */
		SCLog(TRUE, LOG_ERR, CFSTR("mach_port_insert_right(): %s"), mach_error_string(status));
		_SCErrorSet(status);
		return FALSE;
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
		/*
		 * We can't request a notification for our own port!  This should
		 * only happen if someone stomped on OUR port (so let's leave
		 * the port alone).
		 */
		SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreNotifyCallback mach_port_request_notification(): %s"), mach_error_string(status));
		_SCErrorSet(status);
		return FALSE;
	}

	if (oldNotify != MACH_PORT_NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreNotifyCallback(): oldNotify != MACH_PORT_NULL"));
	}

	/* Requesting notification via mach port */
	status = notifyviaport(storePrivate->server,
			       port,
			       0,
			       (int *)&sc_status);

	if (status != KERN_SUCCESS) {
		if (status == MACH_SEND_INVALID_DEST) {
			/* the server's gone and our session port's dead, remove the dead name right */
			(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
		} else {
			/* we got an unexpected error, leave the [session] port alone */
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreNotifyCallback notifyviaport(): %s"), mach_error_string(status));
		}
		storePrivate->server = MACH_PORT_NULL;

		if (status == MACH_SEND_INVALID_DEST) {
			/* remove the send right that we tried (but failed) to pass to the server */
			(void) mach_port_deallocate(mach_task_self(), port);
		}

		/* remove our receive right  */
		(void) mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
		_SCErrorSet(status);
		return FALSE;
	}

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	/* set notifier active */
	storePrivate->notifyStatus	= Using_NotifierInformViaCallback;

	/* Creating/adding a run loop source for the port */
	__MACH_PORT_DEBUG(TRUE, "*** SCDynamicStoreNotifyCallback", port);
	storePrivate->callbackArgument	= arg;
	storePrivate->callbackFunction	= func;
	storePrivate->callbackPort	= CFMachPortCreateWithPort(NULL, port, informCallback, &context, NULL);
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
		if (storePrivate->callbackRLS != NULL) {
			CFRunLoopSourceInvalidate(storePrivate->callbackRLS);
			CFRelease(storePrivate->callbackRLS);
			storePrivate->callbackRLS = NULL;
		}

		/* invalidate port */
		if (storePrivate->callbackPort != NULL) {
			__MACH_PORT_DEBUG(TRUE, "*** rlsCallback w/MACH_NOTIFY_NO_SENDERS", CFMachPortGetPort(storePrivate->callbackPort));
			CFMachPortInvalidate(storePrivate->callbackPort);
			CFRelease(storePrivate->callbackPort);
			storePrivate->callbackPort = NULL;
		}

		return;
	}

	/* signal the real runloop source */
	if (storePrivate->rls != NULL) {
		CFRunLoopSourceSignal(storePrivate->rls);
	}
	return;
}


static void
portInvalidate(CFMachPortRef port, void *info) {
	mach_port_t	mp	= CFMachPortGetPort(port);

	__MACH_PORT_DEBUG(TRUE, "*** portInvalidate", mp);
	/* remove our receive right  */
	(void)mach_port_mod_refs(mach_task_self(), mp, MACH_PORT_RIGHT_RECEIVE, -1);
}


static void
rlsSchedule(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

#ifdef	DEBUG
	SCLog(_sc_verbose, LOG_DEBUG,
	      CFSTR("schedule notifications for mode %@"),
	      (rl != NULL) ? mode : CFSTR("libdispatch"));
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
			SCLog(TRUE, LOG_ERR, CFSTR("rlsSchedule mach_port_allocate(): %s"), mach_error_string(status));
			return;
		}

		status = mach_port_insert_right(mach_task_self(),
						port,
						port,
						MACH_MSG_TYPE_MAKE_SEND);
		if (status != KERN_SUCCESS) {
			/*
			 * We can't insert a send right into our own port!  This should
			 * only happen if someone stomped on OUR port (so let's leave
			 * the port alone).
			 */
			SCLog(TRUE, LOG_ERR, CFSTR("rlsSchedule mach_port_insert_right(): %s"), mach_error_string(status));
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
			/*
			 * We can't request a notification for our own port!  This should
			 * only happen if someone stomped on OUR port (so let's leave
			 * the port alone).
			 */
			SCLog(TRUE, LOG_ERR, CFSTR("rlsSchedule mach_port_request_notification(): %s"), mach_error_string(status));
			return;
		}

		if (oldNotify != MACH_PORT_NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("rlsSchedule(): oldNotify != MACH_PORT_NULL"));
		}

		__MACH_PORT_DEBUG(TRUE, "*** rlsSchedule", port);
		status = notifyviaport(storePrivate->server, port, 0, (int *)&sc_status);
		if (status != KERN_SUCCESS) {
			if (status == MACH_SEND_INVALID_DEST) {
				/* the server's gone and our session port's dead, remove the dead name right */
				(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
			} else {
				/* we got an unexpected error, leave the [session] port alone */
				SCLog(TRUE, LOG_ERR, CFSTR("rlsSchedule notifyviaport(): %s"), mach_error_string(status));
			}
			storePrivate->server = MACH_PORT_NULL;

			if (status == MACH_SEND_INVALID_DEST) {
				/* remove the send right that we tried (but failed) to pass to the server */
				(void) mach_port_deallocate(mach_task_self(), port);
			}

			/* remove our receive right  */
			(void) mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
			return;
		}

		__MACH_PORT_DEBUG(TRUE, "*** rlsSchedule (after notifyviaport)", port);
		storePrivate->callbackPort = CFMachPortCreateWithPort(NULL, port, rlsCallback, &context, NULL);
		if (storePrivate->callbackPort == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("*** CFMachPortCreateWithPort returned NULL while attempting to schedule"));
			SCLog(TRUE, LOG_ERR, CFSTR("*** a SCDynamicStore notification.  Did this process call \"fork\" without"));
			SCLog(TRUE, LOG_ERR, CFSTR("*** calling \"exec\""));

			/* the server's gone and our session port's dead, remove the dead name right */
			(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
			storePrivate->server = MACH_PORT_NULL;

			/* remove our receive right  */
			(void) mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
			return;
		}
		CFMachPortSetInvalidationCallBack(storePrivate->callbackPort, portInvalidate);
		storePrivate->callbackRLS = CFMachPortCreateRunLoopSource(NULL, storePrivate->callbackPort, 0);
	}

	if ((rl != NULL) && (storePrivate->callbackRLS != NULL)) {
		CFRunLoopAddSource(rl, storePrivate->callbackRLS, mode);
		__MACH_PORT_DEBUG(TRUE, "*** rlsSchedule (after CFRunLoopAddSource)", CFMachPortGetPort(storePrivate->callbackPort));
	}

	return;
}


static void
rlsCancel(void *info, CFRunLoopRef rl, CFStringRef mode)
{
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

#ifdef	DEBUG
	SCLog(_sc_verbose, LOG_DEBUG,
	      CFSTR("cancel notifications for mode %@"),
	      (rl != NULL) ? mode : CFSTR("libdispatch"));
#endif	/* DEBUG */

	if ((rl != NULL) && (storePrivate->callbackRLS != NULL)) {
		CFRunLoopRemoveSource(rl, storePrivate->callbackRLS, mode);
	}

	if (--storePrivate->rlsRefs == 0) {
		int		sc_status;
		kern_return_t	status;

#ifdef	DEBUG
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  cancel callback runloop source"));
#endif	/* DEBUG */
		__MACH_PORT_DEBUG((storePrivate->callbackPort != NULL),
				  "*** rlsCancel",
				  CFMachPortGetPort(storePrivate->callbackPort));

		if (storePrivate->callbackRLS != NULL) {
			/* invalidate & remove the run loop source */
			CFRunLoopSourceInvalidate(storePrivate->callbackRLS);
			CFRelease(storePrivate->callbackRLS);
			storePrivate->callbackRLS = NULL;
		}

		if (storePrivate->callbackPort != NULL) {
			/* invalidate port */
			__MACH_PORT_DEBUG((storePrivate->callbackPort != NULL),
					  "*** rlsCancel (before invalidating CFMachPort)",
					  CFMachPortGetPort(storePrivate->callbackPort));
			CFMachPortInvalidate(storePrivate->callbackPort);
			CFRelease(storePrivate->callbackPort);
			storePrivate->callbackPort = NULL;
		}

		if (storePrivate->server != MACH_PORT_NULL) {
			status = notifycancel(storePrivate->server, (int *)&sc_status);
			if (status != KERN_SUCCESS) {
				if (status == MACH_SEND_INVALID_DEST) {
					/* the server's gone and our session port's dead, remove the dead name right */
					(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
				} else {
					/* we got an unexpected error, leave the [session] port alone */
					SCLog(TRUE, LOG_ERR, CFSTR("rlsCancel notifycancel(): %s"), mach_error_string(status));
				}
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

	if (storePrivate->rlsContext.retain != NULL) {
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
	CFMutableStringRef		result;
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)info;
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
		if (storePrivate->rls == NULL) {
			_SCErrorSet(kSCStatusFailed);
		}
	}

	return storePrivate->rls;
}


#if	!TARGET_OS_IPHONE
static boolean_t
SCDynamicStoreNotifyMIGCallback(mach_msg_header_t *message, mach_msg_header_t *reply)
{
	SCDynamicStorePrivateRef	storePrivate;

	storePrivate = dispatch_get_context(dispatch_get_current_queue());
	if (storePrivate != NULL) {
		CFRetain(storePrivate);
		dispatch_async(storePrivate->dispatchQueue, ^{
			rlsPerform(storePrivate);
			CFRelease(storePrivate);
		});
	}
	reply->msgh_remote_port = MACH_PORT_NULL;
	return false;
}


Boolean
SCDynamicStoreSetDispatchQueue(SCDynamicStoreRef store, dispatch_queue_t queue)
{
	Boolean				ok		= FALSE;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

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

	if (queue != NULL) {
		dispatch_queue_attr_t	attr;
		mach_port_t		mp;
		long			res;

		if ((storePrivate->dispatchQueue != NULL) || (storePrivate->rls != NULL)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}

		if (storePrivate->notifyStatus != NotifierNotRegistered) {
			/* sorry, you can only have one notification registered at once... */
			_SCErrorSet(kSCStatusNotifierActive);
			return FALSE;
		}

		/*
		 * mark our using of the SCDynamicStore notifications, create and schedule
		 * the notification port (storePrivate->callbackPort), and a bunch of other
		 * "setup"
		 */
		storePrivate->notifyStatus = Using_NotifierInformViaDispatch;
		rlsSchedule((void*)store, NULL, NULL);
		storePrivate->dispatchQueue = queue;
		dispatch_retain(storePrivate->dispatchQueue);

		/*
		 * create a queue for the mig source, we'll use this queue's context
		 * to carry the store pointer for the callback code.
		 */
		attr = dispatch_queue_attr_create();
		res = dispatch_queue_attr_set_finalizer(attr,
							^(dispatch_queue_t dq) {
								SCDynamicStoreRef	store;

								store = (SCDynamicStoreRef)dispatch_get_context(dq);
								CFRelease(store);
							});
		if (res != 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStore dispatch_queue_attr_set_finalizer() failed"));
			dispatch_release(attr);
			_SCErrorSet(kSCStatusFailed);
			goto cleanup;
		}
		storePrivate->callbackQueue = dispatch_queue_create("com.apple.SCDynamicStore.notifications", attr);
		dispatch_release(attr);
		if (storePrivate->callbackQueue == NULL){
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStore dispatch_queue_create() failed"));
			_SCErrorSet(kSCStatusFailed);
			goto cleanup;
		}
		CFRetain(store);	// Note: will be released when the dispatch queue is released
		dispatch_set_context(storePrivate->callbackQueue, (void *)store);

		dispatch_suspend(storePrivate->callbackQueue);
		mp = CFMachPortGetPort(storePrivate->callbackPort);
		storePrivate->callbackSource = dispatch_source_mig_create(mp, sizeof(mach_msg_header_t), NULL, storePrivate->callbackQueue, SCDynamicStoreNotifyMIGCallback);
		if (storePrivate->callbackSource == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStore dispatch_source_mig_create() failed"));
			_SCErrorSet(kSCStatusFailed);
			goto cleanup;
		}
		dispatch_resume(storePrivate->callbackQueue);

		ok = TRUE;
		goto done;
	} else {
		if (storePrivate->dispatchQueue == NULL) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return FALSE;
		}

		ok = TRUE;
	}

    cleanup :

	if (storePrivate->callbackSource != NULL) {
		dispatch_cancel(storePrivate->callbackSource);
		dispatch_release(storePrivate->callbackSource);
		storePrivate->callbackSource = NULL;
	}
	if (storePrivate->callbackQueue != NULL) {
		dispatch_release(storePrivate->callbackQueue);
		storePrivate->callbackQueue = NULL;
	}
	dispatch_release(storePrivate->dispatchQueue);
	storePrivate->dispatchQueue = NULL;
	rlsCancel((void*)store, NULL, NULL);
	storePrivate->notifyStatus = NotifierNotRegistered;

    done :

	return ok;
}
#endif	// !TARGET_OS_IPHONE
