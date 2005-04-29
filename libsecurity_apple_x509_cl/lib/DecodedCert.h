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
 * DecodedCert.h - object representing an NSS-decoded cert, with extensions
 * parsed and decoded (still in NSS format).
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 *
 * See DecodedItem.h for details on the care and feeding of this
 * module. 
 */

#ifndef	_DECODED_CERT_H_
#define _DECODED_CERT_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmdata.h>

#include "DecodedItem.h"
#include <Security/X509Templates.h>
#include <security_asn1/SecNssCoder.h>

class DecodedCert : public DecodedItem
{
	NOCOPY(DecodedCert)
public:
	/* construct empty cert, no decoded extensions */
	DecodedCert(
		AppleX509CLSession	&session);
	
	/* one-shot constructor, decoding from DER-encoded data */
	DecodedCert(
		AppleX509CLSession	&session,
		const CssmData 		&encodedCert);
		
	~DecodedCert();
	
	void encodeExtensions();
	
	/* decode TBSCert and its extensions */
	void decodeTbs(
		const CssmData	&encodedTbs);
		
	/* encode TBSCert and its extensions */
	void encodeTbs(
		CssmOwnedData	&encodedTbs);
		
	/***
	 *** field accessors (in CertFields.cpp)
	 ***/
	
	/* 
	 * Obtain the index'th occurrence of field specified by fieldId.
	 * Format of the returned field depends on fieldId.
	 * Returns total number of fieldId fields in the cert if index is 0.
	 * Returns true if specified field was found, else returns false. 
	 */
	bool getCertFieldData(
		const CssmOid		&fieldId,			// which field
		unsigned			index,				// which occurrence (0 = first)
		uint32				&numFields,			// RETURNED
		CssmOwnedData		&fieldValue);		// RETURNED

	/*
	 * Set the field specified by fieldId in TBS. 
	 * Note no index - individual field routines either append (for extensions)
	 * or throw if field already set (for all others) 
	 */
	void setCertField(
		const CssmOid		&fieldId,		// which field
		const CssmData		&fieldValue);	

	/*
	 * Free the fieldId-specific data referred to by fieldValue.get().data().
	 */
	static void freeCertFieldData(
		const CssmOid		&fieldId,
		CssmOwnedData		&fieldValue);

	void getAllParsedCertFields(
		uint32 				&NumberOfFields,		// RETURNED
		CSSM_FIELD_PTR 		&CertFields);			// RETURNED

	static void describeFormat(
		Allocator 		&alloc,
		uint32 				&NumberOfFields,
		CSSM_OID_PTR 		&OidList);

	/*
  	 * Obtain a CSSM_KEY from a decoded cert, inferring as much as we can
	 * from required fields (subjectPublicKeyInfo) and extensions (for 
	 * KeyUse).
	 */
	CSSM_KEY_PTR extractCSSMKey(
		Allocator		&alloc) const;

	CSSM_KEYUSE inferKeyUsage() const;
	
	NSS_Certificate			mCert;
};

#endif	/* _DECODED_CERT_H_ */
