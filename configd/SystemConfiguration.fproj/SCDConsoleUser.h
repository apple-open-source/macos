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

#ifndef _SCDCONSOLEUSER_H
#define _SCDCONSOLEUSER_H

#include <sys/cdefs.h>

/*!
	@header SCDConsoleUser.h
	The SystemConfiguration framework provides access to the data used
		to configure a running system.

	Specifically, the SCDConsoleUserXXX() API's allow an application
		to determine (or set) the login/user currently using the
		console.

	The APIs provided by this framework communicate with the "configd"
		daemon to obtain information regarding the systems current
		configuration.
 */


__BEGIN_DECLS

/*!
	@function SCDKeyCreateConsoleUser
	@discussion Creates a key which can be used by the SCDNotifierAdd()
		function to receive notifications when the current "Console"
		user changes.
	@result A notification string for the current "Console" user.
*/
CFStringRef	SCDKeyCreateConsoleUser	();

/*!
	@function SCDConsoleUserGet
	@discussion Gets the name, user ID, and group ID of the currently
		logged in user.
	@param user A pointer to a character buffer of at least size
		userlen. The returned name is null-terminated unless
		in-sufficient space is provided.If NULL, this value
		will not be returned.
	@param userlen Pass an integer specifying the maximum size of the
		user buffer.
	@param uid A pointer to memory which will be filled with the user ID
		of the current "Console" user. If NULL, this value will not
		be returned.
	@param gid A pointer to memory which will be filled with the group ID
		of the current "Console" user. If NULL, this value will not be
		returned.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDConsoleUserGet	(char		*user,
					 int		userlen,
					 uid_t		*uid,
					 gid_t		*gid);

/*!
	@function SCDConsoleUserSet
	@discussion Sets the name, user ID, and group ID of the currently
		logged in user.
	@param user A pointer to a character buffer containing the name of
		the current "Console" user. If NULL, any current "Console"
		user information will be reset.
	@param uid The user ID of the current "Console" user.
	@param gid The group ID of the current "Console" user.
	@result A constant of type SCDStatus indicating the success (or failure) of
	 the call.
 */
SCDStatus	SCDConsoleUserSet	(const char	*user,
					 uid_t		uid,
					 gid_t		gid);

__END_DECLS

#endif /* _SCDCONSOLEUSER_H */
