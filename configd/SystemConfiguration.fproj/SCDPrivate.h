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

#ifndef _SCDPRIVATE_H
#define _SCDPRIVATE_H

#include <SystemConfiguration/SCD.h>

#include <sys/cdefs.h>
#include <sys/types.h>
#include <regex.h>
#include <pthread.h>
#include <CoreFoundation/CFMachPort.h>


/* Define the per-session (and global) flags */
typedef struct {
	int			debug;
	int			verbose;
	boolean_t		isLocked;
	boolean_t		useSyslog;
	boolean_t		useCFRunLoop;
} _SCDFlags;


/* Define the status of any registered notification. */
typedef enum {
	NotifierNotRegistered = 0,
	Using_NotifierWait,
	Using_NotifierInformViaCallback,
	Using_NotifierInformViaMachPort,
	Using_NotifierInformViaFD,
	Using_NotifierInformViaSignal,
} _SCDNotificationStatus;


typedef struct {

	/* server side of the "configd" session */
	mach_port_t		server;

	/* per-session flags */
	_SCDFlags		flags;

	/* SCDKeys being watched */
	CFMutableSetRef		keys;
	CFMutableSetRef		reKeys;

	/* current status of notification requests */
	_SCDNotificationStatus	notifyStatus;

	/* "client" information associated with SCDNotifierInformViaCallback() */
	SCDCallbackRoutine_t	callbackFunction;
	void			*callbackArgument;
	CFMachPortRef		callbackPort;
	CFRunLoopSourceRef	callbackRunLoopSource;	/* XXX CFMachPortInvalidate() doesn't work */
	pthread_t		callbackHelper;

	/* "server" information associated with SCDNotifierInformViaMachPort() */
	mach_port_t		notifyPort;
	mach_msg_id_t		notifyPortIdentifier;

	/* "server" information associated with SCDNotifierInformViaFD() */
	int			notifyFile;
	int			notifyFileIdentifier;

	/* "server" information associated with SCDNotifierInformViaSignal() */
	int			notifySignal;
	task_t			notifySignalTask;

} SCDSessionPrivate, *SCDSessionPrivateRef;


typedef struct {

	/* configuration data associated with key */
	CFPropertyListRef	data;

	/* instance value of last fetched data */
	int			instance;

} SCDHandlePrivate, *SCDHandlePrivateRef;


/* per-session options */
typedef enum {
	kSCDOptionIsLocked = 1024,
} SCDServerSessionOptions;


/* global options */
typedef enum {
	kSCDOptionIsServer = 2048,
} SCDServerGlobalOptions;


__BEGIN_DECLS

SCDSessionRef	_SCDSessionCreatePrivate	();

void		_SCDHandleSetInstance		(SCDHandleRef	handle,
						 int		instance);

mach_msg_id_t	_waitForMachMessage		(mach_port_t	port);

void		_showMachPortStatus		();
void		_showMachPortReferences		(mach_port_t	port);

__END_DECLS

#endif /* !_SCDPRIVATE_H */
