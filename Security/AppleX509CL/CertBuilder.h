/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
 * CertBuilder.h - sublasses of various snacc-generated cert-related
 * classes. 
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */

#ifndef	_CERT_BUILDER_H_
#define _CERT_BUILDER_H_

#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>
#include <Security/sm_x501if.h>
#include <Security/sm_x520sa.h>
#include <Security/sm_x411mtsas.h>
#include <Security/sm_x509cmn.h>
#include <Security/sm_x509af.h>
#include <Security/pkcs9oids.h>
#include <Security/sm_x509ce.h>
#include <Security/sm_cms.h>
#include <Security/sm_ess.h>

#include <Security/cssmtype.h>

/*
 * Name is a complex structure which boils down to an arbitrarily
 * large array of (usually) printable names. We facilitate the
 * construction of the array, one AttributeTypeAndDistinguishedValue
 * per RelativeDistinguishedName. This is the format commonly used
 * in the real world, though it's legal to have multiple ATDVs
 * per RDN - we just don't do it here. 
 *
 * Typically the object manipulated here is inserted into a 
 * CertificateToSign object, as issuer or subject. 
 */
class NameBuilder : public Name		// Name from sm_x501if
{
public:
	void addATDV(  
		const AsnOid					&type,		// id_at_commonName, etc. 
													//   from sm_x520sa
		const char 						*value,		// the bytes
		size_t							valueLen,
		DirectoryString::ChoiceIdEnum	stringType,	// printableStringCid, etc.
													//   from sm_x520sa
		bool							primaryDistinguished = true);   
};


/*
 * Custom AsnOid, used for converting CssmOid to AsnOid. The Snacc class
 * declaration doesn't provide a means to construct from, or set by,
 * pre-encoded OID bytes (which are available in a CssmOid).
 */
class OidBuilder : public AsnOid
{
public:
	OidBuilder(const CSSM_OID &coid);
	~OidBuilder() { }
};

#endif	/* _CERT_BUILDER_H_ */

