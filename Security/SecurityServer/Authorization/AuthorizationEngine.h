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
 *  AuthorizationEngine.h
 *  Authorization
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

#ifndef _H_AUTHORIZATIONENGINE
#define _H_AUTHORIZATIONENGINE  1

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>
#include "AuthorizationData.h"
#include "AuthorizationDBPlist.h"

#include <Security/threading.h>
#include <Security/osxsigning.h>

#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <sys/stat.h>
#include <sys/types.h>

class AuthorizationToken;

using Authorization::AuthorizationDBPlist;

namespace Authorization
{

class Error : public CssmCommonError {
protected:
    Error(int err);
public:
    const int error;
    virtual CSSM_RETURN cssmError() const throw();
    virtual OSStatus osStatus() const throw();
    virtual const char *what () const throw();
	// @@@ Default value should be internal error.
    static void throwMe(int err = -1) __attribute((noreturn));
};


/* The engine which performs the actual authentication and authorization computations.

	The implementation of a typical call to AuthorizationCreate would look like:
	
	Get the current shared CredentialSet for this session.
	Call authorizedRights() with inRights and the shared CredentialSet.
	Compute the difference set between the rights requested and the rights returned from authorizedRights().  
	Call credentialIds() with the rights computed above (for which we have no credentials yet).
	Call aquireCredentials() for the credentialIds returned from credentialIds()
	For each credential returned place it in the session (replacing when needed) if shared() returns true.
	The authorization returned to the user should now refer to the credentials in the session and the non shared ones returned by aquireCredentials().

	When a call to AuthorizationCopyRights() is made, just call authorizedRights() using the union of the session credentials and the credentials tied to the authorization specified.

	When a call to AuthorizationCopyInfo() is made, ask the Credential specified by tag for it info and return it.

	When a call to AuthorizationFree() is made, delete all the non-shared credentials ascociated with the authorization specified.  If the kAuthorizationFreeFlagDestroy is set.  Also delete the shared credentials ascociated with the authorization specified.
 */
class Engine
{
public:
	Engine(const char *configFile);
	~Engine();

	OSStatus authorize(const AuthItemSet &inRights, const AuthItemSet &environment,
		AuthorizationFlags flags, const CredentialSet *inCredentials, CredentialSet *outCredentials,
		AuthItemSet &outRights, AuthorizationToken &auth);
	OSStatus getRule(string &inRightName, CFDictionaryRef *outRuleDefinition);
	OSStatus setRule(const char *inRightName, CFDictionaryRef inRuleDefinition, const CredentialSet *inCredentials, CredentialSet *outCredentials, AuthorizationToken &auth);
	OSStatus removeRule(const char *inRightName, const CredentialSet *inCredentials, CredentialSet *outCredentials, AuthorizationToken &auth);

private:
	OSStatus verifyModification(string inRightName, bool remove,
	const CredentialSet *inCredentials, CredentialSet *outCredentials, AuthorizationToken &auth);

	AuthorizationDBPlist mAuthdb;
    mutable Mutex mLock;
};

}; // namespace Authorization

#endif /* ! _H_AUTHORIZATIONENGINE */
