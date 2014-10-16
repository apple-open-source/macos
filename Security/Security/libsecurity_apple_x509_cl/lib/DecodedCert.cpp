/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
 * DecodedCert.cpp - object representing a decoded cert, in NSS
 * format, with extensions parsed and decoded (still in NSS format).
 *
 * Copyright (c) 2000,2011,2014 Apple Inc. 
 */

#include "DecodedCert.h"
#include "clNssUtils.h"
#include "cldebugging.h"
#include "AppleX509CLSession.h"
#include "CSPAttacher.h"
#include <Security/cssmapple.h>
#include <Security/oidscert.h>

DecodedCert::DecodedCert(
	AppleX509CLSession	&session)
	: DecodedItem(session)
{
	memset(&mCert, 0, sizeof(mCert));
}

/* one-shot constructor, decoding from DER-encoded data */
DecodedCert::DecodedCert(
	AppleX509CLSession	&session,
	const CssmData 	&encodedCert)
	: DecodedItem(session)
{
	memset(&mCert, 0, sizeof(mCert));
	PRErrorCode prtn = mCoder.decode(encodedCert.data(), encodedCert.length(), 
		kSecAsn1SignedCertTemplate, &mCert);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
	mDecodedExtensions.decodeFromNss(mCert.tbs.extensions);
	mState = IS_DecodedAll;
}
		
DecodedCert::~DecodedCert()
{
}
	
/* decode TBSCert and its extensions */
void DecodedCert::decodeTbs(
	const CssmData	&encodedTbs)
{
	assert(mState == IS_Empty);
	
	memset(&mCert, 0, sizeof(mCert));
	PRErrorCode prtn = mCoder.decode(encodedTbs.data(), encodedTbs.length(), 
		kSecAsn1TBSCertificateTemplate, &mCert.tbs);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
	mDecodedExtensions.decodeFromNss(mCert.tbs.extensions);
	mState = IS_DecodedTBS;
}

void DecodedCert::encodeExtensions()
{
	NSS_TBSCertificate &tbs = mCert.tbs;
	assert(mState == IS_Building);
	assert(tbs.extensions == NULL);

	if(mDecodedExtensions.numExtensions() == 0) {
		/* no extensions, no error */
		return;
	}
	mDecodedExtensions.encodeToNss(tbs.extensions);
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
	assert(mState == IS_Building);

	/* enforce required fields - could go deeper, maybe we should */
	NSS_TBSCertificate &tbs = mCert.tbs;
	if((tbs.signature.algorithm.Data == NULL) ||
	   (tbs.issuer.rdns == NULL) ||
	   (tbs.subject.rdns == NULL) ||
	   (tbs.subjectPublicKeyInfo.subjectPublicKey.Data == NULL)) {
		clErrorLog("DecodedCert::encodeTbs: incomplete TBS");
		/* an odd, undocumented error return */
		CssmError::throwMe(CSSMERR_CL_NO_FIELD_VALUES);
	}
	
	PRErrorCode prtn;
	prtn = SecNssEncodeItemOdata(&tbs, kSecAsn1TBSCertificateTemplate,
		encodedTbs);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
	}
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
	const DecodedExten *decodedExten;
	uint32 numFields;
	
	/* Basic KeyUsage */
	decodedExten = DecodedItem::findDecodedExt(CSSMOID_KeyUsage, false, 
		0, numFields);
	if(decodedExten) {
		CSSM_DATA *ku = (CSSM_DATA *)decodedExten->nssObj();
		assert(ku != NULL);
		CE_KeyUsage kuse = clBitStringToKeyUsage(*ku);
		if(kuse & CE_KU_DigitalSignature) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(kuse & CE_KU_NonRepudiation) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(kuse & CE_KU_KeyEncipherment) {
			keyUse |= CSSM_KEYUSE_WRAP;
		}
		if(kuse & CE_KU_KeyAgreement) {
			keyUse |= CSSM_KEYUSE_DERIVE;
		}
		if(kuse & CE_KU_KeyCertSign) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(kuse & CE_KU_CRLSign) {
			keyUse |= CSSM_KEYUSE_VERIFY;
		}
		if(kuse & CE_KU_DataEncipherment) {
			keyUse |= CSSM_KEYUSE_ENCRYPT;
		}
	}
	
	/* Extended key usage */
	decodedExten = DecodedItem::findDecodedExt(CSSMOID_ExtendedKeyUsage, 
			false, 0, numFields);
	if(decodedExten) {
		NSS_ExtKeyUsage *euse = (NSS_ExtKeyUsage *)decodedExten->nssObj();
		assert(euse != NULL);
		unsigned numUses = clNssArraySize((const void **)euse->purposes);
		for(unsigned dex=0; dex<numUses; dex++) {
		const CSSM_OID *thisUse = euse->purposes[dex];
			if(clCompareCssmData(thisUse, &CSSMOID_ExtendedKeyUsageAny)) {
				/* we're done */
				keyUse = CSSM_KEYUSE_ANY;	
				break;
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_ServerAuth)) {
				keyUse |= (CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DERIVE);	
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_ClientAuth)) {
				keyUse |= (CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DERIVE);	
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_ExtendedUseCodeSigning)) {
				keyUse |= CSSM_KEYUSE_VERIFY;	
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_EmailProtection)) {
				keyUse |= 
					(CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_DERIVE);
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_TimeStamping)) {
				keyUse |= CSSM_KEYUSE_VERIFY;	
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_OCSPSigning)) {
				keyUse |= CSSM_KEYUSE_VERIFY;	
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_APPLE_EKU_SYSTEM_IDENTITY)) {
				/* system identity - fairly liberal: CMS as well as SSL */
				keyUse |= 
					(CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_ENCRYPT);
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_KERBv5_PKINIT_KP_CLIENT_AUTH)) {
				/* 
				 * Kerberos PKINIT client: 
				 * -- KDC verifies client signature in a CMS msg in AS-REQ
				 * -- KDC encrypts for client in a CMS msg in AS-REP
				 */
				keyUse |= (CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_WRAP);
			}
			else if(clCompareCssmData(thisUse, &CSSMOID_KERBv5_PKINIT_KP_KDC)) {
				/* 
				 * Kerberos PKINIT server: 
				 * -- client verifies KDC signature in a CMS msg in AS-REP
				 */
				keyUse |= CSSM_KEYUSE_VERIFY;
			}
		}
	}
	
	/* NetscapeCertType */
	decodedExten = DecodedItem::findDecodedExt(CSSMOID_NetscapeCertType, 
			false, 0, numFields);
	if(decodedExten) {
		/* nssObj() is a CSSM_DATA ptr, whose Data points to the bits we want */
		CSSM_DATA *nctData = (CSSM_DATA *)decodedExten->nssObj();
		if((nctData != NULL) && (nctData->Length > 0)) {
			CE_NetscapeCertType nct = ((uint16)nctData->Data[0]) << 8;
			if(nctData->Length > 1) {
				nct |= nctData->Data[1];
			}
		
			/* All this usage bits imply signature verify capability */
			if(nct & (CE_NCT_SSL_Client | CE_NCT_SSL_Server | CE_NCT_SMIME | CE_NCT_ObjSign |
					  CE_NCT_SSL_CA | CE_NCT_SMIME_CA | CE_NCT_ObjSignCA)) {
				keyUse |= CSSM_KEYUSE_VERIFY;
			}
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
	Allocator		&alloc)	const
{
	const CSSM_X509_SUBJECT_PUBLIC_KEY_INFO &keyInfo = 
		mCert.tbs.subjectPublicKeyInfo;
	return CL_extractCSSMKeyNSS(keyInfo, alloc, this);
}

