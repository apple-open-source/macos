/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
// cssmcert - CSSM layer certificate (CL) related objects.
//
#include <security_cdsa_utilities/cssmcert.h>
#include <security_utilities/debugging.h>


namespace Security {


//
// Construct an EncodedCertificate
//
EncodedCertificate::EncodedCertificate(CSSM_CERT_TYPE type, CSSM_CERT_ENCODING enc,
	const CSSM_DATA *data)
{
	clearPod();
	CertType = type;
	CertEncoding = enc;
	if (data)
		CertBlob = *data;
}


//
// Construct an empty CertGroup.
//
CertGroup::CertGroup(CSSM_CERT_TYPE ctype,
        CSSM_CERT_ENCODING encoding, CSSM_CERTGROUP_TYPE type)
{
    clearPod();
    CertType = ctype;
    CertEncoding = encoding;
    CertGroupType = type;
}


//
// Free all memory in a CertGroup
//
void CertGroup::destroy(Allocator &allocator)
{
	switch (type()) {
	case CSSM_CERTGROUP_DATA:
		// array of CSSM_DATA elements
		for (uint32 n = 0; n < count(); n++)
			allocator.free(blobCerts()[n].data());
		allocator.free (blobCerts ());
		break;
	case CSSM_CERTGROUP_ENCODED_CERT:
		for (uint32 n = 0; n < count(); n++)
			allocator.free(encodedCerts()[n].data());
		allocator.free (blobCerts ());
		break;
	case CSSM_CERTGROUP_PARSED_CERT:
		// CSSM_PARSED_CERTS array -- unimplemented
	case CSSM_CERTGROUP_CERT_PAIR:
		// CSSM_CERT_PAIR array -- unimplemented
		break;
	}
}


}	// end namespace Security
