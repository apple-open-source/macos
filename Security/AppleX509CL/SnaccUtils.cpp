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
 * CertSNACC.cpp - snacc-related cert functions
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */
 
#include "SnaccUtils.h"
#include "cldebugging.h"
#include <Security/pkcs1oids.h>
#include <Security/cdsaUtils.h>
#include <Security/cssmapple.h>
#include <Security/appleoids.h>
#include <Security/globalizer.h>

#define DEBUG_DECODE	0
#if		DEBUG_DECODE
#define ddprintf(x)		printf  x
#else
#define ddprintf(x)
#endif

/*
 * AsnOid "constants" which we construct and cache on demand to avoid the 
 * somewhat expensive op of constructing them every time we test for equality 
 * in CL_snaccOidToCssmAlg.
 */
class AlgOidCache
{
public:
	AlgOidCache() :
		mRsaEncryption(rsaEncryption_arc),
		mMd2WithRSAEncryption(md2WithRSAEncryption_arc),
		mMd5WithRSAEncryption(md5WithRSAEncryption_arc),
		mSha1withRSAEncryption(sha1withRSAEncryption_arc),
		mId_dsa(id_dsa_arc),
		mId_dsa_with_sha1(id_dsa_with_sha1_arc),
		mAppleFee(appleFee_arc),
		mAppleAsc(appleAsc_arc),
		mAppleFeeMD5(appleFeeMD5_arc),
		mAppleFeeSHA1(appleFeeSHA1_arc),
		mAppleFeed(appleFeed_arc),
		mAppleFeedExp(appleFeedExp_arc),
		mAppleECDSA(appleECDSA_arc)
			{ }
		
	AsnOid 	mRsaEncryption;
	AsnOid	mMd2WithRSAEncryption;
	AsnOid	mMd5WithRSAEncryption;
	AsnOid	mSha1withRSAEncryption;
	AsnOid	mId_dsa;
	AsnOid	mId_dsa_with_sha1;
	AsnOid	mAppleFee;
	AsnOid	mAppleAsc;
	AsnOid	mAppleFeeMD5;
	AsnOid	mAppleFeeSHA1;
	AsnOid	mAppleFeed;
	AsnOid	mAppleFeedExp;
	AsnOid	mAppleECDSA;
};

static ModuleNexus<AlgOidCache> algOidCache;

/*
 * To ensure a secure means of signing and verifying TBSCert blobs, we
 * provide these functions to encode and decode just the top-level
 * elements of a certificate. Snacc doesn't allow you to specify, for
 * example, a fully encoded TBSCert prior to encoding the whole cert after
 * signing it - you have to decode the TBSCert, put it and the other
 * components into a Cert, and then encode the whole thing. Unfortunately
 * there is no guarantee that when you decode and re-encode a TBSCert blob,
 * you get the same thing you started with (although with DER rules, as
 * opposed to BER rules, you should). Thus when signing, we sign the TBSCert
 * and encode the signed cert here without ever decoding the TBSCert (or,
 * at least, without using the decoded version to get the encoded TBS blob).
 */

void 
CL_certDecodeComponents(
	const CssmData 	&signedCert,		// DER-encoded
	CssmOwnedData	&TBSCert,			// still DER-encoded
	CssmOwnedData	&algId,				// ditto
	CssmOwnedData	&rawSig)			// raw bits (not an encoded AsnBits)
{
	CssmAutoData encodedSig(rawSig.allocator);

	/* drop signedCert into an AsnBuf for processing */
	AsnBuf buf;
	buf.InstallData(reinterpret_cast<char *>(signedCert.data()), signedCert.length());
	
	/* based on snacc-generated Certificate::BDec() and BDecContent() */
	AsnTag tag;
	AsnLen bytesDecoded = 0;
	AsnLen decLen;			// from BDecLen
	AsnLen totalLen;		// including tag and ASN length 
	char *elemStart;		// ptr to start of element, including tag
	
	int  rtn;
    ENV_TYPE env;
    if ((rtn = setjmp (env)) == 0) {
		tag = BDecTag (buf, bytesDecoded, env);
		if (tag != MAKE_TAG_ID (UNIV, CONS, SEQ_TAG_CODE)) {
			errorLog1("CL_CertDecodeComponents: bad first-level tag (0x%x)\n", tag);
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
		}
		decLen = BDecLen (buf, bytesDecoded, env);		// of total
		/* FIXME - we should be able to ensure right here that we have enough */
		
		/* First element, TBSCert */
		/* Note we need to include the tag and content in the outgoing blobs */
		elemStart = buf.DataPtr() + bytesDecoded;
		tag = BDecTag (buf, bytesDecoded, env);
		if(tag != MAKE_TAG_ID (UNIV, CONS, SEQ_TAG_CODE)) {
			errorLog1("CL_CertDecodeComponents: bad TBSCert tag (0x%x)\n", tag);
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
		}

		decLen = BDecLen (buf, bytesDecoded, env);					// DER 'length'
		/* buf now at first content byte; simulate grabbing content */
		totalLen = decLen + (bytesDecoded - (elemStart - buf.DataPtr())); 
		buf.Skip(decLen);
		bytesDecoded += decLen;
		TBSCert.copy(elemStart, totalLen);
		ddprintf(("CL_certDecodeComponents: TBS len %d; data %02x %02x %02x %02x...\n",
			totalLen, ((uint8 *)elemStart)[0], ((uint8 *)elemStart)[1],
			((uint8 *)elemStart)[2], ((uint8 *)elemStart)[3]));
		
		/* next element, algId */
		elemStart = buf.DataPtr() + bytesDecoded;
		tag = BDecTag (buf, bytesDecoded, env);
		if(tag != MAKE_TAG_ID (UNIV, CONS, SEQ_TAG_CODE)) {
			errorLog1("CL_CertDecodeComponents: bad AlgId tag (0x%x)\n", tag);
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
		}
		decLen = BDecLen (buf, bytesDecoded, env);
		totalLen = decLen + (bytesDecoded - (elemStart - buf.DataPtr())); 
		buf.Skip(decLen);
		bytesDecoded += decLen;
		algId.copy(elemStart, totalLen);
		ddprintf(("CL_certDecodeComponents: algId len %d; data %02x %02x %02x...\n",
			totalLen, ((uint8 *)elemStart)[0], ((uint8 *)elemStart)[1],
			((uint8 *)elemStart)[2]));
		
		/* next element, signature */
		elemStart = buf.DataPtr() + bytesDecoded;
		tag = BDecTag (buf, bytesDecoded, env);
		if((tag != MAKE_TAG_ID (UNIV, CONS, BITSTRING_TAG_CODE)) &&
		   (tag != MAKE_TAG_ID (UNIV, PRIM, BITSTRING_TAG_CODE))) {
			errorLog1("CL_CertDecodeComponents: bad sig tag 0x%x\n", tag);
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
		}
		decLen = BDecLen (buf, bytesDecoded, env);		
		totalLen = decLen + (bytesDecoded - (elemStart - buf.DataPtr())); 
		encodedSig.copy(elemStart, totalLen);
		ddprintf(("CL_certDecodeComponents: encodedSig len %d; data %02x %02x "
			"%02x %02x...\n",
			totalLen, ((uint8 *)elemStart)[0], ((uint8 *)elemStart)[1],
			((uint8 *)elemStart)[2], ((uint8 *)elemStart)[3]));
		
		/*
		 * encodedSig is a DER-encoded AsnBits. Decode for caller.
		 */
		SC_decodeAsnBitsToCssmData(encodedSig.get(), rawSig);
		ddprintf(("CL_certDecodeComponents: rawSig len %d\n", rawSig.length()));
		/* 
		 * OK, if we get here, we can skip the remaining stuff from 
		 * Certificate::BDecContent(), which involves getting to the end 
		 * of indefinte-length data.
		 */
	}
	else {
		errorLog0("CL_CertDecodeComponents: longjmp during decode\n");
		TBSCert.reset();
		algId.reset();
		rawSig.reset();
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
}

/*
 * Given pre-DER-encoded blobs, do the final encode step for a signed cert.
 */
void 
CL_certEncodeComponents(
	const CssmData		&TBSCert,		// DER-encoded
	const CssmData		&algId,			// ditto
	const CssmData		&rawSig,		// raw bits, not encoded
	CssmOwnedData 		&signedCert)	// DER-encoded
{
	/* first BER-encode the signature */
	AsnBits snaccSig(reinterpret_cast<char *>(rawSig.data()), 
			rawSig.length() * 8);
	CssmAutoData encodedSig(signedCert.allocator);
	SC_encodeAsnObj(snaccSig, encodedSig, rawSig.length() + 10);
	
	/* 
	 * OK, we have all three cert components already DER-encoded. The encoded
	 * cert is just (tag | contentLength | TBSCert | algId | encodedSig). 
	 * To avoid an unneccessary copy at the end of the encode, figure out
	 * the length of tag and contentLength. The tag is known to be one byte. 
	 */
	size_t contentLen = TBSCert.length() + algId.length() + encodedSig.length();
	size_t lenLen = SC_lengthOfLength(contentLen);
	size_t totalLen = 1 /* tag */ + lenLen /* length bytes */ + contentLen;
	signedCert.malloc(totalLen);
	
	/* tag */
	char *cp = (char *)signedCert.data();
	*cp++ = UNIV | CONS | SEQ_TAG_CODE;
	
	/* length */
	SC_encodeLength(contentLen, cp, lenLen);
	cp += lenLen;
	
	/* concatenate the existing components */
	memcpy(cp, TBSCert.data(), TBSCert.length());
	cp += TBSCert.length();
	memcpy(cp, algId.data(), algId.length());
	cp += algId.length();
	memcpy(cp, encodedSig.data(), encodedSig.length());
	CASSERT((cp + encodedSig.length()) == 
		((char *)signedCert.data() + signedCert.length()));
}

/* malloc/copy a CsmmOid from a snacc-style AsnOid. */
void CL_snaccOidToCssm(
	const AsnOid 	&inOid,
	CssmOid			&outOid,
	CssmAllocator	&alloc)
{
	outOid.Data = (uint8 *)alloc.malloc(inOid.Len());
	outOid.Length = inOid.Len();
	const char *cp = inOid;
	memcpy(outOid.Data, cp, outOid.Length);
}

/* convert algorithm identifier from CSSM format to snacc format */
void CL_cssmAlgIdToSnacc (
	const CSSM_X509_ALGORITHM_IDENTIFIER &cssmAlgId,
	AlgorithmIdentifier &snaccAlgId)
{
	snaccAlgId.algorithm.Set(reinterpret_cast<char *>(
		cssmAlgId.algorithm.Data), cssmAlgId.algorithm.Length);
	if(cssmAlgId.parameters.Data != NULL) {
		/* optional parameters, raw bytes */
		/* FIXME - is that right? SHould we encode as a bit string?
		 * I've never seen this "ANY" type field used... */
		snaccAlgId.parameters = new AsnAny;
		CSM_Buffer *cbuf = new CSM_Buffer(
			reinterpret_cast<char *>(cssmAlgId.parameters.Data),
				cssmAlgId.parameters.Length);
		snaccAlgId.parameters->value = cbuf;
	}
	else {
		CL_nullAlgParams(snaccAlgId);
	}
}

/* convert algorithm indentifier from snacc format to CSSM format */
void CL_snaccAlgIdToCssm (
	const AlgorithmIdentifier 		&snaccAlgId,
	CSSM_X509_ALGORITHM_IDENTIFIER	&cssmAlgId,
	CssmAllocator					&alloc)
{
	memset(&cssmAlgId, 0, sizeof(CSSM_X509_ALGORITHM_IDENTIFIER));
	
	/* algorithm - required */
	CssmOid &outOid = CssmOid::overlay(cssmAlgId.algorithm);
	CL_snaccOidToCssm(snaccAlgId.algorithm, outOid, alloc);
		
	/* parameters as AsnAny - optional - for now just pass back the raw bytes */
	if(snaccAlgId.parameters != NULL) {
		CSM_Buffer *cbuf = snaccAlgId.parameters->value;
		cssmAlgId.parameters.Data = (uint8 *)alloc.malloc(cbuf->Length());
		cssmAlgId.parameters.Length = cbuf->Length();
		memmove(cssmAlgId.parameters.Data, cbuf->Access(), 
			cssmAlgId.parameters.Length);
	}
}

/* convert between uint32-style CSSM algorithm and snacc-style AsnOid */
CSSM_ALGORITHMS CL_snaccOidToCssmAlg(
	const AsnOid &oid)
{
	AlgOidCache &oc = algOidCache();
	
	CSSM_ALGORITHMS cssmAlg = 0;
	if(oid == oc.mRsaEncryption) {
		cssmAlg = CSSM_ALGID_RSA;
	}
	else if(oid == oc.mMd2WithRSAEncryption) {
		cssmAlg = CSSM_ALGID_MD2WithRSA;
	}
	else if(oid == oc.mMd5WithRSAEncryption) {
		cssmAlg = CSSM_ALGID_MD5WithRSA;
	}
	else if(oid == oc.mSha1withRSAEncryption) {
		cssmAlg = CSSM_ALGID_SHA1WithRSA;
	}
	else if(oid == oc.mId_dsa) {
		cssmAlg = CSSM_ALGID_DSA;
	}
	else if(oid == oc.mId_dsa_with_sha1) {
		cssmAlg = CSSM_ALGID_SHA1WithDSA;
	}
	else if(oid == oc.mAppleFee) {
		cssmAlg = CSSM_ALGID_FEE;
	}
	else if(oid == oc.mAppleAsc) {
		cssmAlg = CSSM_ALGID_ASC;
	}
	else if(oid == oc.mAppleFeeMD5) {
		cssmAlg = CSSM_ALGID_FEE_MD5;
	}
	else if(oid == oc.mAppleFeeSHA1) {
		cssmAlg = CSSM_ALGID_FEE_SHA1;
	}
	else if(oid == oc.mAppleFeed) {
		cssmAlg = CSSM_ALGID_FEED;
	}
	else if(oid == oc.mAppleFeedExp) {
		cssmAlg = CSSM_ALGID_FEEDEXP;
	}
	else if(oid == oc.mAppleECDSA) {
		cssmAlg = CSSM_ALGID_SHA1WithECDSA;
	}
	/* etc. */
	else {
		errorLog0("snaccOidToCssmAlg: unknown alg\n");
		#ifndef	NDEBUG
		printf("Bogus OID: "); oid.Print(cout);
		printf("\n");
		#endif
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
	return cssmAlg;
}

void CL_cssmAlgToSnaccOid(
	CSSM_ALGORITHMS cssmAlg,
	AsnOid &oid)
{
	switch(cssmAlg) {
		case CSSM_ALGID_RSA:
			oid.ReSet(rsaEncryption_arc);
			break;
		case CSSM_ALGID_MD2WithRSA:
			oid.ReSet(md2WithRSAEncryption_arc);
			break;
		case CSSM_ALGID_MD5WithRSA:
			oid.ReSet(md2WithRSAEncryption_arc);
			break;
		case CSSM_ALGID_SHA1WithRSA:
			oid.ReSet(sha1withRSAEncryption_arc);
			break;
		case CSSM_ALGID_DSA:
			oid.ReSet(id_dsa_arc);
			break;
		case CSSM_ALGID_SHA1WithDSA:
			oid.ReSet(id_dsa_with_sha1_arc);
			break;
		case CSSM_ALGID_FEE:
			oid.ReSet(appleFee_arc);
			break;
		case CSSM_ALGID_ASC:
			oid.ReSet(appleAsc_arc);
			break;
		case CSSM_ALGID_FEE_MD5:
			oid.ReSet(appleFeeMD5_arc);
			break;
		case CSSM_ALGID_FEE_SHA1:
			oid.ReSet(appleFeeSHA1_arc);
			break;
		case CSSM_ALGID_FEED:
			oid.ReSet(appleFeed_arc);
			break;
		case CSSM_ALGID_FEEDEXP:
			oid.ReSet(appleFeedExp_arc);
			break;
		case CSSM_ALGID_SHA1WithECDSA:
			oid.ReSet(appleECDSA_arc);
			break;
		/* etc. */
		default:
			errorLog1("cssmAlgToSnaccOid: unknown alg (%d)\n", (int)cssmAlg);
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
}

/* set up a encoded NULL for AlgorithmIdentifier.parameters */
void CL_nullAlgParams(
	AlgorithmIdentifier	&snaccAlgId)
{
	snaccAlgId.parameters = new AsnAny;
	char encodedNull[2] = {NULLTYPE_TAG_CODE, 0};
	CSM_Buffer *cbuf = new CSM_Buffer(encodedNull, 2);
	snaccAlgId.parameters->value = cbuf;
}

/* AsnOcts --> CSSM_DATA */
void CL_AsnOctsToCssmData(
	const AsnOcts 	&octs,
	CSSM_DATA		&cdata,
	CssmAllocator	&alloc)
{
	const char *cp = octs;
	CssmAutoData aData(alloc, (uint8 *)cp, octs.Len());
	cdata = aData.release();
}

#define MAX_NAME_SIZE	(4 * 1024)

/* snacc-style GeneralNames --> CE_GeneralNames */
/* GeneralNames from sm_x509cmn.h */
void CL_snaccGeneralNamesToCdsa(
	GeneralNames &snaccObj,
	CE_GeneralNames &cdsaObj,
	CssmAllocator &alloc)
{
	cdsaObj.numNames = snaccObj.Count();
	if(cdsaObj.numNames == 0) {
		cdsaObj.generalName = NULL;
		return;
	}
	cdsaObj.generalName = (CE_GeneralName *)alloc.malloc(
		cdsaObj.numNames * sizeof(CE_GeneralName));
	snaccObj.SetCurrToFirst();
	CssmAutoData aData(alloc);
	for(unsigned i=0; i<cdsaObj.numNames; i++) {
		CE_GeneralName *currCdsaName = &cdsaObj.generalName[i];
		GeneralName *currSnaccName = snaccObj.Curr();
		
		/* just take the simple ones for now */
		char *src = NULL;
		unsigned len = 0;
		AsnType *toBeEncoded = NULL;
		switch(currSnaccName->choiceId) {
			case GeneralName::otherNameCid:
				/* OTHER_NAME, AsnOid */
				currCdsaName->nameType = GNT_OtherName;
				src = *currSnaccName->otherName;
				len = currSnaccName->otherName->Len();
				break;
			case GeneralName::rfc822NameCid:
				/* IA5String, AsnOcts */
				currCdsaName->nameType = GNT_RFC822Name;
				src = *currSnaccName->rfc822Name;
				len = currSnaccName->rfc822Name->Len();
				break;
			case GeneralName::dNSNameCid:
				/* IA5String, AsnOcts */
				currCdsaName->nameType = GNT_DNSName;
				src = *currSnaccName->dNSName;
				len = currSnaccName->dNSName->Len();
				break;
			case GeneralName::x400AddressCid:
				/* ORAddress from sm_x411mtsas */
				currCdsaName->nameType = GNT_X400Address;
				toBeEncoded = currSnaccName->x400Address;
				break;
			case GeneralName::directoryNameCid:
				/* Name from sm_x501if */
				/* We actually have to to deal with this in CertFields.cpp;
				 * it'll be easy to support this (with a mod to 
				 * CE_GeneralName).
				 */
				currCdsaName->nameType = GNT_DirectoryName;
				toBeEncoded = currSnaccName->directoryName;
				break;
			case GeneralName::ediPartyNameCid:
				/* EDIPartyName from sm_x509cmn */
				currCdsaName->nameType = GNT_EdiPartyName;
				toBeEncoded = currSnaccName->ediPartyName;
				break;
			case GeneralName::uniformResourceIdentifierCid:
				/* IA5String, AsnOcts */
				currCdsaName->nameType = GNT_URI;
				src = *currSnaccName->uniformResourceIdentifier;
				len = currSnaccName->uniformResourceIdentifier->Len();
				break;
			case GeneralName::iPAddressCid:
				/* AsnOcts */
				currCdsaName->nameType = GNT_IPAddress;
				src = *currSnaccName->iPAddress;
				len = currSnaccName->iPAddress->Len();
				break;
			case GeneralName::registeredIDCid:
				/* AsnOid */
				currCdsaName->nameType = GNT_RegisteredID;
				src = *currSnaccName->registeredID;
				len = currSnaccName->registeredID->Len();
				break;
		}
		if(src == NULL) {
			/* punt - encode the complex object and give caller the encoded
			 * bytes */
			CASSERT(toBeEncoded != NULL);
			SC_encodeAsnObj(*toBeEncoded, aData, MAX_NAME_SIZE);
			src = aData;
			len = aData.length();
			aData.release();
			currCdsaName->berEncoded = CSSM_TRUE;
		}
		else {
			CASSERT(toBeEncoded == NULL);
			currCdsaName->berEncoded = CSSM_FALSE;
		}
		
		/* src --> currCdsaName->name */
		currCdsaName->name.Data = (uint8 *)alloc.malloc(len);
		currCdsaName->name.Length = len;
		memmove(currCdsaName->name.Data, src, len);
		
		snaccObj.GoNext();
	}
}

/* CE_GeneralNames --> snacc-style GeneralNames */
/* GeneralNames from sm_x509cmn.h */
GeneralNames *CL_cdsaGeneralNamesToSnacc(
	CE_GeneralNames &cdsaObj)
{
	GeneralNames *snaccObj = new GeneralNames;
	bool abortFlag = false;		// true --> invalid incoming field 
	CssmAllocator &alloc = CssmAllocator::standard();
	
	for(unsigned i=0; i<cdsaObj.numNames; i++) {
		CE_GeneralName *currCdsaName = &cdsaObj.generalName[i];
		char *rawData = reinterpret_cast<char *>(currCdsaName->name.Data);
		unsigned rawDataLen = currCdsaName->name.Length;
		GeneralName *currSnaccName = snaccObj->Append();
		CssmData &berCdata = CssmData::overlay(currCdsaName->name);
		CssmRemoteData berData(alloc, berCdata);
		switch(currCdsaName->nameType) {
			case GNT_OtherName:
				/* OTHER_NAME, AsnOid */
				if(currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::otherNameCid;
				currSnaccName->otherName = new AsnOid(rawData, rawDataLen);
				break;

			case GNT_RFC822Name:
				/* IA5String */
				if(currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::rfc822NameCid;
				currSnaccName->rfc822Name = new IA5String(rawData, rawDataLen);
				break;
			case GNT_DNSName:
				/* IA5String */
				if(currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::dNSNameCid;
				currSnaccName->rfc822Name = new IA5String(rawData, rawDataLen);	
				break;
			
			case GNT_X400Address:
				/* ORAddress from sm_x411mtsas */
				if(!currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::x400AddressCid;
				currSnaccName->x400Address = new ORAddress;
				try {
					SC_decodeAsnObj(berData, *currSnaccName->x400Address);
				}
				catch(...) {
					abortFlag = true;
				}
				break;
			case GNT_DirectoryName:
				/* Name from sm_x501if */
				/* We actually have to to deal with this in CertFields.cpp;
				 * it'll be easy to support this (with a mod to 
				 * CE_GeneralName).
				 */
				if(!currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::directoryNameCid;
				currSnaccName->directoryName = new Name;
				try {
					SC_decodeAsnObj(berData, *currSnaccName->directoryName);
				}
				catch(...) {
					abortFlag = true;
				}
				break;
				
			case GNT_EdiPartyName:
				/* EDIPartyName from sm_x509cmn */
				if(!currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::ediPartyNameCid;
				currSnaccName->ediPartyName = new EDIPartyName;
				try {
					SC_decodeAsnObj(berData, *currSnaccName->ediPartyName);
				}
				catch(...) {
					abortFlag = true;
				}
				break;

			case GNT_URI:
				/* IA5String */
				if(currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::uniformResourceIdentifierCid;
				currSnaccName->uniformResourceIdentifier = 
					new IA5String(rawData, rawDataLen);
				break;

			case GNT_IPAddress:
				/* AsnOcts */
				if(currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::iPAddressCid;
				currSnaccName->iPAddress = new AsnOcts(rawData, rawDataLen);
				break;
			case GNT_RegisteredID:
				/* AsnOid */
				if(currCdsaName->berEncoded) {
					abortFlag = true;
					break;
				}
				currSnaccName->choiceId = GeneralName::registeredIDCid;
				currSnaccName->registeredID = new AsnOid(rawData, rawDataLen);
				break;
		}
		berData.release();
		if(abortFlag) {
			break;
		}
	}
	if(abortFlag) {
		delete snaccObj;
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	return snaccObj;
}

void CL_normalizeString(
	char *strPtr,
	int &strLen)
{
	char *pCh = strPtr;				// working ptr
	char *pD = pCh;					// start of good string chars
	char *pEos = pCh + strLen - 1;
	
	if(strLen == 0) {
		return;
	}

	/* adjust if Length included NULL terminator */
	while(*pEos == 0) {
		pEos--;
	}
	
	/* Remove trailing spaces */
	while(isspace(*pEos)) {
		pEos--;
	}
	
	/* Point to one past last non-space character */
	pEos++;

	/* upper case */
	while(pCh < pEos) {
		*pCh++ = toupper(*pCh);
	}
	
	/* clean out whitespace */
	/* 
	 * 1. skip all leading whitespace 
	 */
	pCh = pD;
	while(isspace(*pCh) && (pCh < pEos)) {
		pCh++;
	}
	
	/*
	 * 2. eliminate multiple whitespace.
	 *    pCh points to first non-white char.
	 *	  pD still points to start of string
	 */
	char ch;
	while(pCh < pEos) {
		ch = *pCh++;
		*pD++ = ch;		// normal case
		if( isspace(ch) ){
			/* skip 'til next nonwhite */
			while(isspace(*pCh) && (pCh < pEos)) {
				pCh++;
			}
		}
	};

	strLen = pD - strPtr;
}

/* 
 * Normalize an RDN. Per RFC2459 (4.1.2.4), printable strings are case 
 * insensitive and we're supposed to ignore leading and trailing 
 * whitespace, and collapse multiple whitespace characters into one. 
 */
void CL_normalizeX509Name(
	Name &name,
	CssmAllocator &alloc)
{
	RDNSequence *rdns = name.rDNSequence;
	int numRdns = rdns->Count();
	if((rdns == NULL) || (numRdns == 0)) {
		/* not technically an error */
		return;
	}
	
	rdns->SetCurrElmt(0);
	for(int rdnDex=0; rdnDex<numRdns; rdnDex++) {
		/* from sm_x501if */
		RelativeDistinguishedName *rdn = rdns->Curr();
		if(rdn == NULL) {
			/* not sure how this can happen... */
			dprintf1("clNormalizeX509Name: NULL rdn at index %d\n", rdnDex);
			rdns->GoNext();	
			continue;
		}
		int numAttrs = rdn->Count();
		if(numAttrs == 0) {
			dprintf1("clNormalizeX509Name: zero numAttrs at index %d\n", rdnDex);
			rdns->GoNext();		
			continue;
		}
		
		/* descend into array of attribute/values */
		rdn->SetCurrElmt(0);
		for(int attrDex=0; attrDex<numAttrs; attrDex++) {
			/* from sm_x501if */
			AttributeTypeAndDistinguishedValue *att = rdn->Curr();
			if(att == NULL) {
				/* not sure how this can happen... */
				dprintf1("clNormalizeX509Name: NULL att at index %d\n", attrDex);
				rdn->GoNext();
				continue;
			}
			
			/* 
			 * att->value is an AsnAny (CSM_Buffer) containing an encoded
			 * string - supposedly a DirectoryString, but some certs put an
			 * IA5String here which is not handled by DirectoryString.
			 *
			 * (See e.g. the Thawte serverbasic cert, which has an email 
			 * address in IA5String format.) In the IA5String case we skip the
			 * normalization.
			 *
			 * Anyway, figure out what's there, snag the raw string, normalize the 
			 * string, cook up an appropriate DirectoryString for it, encode the
			 * result, and put the encoding back in att->value.
			 */
			CSM_Buffer				*cbuf = att->value.value;
			DirectoryString			dirStr;
			char 					*cbufData = const_cast<char *>(cbuf->Access());
			CssmData				encodedStr(cbufData, cbuf->Length());
			
			/* avoid exception if this is an IA5String... */
			char tagByte = cbufData[0];
			if((tagByte == (UNIV | PRIM | IA5STRING_TAG_CODE)) ||
			   (tagByte == (UNIV | CONS | IA5STRING_TAG_CODE))) {
				/* can't normalize */
				return;
			}
			try {
				SC_decodeAsnObj(encodedStr, dirStr);
			}
			catch (...) {
				/* can't normalize */
				errorLog0("clNormalizeX509Name: malformed DirectoryString (1)\n");
				return;
			} 

			/* normalize, we don't need to know what kind of string it is */
			char *strPtr = *dirStr.teletexString;
			int newLen = dirStr.teletexString->Len();
			CL_normalizeString(strPtr, newLen);
			
			/* set new AsnOcts data from normalized version, freeing old */
			dirStr.teletexString->ReSet(strPtr, newLen);
			
			/* encode result */
			CssmAutoData normEncoded(alloc);
			SC_encodeAsnObj(dirStr, normEncoded, newLen + 8);
			
			/* set new AsnAny data */
			cbuf->Set((char *)normEncoded.data(), normEncoded.length());
			
			rdn->GoNext();
		}	/* for each attribute/value */
	rdns->GoNext();
	}		/* for each RDN */
}


