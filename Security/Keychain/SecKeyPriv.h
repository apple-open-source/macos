/*
 *  SecKeyPriv.h
 *  Security
 *
 *  Created by Michael Brouwer on Fri Nov 08 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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

/*!
	@header SecKeyPriv
	The functions provided in SecKeyPriv implement a particular type of SecKeychainItem which represents a key.  SecKeys might be stored in a SecKeychain, but can also be used as transient object representing keys.

	Most SecKeychainItem* functions will work on an SecKeyRef.
*/

#ifndef _SECURITY_SECKEYPRIV_H_
#define _SECURITY_SECKEYPRIV_H_

#include <Security/SecKey.h>


#if defined(__cplusplus)
extern "C" {
#endif

/*!
	@typedef SecCredentialType
	@abstract Determines the type of credential returned by SecKeyGetCredentials.
*/
typedef uint32 SecCredentialType;

/*!
	@enum SecCredentialType
	@abstract Determines the type of credential returned by SecKeyGetCredentials.
	@constant kSecCredentialTypeWithUI will cause UI to happen if needed.
	@constant kSecCredentialTypeNoUI will fail if UI would of been required.
	@constant kSecCredentialTypeDefault will choose to do UI when other SecKeychain calls currently do.
*/
enum
{
	kSecCredentialTypeDefault = 0,
	kSecCredentialTypeWithUI,
	kSecCredentialTypeNoUI
};


/*!
	@function SecKeyGetCredentials
	@abstract For a given key return a const CSSM_ACCESS_CREDENTIALS * which will allow the key to be used.
	@param keyRef The key for which a credential is requested.
    @param operation the type of operation which is going to be perform on this key.  Examples are: CSSM_ACL_AUTHORIZATION_SIGN, CSSM_ACL_AUTHORIZATION_DECRYPT, CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED.
    @param credentialType The type of credential requested.
	@param outCredentials Output a pointer to a const CSSM_ACCESS_CREDENTIALS * is returned here which remains valid at least as long as the keyRef itself remains valid, which can be used in CDSA calls.
	@result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecKeyGetCredentials(
        SecKeyRef keyRef,
		CSSM_ACL_AUTHORIZATION_TAG operation,
		SecCredentialType credentialType,
        const CSSM_ACCESS_CREDENTIALS **outCredentials);

/*!
	@function SecKeyImportPair
	@abstract Takes an asymmetric key pair and stores it in the keychain specified by the keychain parameter.
	@param keychainRef A reference to the keychain in which to store the private and public key items. Specify NULL for the default keychain.
    @param publicCssmKey A CSSM_KEY which is valid for the CSP returned by SecKeychainGetCSPHandle().  This may be a normal key or reference key.
    @param privateCssmKey A CSSM_KEY which is valid for the CSP returned by SecKeychainGetCSPHandle().  This may be a normal key or reference key.
    @param initialAccess A SecAccess object that determines the initial access rights to the private key.  The public key is given an any/any acl by default.
    @param publicKey Optional output pointer to the keychain item reference of the imported public key. The caller must call CFRelease on this value if it is returned.
    @param privateKey Optional output pointer to the keychain item reference of the imported private key. The caller must call CFRelease on this value if it is returned.
	@result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecKeyImportPair(
        SecKeychainRef keychainRef,
		const CSSM_KEY *publicCssmKey,
		const CSSM_KEY *privateCssmKey,
        SecAccessRef initialAccess,
        SecKeyRef* publicKey,
        SecKeyRef* privateKey);


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYPRIV_H_ */

