/*
 * Copyright (c) 2007 Apple Inc. All Rights Reserved.
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

//
// csutilities - miscellaneous utilities for the code signing implementation
//
// This is a collection of odds and ends that wouldn't fit anywhere else.
// The common theme is that the contents are otherwise naturally homeless.
//
#ifndef _H_CSUTILITIES
#define _H_CSUTILITIES

#include <Security/Security.h>
#include <security_utilities/hashing.h>
#include <security_cdsa_utilities/cssmdata.h>


namespace Security {
namespace CodeSigning {


//
// Calculate canonical hashes of certificate.
// This is simply defined as (always) the SHA1 hash of the DER.
//
void hashOfCertificate(const void *certData, size_t certLength, SHA1::Digest digest);
void hashOfCertificate(SecCertificateRef cert, SHA1::Digest digest);


//
// Check to see if a certificate contains a particular field, by OID. This works for extensions,
// even ones not recognized by the local CL. It does not return any value, only presence.
//
bool certificateHasField(SecCertificateRef cert, const CssmOid &oid);


} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_CSUTILITIES
