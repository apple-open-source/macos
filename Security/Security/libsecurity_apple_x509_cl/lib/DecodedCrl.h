/*
 * Copyright (c) 2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * DecodedCrl.h - object representing a decoded cert in NSS form, with
 * extensions parsed and decoded (still in NSS format).
 *
 *
 * See DecodedItem.h for details on the care and feeding of this
 * module. 
 */

#ifndef	_DECODED_CRL_H_
#define _DECODED_CRL_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmdata.h>

#include "DecodedItem.h"
#include <Security/X509Templates.h>

class DecodedCrl : /* for now public CertificateList, */ public DecodedItem
{
	NOCOPY(DecodedCrl)
public:
	/* construct empty CRL, no decoded extensions */
	DecodedCrl(
		AppleX509CLSession	&session);
	
	/* one-shot constructor, decoding from DER-encoded data */
	DecodedCrl(
		AppleX509CLSession	&session,
		const CssmData 		&encodedCrl);
		
	~DecodedCrl();
	
	/* decode CRLToSign and its extensions */
	void decodeCts(
		const CssmData	&encodedCTS);
		
	/* encode TBSCert and its extensions */
	void encodeExtensions();
	void encodeCts(
		CssmOwnedData	&encodedTbs);
		
	/***
	 *** field accessors (in CrlFields.cpp)
	 ***/
	
	/* 
	 * Obtain the index'th occurrence of field specified by fieldId.
	 * Format of the returned field depends on fieldId.
	 * Returns total number of fieldId fields in the cert if index is 0.
	 * Returns true if specified field was found, else returns false. 
	 */
	bool getCrlFieldData(
		const CssmOid		&fieldId,			// which field
		unsigned			index,				// which occurrence (0 = first)
		uint32				&numFields,			// RETURNED
		CssmOwnedData		&fieldValue);		// RETURNED

	/*
	 * Set the field specified by fieldId in TBS. 
	 * Note no index - individual field routines either append (for extensions)
	 * or throw if field already set (for all others) 
	 */
	void setCrlField(
		const CssmOid		&fieldId,		// which field
		const CssmData		&fieldValue);	

	/*
	 * Free the fieldId-specific data referred to by fieldValue.get().data().
	 */
	static void freeCrlFieldData(
		const CssmOid		&fieldId,
		CssmOwnedData		&fieldValue);

	void getAllParsedCrlFields(
		uint32 				&NumberOfFields,		// RETURNED
		CSSM_FIELD_PTR 		&CertFields);			// RETURNED

	static void describeFormat(
		Allocator 		&alloc,
		uint32 				&NumberOfFields,
		CSSM_OID_PTR 		&OidList);

	NSS_Crl	mCrl;
	
};

#endif	/* _DECODED_CRL_H_ */
