/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
	@header SecManifest
	The functions and data types in SecManifest implement file, directory, and
	data signing.
*/

#ifndef _SECURITY_SECMANIFEST_H_
#define _SECURITY_SECMANIFEST_H_

#include <Security/SecTrust.h>
#include <Security/SecIdentity.h>
#include <Security/SecBase.h>


#if defined(__cplusplus)
extern "C" {
#endif

enum {
	errSecManifestNotSupported   = -22040,  /* The specified object can't be placed in a manifest */
	errSecManifestNoSigners		 = -22041,  /* There must be at least one signer for a manifest */
	errSecManifestCMSFailure	 = -22042,  /* A problem occurred with CMS */
	errSecManifestIsNotEmpty	 = -20043,  /* The manifest was not empty before create from external representation */
	errSecManifestDidNotVerify   = -20044,  /* The manifest did not verify */
	errSecManifestDamaged		 = -20045,  /* The manifest was damaged */
	errSecManifestNotEqual		 = -20046,  /* The manifests were not equal */
	errSecManifestBadResult		 = -20057,  /* A manifest callback returned an invalid result */
	errSecManifestNoPolicy		 = -20058,  /* Couldn't find the default policy */
	errSecManifestInvalidException  = -20059,  /* Exception list members must be CFStrings */
	errSecManifestNoSignersFound = -20060,	/* No signers were found in the manifest */
};

typedef UInt32 SecManifestCompareOptions;
enum {kSecManifestVerifyOwnerAndGroup = 0x1};

/*!
	@typedef SecManifestRef
	@abstract A pointer to an opaque manifest structure
*/
typedef struct OpaqueSecManifestRef *SecManifestRef;

/*!
	@function SecManifestGetVersion
	@abstract Determines the version of the SecManifest API installed on the
			  user's system.
	@param version On return, a pointer to the version number of the SecManifest
				   API installed on the system.
	@result A result code.
*/
OSStatus SecManifestGetVersion(UInt32 *version);

/*!
	@function SecManifestCreate
	@abstract Creates a new manifest object for signing.
	@param manifest On return, a porinter to a manifest reference.  The memory
					that manifest occupies must be released by calling
					SecManifestRelease when you are finished with it.
	@result A result code.
*/
OSStatus SecManifestCreate(SecManifestRef *manifest);

/*!
	@function SecManifestRelease
	@abstract Destroys a manifest object
	@param manifest The manifest to destroy.
*/

void SecManifestRelease(SecManifestRef manifest);

typedef enum _SecManifestTrustCallbackResult 
{
	kSecManifestDoNotVerify,
	kSecManifestSignerVerified,
	kSecManifestContinue,
	kSecManifestFailed
} SecManifestTrustCallbackResult;

typedef SecManifestTrustCallbackResult(*SecManifestTrustSetupCallback)
			(SecTrustRef trustRef, void* setupContext);
typedef SecManifestTrustCallbackResult(*SecManifestTrustEvaluateCallback)
			(SecTrustRef trustRef, SecTrustResultType result,
			 void *evaluateContext);

/*!
	@function SecManifestVerifySignature
	@abstract Verifies a signature created with SecManifestCreateSignature,
	@param data The signature to verify.
	@param setupCallback Called before trust is verified for a signer.  This
						 allows the user to modify the SecTrustRef if needed
						 (see the SecTrust documentation).
	@param setupContext User defined.
	@param evaluateCallback Called after SecTrustEvaluate has been called for a
							signer if the result was not trusted. This allows
							the developer to query the user as to whether or not
							to trust the signer.
	@param evaluateContext User defined.
	@param manifest Optional return of the verified manifest
*/

OSStatus SecManifestVerifySignature(CFDataRef data,
									SecManifestTrustSetupCallback setupCallback,
									void* setupContext,
									SecManifestTrustEvaluateCallback evaluateCallback,
									void* evaluateContext,
									SecManifestRef *manifest);

/*!
	@function SecManifestVerifySignature
	@abstract Verifies a signature created with SecManifestCreateSignature,
	@param data The signature to verify.
	@param setupCallback Called before trust is verified for a signer.  This
						 allows the user to modify the SecTrustRef if needed
						 (see the SecTrust documentation).
	@param setupContext User defined.
	@param evaluateCallback Called after SecTrustEvaluate has been called for a
							signer if the result was not trusted. This allows
							the developer to query the user as to whether or not
							to trust the signer.
	@param evaluateContext User defined.
	@param policyRef A SecPolicyRef used to evaluate the signature.  Pass NULL to use the default policy
	@param manifest Optional return of the verified manifest
*/
OSStatus SecManifestVerifySignatureWithPolicy(CFDataRef data,
											  SecManifestTrustSetupCallback setupCallback,
											  void* setupContext,
											  SecManifestTrustEvaluateCallback evaluateCallback,
											  void* evaluateContext,
											  SecPolicyRef policyRef,
											  SecManifestRef *manifest);
/*!
	@function SecManifestCreateSignature
	@abstract Creates a signature.
	@param manifest The manifest from which to create the signature.
	@param options Reserved for future use.
	@param data On return, the external representation.  The memory that data
				occupies must be released by calling CFRelease when finished
				with it.
	@result A result code.
*/
OSStatus SecManifestCreateSignature(SecManifestRef manifest,
									UInt32 options, 
									CFDataRef *data);

/*!
	@function SecManifestAddObject
	@abstract Adds data to be signed or verified to the manifest object.
	@param manifest The manifest object.
	@param object The object to add.
	@param exceptionList If data points to a directory, this contains an
						 optional list of CFStrings, relative to object, that will
						 not be included in the manifest.
	@result A result code.
	@discussion object may either be a CFURL that points to a file URL, or a
				SecManifestData, which points to arbitrary data.
*/
OSStatus SecManifestAddObject(SecManifestRef manifest,
							  CFTypeRef object,
							  CFArrayRef exceptionList);

/*!
	@function SecManifestCompare
	@abstraact Compare one manifest to another.
	@param manifest1 A manifest to be compared for equality.
	@param manifest2 A manifest to be compared for equality.
	@result A result code.
*/
OSStatus SecManifestCompare(SecManifestRef manifest1, 
							SecManifestRef manifest2, 
							SecManifestCompareOptions options);

/*!
	@function SecManifestAddSigner
	@abstract Add an identity to the list of identities that will sign the
			  manifest.
	@param manifest The manifest to sign.
	@param identity The identity to be used to sign the manifest.
	@result A result code.
	@discussion Multiple signers are supported.  The actual signing does not
				take place until SecManifestCreateExternalRepresentation is
				called.
*/
OSStatus SecManifestAddSigner(SecManifestRef manifest,
							  SecIdentityRef identity);

#if defined(__cplusplus)
}
#endif

#endif /* ! _SECURITY_SECMANIFEST_H_ */

