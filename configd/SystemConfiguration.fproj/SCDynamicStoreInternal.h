/*
 * Copyright (c) 2000-2004, 2006, 2009 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCDYNAMICSTOREINTERNAL_H
#define _SCDYNAMICSTOREINTERNAL_H

#include <Availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#if	!TARGET_OS_IPHONE
#include <dispatch/dispatch.h>
#endif	// !TARGET_OS_IPHONE
#include <sys/types.h>
#include <mach/mach.h>
#include <pthread.h>
#include <regex.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SCDynamicStore.h>


/* Define the status of any registered notification. */
typedef enum {
	NotifierNotRegistered = 0,
	Using_NotifierWait,
	Using_NotifierInformViaCallback,
	Using_NotifierInformViaMachPort,
	Using_NotifierInformViaFD,
	Using_NotifierInformViaSignal,
	Using_NotifierInformViaRunLoop,
	Using_NotifierInformViaDispatch
} __SCDynamicStoreNotificationStatus;


typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* server side of the "configd" session */
	mach_port_t			server;

	/* per-session flags */
	Boolean				locked;
	Boolean				useSessionKeys;

	/* current status of notification requests */
	__SCDynamicStoreNotificationStatus	notifyStatus;

	/* "client" information associated with SCDynamicStoreCreateRunLoopSource() */
	CFIndex				rlsRefs;
	CFRunLoopSourceRef		rls;
	SCDynamicStoreCallBack		rlsFunction;
	SCDynamicStoreContext		rlsContext;

	/* "client" information associated with SCDynamicStoreNotifyCallback() */
	SCDynamicStoreCallBack_v1	callbackFunction;
	void				*callbackArgument;
	CFMachPortRef			callbackPort;
	CFRunLoopSourceRef		callbackRLS;

#if	!TARGET_OS_IPHONE
	/* "client" information associated with SCDynamicStoreSetDispatchQueue() */
	dispatch_queue_t		dispatchQueue;
	dispatch_source_t		callbackSource;
	dispatch_queue_t		callbackQueue;
#endif	// !TARGET_OS_IPHONE

	/* "server" SCDynamicStoreKeys being watched */
	CFMutableSetRef			keys;
	CFMutableSetRef			patterns;

	/* "server" information associated with SCDynamicStoreNotifyMachPort() */
	mach_port_t			notifyPort;
	mach_msg_id_t			notifyPortIdentifier;

	/* "server" information associated with SCDynamicStoreNotifyFileDescriptor() */
	int				notifyFile;
	int				notifyFileIdentifier;

	/* "server" information associated with SCDynamicStoreNotifySignal() */
	int				notifySignal;
	task_t				notifySignalTask;

} SCDynamicStorePrivate, *SCDynamicStorePrivateRef;


__BEGIN_DECLS

SCDynamicStorePrivateRef
__SCDynamicStoreCreatePrivate		(CFAllocatorRef			allocator,
					 const CFStringRef		name,
					 SCDynamicStoreCallBack		callout,
					 SCDynamicStoreContext		*context);

__END_DECLS

#endif /* _SCDYNAMICSTOREINTERNAL_H */
