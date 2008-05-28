/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

/*!
	@header SecCodeSigner
	SecCodeSigner represents an object that signs code.
*/
#ifndef _H_SECCODESIGNER
#define _H_SECCODESIGNER

#ifdef __cplusplus
extern "C" {
#endif

#include <Security/CSCommon.h>

/*!
	@typedef SecCodeSignerRef
	This is the type of a reference to a code requirement.
*/
typedef struct __SecCodeSigner *SecCodeSignerRef;	/* code signing object */


/*!
	@function SecCodeGetTypeID
	Returns the type identifier of all SecCode instances.
*/
CFTypeID SecCodeSignerGetTypeID(void);


/*!
	@constant kSecCodeSignerIdentity A SecIdentityRef describing the signing identity
		to use for signing code. This is a mandatory parameter.
	@constant kSecCodeSignerDetached If present, a detached code signature is generated.
		If absent, code signature data is written into the target code (which must
		be writable). The value is a CFURL identifying the file that is replaced with
		the detached signature data.
	@constant kSecCodeSignerRequirements Internal code requirements.
	@constant kSecCodeSignerFlags Signature flags.
 */
extern const CFStringRef kSecCodeSignerApplicationData;
extern const CFStringRef kSecCodeSignerDetached;
extern const CFStringRef kSecCodeSignerDryRun;
extern const CFStringRef kSecCodeSignerEntitlements;
extern const CFStringRef kSecCodeSignerFlags;
extern const CFStringRef kSecCodeSignerIdentifier;
extern const CFStringRef kSecCodeSignerIdentifierPrefix;
extern const CFStringRef kSecCodeSignerIdentity;
extern const CFStringRef kSecCodeSignerPageSize;
extern const CFStringRef kSecCodeSignerRequirements;
extern const CFStringRef kSecCodeSignerResourceRules;
extern const CFStringRef kSecCodeSignerSigningTime;


/*!
	@function SecCodeSignerCreate
	Create a (new) SecCodeSigner object to be used for signing code.

	@param parameters An optional CFDictionary containing parameters that influence
		signing operations with the newly created SecCodeSigner. If NULL, defaults
		are applied to all parameters; note however that some parameters do not have
		useful defaults, and will need to be set before signing is attempted.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param staticCode On successful return, a SecStaticCode object reference representing
	the file system origin of the given SecCode. On error, unchanged.
	@result Upon success, noErr. Upon error, an OSStatus value documented in
	CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeSignerCreate(CFDictionaryRef parameters, SecCSFlags flags,
	SecCodeSignerRef *signer);


/*!
	@function SecCodeSignerAddSignature
	Create a code signature and add it to the StaticCode object being signed.

	@param signer A SecCodeSigner object containing all the information required
	to sign code.
	@param code A valid SecStaticCode object reference representing code files
	on disk. This code will be signed, and will ordinarily be modified to contain
	the resulting signature data.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param errors An optional pointer to a CFErrorRef variable. If the call fails
	(and something other than noErr is returned), and this argument is non-NULL,
	a CFErrorRef is stored there further describing the nature and circumstances
	of the failure. The caller must CFRelease() this error object when done with it.
	@result Upon success, noErr. Upon error, an OSStatus value documented in
	CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeSignerAddSignature(SecCodeSignerRef signer,
	SecStaticCodeRef code, SecCSFlags flags);
	
OSStatus SecCodeSignerAddSignatureWithErrors(SecCodeSignerRef signer,
	SecStaticCodeRef code, SecCSFlags flags, CFErrorRef *errors);


#ifdef __cplusplus
}
#endif

#endif //_H_SECCODESIGNER
