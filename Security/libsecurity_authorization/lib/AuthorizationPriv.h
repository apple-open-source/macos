/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
 *  AuthorizationPriv.h -- Authorization SPIs
 *  Private APIs for implementing access control in applications and daemons.
 *  
 */

#ifndef _SECURITY_AUTHORIZATIONPRIV_H_
#define _SECURITY_AUTHORIZATIONPRIV_H_

#include <Security/Authorization.h>
#include <Security/AuthSession.h>
#include <sys/types.h>	// uid_t

#if defined(__cplusplus)
extern "C" {
#endif


/*!
	@header AuthorizationPriv
	Version 1.1 04/2003

	This header contains private APIs for authorization services.
	This is the private extension of <Security/Authorization.h>, a public header file.
*/

/*!
	@enum Private (for now) AuthorizationFlags
*/
enum {
	kAuthorizationFlagLeastPrivileged		= (1 << 5)
};

/*!
	@function SessionSetDistinguishedUser
	This function allows the creator of a (new) security session to associate an arbitrary
	UNIX user identity (uid) with the session. This uid can be retrieved with
	SessionGetDistinguishedUser by anyone who knows the session's id, and may also
	be used by the system for identification (but not authentication) purposes.
	
	This call can only be made by the process that created the session, and only
	once.
	
	This is a private API, and is subject to change.
	
	@param session (input) Session-id for which to set the uid. Can be one of the
        special constants defined in AuthSession.h.
	@param user (input) The uid to set.
 */
OSStatus SessionSetDistinguishedUser(SecuritySessionId session, uid_t user);


/*!
	@function SessionGetDistinguishedUser
	Retrieves the distinguished uid of a session as set by the session creator
	using the SessionSetDistinguishedUser call.
	
	@param session (input) Session-id for which to set the uid. Can be one of the
        special constants defined in AuthSession.h.
	@param user (output) Will receive the uid. Unchanged on error.
 */
OSStatus SessionGetDistinguishedUser(SecuritySessionId session, uid_t *user);

/*!
	@function SessionSetUserPreferences
	Set preferences from current application context for session (for use during agent interactions).
	
	@param session (input) Session-id for which to set the user preferences. Can be one of the special constants defined in AuthSession.h.
 */
OSStatus SessionSetUserPreferences(SecuritySessionId session);


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTHORIZATIONPRIV_H_ */
