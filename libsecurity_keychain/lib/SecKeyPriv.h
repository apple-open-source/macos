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
 *
 * SecKeyPriv.h - SPIs to SecKeyRef objects.
 */

/*!
	@header SecKeyPriv
	The functions provided in SecKeyPriv implement a particular type of SecKeychainItem which represents a key.  SecKeys might be stored in a SecKeychain, but can also be used as transient objects representing keys.

	Most SecKeychainItem* functions will work on a SecKeyRef.
*/

#ifndef _SECURITY_SECKEYPRIV_H_
#define _SECURITY_SECKEYPRIV_H_

#include <Security/SecKey.h>
#include <Security/x509defs.h>
#include <AvailabilityMacros.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*!
	@function SecKeyGetAlgorithmID
	@abstract Returns a pointer to a CSSM_X509_ALGORITHM_IDENTIFIER structure for the given key.
    @param key A key reference.
    @param algid On return, a pointer to a CSSM_X509_ALGORITHM_IDENTIFIER structure.
	@result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecKeyGetAlgorithmID(SecKeyRef key, const CSSM_X509_ALGORITHM_IDENTIFIER **algid);

/*!
	@function SecKeyGetStrengthInBits
	@abstract Returns key strength in bits for the given key.
    @param key A key reference.
    @param algid A pointer to a CSSM_X509_ALGORITHM_IDENTIFIER structure, as returned from a call to SecKeyGetAlgorithmID.
    @param strength On return, the key strength in bits.
	@result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecKeyGetStrengthInBits(SecKeyRef key, const CSSM_X509_ALGORITHM_IDENTIFIER *algid, unsigned int *strength);

/*!
	@function SecKeyImportPair
	@abstract Takes an asymmetric key pair and stores it in the keychain specified by the keychain parameter.
	@param keychainRef A reference to the keychain in which to store the private and public key items. Specify NULL for the default keychain.
    @param publicCssmKey A CSSM_KEY which is valid for the CSP returned by SecKeychainGetCSPHandle(). This may be a normal key or reference key.
    @param privateCssmKey A CSSM_KEY which is valid for the CSP returned by SecKeychainGetCSPHandle(). This may be a normal key or reference key.
    @param initialAccess A SecAccess object that determines the initial access rights to the private key. The public key is given an any/any acl by default.
    @param publicKey Optional output pointer to the keychain item reference of the imported public key. The caller must call CFRelease on this value if it is returned.
    @param privateKey Optional output pointer to the keychain item reference of the imported private key. The caller must call CFRelease on this value if it is returned.
	@result A result code.  See "Security Error Codes" (SecBase.h).
    @deprecated in 10.5 and later. Use the SecKeychainItemImport function instead; see <Security/SecImportExport.h>
*/
OSStatus SecKeyImportPair(
        SecKeychainRef keychainRef,
		const CSSM_KEY *publicCssmKey,
		const CSSM_KEY *privateCssmKey,
        SecAccessRef initialAccess,
        SecKeyRef* publicKey,
        SecKeyRef* privateKey)
		DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function SecKeyCreate
    @abstract Generate a temporary floating key reference for a CSSM_KEY.
    @param key A pointer to a CSSM_KEY structure.
    @param keyRef On return, a key reference.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion Warning: this function is NOT intended for use outside the Security stack in its current state. <rdar://3201885>
*/
OSStatus SecKeyCreate(const CSSM_KEY *key, SecKeyRef* keyRef);


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYPRIV_H_ */

