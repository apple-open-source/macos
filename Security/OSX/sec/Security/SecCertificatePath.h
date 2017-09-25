/*
 * Copyright (c) 2007-2009,2012-2017 Apple Inc. All Rights Reserved.
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

/* Create a new certificate path from an xpc_array of datas. */
SecCertificatePathRef SecCertificatePathCreateWithXPCArray(xpc_object_t xpc_path, CFErrorRef *error);

/* Create a new certificate path from a CFArray of datas. */
SecCertificatePathRef SecCertificatePathCreateDeserialized(CFArrayRef certificates, CFErrorRef *error);

/* Create an array of CFDataRefs from a certificate path. */
xpc_object_t SecCertificatePathCopyXPCArray(SecCertificatePathRef path, CFErrorRef *error);

/* Create an array of SecCertificateRefs from a certificate path. */
CFArrayRef SecCertificatePathCopyCertificates(SecCertificatePathRef path, CFErrorRef *error);

/* Create a new certificate path from an array of SecCertificateRefs. */
SecCertificatePathRef SecCertificatePathCreateWithCertificates(CFArrayRef certificates, CFErrorRef *error);

/* Create a serialized Certificate Array from a certificate path. */
CFArrayRef SecCertificatePathCreateSerialized(SecCertificatePathRef path, CFErrorRef *error);

CFIndex SecCertificatePathGetCount(
	SecCertificatePathRef certificatePath);

SecCertificateRef SecCertificatePathGetCertificateAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix);

/* Return the index of certificate in path or kCFNotFound if certificate is
   not in path. */
CFIndex SecCertificatePathGetIndexOfCertificate(SecCertificatePathRef path,
    SecCertificateRef certificate);

SecKeyRef SecCertificatePathCopyPublicKeyAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix);

__END_DECLS

#endif /* !_SECURITY_SECCERTIFICATEPATH_H_ */
