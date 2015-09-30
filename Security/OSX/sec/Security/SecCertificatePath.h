/*
 * Copyright (c) 2007-2009,2012-2013 Apple Inc. All Rights Reserved.
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
	@header SecCertificatePath
	CoreFoundation based certificate path object
*/

#ifndef _SECURITY_SECCERTIFICATEPATH_H_
#define _SECURITY_SECCERTIFICATEPATH_H_

#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFError.h>
#include <xpc/xpc.h>

__BEGIN_DECLS

typedef struct SecCertificatePath *SecCertificatePathRef;

/* SecCertificatePath API functions. */
CFTypeID SecCertificatePathGetTypeID(void);

/* Create a new certificate path from an old one. */
SecCertificatePathRef SecCertificatePathCreate(SecCertificatePathRef path,
	SecCertificateRef certificate);

/* Create a new certificate path from an xpc_array of datas. */
SecCertificatePathRef SecCertificatePathCreateWithXPCArray(xpc_object_t xpc_path, CFErrorRef *error);

/* Create an array of CFDataRefs from a certificate path. */
xpc_object_t SecCertificatePathCopyXPCArray(SecCertificatePathRef path, CFErrorRef *error);

SecCertificatePathRef SecCertificatePathCopyAddingLeaf(SecCertificatePathRef path,
SecCertificateRef leaf);

/* Return a new certificate path without the first skipCount certificates. */
SecCertificatePathRef SecCertificatePathCopyFromParent(SecCertificatePathRef path, CFIndex skipCount);

/* Record the fact that we found our own root cert as our parent
   certificate. */
void SecCertificatePathSetSelfIssued(
	SecCertificatePathRef certificatePath);

void SecCertificatePathSetIsAnchored(
	SecCertificatePathRef certificatePath);

/* Return the index of the first non anchor certificate in the chain that is
   self signed counting from the leaf up.  Return -1 if there is none. */
CFIndex SecCertificatePathSelfSignedIndex(
	SecCertificatePathRef certificatePath);

Boolean SecCertificatePathIsAnchored(
	SecCertificatePathRef certificatePath);

void SecCertificatePathSetNextSourceIndex(
	SecCertificatePathRef certificatePath, CFIndex sourceIndex);

CFIndex SecCertificatePathGetNextSourceIndex(
	SecCertificatePathRef certificatePath);

CFIndex SecCertificatePathGetCount(
	SecCertificatePathRef certificatePath);

SecCertificateRef SecCertificatePathGetCertificateAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix);

/* Return the index of certificate in path or kCFNotFound if certificate is
   not in path. */
CFIndex SecCertificatePathGetIndexOfCertificate(SecCertificatePathRef path,
    SecCertificateRef certificate);

/* Return the root certificate for certificatePath.  Note that root is just
   the top of the path as far as it is constructed.  It may or may not be
   trusted or self signed.  */
SecCertificateRef SecCertificatePathGetRoot(
	SecCertificatePathRef certificatePath);

SecKeyRef SecCertificatePathCopyPublicKeyAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix);

typedef CFIndex SecPathVerifyStatus;
enum {
	kSecPathVerifiesUnknown = -1,
	kSecPathVerifySuccess = 0,
	kSecPathVerifyFailed = 1
};

SecPathVerifyStatus SecCertificatePathVerify(
	SecCertificatePathRef certificatePath);

CFIndex SecCertificatePathScore(SecCertificatePathRef certificatePath,
	CFAbsoluteTime verifyTime);


/*
Node based version is possible.  We need to make sure we extract algorithm oid and parameters in the chain.  When constructing a new path (with a new parent from a path with the child at it's head), we duplicate each child node for which we could not previously establish a public key because the parameters were missing and there was no cert with the same algorithm in the chain which does have parameters.  This is because, when extended with a different parent certificate that has different parameters for the childs algorithm, the signatures in the child chain must be reverified using the new parameters and therefore might yeild a different result.
We could allow more sharing if we stored the parameters found in the search up the chain in each node, and only duplicate the nodes if the parameters differ and then reset the isSigned status of each node with changed parameters. */

__END_DECLS

#endif /* !_SECURITY_SECCERTIFICATEPATH_H_ */
