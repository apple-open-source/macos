/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
 *  AuthorizationPriv.h -- Authorization SPIs
 *  Private APIs for implementing access control in applications and daemons.
 *  
 */

#ifndef _SECURITY_AUTHORIZATIONPRIV_H_
#define _SECURITY_AUTHORIZATIONPRIV_H_

#include <Security/Authorization.h>

#if defined(__cplusplus)
extern "C" {
#endif


/*!
	@header AuthorizationPriv
	Version 1.1 04/2003

	This header contains private APIs for authorization services.
	This is the private extension of <Security/Authorization.h>, a public header file.
*/


/* meta-rightname prefixes that configure authorization for policy changes */

/*!
	@defined kConfigRightAdd
	meta-rightname for prefix adding rights.
*/
#define kAuthorizationConfigRightAdd	"config.add."
/*!
	@defined kConfigRightModify
	meta-rightname prefix for modifying rights.
*/
#define kAuthorizationConfigRightModify	"config.modify."
/*!
	@defined kConfigRightRemove
	meta-rightname prefix for removing rights.
*/
#define kAuthorizationConfigRightRemove	"config.remove."
/*!
	@defined kConfigRight
	meta-rightname prefix.
*/
#define kConfigRight					"config."

/*!
	@defined kRuleIsRoot
	canned rule for daemon to daemon convincing (see AuthorizationDB.h for public ones)
*/
#define kAuthorizationRuleIsRoot				"is-root"

/* rule classes the specify behavior */

/*!	@defined kAuthorizationRuleClass
	Specifying rule class 
*/
#define kAuthorizationRuleClass					"class"

/*! @defined kAuthorizationRuleClassUser
	Specifying user class
*/
#define kAuthorizationRuleClassUser				"user"

/*! @defined kAuthorizationRuleClassMechanisms
	Specifying evaluate-mechanisms class
*/
#define kAuthorizationRuleClassMechanisms		"evaluate-mechanisms"

/* rule attributes to specify above classes */

/*! @defined kAuthorizationRuleParameterGroup
	string, group specification for user rules. 
*/
#define kAuthorizationRuleParameterGroup		"group"

/*! @defined kAuthorizationRuleParameterKofN
	number, k specification for k-of-n
*/
#define kAuthorizationRuleParameterKofN			"k-of-n"

/*! @defined kAuthorizationRuleParameterRules
	rules specification for rule delegation (incl. k-of-n)
*/
#define kAuthorizationRuleParameterRules		"rules"

/*! @defined kAuthorizationRuleParameterMechanisms
	mechanism specification, a sequence of mechanisms to be evaluated */
#define kAuthorizationRuleParameterMechanisms	"mechanisms"

/*! @defined kAuthorizationRightParameterTimeout
	timeout if any when a remembered right expires.
	special values:
	- not specified retains previous behavior: most privileged, credential based.
	- zero grants the right once
(can be achieved with zero credential timeout, needed?)
	- all other values are interpreted as number of seconds since granted.
*/
#define kAuthorizationRightParameterTimeout	"timeout-right"

/*! @defined kAuthorizationRuleParameterCredentialTimeout
	timeout if any for the use of cached credentials when authorizing rights.
	- not specified allows for any credentials regardless of age; rights will be remembered in authorizations, removing a credential does not stop it from granting this right, specifying a zero timeout for the right will delegate it back to requiring a credential.
	- all other values are interpreted as number of seconds since the credential was created
	- zero only allows for the use of credentials created "now" // This is deprecated by means of specifying zero for kRightTimeout
*/
#define kAuthorizationRuleParameterCredentialTimeout		"timeout"

/*!	@defined kAuthorizationRuleParameterCredentialShared
	boolean that indicates whether credentials acquired during authorization are added to the shared pool.
*/
#define kAuthorizationRuleParameterCredentialShared		"shared"

/*! @defined kAuthorizationRuleParameterAllowRoot
	boolean that indicates whether to grant a right purely because the caller is root */
#define kAuthorizationRuleParameterAllowRoot		"allow-root"

/*! @defined kAuthorizationRuleParameterCredentialSessionOwner
	boolean that indicates whether to grant a right based on a valid session-owner credential */
#define kAuthorizationRuleParameterCredentialSessionOwner		"session-owner"

/*! @defined kRuleDefaultPrompt
	dictionary of localization-name and localized prompt pairs */
#define kAuthorizationRuleParameterDefaultPrompt	"default-prompt"

/*!
    @function AuthorizationBindPrivilegedPort

    @param fileDescriptor (input)
	
	@param name (input)
	
    @param authorization (input) The authorization object on which this operation is performed.
	
	@param flags (input) Bit mask of option flags to this call.

    @result errAuthorizationSuccess 0 No error.
*/
OSStatus AuthorizationBindPrivilegedPort(int fileDescriptor,
	const struct sockaddr_in *name,
	AuthorizationRef authorization,
	AuthorizationFlags flags);

int __authorization_bind(int s, const struct sockaddr_in *name);


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTHORIZATIONPRIV_H_ */
