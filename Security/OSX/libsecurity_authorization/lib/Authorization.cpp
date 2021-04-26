/*
 * Copyright (c) 2000-2004,2011-2014 Apple Inc. All Rights Reserved.
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


//
// Authorization.cpp
//
// This file is the unified implementation of the Authorization and AuthSession APIs.
//
#include <stdint.h>
#include <Security/AuthSession.h>
#include <Security/AuthorizationPriv.h>
#include <security_utilities/ccaudit.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/SecBase.h>
#include <security_utilities/logging.h>
#include "LegacyAPICounts.h"

//
// This no longer talks to securityd; it is a kernel function.
//
OSStatus SessionGetInfo(SecuritySessionId requestedSession,
    SecuritySessionId *sessionId,
    SessionAttributeBits *attributes)
{
    BEGIN_API_NO_METRICS
    if (requestedSession != noSecuritySession && requestedSession != callerSecuritySession) {
        static dispatch_once_t countToken;
        countLegacyAPI(&countToken, __FUNCTION__);
    }
	CommonCriteria::AuditInfo session;
	if (requestedSession == callerSecuritySession)
		session.get();
	else
		session.get(requestedSession);
	if (sessionId)
		*sessionId = session.sessionId();
	if (attributes)
        *attributes = (SessionAttributeBits)session.flags();
    END_API(CSSM)
}


//
// Create a new session.
// This no longer talks to securityd; it is a kernel function.
// Securityd will pick up the new session when we next talk to it.
//
OSStatus SessionCreate(SessionCreationFlags flags,
    SessionAttributeBits attributes)
{
    BEGIN_API

	// we don't support the session creation flags anymore
	if (flags)
		Syslog::warning("SessionCreate flags=0x%lx unsupported (ignored)", (unsigned long)flags);
	CommonCriteria::AuditInfo session;
	session.create(attributes);
        
	// retrieve the (new) session id and set it into the process environment
	session.get();
	char idString[80];
	snprintf(idString, sizeof(idString), "%x", session.sessionId());
	setenv("SECURITYSESSIONID", idString, 1);

    END_API(CSSM)
}


//
// Get and set the distinguished uid (optionally) associated with the session.
//
OSStatus SessionSetDistinguishedUser(SecuritySessionId session, uid_t user)
{
	BEGIN_API
	CommonCriteria::AuditInfo session;
	session.get();
	session.ai_auid = user;
	session.set();
	END_API(CSSM)
}


OSStatus SessionGetDistinguishedUser(SecuritySessionId session, uid_t *user)
{
    BEGIN_API
	CommonCriteria::AuditInfo session;
	session.get();
	Required(user) = session.uid();
    END_API(CSSM)
}

OSStatus SessionSetUserPreferences(SecuritySessionId session)
{
    return errSecSuccess;
}
