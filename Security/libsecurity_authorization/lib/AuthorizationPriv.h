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
#include <mach/message.h>

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
    @function AuthorizationCreateWithAuditToken
    @abstract Create a AuthorizationRef for the process that sent the mach message
        represented by the audit token. Requires root.
    @param token The audit token of a mach message
    @param environment (input/optional) An AuthorizationItemSet containing enviroment state used when making the autorization decision.  See the AuthorizationEnvironment type for details.
    @param flags (input) options specified by the AuthorizationFlags enum.  set all unused bits to zero to allow for future expansion.
    @param authorization (output) A pointer to an AuthorizationRef to be returned.  When the returned AuthorizationRef is no longer needed AuthorizationFree should be called to prevent anyone from using the aquired rights.
 
    @result errAuthorizationSuccess 0 authorization or all requested rights succeeded.
 
    errAuthorizationDenied -60005 The authorization for one or more of the requested rights was denied.
*/

OSStatus AuthorizationCreateWithAuditToken(audit_token_t token,
    const AuthorizationEnvironment *environment,
    AuthorizationFlags flags,
    AuthorizationRef *authorization);

/*!
    @function AuthorizationExecuteWithPrivilegesExternalForm
    Run an executable tool with enhanced privileges after passing
    suitable authorization procedures.

    @param authorization in external form that is used to authorize
    access to the enhanced privileges. It is also passed to the tool for
    further access control.
    @param pathToTool Full pathname to the tool that should be executed
    with enhanced privileges.
    @param options Option bits (reserved). Must be zero.
    @param arguments An argv-style vector of strings to be passed to the tool.
    @param communicationsPipe Assigned a UNIX stdio FILE pointer for
    a bidirectional pipe to communicate with the tool. The tool will have
    this pipe as its standard I/O channels (stdin/stdout). If NULL, do not
    establish a communications pipe.

    @discussion This function has been deprecated and should no longer be used.
    Use a launchd-launched helper tool and/or the Service Mangement framework
    for this functionality.
*/
    
OSStatus AuthorizationExecuteWithPrivilegesExternalForm(const AuthorizationExternalForm * extForm,
    const char *pathToTool,
    AuthorizationFlags flags,
    char *const *arguments,
    FILE **communicationsPipe) __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_1,__MAC_10_7,__IPHONE_NA,__IPHONE_NA);

/*
    @function AuthorizationDismiss
    @abstract Dismisses all Authorization dialogs associated to the calling process.
        Any active authorization requests will be canceled and return errAuthorizationDenied
*/

OSStatus AuthorizationDismiss(void);
    
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

    
/*!
    @function AuthorizationEnableSmartCard
    Enable or disable system login using smartcard or get current status.
 
    @param authorization (input) The authorization object on which this operation is performed.
    @param enable (input) desired smartcard login support state, TRUE to enable, FALSE to disable
 */
OSStatus AuthorizationEnableSmartCard(AuthorizationRef authRef, Boolean enable);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTHORIZATIONPRIV_H_ */
