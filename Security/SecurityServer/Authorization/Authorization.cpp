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


//
// Authorization.cpp
//
// This file is the unified implementation of the Authorization and AuthSession APIs.
//
#include <Security/Authorization.h>
#include <Security/AuthSession.h>
#include "AuthorizationWalkers.h"
#include <Security/mach++.h>
#include <Security/globalizer.h>
#include <Security/cssmalloc.h>
#include <Security/ssclient.h>

using namespace SecurityServer;
using namespace MachPlusPlus;


//
// Shared cached client object
//
class AuthClient : public SecurityServer::ClientSession {
public:
	AuthClient()
	: SecurityServer::ClientSession(CssmAllocator::standard(), CssmAllocator::standard())
	{ }
};

static ModuleNexus<AuthClient> server;


//
// Create an Authorization
//
OSStatus AuthorizationCreate(const AuthorizationRights *rights,
	const AuthorizationEnvironment *environment,
	AuthorizationFlags flags,
	AuthorizationRef *authorization)
{
	BEGIN_API
	AuthorizationBlob result;
	server().authCreate(rights, environment, flags, result);
	if (authorization)
	{
		*authorization = 
			(AuthorizationRef) new(server().returnAllocator) AuthorizationBlob(result);
	}
	else
	{
		// If no authorizationRef is desired free the one we just created.
		server().authRelease(result, flags);
	}
	END_API(CSSM)
}


//
// Free an authorization reference
//
OSStatus AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authRelease(Required(auth), flags);
	server().returnAllocator.free(auth);
	END_API(CSSM)
}


//
// Augment and/or interrogate an authorization
//
OSStatus AuthorizationCopyRights(AuthorizationRef authorization,
	const AuthorizationRights *rights,
	const AuthorizationEnvironment *environment,
	AuthorizationFlags flags,
	AuthorizationRights **authorizedRights)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authCopyRights(Required(auth), rights, environment, flags, authorizedRights);
	END_API(CSSM)
}


//
// Retrieve side-band information from an authorization
//
OSStatus AuthorizationCopyInfo(AuthorizationRef authorization, 
	AuthorizationString tag,
	AuthorizationItemSet **info)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authCopyInfo(Required(auth), tag, Required(info));
	END_API(CSSM)
}


//
// Externalize and internalize authorizations
//
OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization,
	AuthorizationExternalForm *extForm)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authExternalize(Required(auth), *extForm);
	END_API(CSSM)
}

OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm,
	AuthorizationRef *authorization)
{
	BEGIN_API
	AuthorizationBlob result;
	server().authInternalize(*extForm, result);
	Required(authorization) = 
		(AuthorizationRef) new(server().returnAllocator) AuthorizationBlob(result);
	END_API(CSSM)
}


//
// Free an ItemSet structure returned from an API call. This is a local operation.
// Since we allocate returned ItemSets as compact blobs, this is just a simple
// free() call.
//
OSStatus AuthorizationFreeItemSet(AuthorizationItemSet *set)
{
	BEGIN_API
	server().returnAllocator.free(set);
	return errAuthorizationSuccess;
	END_API(CSSM)
}


//
// Get session information
//
OSStatus SessionGetInfo(SecuritySessionId session,
    SecuritySessionId *sessionId,
    SessionAttributeBits *attributes)
{
    BEGIN_API
    SecuritySessionId sid = session;
    server().getSessionInfo(sid, *attributes);
    if (sessionId)
        *sessionId = sid;
    END_API(CSSM)
}


//
// Create a new session
//
OSStatus SessionCreate(SessionCreationFlags flags,
    SessionAttributeBits attributes)
{
    BEGIN_API

    // just to be on the safe side, drop any cached connection to the SecurityServer
    server.reset();
    
    // unless the (expert) caller has already done so, create a sub-bootstrap and set it
    // note that this is inherently thread-unfriendly; we can't do anything about that
    // (caller's responsibility)
    Bootstrap bootstrap;
    if (!(flags & sessionKeepCurrentBootstrap)) {
        TaskPort self;
        bootstrap = bootstrap.subset(self);
        self.bootstrap(bootstrap);
    }
    
    // now call the SecurityServer and tell it to initialize the (new) session
    server().setupSession(flags, attributes);

    END_API(CSSM)
}
