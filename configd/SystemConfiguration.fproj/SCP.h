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

#ifndef _SCP_H
#define _SCP_H

#include <CoreFoundation/CoreFoundation.h>
#include <sys/cdefs.h>

/*!
	@header SCP.h
	The SystemConfiguration framework provides access to the data used
		to configure a running system.

	Specifically, the SCPxxx() API's allow an application to load and
		store XML configuration data in a controlled manner
		and provides the necessary notifications to other
		applications which need to be aware of configuration
		changes.

	The APIs provided by this framework communicate with the "configd"
		daemon for any tasks requiring synchronization and/or
		notification.
 */


/*!
	@enum SCPStatus
	@discussion Returned status codes.
	@constant SCP_OK		Success
	@constant SCP_NOSESSION		Preference session not active
	@constant SCP_BUSY		Configuration daemon busy
	@constant SCD_NEEDLOCK		Lock required for this operation
	@constant SCP_EACCESS		Permission denied (must be root to obtain lock)
	@constant SCP_ENOENT		Configuration file not found
	@constant SCP_BADCF		Configuration file corrupt
	@constant SCD_NOKEY		No such key
	@constant SCD_NOLINK		No such link
	@constant SCP_EXISTS		Key already defined
	@constant SCP_STALE		Write attempted on stale version of object
	@constant SCP_INVALIDARGUMENT	Invalid argument
	@constant SCP_FAILED		Generic error
 */
typedef enum {
	SCP_OK			= 0,	/* Success */
	SCP_NOSESSION		= 1024,	/* Preference session not active */
	SCP_BUSY		= 1025,	/* Preferences update currently in progress */
	SCP_NEEDLOCK		= 1026,	/* Lock required for this operation */
	SCP_EACCESS		= 1027,	/* Permission denied */
	SCP_ENOENT		= 1028,	/* Configuration file not found */
	SCP_BADCF		= 1029,	/* Configuration file corrupt */
	SCP_NOKEY		= 1030,	/* No such key */
	SCP_NOLINK		= 1031,	/* No such link */
	SCP_EXISTS		= 1032,	/* No such key */
	SCP_STALE		= 1033,	/* Write attempted on stale version of object */
	SCP_INVALIDARGUMENT	= 1034,	/* Invalid argument */
	SCP_FAILED		= 9999	/* Generic error */
} SCPStatus;


/*!
	@enum SCPOption
	@discussion Used with the SCP[User]Open() and SCP[User]NotificationKeyCreate()
		to describe the prefsID CFStringRef argument.
	@constant kSCPOptionCreatePrefs Specifies that the preferences file should
		be created if it does not exist.
 */
typedef enum {
	kSCPOpenCreatePrefs	= 1,	/* create preferences file if not found */
} SCPOption;


/*!
	@enum SCPKeyType
	@discussion Used with the SCDList() and SCDNotifierAdd() functions to describe
		the CFStringRef argument.
	@constant kSCDKeyLock	key used when exclusive access to the stored preferences
		is obtained or released.
	@constant kSCDKeyCommit	key used when new preferences are committed to the store
	@constant kSCDKeyApply	key used when new preferences are to be applied to the
		active system configuration.
 */
typedef enum {
	kSCPKeyLock	= 1,
	kSCPKeyCommit	= 2,
	kSCPKeyApply	= 3,
} SCPKeyType;


/*!
	@typedef SCPSessionRef
	@discussion This is the type of a handle to an open "session" for
		accessing system configuration preferences.
 */
typedef void *		SCPSessionRef;


__BEGIN_DECLS

/*!
	@function SCPOpen
	@discussion Initiates access to the per-system set of configuration
		preferences.

	This function will ensure that the current state of the prefsID is
	retrieved (by reading the whole thing into memory, or at least,
	open()'ing the file and keeping it open)
	@param session A pointer to memory which will be filled with an
		SCPSessionRef handle to be used for all subsequent requests.
		If a session cannot be established, the contents of
		memory pointed to by this parameter are undefined.
	@param name Pass a string which describes the name of the calling
		process.
	@param prefsID Pass a string which identifies the name of the
		group of preferences to be accessed/updated. A NULL value
		specifies the default system configuration preferences.
	@param options Pass a bitfield of type SCPOpenOption containing
		one or more flags describing how the preferences should
		be accessed.

	@result A constant of type SCPStatus indicating the success (or
		failure) of the call.
 */
SCPStatus	SCPOpen				(SCPSessionRef		*session,
						 CFStringRef		name,
						 CFStringRef		prefsID,
						 int			options);

/*!
	@function SCPUserOpen
	@discussion Initiates access to the per-user set of configuration
		preferences.

	This function will ensure that the current state of the prefsID is
	retrieved (by reading the whole thing into memory, or at least,
	open()'ing the file and keeping it open)
	@param session A pointer to memory which will be filled with an
		SCPSessionRef handle to be used for all subsequent requests.
		If a session cannot be established, the contents of
		memory pointed to by this parameter are undefined.
	@param name Pass a string which describes the name of the calling
		process.
	@param prefsID Pass a string which identifies the name of the
		group of preferences to be accessed/updated.
	@param user Pass a string which identifies the user/login who's
		preferences should be accessed.  A NULL value specifies
		the current console user.
	@param options Pass a bitfield of type SCPOpenOption containing
		one or more flags describing how the preferences should
		be accessed.

	@result A constant of type SCPStatus indicating the success (or
		failure) of the call.
 */
SCPStatus	SCPUserOpen			(SCPSessionRef		*session,
						 CFStringRef		name,
						 CFStringRef		prefsID,
						 CFStringRef		user,
						 int			options);

/*!
	@function SCPClose
	@discussion Terminates access to a set of configuration preferences.

	This function frees/closes all allocated/opened resources. Any
	uncommitted changes are NOT written.
	@param session Pass a pointer to the SCPSessionRef handle which should
		be closed.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call.
 */
SCPStatus	SCPClose			(SCPSessionRef		*session);

/*!
	@function SCPLock
	@discussion Locks access to the configuration preferences.

	This function obtains exclusive access to the configuration
	preferences associated with this prefsID. Clients attempting
	to obtain exclusive access the preferences will either receive
	an SCP_BUSY error or block waiting for the lock to be released.
	@param session Pass an SCPSessionRef handle which should be used for
		all API calls.
	@param wait Pass a boolean flag indicating whether the calling process
		should block waiting for another process to complete its update
		operation and release its lock.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_BUSY, SCP_EACCESS, SCP_STALE.
 */
SCPStatus	SCPLock				(SCPSessionRef		session,
						 boolean_t		wait);

/*!
	@function SCPCommit
	@discussion Commits changes made to the configuration preferences to
		persitent storage.

	This function commits the any changes to permanent storage. An
	implicit call to SCPLock/SCPUnlock will be made if exclusive
	access had not been established.
	@param session Pass an SCPSessionRef handle which should be used for
		all API calls.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_BUSY, SCP_EACCESS, SCP_STALE.
 */
SCPStatus	SCPCommit			(SCPSessionRef		session);

/*!
	@function SCPApply
	@discussion Requests that the currently stored configuration
		preferences be applied to the active configuration.
	@param session Pass an SCPSessionRef handle which should be used for
		all API calls.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_EACCESS.
 */
SCPStatus	SCPApply			(SCPSessionRef		session);

/*!
	@function SCPUnlock
	@discussion Releases exclusive access to the configuration preferences.

	This function releases the exclusive access "lock" fr this prefsID.
	Other clients will be now be able to establish exclusive access to
	the preferences.
	@param session Pass an SCPSessionRef handle which should be used for
		all API calls.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call.
 */
SCPStatus	SCPUnlock			(SCPSessionRef		session);

/*!
	@function SCPGetSignature
	@discussion Returns an sequence of bytes which can be used to determine
		if the saved configuration preferences have changed.
	@param session Pass an SCPSessionRef handle which should be used for
		all API calls.
	@param signature Pass a pointer to a CFDataRef which will be reflect
		the signature of the configuration preferences at the time
		of the call to SCPOpen().
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call.
 */
SCPStatus	SCPGetSignature			(SCPSessionRef		session,
						 CFDataRef		*signature);

/*!
	@function SCPList
	@discussion Returns an array of currently defined preference keys.
	@param session Pass an SCPSessionRef handle which should be used for
		all API calls.
	@param keys Pass a pointer to a CFArrayRef which will be set to a new
		array of currently defined preference keys.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call.
 */
SCPStatus	SCPList				(SCPSessionRef		session,
						 CFArrayRef		*keys);

/*!
	@function SCPGet
	@discussion Returns the data associated with a preference key.

	This function retrieves data associated with a key for the prefsID.
	You "could" read stale data and not know it, unless you first call
	SCPLock().
	@param session Pass an SCPSessionRef handle which should be used for
		all API calls.
	@param key Pass a reference to the preference key to be returned.
	@param data Pass a pointer to a CFPropertyListRef which will be set to a
		new object containing the data associated with the
		configuration preference.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_NOKEY.
 */
SCPStatus	SCPGet				(SCPSessionRef		session,
						 CFStringRef		key,
						 CFPropertyListRef	*data);

/*!
	@function SCPAdd
	@discussion Adds data for a preference key.

	This function associates new data with the specified key. In order
	to commit these changes to permanent storage a call must be made to
	SCDPCommit().
	@param session Pass the SCPSessionRef handle which should be used to
		communicate with the APIs.
	@param key Pass a reference to the preference key to be updated.
	@param data Pass a reference to the CFPropertyListRef object containing the
		data to be associated with the configuration preference.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_EXISTS.
 */
SCPStatus	SCPAdd				(SCPSessionRef		session,
						 CFStringRef		key,
						 CFPropertyListRef	data);

/*!
	@function SCPSet
	@discussion Updates the data associated with a preference key.

	This function creates (or updates) the data associated with the
	specified key. In order to commit these changes to permanent
	storage a call must be made to SCDPCommit().
	@param session Pass the SCPSessionRef handle which should be used to
		communicate with the APIs.
	@param key Pass a reference to the preference key to be updated.
	@param data Pass a reference to the CFPropertyListRef object containing the
		data to be associated with the configuration preference.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK.
 */
SCPStatus	SCPSet				(SCPSessionRef		session,
						 CFStringRef		key,
						 CFPropertyListRef	data);

/*!
	@function SCPRemove
	@discussion Removes the data associated with a preference key.

	This function removes the data associated with the specified
	key. In order to commit these changes to permanent storage a
	call must be made to SCDPCommit().
	@param session Pass the SCPSessionRef handle which should be used to
		communicate with the APIs.
	@param key Pass a reference to the preference key to be removed.
	@result A constant of type SCPStatus indicating the success (or
		failure) of the call. Possible return values include: SCP_OK,
		SCP_NOKEY.
 */
SCPStatus	SCPRemove			(SCPSessionRef		session,
						 CFStringRef		key);

/*!
	@function SCPNotificationKeyCreate
	@discussion Creates a key which can be used by the SCDNotifierAdd()
		function to receive notifications of changes to the saved
		preferences.
	@param prefsID Pass a string which identifies the name of the
		preferences to be accessed/updated. A NULL value specifies
		the default system configuration preferences.
	@param keyType Pass a kSCPKeyType indicating the type a notification
		key to be returned.
	@result A notification string for the specified preference identifier.
 */
CFStringRef	SCPNotificationKeyCreate	(CFStringRef		prefsID,
						 int			keyType);

/*!
	@function SCPUserNotificationKeyCreate
	@discussion Creates a key which can be used by the SCDNotifierAdd()
		function to receive notifications of changes to the saved
		preferences.
	@param prefsID Pass a string which identifies the name of the
		preferences to be accessed/updated. A NULL value specifies
		the default system configuration preferences.
	@param user Pass a string which identifies the user/login who's
		preferences should be accessed.  A NULL value specifies
		the current console user.
	@param keyType Pass a kSCPKeyType indicating the type a notification
		key to be returned.
	@result A notification string for the specified preference identifier.
 */
CFStringRef	SCPUserNotificationKeyCreate	(CFStringRef		prefsID,
						 CFStringRef		user,
						 int			keyType);

/*!
	@function SCPError
	@discussion
	@param status
	@result
 */
const char *	SCPError			(SCPStatus		status);

__END_DECLS

#endif /* _SCP_H */
