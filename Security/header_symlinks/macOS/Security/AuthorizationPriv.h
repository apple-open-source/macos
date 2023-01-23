/*
 * Copyright (c) 2002-2004,2011-2014 Apple Inc. All Rights Reserved.
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
#include <CoreFoundation/CFDictionary.h>

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
	kAuthorizationFlagLeastPrivileged		= (1 << 5),
	kAuthorizationFlagSheet					= (1 << 6),
	kAuthorizationFlagIgnorePasswordOnly	= (1 << 7),
    kAuthorizationFlagIgnoreDarkWake        = (1 << 8),
};

/*!
    @enum Private operations for AuthorizationHandlePreloginOverride
*/
enum {
    kAuthorizationOverrideOperationSet,
    kAuthorizationOverrideOperationReset,
    kAuthorizationOverrideOperationQuery
};

/*!
    @function AuthorizationCreateWithAuditToken
    @abstract Create a AuthorizationRef for the process that sent the mach message
        represented by the audit token. Requires root.
    @param token The audit token of a mach message
    @param environment (input/optional) An AuthorizationItemSet containing environment state used when making the autorization decision.  See the AuthorizationEnvironment type for details.
    @param flags (input) options specified by the AuthorizationFlags enum.  set all unused bits to zero to allow for future expansion.
    @param authorization (output) A pointer to an AuthorizationRef to be returned.  When the returned AuthorizationRef is no longer needed AuthorizationFree should be called to prevent anyone from using the acquired rights.
 
    @result errAuthorizationSuccess 0 authorization or all requested rights succeeded.
 
    errAuthorizationDenied -60005 The authorization for one or more of the requested rights was denied.
*/

OSStatus AuthorizationCreateWithAuditToken(audit_token_t token,
    const AuthorizationEnvironment * _Nullable environment,
    AuthorizationFlags flags,
    AuthorizationRef _Nullable * _Nonnull authorization);

/*!
    @function AuthorizationExecuteWithPrivilegesExternalForm
    Run an executable tool with enhanced privileges after passing
    suitable authorization procedures.

    @param extForm authorization in external form that is used to authorize
    access to the enhanced privileges. It is also passed to the tool for
    further access control.
    @param pathToTool Full pathname to the tool that should be executed
    with enhanced privileges.
    @param flags Option bits (reserved). Must be zero.
    @param arguments An argv-style vector of strings to be passed to the tool.
    @param communicationsPipe Assigned a UNIX stdio FILE pointer for
    a bidirectional pipe to communicate with the tool. The tool will have
    this pipe as its standard I/O channels (stdin/stdout). If NULL, do not
    establish a communications pipe.

    @discussion This function has been deprecated and should no longer be used.
    Use a launchd-launched helper tool and/or the Service Mangement framework
    for this functionality.
*/
    
OSStatus AuthorizationExecuteWithPrivilegesExternalForm(const AuthorizationExternalForm * _Nonnull extForm,
    const char * _Nonnull pathToTool,
    AuthorizationFlags flags,
    char * _Nonnull const * _Nonnull arguments,
    FILE * _Nullable * _Nonnull communicationsPipe) __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_1,__MAC_10_7,__IPHONE_NA,__IPHONE_NA);

/*!
 @function AuthorizationExecuteWithPrivileges
 Run an executable tool with enhanced privileges after passing
 suitable authorization procedures.
 @param authorization An authorization reference that is used to authorize
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
OSStatus AuthorizationExecuteWithPrivileges(AuthorizationRef _Nonnull authorization,
												const char * _Nonnull pathToTool,
												AuthorizationFlags options,
												char * __nonnull const * __nonnull arguments,
												FILE * __nullable * __nullable communicationsPipe) __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_1,__MAC_10_7,__IPHONE_NA,__IPHONE_NA);

/*!
 @function AuthorizationCopyRightProperties
 Returns a dictionary with the properties associated with the given authorization right
 @param rightName right name for which we need the propertiers
 @param output CFDictionaryRef which will hold the properties

 */
OSStatus AuthorizationCopyRightProperties(const char * __nonnull rightName, CFDictionaryRef __nullable * __nullable output) __OSX_AVAILABLE_STARTING(__MAC_10_15, __IPHONE_NA);

/*!
 @function AuthorizationCopyPrivilegedReference
 From within a tool launched via the AuthorizationExecuteWithPrivileges function
 ONLY, retrieve the AuthorizationRef originally passed to that function.
 While AuthorizationExecuteWithPrivileges already verified the authorization to
 launch your tool, the tool may want to avail itself of any additional pre-authorizations
 the caller may have obtained through that reference.

 @discussion This function has been deprecated and should no longer be used.
 Use a launchd-launched helper tool and/or the Service Mangement framework
 for this functionality.
 */
OSStatus AuthorizationCopyPrivilegedReference(AuthorizationRef __nullable * __nonnull authorization,
												  AuthorizationFlags flags) __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_1,__MAC_10_7,__IPHONE_NA,__IPHONE_NA);

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
OSStatus SessionGetDistinguishedUser(SecuritySessionId session, uid_t * _Nonnull user);

/*!
	@function SessionSetUserPreferences
	Set preferences from current application context for session (for use during agent interactions).
	
	@param session (input) Session-id for which to set the user preferences. Can be one of the special constants defined in AuthSession.h.
 */
OSStatus SessionSetUserPreferences(SecuritySessionId session);

    
/*!
    @function AuthorizationEnableSmartCard
    Enable or disable system login using smartcard or get current status.
 
    @param authRef (input) The authorization object on which this operation is performed.
    @param enable (input) desired smartcard login support state, TRUE to enable, FALSE to disable
 */
OSStatus AuthorizationEnableSmartCard(AuthorizationRef _Nonnull authRef, Boolean enable);

/*!
     @function AuthorizationExecuteWithPrivilegesInternal
     Run an executable tool with enhanced privileges after passing
     suitable authorization procedures. Allows better control and communication
     with privileged tool.
     
     @param authorization An authorization reference that is used to authorize
     access to the enhanced privileges. It is also passed to the tool for
     further access control.
     @param pathToTool Full pathname to the tool that should be executed
     with enhanced privileges.
     @param arguments An argv-style vector of strings to be passed to the tool.
     @param newProcessPid (output, optional) PID of privileged process is stored here.
     @param uid Desired UID under which privileged tool should be running.
     @param stdOut File descriptor of the pipe which should be used to receive stdout from the privileged tool, use -1 if not needed.
     @param stdErr File descriptor of the pipe which should be used to receive stderr from the privileged tool, use -1 if not needed.
     @param stdIn File descriptor which will contain write-end of the stdin pipe of the privileged tool, use -1 if not needed.
     @param processFinished This block is called when privileged process finishes.
     */
OSStatus AuthorizationExecuteWithPrivilegesInternal(const AuthorizationRef _Nonnull authorization,
                                                        const char * _Nonnull pathToTool,
                                                        const char * _Nonnull const * _Nonnull arguments,
                                                        pid_t * _Nullable newProcessPid,
                                                        const uid_t uid,
                                                        int stdOut,
                                                        int stdErr,
                                                        int stdIn,
                                                        void(^__nullable processFinished)(const int exitStatus));
    
/*!
     @function AuthorizationExecuteWithPrivilegesExternalFormInternal
     Run an executable tool with enhanced privileges after passing
     suitable authorization procedures. Allows better control and communication
     with privileged tool.
     
     @param extAuthorization authorization in external form that is used to authorize
     access to the enhanced privileges. It is also passed to the tool for
     further access control.
     @param pathToTool Full pathname to the tool that should be executed
     with enhanced privileges.
     @param arguments An argv-style vector of strings to be passed to the tool.
     @param newProcessPid (output, optional) PID of privileged process is stored here.
     @param uid Desired UID under which privileged tool should be running.
     @param stdOut File descriptor of the pipe which should be used to receive stdout from the privileged tool, use -1 if not needed.
     @param stdErr File descriptor of the pipe which should be used to receive stderr from the privileged tool, use -1 if not needed.
     @param stdIn File descriptor which will contain write-end of the stdin pipe of the privileged tool, use -1 if not needed.
     @param processFinished This block is called when privileged process finishes.
     */
OSStatus AuthorizationExecuteWithPrivilegesExternalFormInternal(const AuthorizationExternalForm * _Nonnull extAuthorization,
                                                                    const char * _Nonnull pathToTool,
                                                                    const char * _Nullable const * _Nullable arguments,
                                                                    pid_t * _Nullable newProcessPid,
                                                                    const uid_t uid,
                                                                    int stdOut,
                                                                    int stdErr,
                                                                    int stdIn,
                                                                    void(^__nullable processFinished)(const int exitStatus));

/*!
    @function AuthorizationCopyPreloginUserDatabase
    Fills output with a CFArrayRef with user database from Prelogin volume

    @param volumeUuid Optional uuid of the volume for which user database will be returned. If not set, users from all volumes are returned.
    @param flags Specifies subset of data required in the output
    @param output Output array of dictionaries - each dictionary with details for each user
*/
OSStatus AuthorizationCopyPreloginUserDatabase(const char * _Nullable const volumeUuid, const UInt32 flags, CFArrayRef _Nonnull * _Nonnull output);

/*!
    @function AuthorizationCopyPreloginPreferencesValue
    Fills output with a CFTypeRef of a value of the item
 
    @param volumeUuid Specifies uuid of the volume for which preferences are stored
    @param username If NULL, global pref value is queried, otherwise user-specific preferences are queried
    @param domain preference domain like "com.apple.tokenlogin"
    @param item specifies name of the item to be returned
    @param output Output CFTypeRef with the value of the desired preference
*/
OSStatus AuthorizationCopyPreloginPreferencesValue(const char * _Nonnull const volumeUuid, const char * _Nullable const username, const char * _Nonnull const domain, const char * _Nullable const item, CFTypeRef _Nonnull * _Nonnull output);

/*!
    @function AuthorizationHandlePreloginOverride
    Handles FVUnlock Smartcard Enforcement
 
    @param volumeUuid Specifies uuid of the volume for which the operation will be executed
    @param operation Specifies required operation:
        kAuthorizationOverrideOperationSet - temporarily disable SC enforcement
        kAuthorizationOverrideOperationReset - turn off temporary SC enforcement
        kAuthorizationOverrideOperationQuery - query current status
    @param result If operation was to query current status, true will be set if SC enforcement is temporarily disabled or false if not
*/
OSStatus AuthorizationHandlePreloginOverride(const char * _Nonnull const volumeUuid, const char operation, Boolean * _Nullable result);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTHORIZATIONPRIV_H_ */
