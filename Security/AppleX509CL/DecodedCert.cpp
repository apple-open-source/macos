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
 * DecodedCert.cpp - object representing a snacc-decoded cert, with extensions
 * parsed and decoded (still in snacc format).
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */

#include "DecodedCert.h"
#include "SnaccUtils.h"
#include "cldebugging.h"
#include "AppleX509CLSession.h"
#include "CSPAttacher.h"
#include <Security/cdsaUtils.h>
#include <Security/cssmapple.h>

DecodedCert::DecodedCert(
	AppleX509CLSession	&session)
	: alloc(CssmAllocator::standard()),
	  mSession(session)
{
	certificateToSign = new CertificateToSign;
	reset();
}

/* one-shot constructor, decoding from DER-encoded data */
DecodedCert::DecodedCert(
	AppleX509CLSession	&session,
	const CssmData 	&encodedCert)
	: alloc(CssmAllocator::standard()),
	  mSession(session)
{
	reset();
	SC_decodeAsnObj(encodedCert, *this);
	decodeExtensions();
	mState = CS_DecodedCert;
}
		
DecodedCert::~DecodedCert()
{
	/* free all extensions */
	unsigned dex;
	
	for(dex=0; dex<mNumExtensions; dex++) {
		DecodedExten *exten = &mExtensions[dex];
		delete exten->extnId;
		delete exten->snaccObj;
	}
	alloc.free(mExtensions);
	reset();
}
	
/* decode TBSCert and its extensions */
void DecodedCert::decodeTbs(
	const CssmData	&encodedTbs)
{
	CASSERT(mState == CS_Empty);
	CASSERT(certificateToSign != NULL);
	try {
		SC_decodeAsnObj(encodedTbs, *certificateToSign);
	}
	catch (...) {
		errorLog0("decodeTbs: tbs.BDec failure\n");
		/* FIXME - leave in bad state? delete and clear? let's be cautious...*/
		delete certificateToSign;
		certificateToSign = new CertificateToSign;
	}
	decodeExtensions();
	mState = CS_DecodedTBS;
}

/*
 * FIXME : how to determine max encoding size at run time!?
 */
#define MAX_TEMPLATE_SIZE	(8 * 1024)

/* encode TBS component; only called from CertCreateTemplate */
void DecodedCert::encodeTbs(
	CssmOwnedData	&encodedTbs)
{
	encodeExtensions();
	CASSERT(mState == CS_Building);
	if(certificateToSign == NULL) {
		errorLog0("DecodedCert::encodeTbs: no TBS\n");
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	
	/* enforce required fields - could go deeper, maybe we should */
	if((certificateToSign->signature == NULL) ||
	   (certificateToSign->issuer == NULL) ||
	   (certificateToSign->validity == NULL) ||
	   (certificateToSign->subject == NULL) ||
	   (certificateToSign->subjectPublicKeyInfo == NULL)) {
		errorLog0("DecodedCert::encodeTbs: incomplete TBS\n");
		/* an odd, undocumented error return */
		CssmError::throwMe(CSSMERR_CL_NO_FIELD_VALUES);
	}
	SC_encodeAsnObj(*certificateToSign, encodedTbs, MAX_TEMPLATE_SIZE);
}

/*
 * Cook up CSSM_KEYUSE, gleaning as much as possible from
 * (optional) extensions. If no applicable extensions available,
 * we'll just return CSSM_KEYUSE_ANY.
 *
 * Note that the standard KeyUsage flags involving 'signing' translate
 * to verify since we're only dealing with public keys. 
 */
CSSM_KEYUSE DecodedCert::inferKeyUsage() const
{
	CSSM_KEYUSE keyUse = 0;
	DecodedExten *decodedExten;
	uint32 numFields;
	
	decodedExten = findDecodedExt(id_ce_keyUsage, false, 0, numFields);
	if(decodedExten) {
		KeyUsage *ku = dynamic_cast<KeyUsage *>(decodedExten->snaccObj);
		if(ku == NULL) {
			errorLog0("inferKeyUsage: dynamic_cast failure(1)\n");
			CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
		}
		if(ku->GetBit(KeyUsage::digitalSignature)) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(ku->GetBit(KeyUsage::nonRepudiation)) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(ku->GetBit(KeyUsage::keyEncipherment)) {
			keyUse |= CSSM_KEYUSE_WRAP;
		}
		if(ku->GetBit(KeyUsage::keyAgreement)) {
			keyUse |= CSSM_KEYUSE_DERIVE;
		}
		if(ku->GetBit(KeyUsage::keyCertSign)) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(ku->GetBit(KeyUsage::cRLSign)) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(ku->GetBit(KeyUsage::dataEncipherment)) {
			keyUse |= CSSM_KEYUSE_ENCRYPT;
		}
	}
	decodedExten = findDecodedExt(id_ce_extKeyUsage, false, 0, numFields);
	if(decodedExten) {
		ExtKeyUsageSyntax *eku = 
			dynamic_cast<ExtKeyUsageSyntax *>(decodedExten->snaccObj);
		if(eku == NULL) {
			errorLog0("inferKeyUsage: dynamic_cast failure(2)\n");
			CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
		}
		unsigned numOids = eku->Count();
		eku->SetCurrToFirst();
		unsigned oidDex;
		for(oidDex=0; oidDex<numOids; oidDex++) {
			KeyPurposeId *purp = eku->Curr();
			if(*purp == id_kp_codeSigning) {
				keyUse |= CSSM_KEYUSE_VERIFY;
			}
			/* I don't think the other purposes are useful... */
			eku->GoNext();
		}
	}
	if(keyUse == 0) {
		/* Nothing found; take the default. */
		keyUse = CSSM_KEYUSE_ANY;
	}
	return keyUse;
}

/*
 * Obtain a CSSM_KEY from a decoded cert, inferring as much as we can
 * from required fields (subjectPublicKeyInfo) and extensions (for 
 * KeyUse).
 */
CSSM_KEY_PTR DecodedCert::extractCSSMKey(
	CssmAllocator		&alloc)	const
{
	CASSERT(certificateToSign != NULL);
	SubjectPublicKeyInfo *snaccKeyInfo = certificateToSign->subjectPublicKeyInfo;
	if((snaccKeyInfo == NULL) ||
	   (snaccKeyInfo->algorithm == NULL)) {
		CssmError::throwMe(CSSMERR_CL_NO_FIELD_VALUES);
	}
	CSSM_KEY_PTR cssmKey = (CSSM_KEY_PTR) alloc.malloc(sizeof(CSSM_KEY));
	memset(cssmKey, 0, sizeof(CSSM_KEY));
	CSSM_KEYHEADER &hdr = cssmKey->KeyHeader;
	CssmRemoteData keyData(alloc, cssmKey->KeyData);
	try {
		hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
		/* CspId blank */
		hdr.BlobType = CSSM_KEYBLOB_RAW;
		hdr.AlgorithmId = CL_snaccOidToCssmAlg(snaccKeyInfo->algorithm->algorithm);
			
		/* 
		 * Format inferred from AlgorithmId. I have never seen these defined
		 * anywhere, e.g., whart's the format of an RSA public key in a cert?
		 * X509 certainly doesn't say. However. the following two cases are known
		 * to be correct. 
		 */
		switch(hdr.AlgorithmId) {
			case CSSM_ALGID_RSA:
				hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
				break;
			case CSSM_ALGID_DSA:
				hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
				break;
			case CSSM_ALGID_FEE:
				/* CSSM_KEYBLOB_RAW_FORMAT_NONE --> DER encoded */
				hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
				break;
			default:
				/* punt */
				hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
		}
		hdr.KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
		
		/* KeyUsage inferred from extensions */
		hdr.KeyUsage = inferKeyUsage();
		
		/* start/end date unknown, leave zero */
		hdr.WrapAlgorithmId = CSSM_ALGID_NONE;
		hdr.WrapMode = CSSM_ALGMODE_NONE;
		
		/*
		 * subjectPublicKeyInfo.subjectPublicKey (AsnBits) ==> KeyData
		 */
		SC_asnBitsToCssmData(snaccKeyInfo->subjectPublicKey, keyData);
		keyData.release();

		/*
		 * LogicalKeySizeInBits - ask the CSP
		 */
		CSSM_CSP_HANDLE cspHand = getGlobalCspHand(true);
		CSSM_KEY_SIZE keySize;
		CSSM_RETURN crtn;
		crtn = CSSM_QueryKeySizeInBits(cspHand, NULL, cssmKey, &keySize);
		if(crtn) {
			CssmError::throwMe(crtn);
		}
		cssmKey->KeyHeader.LogicalKeySizeInBits = 
			keySize.LogicalKeySizeInBits;
	}
	catch (...) {
		alloc.free(cssmKey);
		throw;
	}
	return cssmKey;
}

void DecodedCert::freeCSSMKey(
	CSSM_KEY_PTR		cssmKey,
	CssmAllocator		&alloc,
	bool				freeTop)
{
	if(cssmKey == NULL) {
		return;
	}
	alloc.free(cssmKey->KeyData.Data);
	memset(cssmKey, 0, sizeof(CSSM_KEY));
	if(freeTop) {
		alloc.free(cssmKey);
	}
}

