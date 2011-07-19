/*
 * Copyright (c) 2000, 2001, 2004, 2005, 2010 Apple Inc. All rights reserved.
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

#ifndef _SCDYNAMICSTOREPRIVATE_H
#define _SCDYNAMICSTOREPRIVATE_H

#include <sys/cdefs.h>
#include <mach/message.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCDynamicStore.h>

/*!
	@header SCDynamicStorePrivate
	@discussion The SCDynamicStore APIs provide access to a key-value
		dictionary maintained by a server process.  The dictionary is
		accessible by all processes on the system.  The APIs allow you
		to list the current key-value pairs, add or remove key-value
		pairs, add or change the values associated with a key, and
		request change notifications.

		The SCDynamicStore is not "persistent" (the store content
		starts off empty and is not saved across boot).

		All interaction with the SCDynamicStore [server] is via Mach
		IPC (MiG) messages.

		A client process, using the SCDynamicStoreSetNotificationKeys
		API, specifies a list of specific keys of interest and/or a
		list of regex(3) pattern strings that will be matched on
		current (and future) keys in the store.  Any matched keys that
		are added, updated, or removed will trigger a notification.
		The notification is delivered to the monitoring session via
		a "something has changed" Mach IPC message.  The callback
		notification, as part of its implementation, issues a request
		to the server to identify the specific list of changes and
		this list is passed along as part of the callback.  Any
		subsequent changes to the SCDynamicStore will trigger a new
		"something has changed" Mach IPC message and an additional
		callback.

		Note: the list (array) of changed keys passed to the
		      notification callback will be always be non-NULL
		      and non-empty with one exception.  That exception
		      is when the SCDynamicStore server has been restarted.
		      In that case, if no reconnect callback was setup
		      with the SCDynamicStoreSetReconnectCallBack API
		      then a non-NULL but empty array will be passed.

		Disconnect/reconnect considerations:

		1. We don't expect the SCDynamicStore server to fail but one
		   should always be prepared for the unexpected.

		2. Processes that write to the SCDynamicStore should be
		   prepared to repost any content when/if the server fails.
		   A callout, registered with the SCDynamicStoreSetReconnectCallBack
		   API, should be used to post any updates the SCDynamicStore
		   after a failure.

		3. Processes that cache SCDynamicStore content (or otherwise
		   maintain state based on previous notifications) should be
		   aware that all store content is lost when/if the server
		   fails.  After handling a SCDynamicStore notification with
		   no keys or a disconnect/reconnect callout, your code should
		   assume that any cached content is no longer valid.

		Performance considerations:

		1. We recommend that any code trying to capture a snapshot of
		   more than one SCDynamicStore key should use the SCDynamicStoreCopyMultiple
		   API (and not make multiple calls to SCDynamicStoreCopyValue).

		2. We recommend that any code making multiple (and related)
		   changes to the SCDynamicStore should batch them into a
		   single call using the SCDynamicStoreSetMultiple API (and
		   not make multiple calls to SCDynamicStoreSetValue).
 */

/*!
	@typedef SCDynamicStoreCallBack
	@discussion Type of the callback function used when a
		dynamic store change is delivered.
	@param store The "dynamic store" session.
	@param info ....
 */
typedef boolean_t (*SCDynamicStoreCallBack_v1)	(
						SCDynamicStoreRef	store,
						void			*info
						);

/*!
	@typedef SCDynamicStoreDisconnectCallBack
	@discussion Type of callback function used when notification of
		the dynamic store session being disconnected is delivered.
	@param store The dynamic store session.
	@param info A C pointer to a user-specified block of data.
 */
typedef void (*SCDynamicStoreDisconnectCallBack)	(
							 SCDynamicStoreRef	store,
							 void			*info
							 );


__BEGIN_DECLS

/*!
	@function SCDynamicStoreLock
	@discussion Locks access to the configuration "dynamic store".  All
		other clients attempting to access the "dynamic store" will
		block. All change notifications will be deferred until the
		lock is released.
	@param store The "dynamic store" session that should be locked.
	@result TRUE if the lock was obtained; FALSE if an error was encountered.
 */
Boolean
SCDynamicStoreLock			(SCDynamicStoreRef		store);

/*!
	@function SCDynamicStoreUnlock
	@discussion Unlocks access to the configuration "dynamic store".  Other
		clients will be able to access the "dynamic store". Any change
		notifications will be delivered.
	@param store The "dynamic store" session that should be unlocked.
	@result TRUE if the lock was released; FALSE if an error was encountered.
 */
Boolean
SCDynamicStoreUnlock			(SCDynamicStoreRef		store);

/*!
	@function SCDynamicStoreTouchValue
	@discussion Updates the value of the specified key in the
		"dynamic store".
		If the value does not exist then a CFDate object
		will be associated with the key.
		If the associated data is already a CFDate object
		then it will be updated with the current time.
	@param store The "dynamic store" session.
	@param key The key of the value to updated.
	@result TRUE if the value was updated; FALSE if an error was encountered.
 */
Boolean
SCDynamicStoreTouchValue		(SCDynamicStoreRef		store,
					 CFStringRef			key);

/*!
	@function SCDynamicStoreAddWatchedKey
	@discussion Adds the specified key to the list of "dynamic store"
		values that are being monitored for changes.
	@param store The "dynamic store" session being watched.
	@param key The key to be monitored.
	@param isRegex A booolean indicating whether a specific key should
		be monitored or if it consists of a regex(3) pattern string
		of keys.
	@result TRUE if the key was successfully added to the "watch"
		list; FALSE if an error was encountered.
 */
Boolean
SCDynamicStoreAddWatchedKey		(SCDynamicStoreRef		store,
					 CFStringRef			key,
					 Boolean			isRegex);

/*!
	@function SCDynamicStoreRemoveWatchedKey
	@discussion Removes the specified key from the list of "dynamic store"
		values that are being monitored for changes.
	@param store The "dynamic store" session being watched.
	@param key The key that should no longer be monitored.
	@param isRegex A booolean indicating whether a specific key should
		be monitored or if it consists of a regex(3) pattern string
		of keys.
	@result TRUE if the key was successfully removed from the "watch"
		list; FALSE if an error was encountered.
 */
Boolean
SCDynamicStoreRemoveWatchedKey		(SCDynamicStoreRef		store,
					 CFStringRef			key,
					 Boolean			isRegex);

/*!
	@function SCDynamicStoreNotifyCallback
	@discussion Requests that the specified function be called whenever a
		change has been detected to one of the "dynamic store" values
		being monitored.

	The callback function will be called with two arguments, store and
	context, that correspond to the current "dynamic store" session and
	the provided context argument.

	The callback function should return a Boolean value indicating
	whether an error occurred during execution of the callback.

	Note: An additional run loop source will be added for the notification.
	This additional source will be removed if the notification is cancelled
	or if the callback indicates that an error was detected.

	@param store The "dynamic store" session.
	@param runLoop A pointer to the run loop.
	@param func The callback function to call for each notification.
		If this parameter is not a pointer to a function of the
		correct prototype, the behavior is undefined.
	@param context A pointer-sized user-defined value, that is passed as
		the second parameter to the notification callback function,
		but is otherwise unused by this function.  If the context
		is not what is expected by the notification function, the
		behavior is undefined.
	@result TRUE if the notification callback runloop source was
		successfully added; FALSE if an error was encountered.
 */
Boolean
SCDynamicStoreNotifyCallback		(SCDynamicStoreRef		store,
					 CFRunLoopRef			runLoop,
					 SCDynamicStoreCallBack_v1	func,
					 void				*context);

/*!
	@function SCDynamicStoreNotifyMachPort
	@discussion Allocates a mach port that can be used to detect changes to
		one of the system configuration data entries associated with the
		current session's notifier keys. When a change is detected, an
		empty (no data) mach message with the specified identifier will
		be delivered to the calling application via the allocated port.

	@param store An SCDynamicStoreRef that should be used for communication with the server.
	@param msgid A mach message ID to be included with any notifications.
	@param port A pointer to a mach port.  Upon return, port will be filled
		with the mach port that will be used for any notifications.
	@result A boolean indicating the success (or failure) of the call.
 */
Boolean
SCDynamicStoreNotifyMachPort		(SCDynamicStoreRef		store,
					 mach_msg_id_t			msgid,
					 mach_port_t			*port);

/*!
	@function SCDynamicStoreNotifyFileDescriptor
	@discussion Allocates a file descriptor that can be used to detect changes
		to one of the system configuration data entries associated with the
		current session's notifier keys. When a change is detected, the
		specified identifier (4 bytes) will be delivered to the calling
		application via the allocated file descriptor.

	@param store An SCDynamicStoreRef that should be used for communication with the server.
	@param identifier A (4 byte) integer that can be used to identify this
		notification.
	@param fd A pointer to a file descriptor.  Upon return, fd will
		contain the file descriptor that will be used for any notifications.
	@result A boolean indicating the success (or failure) of the call.
 */
Boolean
SCDynamicStoreNotifyFileDescriptor	(SCDynamicStoreRef		store,
					 int32_t			identifier,
					 int				*fd);

/*!
	@function SCDynamicStoreNotifySignal
	@discussion Requests that the specified BSD signal be sent to the process
		with the indicated process id whenever a change has been detected
		to one of the system configuration data entries associated with the
		current session's notifier keys.

		Note: this function is not valid for "configd" plug-ins.

	@param store An SCDynamicStoreRef that should be used for communication with the server.
	@param pid A UNIX process ID that should be signalled for any notifications.
	@param sig A signal number to be used.
	@result A boolean indicating the success (or failure) of the call.
 */
Boolean
SCDynamicStoreNotifySignal		(SCDynamicStoreRef		store,
					 pid_t				pid,
					 int				sig);

/*!
	@function SCDynamicStoreNotifyWait
	@discussion Waits for a change to be made to a value in the
		"dynamic store" that is being monitored.
	@param store The "dynamic store" session.
	@result TRUE if a change has been detected; FALSE if an error was encountered.
 */
Boolean
SCDynamicStoreNotifyWait		(SCDynamicStoreRef		store);

/*!
	@function SCDynamicStoreNotifyCancel
	@discussion Cancels any outstanding notification requests for
		this "dynamic store" session.

	@param store The "dynamic store" session.
	@result TRUE if all notifications have been cancelled; FALSE if an
		error was encountered.
 */
Boolean
SCDynamicStoreNotifyCancel		(SCDynamicStoreRef		store);

/*!
	@function SCDynamicStoreSetDisconnectCallBack
	@discussion Assigns a callback to a SCDynamicStore session.  The function
		is called when the session has been disconnected.  The callback
		should be established before a client writes any content to the
		SCDynamicStore to ensure that the information can be re-posted
		when/if a disconnect is detected.
	@param store A reference to the dynamic store session.
	@param callout The function to be called when the session was disconnected.
		If NULL, the current callback is removed.
	@result Returns TRUE on success, FALSE on failure.
 */
Boolean
SCDynamicStoreSetDisconnectCallBack	(
					 SCDynamicStoreRef			store,
					 SCDynamicStoreDisconnectCallBack	callout
					)				__OSX_AVAILABLE_STARTING(__MAC_10_7,__IPHONE_5_0/*SPI*/);

Boolean
SCDynamicStoreSnapshot			(SCDynamicStoreRef		store);

__END_DECLS

#endif /* _SCDYNAMICSTOREPRIVATE_H */
