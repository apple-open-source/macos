/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 *  AuthSession.h
 *  AuthSession - APIs for managing login, authorization, and security Sessions.
 */
#if !defined(__AuthSession__)
#define __AuthSession__ 1

#include <Security/Authorization.h>

#if defined(__cplusplus)
extern "C" {
#endif


/*!
	@header AuthSession

	The Session API provides specialized applications access to Session management and inquiry
    functions. This is a specialized API that should not be of interest to most people.
    
    If you do not know what "Session" means in the context of MacOS Authorization and security,
    please check with your documentation and come back when you have figured it out - we won't
    explain it here.
    
    This API is tentative, preliminary, incomplete, internal, and subject to change.
    You have been warned.
*/


/*!
	@typedef SecuritySessionId
	These are externally visible identifiers for authorization sessions.
        Different sessions have different identifiers; beyond that, you can't
        tell anything from these values.
    SessionIds can be compared for equality as you'd expect, but you should be careful
        to use attribute bits wherever appropriate. For example, don't rely on there being
        "the" graphical login session - some day, we may have more than one...
*/
typedef UInt32 SecuritySessionId;


/*!
    @enum SecuritySessionId
    Here are some special values for SecuritySessionId. You may specify those
        on input to SessionAPI functions. They will never be returned from such
        functions.
*/
enum {
    noSecuritySession                      = 0,     /* definitely not a valid SecuritySessionId */
    callerSecuritySession                  = -1     /* the Session I (the caller) am in */
};


/*!
    @enum SessionAttributeBits
    Each Session has a set of attribute bits. You can get those from the
        SessionGetInfo API function.
 */
typedef UInt32 SessionAttributeBits;
 
enum {
    sessionIsRoot                          = 0x0001, /* is the root session (startup/system programs) */
    sessionHasGraphicAccess                = 0x0010, /* graphic subsystem (CoreGraphics et al) available */
    sessionHasTTY                          = 0x0020, /* /dev/tty is available */
    sessionIsRemote                        = 0x1000, /* session was established over the network */

    sessionWasInitialized                  = 0x8000  /* session has been set up by its leader */
};


/*!
    @enum SessionCreationFlags
    These flags control how a new session is created by SessionCreate.
        They have no permanent meaning beyond that.
 */
typedef UInt32 SessionCreationFlags;
 
enum {
    sessionKeepCurrentBootstrap             = 0x8000 /* caller has allocated sub-bootstrap (expert use only) */
};
 
 
/*!
	@enum SessionStatus
	Error codes returned by AuthSession API.
    Note that the AuthSession APIs can also return Authorization API error codes.
*/
enum {
    errSessionSuccess                       = 0,      /* all is well */
    errSessionInvalidId                     = -60500, /* invalid session id specified */
    errSessionInvalidAttributes             = -60501, /* invalid set of requested attribute bits */
    errSessionAuthorizationDenied           = -60502, /* you are not allowed to do this */

    errSessionInternal                      = errAuthorizationInternal,	/* internal error */
	errSessionInvalidFlags                  = errAuthorizationInvalidFlags /* invalid flags/options */
};


/*!
    @function SessionGetInfo
    Obtain information about a session.

    @param session (input) The Session you are asking about. Can be one of the
        special constants defined above.
	
	@param sessionId (output/optional) The actual SecuritySessionId for the session you asked about.
        Will never be one of those constants.
        
    @param attributes (output/optional) Receives the attribute bits for the session.

    @result An OSStatus indicating success (noErr) or an error cause.
    
    errSessionInvalidId -60500 Invalid session id specified

*/
OSStatus SessionGetInfo(SecuritySessionId session,
    SecuritySessionId *sessionId,
    SessionAttributeBits *attributes);
    

/*!
    @function SessionCreate
    This (very specialized) function creates and/or initializes a security session.
        It always sets up the session that the calling process belongs to - you cannot
        create a session for someone else.
    By default, a new bootstrap subset port is created for the calling process. The process
        acquires this new port as its bootstrap port, which all its children will inherit.
        If you happen to have created the subset port on your own, you can pass the
        sessionKeepCurrentBootstrap flag, and SessionCreate will use it. Note however that
        you cannot supersede a prior SessionCreate call that way; only a single SessionCreate
        call is allowed for each Session (however made).
    
    @param flags Flags controlling how the session is created.
    
    @param attributes The set of attribute bits to set for the new session.
        Not all bits can be set this way.
    
    @result An OSStatus indicating success (noErr) or an error cause.
    
    errSessionInvalidAttributes -60501 Attempt to set invalid attribute bits	
    errSessionAuthorizationDenied -60502 Attempt to re-initialize a session
    errSessionInvalidFlags -60011 Attempt to specify unsupported flag bits
    
*/
OSStatus SessionCreate(SessionCreationFlags flags,
    SessionAttributeBits attributes);
    

#if defined(__cplusplus)
}
#endif

#endif /* ! __AuthSession__ */
