/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
#include "csutilities.h"
#include <Security/SecCertificatePriv.h>
#include <security_codesigning/requirement.h>
#include <security_utilities/debugging.h>
#include <security_utilities/errors.h>

namespace Security {
namespace CodeSigning {


//
// Calculate the canonical hash of a certificate, given its raw (DER) data.
//
void hashOfCertificate(const void *certData, size_t certLength, SHA1::Digest digest)
{
	SHA1 hasher;
	hasher(certData, certLength);
	hasher.finish(digest);
}


//
// Ditto, given a SecCertificateRef
//
void hashOfCertificate(SecCertificateRef cert, SHA1::Digest digest)
{
	assert(cert);
	CSSM_DATA certData;
	MacOSError::check(SecCertificateGetData(cert, &certData));
	hashOfCertificate(certData.Data, certData.Length, digest);
}


//
// Check to see if a certificate contains a particular field, by OID. This works for extensions,
// even ones not recognized by the local CL. It does not return any value, only presence.
//
bool certificateHasField(SecCertificateRef cert, const CssmOid &oid)
{
	assert(cert);
	CSSM_DATA *value;
	switch (OSStatus rc = SecCertificateCopyFirstFieldValue(cert, &oid, &value)) {
	case noErr:
		MacOSError::check(SecCertificateReleaseFirstFieldValue(cert, &oid, value));
		return true;					// extension found by oid
	case CSSMERR_CL_UNKNOWN_TAG:
		break;							// oid not recognized by CL - continue below
	default:
		MacOSError::throwMe(rc);		// error: fail
	}
	
	// check the CL's bag of unrecognized extensions
	CSSM_DATA **values;
	bool found = false;
	if (SecCertificateCopyFieldValues(cert, &CSSMOID_X509V3CertificateExtensionCStruct, &values))
		return false;	// no unrecognized extensions - no match
	if (values)
		for (CSSM_DATA **p = values; *p; p++) {
			const CSSM_X509_EXTENSION *ext = (const CSSM_X509_EXTENSION *)(*p)->Data;
			if (oid == ext->extnId) {
				found = true;
				break;
			}
		}
	MacOSError::check(SecCertificateReleaseFieldValues(cert, &CSSMOID_X509V3CertificateExtensionCStruct, values));
	return found;
}


} // end namespace CodeSigning
} // end namespace Security
