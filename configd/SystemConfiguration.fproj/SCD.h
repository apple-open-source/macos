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

#ifndef _SCD_H
#define _SCD_H

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRunLoop.h>
#include <mach/message.h>
#include <sys/cdefs.h>
#include <sys/syslog.h>


/*!
	@header SCD.h
	The SystemConfiguration framework provides access to the data used to configure a running system.  The APIs provided by this framework communicate with the "configd" daemon.

The "configd" daemon manages a "cache" reflecting the desired configuration settings as well as the current state of the system.  The daemon provides a notification mechanism for user-level processes which need to be aware of changes made to the "cache" data.  Lastly, the daemon loads a number of bundles(or plug-ins) which monitor low-level kernel events and, via a set of policy modules, keep this cached data up to date.

The "configd" daemon also provides an address space/task/process which can be used by other CFRunLoop based functions which would otherwise require their own process/daemon for execution.

 */


/*!
	@enum SCDStatus
	@discussion Returned status codes.
	@constant SCD_OK		Success
	@constant SCD_NOSESSION		Configuration daemon session not active
	@constant SCD_NOSERVER		Configuration daemon not (no longer) available
	@constant SCD_LOCKED		Lock already held
	@constant SCD_NEEDLOCK		Lock required for this operation
	@constant SCD_EACCESS		Permission denied (must be root to obtain lock)
	@constant SCD_NOKEY		No such key
	@constant SCD_EXISTS		Data associated with key already defined
	@constant SCD_STALE		Write attempted on stale version of object
	@constant SCD_INVALIDARGUMENT	Invalid argument
	@constant SCD_NOTIFIERACTIVE	Notifier is currently active
	@constant SCD_FAILED		Generic error
 */
typedef enum {
	SCD_OK			= 0,	/* Success */
	SCD_NOSESSION		= 1,	/* Configuration daemon session not active */
	SCD_NOSERVER		= 2,	/* Configuration daemon not (no longer) available */
	SCD_LOCKED		= 3,	/* Lock already held */
	SCD_NEEDLOCK		= 4,	/* Lock required for this operation */
	SCD_EACCESS		= 5,	/* Permission denied (must be root to obtain lock) */
	SCD_NOKEY		= 6,	/* No such key */
	SCD_EXISTS		= 7,	/* Data associated with key already defined */
	SCD_STALE		= 8,	/* Write attempted on stale version of object */
	SCD_INVALIDARGUMENT	= 9,	/* Invalid argument */
	SCD_NOTIFIERACTIVE	= 10,	/* Notifier is currently active */
	SCD_FAILED		= 9999	/* Generic error */
} SCDStatus;


/*!
	@typedef SCDSessionRef
	@discussion This is the type of a handle to an open "session" with the system
		configuration daemon.
 */
typedef const struct __SCDSession *	SCDSessionRef;

/*!
	@typedef SCDHandleRef
	@discussion This is the type of a handle to data being retrieved from or to be
		stored by the system configuration daemon.
 */
typedef const struct __SCDHandle *	SCDHandleRef;


/*!
	@typedef SCDCallbackRoutine_t
	@discussion Type of the callback function used by the SCDNotifierInformViaCallback()
		function.
	@param session The session handle.
	@param callback_argument The user-defined argument to be passed to the callback
		function.
 */
typedef boolean_t (*SCDCallbackRoutine_t)	 (SCDSessionRef		session,
						  void			*callback_argument);


/*!
	@enum SCDKeyOption
	@discussion Used with the SCDList() and SCDNotifierAdd() functions to describe
		the CFStringRef argument.
	@constant kSCDRegexKey Specifies that the key consists of a regular expression.
 */
typedef enum {
	kSCDRegexKey		= 0100000,	/* pattern string is a regular expression */
} SCDKeyOption;


/*!
	@enum SCDOption
	@discussion Options which determine the run-time processing of the system
		configuration framework functions.
	@constant kSCDOptionDebug Enable debugging
	@constant kSCDOptionVerbose Enable verbose logging
	@constant kSCDOptionUseSyslog Use syslog(3) for log messages
	@constant kSCDOptionUseCFRunLoop Calling application is CFRunLoop() based
 */
typedef enum {
	kSCDOptionDebug		= 0,	/* Enable debugging */
	kSCDOptionVerbose	= 1,	/* Enable verbose logging */
	kSCDOptionUseSyslog	= 2,	/* Use syslog(3) for log messages */
	kSCDOptionUseCFRunLoop	= 3,	/* Calling application is CFRunLoop() based */
} SCDOption;


/*
 * "configd" loadable bundle / plug-in options
 */

/*!
	@typedef SCDBundleStartRoutine_t
	@discussion Type of the start() initializatioin function which will be called
		after all plug-ins have been loaded.  This function should initialize any
		variables, open any sessions with "configd", and register any needed
		notifications.
	@param bundlePath The path name associated with the plug-in / bundle.
	@param bundleName The name of the plug-in / bundle.
 */
typedef void (*SCDBundleStartRoutine_t) (const char *bundlePath,
					 const char *bundleName);

/*!
	@typedef SCDBundlePrimeRoutine_t
	@discussion Type of the prime() initializatioin function which will be
		called after all plug-ins have executed their start() function but
		before the main plug-in run loop is started.  This function should
		be used to initialize any configuration information and/or state
		in the cache.
 */
typedef void (*SCDBundlePrimeRoutine_t) ();

#define	BUNDLE_DIRECTORY	"/SystemConfiguration"	/* [/System/Library]/... */
#define BUNDLE_OLD_SUBDIR	"/"
#define BUNDLE_NEW_SUBDIR	"/Contents/MacOS/"
#define BUNDLE_DIR_EXTENSION	".bundle"
#define BUNDLE_DEBUG_EXTENSION	"_debug"
#define BUNDLE_ENTRY_POINT	"_start"
#define BUNDLE_ENTRY_POINT2	"_prime"


__BEGIN_DECLS

/*!
	@function SCDHandleInit
	@discussion Creates a new handle used to access the cached configuration
		dictionary data.
	@result A new "cache" data handle.
 */
SCDHandleRef	SCDHandleInit			();

/*!
	@function SCDHandleRelease
	@discussion Releases the specified configuration data handle. The dictionary
		(or other CFType) associated with this handle will also be released
		unless a call was previously made to CFRetain().
	@param handle The cache data handle to be released.
 */
void		SCDHandleRelease		(SCDHandleRef		handle);

/*!
	@function SCDHandleGetInstance
	@discussion Returns the instance number associated with the specified
		configuration handle.
	@param handle The cache data handle.
	@result The instance number associated with the specified handle.
 */
int		SCDHandleGetInstance		(SCDHandleRef		handle);

/*!
	@function SCDHandleGetData
	@discussion Returns a reference to the data associated with the specified
		configuration handle.
	@param handle The cache data handle.
	@result The CFType data associated with the specified handle.
 */
CFPropertyListRef SCDHandleGetData		(SCDHandleRef		handle);

/*!
	@function SCDHandleSetData
	@discussion Returns a reference to the data associated with the specified
		configuration handle.
	@param handle The cache data handle.
	@param data The CFType data to be associated with the handle.
 */
void		SCDHandleSetData		(SCDHandleRef		handle,
						 CFPropertyListRef	data);
/*!
	@function SCDOpen
	@discussion Initiates a connection with the configuration daemon.
	@param session A pointer to memory which will be filled with an SCDSessionRef
		handle to be used for all subsequent requests to the server.
		If a session cannot be established with the server, the contents of
		memory pointed to by this parameter are undefined.
	@param name Pass a string which describes the name of the calling process or
		plug-in name of the caller.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDOpen				(SCDSessionRef		*session,
						 CFStringRef		name);

/*!
	@function SCDClose
	@discussion Closes the specified session to the configuration daemon.  All
		outstanding notification requests will be cancelled.
	@param session Pass a pointer to an SCDSessionRef handle which should be closed.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDClose			(SCDSessionRef		*session);

/*!
	@function SCDLock
	@discussion Locks access to the configuration "cache".  All other clients
		attempting to access the "cache" will block. All change notifications
		will be deferred until the lock is released.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDLock				(SCDSessionRef		session);

/*!
	@function SCDUnlock
	@discussion Unlocks access to the configuration "cache".  Other clients will
		be able to access the "cache". Any change notifications will be delivered.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDUnlock			(SCDSessionRef		session);

/*!
	@function SCDList
	@discussion Returns an array of CFStringRefs representing the configuration "cache"
		entries which match the specified pattern key.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key The string which must prefix those keys in the configuration "cache"
		(if regexOptions is zero) or a regex(3) regular expression string which
		will be used to match configuration "cache" (if regexOptions is kSCDRegexKey).
	@param regexOptions Pass a bitfield of type SCDKeyOption containing one or more
		flags describing the pattern key.
	@param subKeys Pass a pointer to a CFArrayRef which will be set to a new
		array of CFStringRefs's matching the provided key.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDList				(SCDSessionRef		session,
						 CFStringRef		key,
						 int			regexOptions,
						 CFArrayRef		*subKeys);

/*!
	@function SCDAdd
	@discussion Creates a new entry in the configuration "cache" using the
		specified key and data.  An error is returned if the key is already
		defined in the dictionary.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key Pass a reference to the CFStringRef object to be created.
	@param handle Pass a reference to the SCDHandle object containing the data
		to be associated with the specified key.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDAdd				(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		handle);

/*!
	@function SCDAddSession
	@discussion Creates a new entry in the configuration "cache" using the
	specified key and data.  This entry will, unless updated by another
	session, automatically be removed when the session is closed.  An
	error is returned if the key is already defined in the dictionary.

	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key Pass a reference to the CFStringRef object to be created.
	@param handle Pass a reference to the SCDHandle object containing the data
		to be associated with the specified key.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDAddSession			(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		handle);

/*!
	@function SCDGet
	@discussion Returns a handle to the configuration "cache" data that corresponds
		to the specified key.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key Pass a reference to the CFStringRef object to be returned.
	@param handle Pass a pointer to a SCDHandleRef which will be set to a
		new object containing the data associated with the specified key.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDGet				(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		*handle);

/*!
	@function SCDSet
	@discussion Updates the entry in the configuration "cache" that corresponds to
		the specified key with the provided data.  An error will be returned if
		the data in the "cache" has been updated since the data handle was last
		updated.
	@param session Pass the SCDSessionRef handle which should be used to communicate
		with the server.
	@param key Pass a reference to the CFStringRef object to be updated.
	@param handle Pass a reference to the SCDHandle object containing the data
		to be associated with the specified key.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDSet				(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		handle);

/*!
	@function SCDRemove
	@discussion Removes the data from the configuration "cache" data which corresponds
		to the specified key.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key Pass a reference to the CFStringRef object to be removed.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDRemove			(SCDSessionRef		session,
						 CFStringRef		key);

/*!
	@function SCDTouch
	@discussion Updates the instance number for the data in the configuration "cache"
		associated with the specified key.  If the specified key does not exist
		then a CFDate object will be associated with the key.  If the associated
		data is already a CFDate object then the value will be updated.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key Pass a reference to the CFStringRef object to be updated.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDTouch			(SCDSessionRef		session,
						CFStringRef		key);

/*
	@function SCDSnapshot
	@discussion Records the current state of configd's cache dictionary into the
		/var/tmp/configd-cache file and configd's session dictionary into the
		/var/tmp/configd-session file.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDSnapshot			(SCDSessionRef		session);

/*!
	@function SCDNotifierList
	@discussion Returns an array of CFStringRefs representing the system configuration
		data entries being monitored for changes.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param regexOptions Pass a bitfield of type SCDKeyOption which specifies whether
		the specific configuration cache key patterns are to be returned or those
		based on regex(3) pattern strings.
	@param notifierKeys Pass a pointer to a CFArrayRef which will be set to a new
		array of CFStringRef's being monitored.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierList			(SCDSessionRef		session,
						 int			regexOptions,
						 CFArrayRef		*notifierKeys);

/*!
	@function SCDNotifierAdd
	@discussion Adds the specified key to the list of system configuration
		data entries which are being monitored for changes.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key Pass a reference to the CFStringRef object to be monitored.
	@param regexOptions Pass a bitfield of type SCDKeyOption which specifies whether
		the key is for a specific configuration cache key or if it consists
		of a regex(3) pattern string.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierAdd			(SCDSessionRef		session,
						 CFStringRef		key,
						 int			regexOptions);

/*!
	@function SCDNotifierRemove
	@discussion Removes the specified key from the list of system configuration
		data entries which are being monitored for changes.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param key Pass a reference to the CFStringRef object which should not be monitored.
	@param regexOptions Pass a bitfield of type SCDKeyOption which specifies whether
		the key is for a specific configuration cache key or if it consists
		of a regex(3) pattern string.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierRemove		(SCDSessionRef		session,
						 CFStringRef		key,
						 int			regexOptions);

/*!
	@function SCDNotifierGetChanges
	@discussion Returns an array of CFStringRefs representing the monitored system
		configuration data entries which have changed since this function
		was last called.
	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param notifierKeys Pass a pointer to a CFArrayRef which will be set to a new
		array of CFStringRef's being monitored.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierGetChanges		(SCDSessionRef		session,
						 CFArrayRef		*notifierKeys);

/*!
	@function SCDNotifierWait
	@discussion Waits for a change to be made to a system configuration data
		entry associated with the current sessions notifier keys.

	Note: this function is not valid for "configd" plug-ins.

	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierWait			(SCDSessionRef		session);

/*!
	@function SCDNotifierInformViaCallback
	@discussion Requests that the specified function be called whenever a change
		has been detected to one of the system configuration data entries
		associated with the current sessions notifier keys.

	The callback function will be called with two arguments, session and arg, which
	correspond to the current session and the provided argument.
	The function should return a boolean value indicating whether an
	error occurred during execution of the callback.

	Note: if the calling application is based on the CFRunLoop() then an additional
	run loop source will be added for the notification.
	Applications which are not based on the CFRunLoop() will have a separate thread
	started to wait for changes.
	In either case, the additional run loop source and/or thread will terminate if
	the notification is cancelled or if the callback indicates that an error was
	detected.

	Note: this function is not valid for "configd" plug-ins.

	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param func Pass a pointer to the callback applier function to call when a
		monitored cache entry is changed.  If this parameter is not a pointer to
		a function of the correct prototype (SCDCallbackRoutine_t), the behavior
		is undefined.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierInformViaCallback	(SCDSessionRef		session,
						 SCDCallbackRoutine_t	func,
						 void			*arg);

/*!
	@function SCDNotifierInformViaMachPort
	@discussion Allocates a mach port which can be used to detect changes to
		one of the system configuration data entries associated with the
		current sessions notifier keys. When a change is detected, an
		empty (no data) mach message with the specified identifier will
		be delivered to the calling application via the allocated port.

	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param msgid Pass a mach message ID to be included with any notifications.
	@param port Pass a pointer to a mach port.  Upon return, port will be filled
		with the mach port which will be used for any notifications.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierInformViaMachPort	(SCDSessionRef		session,
						 mach_msg_id_t		msgid,
						 mach_port_t		*port);

/*!
	@function SCDNotifierInformViaFD
	@discussion Allocates a file descriptor which can be used to detect changes
		to one of the system configuration data entries associated with the
		current sessions notifier keys. When a change is detected, the
		specified identifier (4 bytes) will be delivered to the calling
		application via the allocated file descriptor.

	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param identifier Pass an (4 byte) integer identifer which be used for any
		notifications.
	@param fd Pass a pointer to a file descriptor.  Upon return, fd will be filled
		with the file descriptor which will be used for any notifications.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierInformViaFD		(SCDSessionRef		session,
						 int32_t		identifier,
						 int			*fd);

/*!
	@function SCDNotifierInformViaSignal
	@discussion Requests that the specified BSD signal be sent to the process
		with the indicated process id whenever a change has been detected
		to one of the system configuration data entries associated with the
		current sessions notifier keys.

		Note: this function is not valid for "configd" plug-ins.

	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@param pid Pass a UNIX proces ID which should be signalled for any notifications.
	@param sig Pass a signal number to be used.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierInformViaSignal	(SCDSessionRef		session,
						 pid_t			pid,
						 int			sig);

/*!
	@function SCDNotifierCancel
	@discussion Cancels all outstanding notification delivery request for this
		session.

	@param session Pass a SCDSessionRef handle which should be used for
		communication with the server.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDNotifierCancel		(SCDSessionRef		session);

/*!
	@function SCDOptionGet
	@discussion Returns the value associated with the specified option.
	@param session Pass a SCDSessionRef handle of the option of interest (or
		NULL for the global option setting).
	@param option Pass the SCDOption of interest.
	@result The current value of the specified option.
 */
int		SCDOptionGet			(SCDSessionRef		session,
						 int			option);

/*!
	@function SCDOptionSet
	@discussion Sets the value associated with the specified option.
	@param session Pass a SCDSessionRef handle for the option to be set (or
		NULL for the global option settings).
	@param option Pass the SCDOption to be updated.
	@param value Pass the new value for the option.
	@result The current value of the specified option.
 */
void		SCDOptionSet			(SCDSessionRef		session,
						 int			option,
						 int			value);

/*!
	@function SCDSessionLog
	@discussion Issues a log and/or debug message.
	@param session Pass a SCDSessionRef handle for the current session..
	@param level Pass a syslog(3) logging priority.

	Note: LOG_DEBUG messages will not be issued if the verbose option has not been enabled.
	@param formatString Pass the CF format string
	@result The specified message will be logged.
 */
void		SCDSessionLog			(SCDSessionRef		session,
						 int			level,
						 CFStringRef		formatString,
						 ...);

/*!
	@function SCDLog
	@discussion Issues a log and/or debug message.
	@param level Pass a syslog(3) logging priority.
	@param formatString Pass the CF format string
	@result The specified message will be logged.
 */
void		SCDLog				(int			level,
						 CFStringRef		formatString,
						 ...);

const char *	SCDError			(SCDStatus		status);

__END_DECLS

#endif /* _SCD_H */
