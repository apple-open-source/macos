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
	@function SecKeyGetCSPHandle
	@abstract Returns the CSSM_CSP_HANDLE attachment for the given key reference. The handle is valid until the key reference is released.
    @param keyRef A key reference.
    @param cspHandle On return, a pointer to the CSSM_CSP_HANDLE for the given keychain.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus
SecKeyGetCSPHandle(SecKeyRef keyRef, CSSM_CSP_HANDLE *cspHandle);

OSStatus
SecKeyGetAlgorithmID(SecKeyRef key, const CSSM_X509_ALGORITHM_IDENTIFIER **algid);

OSStatus
SecKeyGetStrengthInBits(SecKeyRef key, const CSSM_X509_ALGORITHM_IDENTIFIER *algid, unsigned int *strength);


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

/*!
	@function SecKeyGenerate
	@abstract Generate a symmetric key and optionally stores it in the keychain specified by the keychainRef parameter.
	@param keychainRef(optional) A reference to the keychain in which to store the private and public key items. Specify NULL to generate a transient key.
    @param algorithm An algorithm for the key pair.  This parameter is ignored if contextHandle is non 0.
    @param keySizeInBits A key size for the key pair.  This parameter is ignored if contextHandle is non 0.
	@param contextHandle(optional) An optional CSSM_CC_HANDLE or 0.  If this argument is not 0 the algorithm and keySizeInBits parameters are ignored.  If extra parameters are needed to generate a key (some algortihms require this) you should create a context using CSSM_CSP_CreateKeyGenContext(), using the CSPHandle obtained by calling SecKeychainGetCSPHandle(). Then use CSSM_UpdateContextAttributes() to add additional parameters and dispose of the context using CSSM_DeleteContext after calling this function.
	@param keyUsage A bit mask indicating all permitted uses for the new key. The bit mask values are defined in cssmtype.h
    @param keyAttr A bit mask defining attribute values for the new key. The bit mask values are equivalent to a CSSM_KEYATTR_FLAGS and are defined in cssmtype.h
    @param initialAccess(optional) A SecAccess object that determines the initial access rights to the key.  This parameter is ignored if the keychainRef is NULL.
    @param key Output pointer to the keychain item reference of the geerated key. Use the SecKeyGetCSSMKey function to obtain the CSSM_KEY. The caller must call CFRelease on this value if it is returned.
	@result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecKeyGenerate(
		SecKeychainRef keychainRef,
		CSSM_ALGORITHMS algorithm,
		uint32 keySizeInBits,
		CSSM_CC_HANDLE contextHandle,
		CSSM_KEYUSE keyUsage,
		uint32 keyAttr,
		SecAccessRef initialAccess,
		SecKeyRef* keyRef);

OSStatus SecKeyCreate(const CSSM_KEY *key,
		SecKeyRef* keyRef);


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYPRIV_H_ */

