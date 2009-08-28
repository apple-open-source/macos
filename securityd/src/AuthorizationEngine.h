/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 *  AuthorizationEngine.h
 *  Authorization
 *
 */
#ifndef _H_AUTHORIZATIONENGINE
#define _H_AUTHORIZATIONENGINE  1

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>
#include <security_cdsa_utilities/AuthorizationData.h>

#include <security_utilities/threading.h>
#include <security_utilities/osxcode.h>

#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "authority.h"

#include "AuthorizationRule.h"
#include "AuthorizationDBPlist.h"

namespace Authorization
{

class Error : public CommonError {
protected:
    Error(int err);
public:
    const int error;
    virtual int unixError() const throw();
    virtual OSStatus osStatus() const throw();
    virtual const char *what () const throw();
    static void throwMe(int err) __attribute((noreturn));
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
