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
 * DecodedCert.h - object representing a snacc-decoded cert, with extensions
 * parsed and decoded (still in snacc format).
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 *
 * This object is how we store certs, both when caching them (explicitly or
 * during a search), and as an intermediate stage during template (TBS, or
 * to-be-signed cert) construction. This is a subclass of the SNACC-generated class
 * Certificate; the main functionality we add is the parsing and decoding of 
 * Extensions. Extensions are not decoded in class Certificate beyond the level
 * of the X.509 Extension object, which just contains the ID (an OID), the
 * critical flag, and an octet string containing an ID-specific thing. 
 *
 * When we decode a cert or a TBS, we also parse the Extension objects, decoding
 * then into specific SNACC classes like KeyUsage or BasicConstriantsSyntax. We
 * keep these decoded extensions in a list of DecodedExten structs. GetCertField
 * ops which access extensions access these DecodedExten structs.
 *
 * When creating a cert template (TBS), each incoming field associated with an 
 * extension is translated into an object like a (SNACC) KeyUsage and stored in
 * our DecodedExten list. 
 *
 * When encoding a TBS, we BER-encode each of the SNACC objects (KeyUsage, etc.) 
 * in our list of DecodedExtens, wrapthe result in an Octet string (actually an
 * AsnOcts) and store it in the SNACC-generated CertificateToSign's extensions
 * list. 
 *
 * Support for extensions which we don't understand is handled as follows. When
 * setting cert fields for such extensions during template construction, the app 
 * has to BER-encode the underlying extension. We just wrap this in an octet string
 * (AsnOcts) and store the result in a DecodedExten without further ado. When 
 * encoding the TBS, this octet string is just copied into the CertificateToSign's 
 * Extension list without further ado. When decoding a cert, if we find an 
 * extension we don't understand, the SNACC object stored in the DecodedExten 
 * is just a copy of the AsnOcts (which is the BER encoding of the underlying 
 * mystery extension wrapped in an Octet string). We pass back the Octet string's 
 * contents (*not* the BER-encoded octet string) during a GetCertField op. 
 */

#ifndef	_DECODED_CERT_H_
#define _DECODED_CERT_H_

#include <Security/cssmtype.h>
#include <Security/cssmdata.h>

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

/* state of a DecodedCert */
typedef enum {
	CS_Empty,
	CS_DecodedCert,		// can't set fields in this state
	CS_DecodedTBS,		// ditto
	CS_Building			// in the process of setting fields
} CertState;

/* means for holding decoded extensions */
typedef struct {
	AsnOid		*extnId;
	bool		critical;
	AsnType		*snaccObj;		// KeyUsage, BasicConstraintsSyntax, etc.
	bool		berEncoded;		// indicates unknown extension which we
								// do not BER-decode when parsing a cert
} DecodedExten;

class AppleX509CLSession;

class DecodedCert : public Certificate
{
public:
	/* construct empty cert, no decoded extensions */
	DecodedCert(
		AppleX509CLSession	&session);
	
	/* one-shot constructor, decoding from DER-encoded data */
	DecodedCert(
		AppleX509CLSession	&session,
		const CssmData 		&encodedCert);
		
	~DecodedCert();
	
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
		CssmOwnedData		&fieldValue) const;	// RETURNED

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
		CssmAllocator 		&alloc,
		uint32 				&NumberOfFields,
		CSSM_OID_PTR 		&OidList);

	/*
  	 * Obtain a CSSM_KEY from a decoded cert, inferring as much as we can
	 * from required fields (subjectPublicKeyInfo) and extensions (for 
	 * KeyUse).
	 */
	CSSM_KEY_PTR extractCSSMKey(
		CssmAllocator		&alloc) const;

	CSSM_KEYUSE inferKeyUsage() const;

private:

	/***
	 *** Extensions support (CertExtensions.cpp)
	 ***/
	 
	/* decode extensions ==> mExtensions */
	void decodeExtensions();

	/* encode mExtensions ==> tbs->Extensions */
	void encodeExtensions();
	
	/* called from decodeExtensions and setField* */
	void addExtension(
		AsnType 			*snaccThing,	// e.g. KeyUsage
		const AsnOid		&extnId,		
		bool				critical,
		bool				berEncoded);

public:

	/* as above, CSSM-centric OID */
	void addExtension(
		AsnType 			*snaccThing,	// e.g. KeyUsage
		const CSSM_OID		&extnId,		
		bool				critical,
		bool				berEncoded)
		{
			AsnOid snaccOid(reinterpret_cast<char *>(extnId.Data), extnId.Length);
			addExtension(snaccThing, snaccOid, critical, berEncoded);
		}

	/* called from getField* and inferKeyUsage */
	/* returns NULL if not found */
	DecodedExten *findDecodedExt(
		const AsnOid		&extnId,		// for known extensions
		bool				unknown,		// otherwise	
		uint32				index,	
		uint32				&numFields) const;

private:
	CertState			mState;
	DecodedExten		*mExtensions;
	unsigned			mNumExtensions;		// # valid DecodedExtens
	unsigned			mSizeofExtensions;	// mallocd size in DecodedExten
	CssmAllocator		&alloc;
	AppleX509CLSession	&mSession;
	
	void reset() 
		{
			mState = CS_Empty;
			mExtensions = NULL;
			mNumExtensions = 0;
			mSizeofExtensions = 0;
		}
};

#endif	/* _DECODED_CERT_H_ */
