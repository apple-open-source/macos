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
 * CertFields.cpp - convert between snacc-based Certificate components and CDSA-style
 *                  fields. A major component of DecodedCert.
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 *
 * The code in this file is dreadfully gross. There is no practical way to do this
 * work (converting between C++ snacc types and C CSDA types) without the kind
 * of brute force code you see here. 
 */

#include "DecodedCert.h"
#include "cldebugging.h"
#include "CertBuilder.h"
#include "CLCertExtensions.h"
#include "SnaccUtils.h"
#include <Security/utilities.h>
#include <Security/oidscert.h>
#include <Security/cssmerr.h>
#include <Security/x509defs.h>
#include <Security/cdsaUtils.h>

/*
 * Routines for common validity checking for certificateToSign fields.
 *
 * Call from setField*: verify field isn't already set, optionally validate
 * input length
 */
static void tbsSetCheck(
	void				*fieldToSet,
	const CssmData		&fieldValue,
	uint32				expLength,
	const char			*op)
{
	if(fieldToSet != NULL) {						
		/* can't add another */
		errorLog1("setField(%s): field already set\n", op);
		CssmError::throwMe(CSSMERR_CL_INVALID_NUMBER_OF_FIELDS);		
	}										
	if((expLength != 0) && (fieldValue.length() != expLength)) {		
		errorLog3("setField(%s): bad length : exp %d got %d\n", 
			op, (int)expLength, (int)fieldValue.length());
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);			
	}
}

/*
 * Call from getField* for unique fields - detect missing field or index out of bounds.
 */
static bool tbsGetCheck(
	void		*requiredField,
	uint32		reqIndex)
{
	if((requiredField == NULL) ||  (reqIndex != 0)) {
		return false;
	}
	else {
		return true;
	}
}

/***
 *** Version
 *** Format = DER-encoded int (max of four bytes in this case)
 ***/
static bool getField_Version (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->version, index)) {
		return false;
	}

	/* cook up big-endian char array representation */
	int ivers = *cert.certificateToSign->version;
	uint32 uvers = static_cast<uint32>(ivers);
	uint8 chars[sizeof(uint32)];
	for(uint32 i=0; i<sizeof(uint32); i++) {
		chars[sizeof(uint32) - 1 -i] = (uint8)uvers;
		uvers >>= 8; 
	}
	fieldValue.copy(chars, sizeof(uint32));
	numFields = 1;
	return true;
}

static void setField_Version (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	tbsSetCheck(cert.certificateToSign->version, fieldValue, 0, "version");
	
	/* get big-endian int from *fieldValue.Data */
	if(fieldValue.length() > sizeof(unsigned)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);			
	}
	uint32 vers = 0;
	uint8 *cp = fieldValue;
	for(unsigned i=0; i<fieldValue.length(); i++) {
		vers <<= 8;
		vers |= cp[i];
	}
	cert.certificateToSign->version = new Version((int)vers);
	cert.certificateToSign->version->Set((int)vers);
}


#if	this_is_a_template
/***
 *** Version
 *** Format = DER-encoded int (always four bytes in this case)
 ***/
static bool getField_Version (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	tbsGetCheck(cert.certificateToSign->version, index);
}
static void setField_Version (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	tbsSetCheck(cert.certificateToSign->version, fieldValue, sizeof(uint32),
		"version");

}
static void freeField_Version (
	CssmOwnedData		&fieldValue)
{
}
#endif

/***
 *** Serial Number
 *** Format = DER-encoded int, variable length
 ***/
static bool getField_SerialNumber (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index > 0) {
		return false;
	}
	
	char *cp = cert.certificateToSign->serialNumber;	
	uint32 len = cert.certificateToSign->serialNumber.Len();
	fieldValue.copy(cp, len);
	numFields = 1;
	return true;
}

static void setField_SerialNumber (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	cert.certificateToSign->serialNumber.Set(fieldValue, fieldValue.Length);
}

/***
 *** Issuer Name, Subject Name (C struct version)
 *** Format = CSSM_X509_NAME
 *** class Name from sm_x501if
 ***/

/* first, the common code */
static bool getField_RDN (
	const Name 			&name,
	uint32				&numFields,		// RETURNED (if successful, 0 or 1)
	CssmOwnedData		&fieldValue)	// RETURNED
{
	RDNSequence *rdns = name.rDNSequence;
	int numRdns = rdns->Count();
	if((rdns == NULL) || (numRdns == 0)) {
		/* not technically an error */
		return false;
	}
	
	/* alloc top-level CSSM_X509_NAME and its RelativeDistinguishedName array */
	CssmAllocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_NAME));
	CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)fieldValue.data();
	memset(x509Name, 0, sizeof(CSSM_X509_NAME));
	x509Name->numberOfRDNs = numRdns;
	x509Name->RelativeDistinguishedName = 
		(CSSM_X509_RDN_PTR)alloc.malloc(sizeof(CSSM_X509_RDN) * numRdns);
	CSSM_X509_RDN_PTR currRdn = x509Name->RelativeDistinguishedName;
	memset(currRdn, 0, sizeof(CSSM_X509_RDN) * numRdns);
	
	rdns->SetCurrElmt(0);
	for(int rdnDex=0; rdnDex<numRdns; rdnDex++) {
		/* from sm_x501if */
		RelativeDistinguishedName *rdn = rdns->Curr();
		if(rdn == NULL) {
			/* not sure how this can happen... */
			dprintf1("getField_RDN: NULL rdn at index %d\n", rdnDex);
			
			/* next snacc RDN but keep CDSA position unchanged */
			rdns->GoNext();				// snacc format
			x509Name->numberOfRDNs--;	// since we're skipping one
			continue;
		}
		int numAttrs = rdn->Count();
		if(numAttrs == 0) {
			dprintf1("getField_RDN: zero numAttrs at index %d\n", rdnDex);
			rdns->GoNext();		
			x509Name->numberOfRDNs--;	// since we're skipping one
			continue;
		}
		
		/* alloc CSSM_X509_TYPE_VALUE_PAIR array for this rdn */
		currRdn->numberOfPairs = numAttrs;
		currRdn->AttributeTypeAndValue = (CSSM_X509_TYPE_VALUE_PAIR_PTR)
			alloc.malloc(sizeof(CSSM_X509_TYPE_VALUE_PAIR) * numAttrs);
		CSSM_X509_TYPE_VALUE_PAIR_PTR currAttr = currRdn->AttributeTypeAndValue;
		memset(currAttr, 0, sizeof(CSSM_X509_TYPE_VALUE_PAIR) * numAttrs);
		
		/* descend into array of attribute/values */
		rdn->SetCurrElmt(0);
		for(int attrDex=0; attrDex<numAttrs; attrDex++) {
			/* from sm_x501if */
			AttributeTypeAndDistinguishedValue *att = rdn->Curr();
			if(att == NULL) {
				/* not sure how this can happen... */
				dprintf1("getField_RDN: NULL att at index %d\n", attrDex);
				rdn->GoNext();
				currRdn->numberOfPairs--;
				continue;
			}

			/*
			 * Convert snacc-style AttributeTypeAndDistinguishedValue to
			 * CSSM-style CSSM_X509_TYPE_VALUE_PAIR
			 *
			 * Hopefully 'value' is one of the types defined in DirectoryString,
			 * defined in sm_x520sa. Some certs use IA5String, which is not
			 * technically legal and is not handled by DirectoryString, so
			 * we have to handle that ourself. See e.g. the Thawte serverbasic 
			 * cert, which has an email address in IA5String format. 
			 */
			CSM_Buffer				*cbuf = att->value.value;
			AsnBuf					buf;
			AsnLen					len = cbuf->Length();
			AsnTag 					tag;
			AsnLen 					elmtLen;
			ENV_TYPE 				env;
			int						val;
			char					*valData;
			int						valLength;
			DirectoryString			*dirStr = NULL;
			
			buf.InstallData(cbuf->Access(), len);
			if ((val = setjmp (env)) == 0) {
				tag = BDecTag (buf, len, env);
				elmtLen = BDecLen (buf, len, env);
			}
			else {
				errorLog0("getField_RDN: malformed DirectoryString (1)\n");
				/* FIXME - throw? Discard the whole cert? What? */
				rdn->GoNext();
				currRdn->numberOfPairs--;
				continue;
			}

			/* current buf ptr is at the string value's contents. */
			if((tag == MAKE_TAG_ID (UNIV, PRIM, IA5STRING_TAG_CODE)) ||
			   (tag == MAKE_TAG_ID (UNIV, CONS, IA5STRING_TAG_CODE))) {
					/* any other printable types not handled by DirectoryString here */
					valData = buf.DataPtr();
					valLength = buf.DataLen();
					// workaround
					delete dirStr;
					dirStr = NULL;
			}
			else {
				/* from sm_x520sa.h */
				AsnLen dec;
				dirStr = new DirectoryString;
				if((val = setjmp (env)) == 0) {
					dirStr->BDecContent(buf, tag, elmtLen, dec, env);
				}
				else {
					errorLog0("getField_RDN: malformed DirectoryString (1)\n");
					/* FIXME - throw? Discard the whole cert? What? */
					rdn->GoNext();
					currRdn->numberOfPairs--;
					continue;
				}
				AsnOcts *octs = NULL;
				switch(dirStr->choiceId) {
					case DirectoryString::printableStringCid:
						octs = dirStr->printableString;
						break;
					case DirectoryString::teletexStringCid:
						octs = dirStr->teletexString;
						break;
					case DirectoryString::universalStringCid:
						octs = dirStr->universalString;
						break;
					case DirectoryString::bmpStringCid:
						octs = dirStr->bmpString;
						break;
					case DirectoryString::utf8StringCid:
						octs = dirStr->utf8String;
						break;
					default:
						/* should never happen unless DirectoryString changes */
						errorLog1("getField_RDN: Bad DirectoryString::choiceId (%d)\n",
							(int)dirStr->choiceId);
						CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
				}
				valData = *octs;
				valLength = octs->Len();
			}	/* normal DirectoryString */
			
			/* OK, set up outgoing CSSM_X509_TYPE_VALUE_PAIR */
			CssmOid &oid = CssmOid::overlay(currAttr->type);
			CL_snaccOidToCssm(att->type, oid, alloc);
			currAttr->valueType = tag >> 24;
			currAttr->value.Data = (uint8 *)alloc.malloc(valLength);
			currAttr->value.Length = valLength;
			memcpy(currAttr->value.Data, valData, valLength);
			
			rdn->GoNext();	// snacc format
			currAttr++;		// CDSA format
			delete dirStr;
		}	/* for eact attr in rdn */

		rdns->GoNext();		// snacc format
		currRdn++;			// CDSA format
	}	/* for each rdn in rdns */
	numFields = 1;
	return true;
}

static void setField_RDN  (
	NameBuilder			&name,
	const CssmData		&fieldValue)
{
	/*
	 * The main job here is extracting attr/value pairs in CSSM format 
	 * from fieldData, and converting them into arguments for NameBuilder.addATDV.
	 * Note that we're taking the default for primaryDistinguished,
	 * because the CDSA CSSM_X509_TYPE_VALUE_PAIR struct doesn't allow for
	 * it. 
	 */
	CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)fieldValue.data();
	for(unsigned rdnDex=0; rdnDex<x509Name->numberOfRDNs; rdnDex++) {
		CSSM_X509_RDN_PTR rdn = &x509Name->RelativeDistinguishedName[rdnDex];
		if(rdn->numberOfPairs != 1) {
			errorLog0("setField_RDN: only one a/v pair per RDN supported\n");
			CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
		}

		CSSM_X509_TYPE_VALUE_PAIR_PTR atv = rdn->AttributeTypeAndValue;
		AsnOid oid;
		oid.Set(reinterpret_cast<char *>(atv->type.Data), atv->type.Length);
		
		DirectoryString::ChoiceIdEnum stringType;
		switch(atv->valueType) {
			case BER_TAG_T61_STRING:
				stringType = DirectoryString::teletexStringCid;
				break;
			case BER_TAG_PRINTABLE_STRING:
				stringType = DirectoryString::printableStringCid;
				break;
			case BER_TAG_PKIX_UNIVERSAL_STRING:
				stringType = DirectoryString::universalStringCid;
				break;
			case BER_TAG_PKIX_BMP_STRING:
				stringType = DirectoryString::bmpStringCid;
				break;
			case BER_TAG_PKIX_UTF8_STRING:
				stringType = DirectoryString::utf8StringCid;
				break;
			default:
				errorLog1("setField_RDN: illegal tag(%d)\n", atv->valueType);
				CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
		}
		name.addATDV(oid,
			reinterpret_cast<char *>(atv->value.Data),
			atv->value.Length,
			stringType);

	}
}

/* common for issuer and subject */
static void freeField_RDN  (
	CssmOwnedData		&fieldValue)
{
	if(fieldValue.data() == NULL) {
		return;
	}
	if(fieldValue.length() != sizeof(CSSM_X509_NAME)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	CssmAllocator &alloc = fieldValue.allocator;
	CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)fieldValue.data();
	for(unsigned rdnDex=0; rdnDex<x509Name->numberOfRDNs; rdnDex++) {
		CSSM_X509_RDN_PTR rdn = &x509Name->RelativeDistinguishedName[rdnDex];
		for(unsigned atvDex=0; atvDex<rdn->numberOfPairs; atvDex++) {
			CSSM_X509_TYPE_VALUE_PAIR_PTR atv = &rdn->AttributeTypeAndValue[atvDex];
			alloc.free(atv->type.Data);
			alloc.free(atv->value.Data);
			memset(atv, 0, sizeof(CSSM_X509_TYPE_VALUE_PAIR));
		}
		alloc.free(rdn->AttributeTypeAndValue);
		memset(rdn, 0, sizeof(CSSM_X509_RDN));
	}
	alloc.free(x509Name->RelativeDistinguishedName);
	memset(x509Name, 0, sizeof(CSSM_X509_NAME));
	
	/* top-level x509Name pointer freed by freeCertFieldData() */
}

/*** issuer ***/
static bool getField_Issuer (
	const DecodedCert	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	bool brtn;
	
	if(!tbsGetCheck(cert.certificateToSign->issuer, index)) {
		return false;
	}
	try {
		brtn = getField_RDN(*cert.certificateToSign->issuer, numFields, fieldValue);
	}
	catch (...) {
		freeField_RDN(fieldValue);
		throw;
	}
	return brtn;
}

static void setField_Issuer  (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	tbsSetCheck(cert.certificateToSign->issuer, fieldValue, sizeof(CSSM_X509_NAME),
		"IssuerName");
	NameBuilder *issuer = new NameBuilder;
	cert.certificateToSign->issuer = issuer;
	setField_RDN(*issuer, fieldValue);
}

/*** subject ***/
static bool getField_Subject (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->subject, index)) {
		return false;
	}
	bool brtn;
	try {
		brtn = getField_RDN(*cert.certificateToSign->subject, numFields, fieldValue);
	}
	catch (...) {
		freeField_RDN(fieldValue);
		throw;
	}
	return brtn;
}

static void setField_Subject  (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	tbsSetCheck(cert.certificateToSign->subject, fieldValue, sizeof(CSSM_X509_NAME),
		"SubjectName");
	NameBuilder *subject = new NameBuilder;
	cert.certificateToSign->subject = subject;
	setField_RDN(*subject, fieldValue);
}

/***
 *** Issuer Name, Subject Name (normalized and encoded version)
 *** Format = CSSM_DATA containing the DER encoding of the normalized name
 *** class Name from sm_x501if
 ***/

/* first, the common code */
static bool getField_normRDN (
	const Name 			&name,
	uint32				&numFields,		// RETURNED (if successful, 0 or 1)
	CssmOwnedData		&fieldValue)	// RETURNED
{
	/*
	 * First step is to make a copy of the existing name. The easiest way to do 
	 * this is to encode and decode.	
	 */
	CssmAllocator &alloc = fieldValue.allocator;
	CssmAutoData encodedName1(alloc);
	/* FIXME - should SC_encodeAsnObj() take a const AsnType & ? */
	SC_encodeAsnObj(const_cast<Name &>(name), encodedName1, MAX_RDN_SIZE);
	Name decodedName;
	SC_decodeAsnObj(encodedName1, decodedName);
	
	/* normalize */
	CL_normalizeX509Name(decodedName, alloc);
	
	/* encode result */
	SC_encodeAsnObj(decodedName, fieldValue, MAX_RDN_SIZE);
	numFields = 1;
	return true;
}

static bool getFieldSubjectNorm(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->subject, index)) {
		return false;
	}
	return getField_normRDN(*cert.certificateToSign->subject, numFields, fieldValue);
}

static bool getFieldIssuerNorm(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->issuer, index)) {
		return false;
	}
	return getField_normRDN(*cert.certificateToSign->issuer, numFields, fieldValue);
}


/***
 *** TBS AlgId, Signature AlgId
 *** Format = CSSM_X509_ALGORITHM_IDENTIFIER
 ***
 *** common code:
 ***/
static void getField_AlgId (
	const AlgorithmIdentifier 	*snaccAlgId,
	CssmOwnedData				&fieldValue)	// RETURNED
{
	CssmAllocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_ALGORITHM_IDENTIFIER));
	CSSM_X509_ALGORITHM_IDENTIFIER *cssmAlgId = 
		(CSSM_X509_ALGORITHM_IDENTIFIER *)fieldValue.data();
	CL_snaccAlgIdToCssm (*snaccAlgId, *cssmAlgId, alloc);
}

static void setField_AlgId (
	AlgorithmIdentifier *snaccAlgId,
	const CssmData		&fieldValue)
{
	CSSM_X509_ALGORITHM_IDENTIFIER *cssmAlgId = 
		(CSSM_X509_ALGORITHM_IDENTIFIER *)fieldValue.data();
	if(cssmAlgId->algorithm.Data == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	CL_cssmAlgIdToSnacc(*cssmAlgId, *snaccAlgId);
}

static void freeField_AlgId (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_ALGORITHM_IDENTIFIER *cssmAlgId = 
		(CSSM_X509_ALGORITHM_IDENTIFIER *)fieldValue.data();
	if(cssmAlgId == NULL) {
		return;
	}
	if(fieldValue.length() != sizeof(CSSM_X509_ALGORITHM_IDENTIFIER)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	CssmAllocator &alloc = fieldValue.allocator;
	alloc.free(cssmAlgId->algorithm.Data);
	alloc.free(cssmAlgId->parameters.Data);
	memset(cssmAlgId, 0, sizeof(CSSM_X509_ALGORITHM_IDENTIFIER));
}


/* TBS AlgId */
static bool getField_TbsAlgId (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	AlgorithmIdentifier *snaccAlgId = cert.certificateToSign->signature;
	if(!tbsGetCheck(snaccAlgId, index)) {
		return false;
	}
	getField_AlgId(snaccAlgId, fieldValue);
	numFields = 1;
	return true;
}

static void setField_TbsAlgId (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	tbsSetCheck(cert.certificateToSign->signature, fieldValue, 
		sizeof(CSSM_X509_ALGORITHM_IDENTIFIER), "TBS_AlgId");
	AlgorithmIdentifier *snaccAlgId = new AlgorithmIdentifier;
	cert.certificateToSign->signature = snaccAlgId;
	setField_AlgId(snaccAlgId, fieldValue);
}

/* Cert AlgId - read only */
static bool getField_CertAlgId (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	AlgorithmIdentifier *snaccAlgId = cert.algorithmIdentifier;
	if(!tbsGetCheck(snaccAlgId, index)) {
		return false;
	}
	getField_AlgId(snaccAlgId, fieldValue);
	numFields = 1;
	return true;
}

/***
 *** Validity not before, not after
 *** Format: CSSM_X509_TIME
 ***/

/*** common code ***/
static void getField_Time (
	const Time 		*snaccTime,
	CssmOwnedData	&fieldValue)	// RETURNED
{
	CssmAllocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_TIME));
	CSSM_X509_TIME *cssmTime = 
		(CSSM_X509_TIME *)fieldValue.data();
	memset(cssmTime, 0, sizeof(CSSM_X509_TIME));

	char *timeStr = NULL;
	int timeStrLen = 0;
	switch(snaccTime->choiceId) {
		case Time::utcTimeCid:
			cssmTime->timeType = BER_TAG_UTC_TIME;
			timeStr = *snaccTime->utcTime;		// an AsnOct
			timeStrLen = snaccTime->utcTime->Len();
			break;
		case Time::generalizedTimeCid:
			timeStr = *snaccTime->generalizedTime;		// an AsnOct
			timeStrLen = snaccTime->generalizedTime->Len();
			cssmTime->timeType = BER_TAG_GENERALIZED_TIME;
			break;
		default:
			/* snacc error, should never happen */
			cssmTime->timeType = BER_TAG_OCTET_STRING;
			timeStr = *snaccTime->generalizedTime;		// an AsnOct
			timeStrLen = snaccTime->generalizedTime->Len();
			break;
	}

	cssmTime->time.Data = reinterpret_cast<uint8 *>(alloc.malloc(timeStrLen));
	cssmTime->time.Length = timeStrLen;
	memcpy(cssmTime->time.Data, timeStr, timeStrLen);
}

static void setField_Time (
	Time 			*snaccTime,
	const CssmData	&fieldValue)
{
	CSSM_X509_TIME *cssmTime = 
		(CSSM_X509_TIME *)fieldValue.data();
	const char *tStr = reinterpret_cast<const char *>(cssmTime->time.Data);
	size_t tLen = cssmTime->time.Length;
	
	switch(cssmTime->timeType) {
		case BER_TAG_GENERALIZED_TIME:
			snaccTime->choiceId = Time::generalizedTimeCid;
			snaccTime->generalizedTime = new GeneralizedTime(tStr, tLen);
			break;
		case BER_TAG_UTC_TIME:
			snaccTime->choiceId = Time::utcTimeCid;
			snaccTime->utcTime = new UTCTime(tStr, tLen);
			break;
		default:
			errorLog1("setField_Time: bad time tag (%d)\n", cssmTime->timeType);
			CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
}

static void freeField_Time (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_TIME *cssmTime = (CSSM_X509_TIME *)fieldValue.data();
	if(cssmTime == NULL) {
		return;
	}
	if(fieldValue.length() != sizeof(CSSM_X509_TIME)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	fieldValue.allocator.free(cssmTime->time.Data);
	memset(cssmTime, 0, sizeof(CSSM_X509_TIME));
}

/*** not before ***/
static bool getField_NotBefore (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->validity, index)) {
		return false;
	}
	if(cert.certificateToSign->validity->notBefore == NULL) {
		return false;
	}
	getField_Time(cert.certificateToSign->validity->notBefore, fieldValue);
	numFields = 1;
	return true;
}

static void setField_NotBefore (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	/* anything could need mallocing except TBS */
	if(cert.certificateToSign->validity == NULL) {
		cert.certificateToSign->validity = new Validity;
	}
	tbsSetCheck(cert.certificateToSign->validity->notBefore, fieldValue, 
		sizeof(CSSM_X509_TIME), "NotBefore");
	cert.certificateToSign->validity->notBefore = new Time;
	setField_Time(cert.certificateToSign->validity->notBefore, fieldValue);
}

/*** not after ***/
static bool getField_NotAfter (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->validity, index)) {
		return false;
	}
	if(cert.certificateToSign->validity->notAfter == NULL) {
		return false;
	}
	getField_Time(cert.certificateToSign->validity->notAfter, fieldValue);
	numFields = 1;
	return true;
}

static void setField_NotAfter (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	/* anything could need mallocing except TBS */
	if(cert.certificateToSign->validity == NULL) {
		cert.certificateToSign->validity = new Validity;
	}
	tbsSetCheck(cert.certificateToSign->validity->notAfter, fieldValue, 
		sizeof(CSSM_X509_TIME), "NotAfter");
	cert.certificateToSign->validity->notAfter = new Time;
	setField_Time(cert.certificateToSign->validity->notAfter, fieldValue);
}

/***
 *** Subject/issuer unique ID
 *** Format: Raw bytes. It's stored in the cert as an ASN bit string; the decoded
 *** bytes are present at this level (i.e., not tag and length in the bytes). 
 *** NOTE: this is not quite accurate in that we only provide byte-aligned size,
 *** not bit-aligned. This field is rarely if ever used so I think it's O, but
 *** beware.
 ***/
static bool getField_SubjectUniqueId (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	UniqueIdentifier *id = cert.certificateToSign->subjectUniqueIdentifier;
	if(!tbsGetCheck(id, index)) {
		return false;
	}
	SC_asnBitsToCssmData(*id, fieldValue);
	numFields = 1;
	return true;
}

static void setField_SubjectUniqueId (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	tbsSetCheck(cert.certificateToSign->subjectUniqueIdentifier, fieldValue, 0,
		"SubjectUniqueID");
	cert.certificateToSign->subjectUniqueIdentifier = new UniqueIdentifier(
		reinterpret_cast<char * const>(fieldValue.Data), fieldValue.Length * 8);
}

static bool getField_IssuerUniqueId (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	UniqueIdentifier *id = cert.certificateToSign->issuerUniqueIdentifier;
	if(!tbsGetCheck(id, index)) {
		return false;
	}
	SC_asnBitsToCssmData(*id, fieldValue);
	numFields = 1;
	return true;
}

static void setField_IssuerUniqueId (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	tbsSetCheck(cert.certificateToSign->issuerUniqueIdentifier, fieldValue, 0,
		"IssuerniqueID");
	cert.certificateToSign->issuerUniqueIdentifier = new UniqueIdentifier(
		reinterpret_cast<char * const>(fieldValue.Data), fieldValue.Length * 8);
}

/***
 *** Public key info
 *** Format = CSSM_X509_SUBJECT_PUBLIC_KEY_INFO
 ***/
static bool getField_PublicKeyInfo (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->subjectPublicKeyInfo, index)) {
		return false;
	}
	SubjectPublicKeyInfo *snaccKeyInfo = cert.certificateToSign->subjectPublicKeyInfo;
	AlgorithmIdentifier *snaccAlgId = snaccKeyInfo->algorithm;
	if(snaccAlgId == NULL) {
		errorLog0("getField_PublicKeyInfo: cert has pubKeyInfo but no algorithm!\n");
		return false;
	}
	CssmAllocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO));
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *cssmKeyInfo = 
		(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)fieldValue.data();
	memset(cssmKeyInfo, 0, sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO));
	CL_snaccAlgIdToCssm(*snaccAlgId, cssmKeyInfo->algorithm, alloc);
	
	/* 
	 * key info - the actual public key blob - is stored in the cert as a bit string;
	 * snacc will give us the actual bits which are invariably yet another DER
	 * encoding (e.g., PKCS1 for RSA public keys). 
	 */
	size_t keyLen = (snaccKeyInfo->subjectPublicKey.BitLen() + 7) / 8;
	cssmKeyInfo->subjectPublicKey.Data = (uint8 *)alloc.malloc(keyLen);
	cssmKeyInfo->subjectPublicKey.Length = keyLen;
	memcpy(cssmKeyInfo->subjectPublicKey.Data, 
		   snaccKeyInfo->subjectPublicKey.BitOcts(),
		   keyLen);
	numFields = 1;
	return true;
}

static void setField_PublicKeyInfo (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	/* This fails if setField_PublicKeyStruct has already been called */
	tbsSetCheck(cert.certificateToSign->subjectPublicKeyInfo, fieldValue, 
		sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO), "PubKeyInfo");
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *cssmKeyInfo = 
		(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)fieldValue.Data;
	if((cssmKeyInfo->subjectPublicKey.Data == NULL) ||
	   (cssmKeyInfo->subjectPublicKey.Length == 0)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}

	SubjectPublicKeyInfo *snaccKeyInfo = new SubjectPublicKeyInfo;
	cert.certificateToSign->subjectPublicKeyInfo = snaccKeyInfo;
	snaccKeyInfo->algorithm = new AlgorithmIdentifier;

	/* common code to convert algorithm info (algID and parameters) */
	const CSSM_X509_ALGORITHM_IDENTIFIER *cssmAlgId = &cssmKeyInfo->algorithm;
	CL_cssmAlgIdToSnacc(*cssmAlgId, *snaccKeyInfo->algorithm);

	/* actual public key blob - AsnBits */
	snaccKeyInfo->subjectPublicKey.Set(reinterpret_cast<char *>
		(cssmKeyInfo->subjectPublicKey.Data), 
		cssmKeyInfo->subjectPublicKey.Length);

}
static void freeField_PublicKeyInfo (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *cssmKeyInfo = 
		(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)fieldValue.data();
	if(cssmKeyInfo == NULL) {
		return;
	}
	CssmAllocator &alloc = fieldValue.allocator;
	CSSM_X509_ALGORITHM_IDENTIFIER *algId = &cssmKeyInfo->algorithm;
	alloc.free(algId->algorithm.Data);
	alloc.free(algId->parameters.Data);
	alloc.free(cssmKeyInfo->subjectPublicKey.Data);
	memset(cssmKeyInfo, 0, sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO));}

/***
 *** key info from CSSM_KEY
 *** Format = CSSM_KEY
 ***/
static bool getField_PublicKeyStruct (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(cert.certificateToSign->subjectPublicKeyInfo, index)) {
		return false;
	}
	CSSM_KEY_PTR cssmKey = cert.extractCSSMKey(fieldValue.allocator);
	fieldValue.set(reinterpret_cast<uint8 *>(cssmKey), sizeof(CSSM_KEY));
	numFields = 1;
	return true;
}

static void setField_PublicKeyStruct (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	/* This fails if setField_PublicKeyInfo has already been called */
	tbsSetCheck(cert.certificateToSign->subjectPublicKeyInfo, fieldValue, 
		sizeof(CSSM_KEY), "PubKey");
	CSSM_KEY_PTR cssmKey = (CSSM_KEY_PTR)fieldValue.data();
	if((cssmKey->KeyData.Data == NULL) ||
	   (cssmKey->KeyData.Data == 0)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}

	SubjectPublicKeyInfo *snaccKeyInfo = new SubjectPublicKeyInfo;
	cert.certificateToSign->subjectPublicKeyInfo = snaccKeyInfo;
	snaccKeyInfo->algorithm = new AlgorithmIdentifier;
	CL_cssmAlgToSnaccOid(cssmKey->KeyHeader.AlgorithmId, 
		snaccKeyInfo->algorithm->algorithm);

	/* NULL algorithm paramneters, always in this case */
	CL_nullAlgParams(*snaccKeyInfo->algorithm);
	
	/* actual public key blob - AsnBits */
	/***
	 *** TBD FIXME if this key is a ref key, null wrap it to a raw key
	 ***/
	if(cssmKey->KeyHeader.BlobType != CSSM_KEYBLOB_RAW) {
			errorLog0("CL SetField: must specify RAW key blob\n");
			CssmError::throwMe(CSSM_ERRCODE_INVALID_FIELD_POINTER);
	}
	snaccKeyInfo->subjectPublicKey.Set(reinterpret_cast<char *>
		(cssmKey->KeyData.Data), cssmKey->KeyData.Length * 8);
}

static void freeField_PublicKeyStruct (
	CssmOwnedData		&fieldValue)
{
	CSSM_KEY_PTR cssmKey = (CSSM_KEY_PTR)fieldValue.data();
	DecodedCert::freeCSSMKey(cssmKey, fieldValue.allocator, false);
}

/***
 *** Signature
 *** Format = raw bytes
 *** read-only
 ***/
static bool getField_Signature (
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if((index > 0) || 							// max of one sig
	   (cert.signatureValue.BitLen() == 0)) {	// no sig - must be TBS only
		return false;
	}
	SC_asnBitsToCssmData(cert.signatureValue, fieldValue);
	numFields = 1;
	return true;
}

/***
 *** end of field-specific triplets
 ***/
 
/* setField for read-only OIDs (i.e., the ones in cert, not TBS) */
static void setField_ReadOnly (
	DecodedCert			&cert,
	const CssmData		&fieldValue)
{
	errorLog0("Attempt to set a read-only field\n");
	CssmError::throwMe(CSSMERR_CL_UNKNOWN_TAG);
}

/*
 * Table to map OID to {get,set,free}field
 */
typedef struct {
	const CSSM_OID		*fieldId;
	getFieldFcn			*getFcn;
	setFieldFcn			*setFcn;
	freeFieldFcn		*freeFcn;		// OPTIONAL - NULL means just free the 
										// top-level data
} oidToFieldFuncs;

static const oidToFieldFuncs fieldFuncTable[] = {
	{ 	&CSSMOID_X509V1Version, 
		&getField_Version, &setField_Version, NULL },
	{ 	&CSSMOID_X509V1SerialNumber, 
		&getField_SerialNumber, &setField_SerialNumber, NULL 	},
	{ 	&CSSMOID_X509V1IssuerNameCStruct, 
		&getField_Issuer, &setField_Issuer, &freeField_RDN },
	{ 	&CSSMOID_X509V1SubjectNameCStruct, 
		&getField_Subject, &setField_Subject, &freeField_RDN },
	{	&CSSMOID_X509V1SignatureAlgorithmTBS,
		&getField_TbsAlgId, &setField_TbsAlgId, &freeField_AlgId },
	{	&CSSMOID_X509V1SignatureAlgorithm, 
		&getField_CertAlgId, &setField_ReadOnly, &freeField_AlgId	},
	{	&CSSMOID_X509V1ValidityNotBefore,
		&getField_NotBefore,	&setField_NotBefore,	&freeField_Time },
	{	&CSSMOID_X509V1ValidityNotAfter,
		&getField_NotAfter, &setField_NotAfter, &freeField_Time },
	{	&CSSMOID_X509V1CertificateIssuerUniqueId,
		&getField_IssuerUniqueId, &setField_IssuerUniqueId, NULL },
	{	&CSSMOID_X509V1CertificateSubjectUniqueId,
		&getField_SubjectUniqueId, &setField_SubjectUniqueId, NULL },
	{	&CSSMOID_X509V1SubjectPublicKeyCStruct,
		&getField_PublicKeyInfo, &setField_PublicKeyInfo, &freeField_PublicKeyInfo },
	{	&CSSMOID_CSSMKeyStruct,
		&getField_PublicKeyStruct, &setField_PublicKeyStruct, 
		&freeField_PublicKeyStruct },
	{	&CSSMOID_X509V1Signature,
		&getField_Signature, &setField_ReadOnly, NULL },
	{   &CSSMOID_X509V1IssuerName, 
		getFieldIssuerNorm, &setField_ReadOnly, NULL },
	{   &CSSMOID_X509V1SubjectName, 
		getFieldSubjectNorm, &setField_ReadOnly, NULL },
		
	/* 
	 * Extensions, implemented in CertExtensions.cpp 
	 * When adding new ones, also add to:
	 *   -- oidToSnaccObj() in CertExtensions.cpp
	 *   -- get/set/free functions in CertExtensions.{cpp,h}
	 */
	{	&CSSMOID_KeyUsage, &getFieldKeyUsage, &setFieldKeyUsage, 
	    &freeFieldSimpleExtension },
	{   &CSSMOID_BasicConstraints, &getFieldBasicConstraints, 
	    &setFieldBasicConstraints, &freeFieldSimpleExtension },
	{	&CSSMOID_ExtendedKeyUsage, &getFieldExtKeyUsage,
		&setFieldExtKeyUsage, &freeFieldExtKeyUsage } ,
	{	&CSSMOID_SubjectKeyIdentifier, &getFieldSubjectKeyId,
		&setFieldSubjectKeyId, &freeFieldSubjectKeyId } ,
	{	&CSSMOID_AuthorityKeyIdentifier, &getFieldAuthorityKeyId,
		&setFieldAuthorityKeyId, &freeFieldAuthorityKeyId } ,
	{	&CSSMOID_SubjectAltName, &getFieldSubjAltName,
		&setFieldSubjAltName, &freeFieldSubjAltName } ,
	{	&CSSMOID_CertificatePolicies, &getFieldCertPolicies,
		&setFieldCertPolicies, &freeFieldCertPolicies } ,
	{	&CSSMOID_NetscapeCertType, &getFieldNetscapeCertType,
		&setFieldNetscapeCertType, &freeFieldSimpleExtension } ,
	{   &CSSMOID_X509V3CertificateExtensionCStruct, &getFieldUnknownExt,
		&setFieldUnknownExt, &freeFieldUnknownExt }
};

#define NUM_KNOWN_FIELDS		(sizeof(fieldFuncTable) / sizeof(oidToFieldFuncs))
#define NUM_STD_CERT_FIELDS		13		/* not including extensions */


/* map an OID to an oidToFieldFuncs */
static const oidToFieldFuncs *oidToFields(
	const CssmOid	&fieldId)
{
	const oidToFieldFuncs *funcPtr = fieldFuncTable;
	
	for(unsigned i=0; i<NUM_KNOWN_FIELDS; i++) {
		if(fieldId == CssmData::overlay(*funcPtr->fieldId)) {
			return funcPtr;
		}
		funcPtr++;
	}
	CssmError::throwMe(CSSMERR_CL_UNKNOWN_TAG);
}


/***
 *** Public functions
 ***/

/* 
 * Obtain the index'th occurrence of field specified by fieldId in specified cert.
 * Format of the returned field depends on fieldId.
 * Returns total number of fieldId fields in the cert if index is 0.
 * FieldValue assumed to be empty on entry. 
 * Returns true if specified field was found, else returns false. 
 */
bool DecodedCert::getCertFieldData(
	const CssmOid		&fieldId,		// which field
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) const	// RETURNED
{ 
	CASSERT(certificateToSign != NULL);
	switch(mState) {
		case CS_Empty:		
		case CS_Building:	
			errorLog0("DecodedCert::getCertField: can't parse undecoded cert!\n");
			CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
		case CS_DecodedCert:
		case CS_DecodedTBS:
			break;
	}
	const oidToFieldFuncs *fieldFuncs = oidToFields(fieldId);
	return fieldFuncs->getFcn(*this, index, numFields, fieldValue);
}
 
/*
 * Set the field specified by fieldId in the specified Cert. 
 * Note no index - individual field routines either append (for extensions)
 * or if field already set ::throwMe(for all others) 
 */
void DecodedCert::setCertField(
	const CssmOid		&fieldId,		// which field
	const CssmData		&fieldValue) 
{
	CASSERT(certificateToSign != NULL);
	switch(mState) {
		case CS_Empty:			// first time thru
			mState = CS_Building;
			break;
		case CS_Building:		// subsequent passes
			break;
		case CS_DecodedCert:
		case CS_DecodedTBS:
			errorLog0("DecodedCert::setCertField: can't build on a decoded cert!\n");
			CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	if((fieldValue.data() == NULL) || (fieldValue.length() == 0)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	const oidToFieldFuncs *fieldFuncs = oidToFields(fieldId);
	const CssmData &value = CssmData::overlay(fieldValue);
	fieldFuncs->setFcn(*this, value);
}

/*
 * Free the fieldId-specific data referred to by fieldValue->Data. 
 */
void DecodedCert::freeCertFieldData(
	const CssmOid		&fieldId,
	CssmOwnedData		&fieldValue)
{
	if((fieldValue.data() == NULL) || (fieldValue.length() == 0)) {
		CssmError::throwMe(CSSM_ERRCODE_INVALID_FIELD_POINTER);
	}
	const oidToFieldFuncs *fieldFuncs = oidToFields(fieldId);
	if(fieldFuncs->freeFcn != NULL) {
		/* optional - simple cases handled below */
		fieldFuncs->freeFcn(fieldValue);
	}
	fieldValue.reset();
	fieldValue.release();

}


/*
 * Common means to get all fields from a decoded cert. Used in 
 * CertGetAllTemplateFields and CertGetAllFields.
 */
void DecodedCert::getAllParsedCertFields(
	uint32 				&NumberOfFields,		// RETURNED
	CSSM_FIELD_PTR 		&CertFields)			// RETURNED
{
	/* this is the max - some might be missing */
	uint32 maxFields = NUM_STD_CERT_FIELDS + mNumExtensions;
	CSSM_FIELD_PTR outFields = (CSSM_FIELD_PTR)malloc(maxFields * sizeof(CSSM_FIELD));
	
	/*
	 * We'll be copying oids and values for fields we find into
	 * outFields; current number of valid fields found in numOutFields.
	 */
	memset(outFields, 0, maxFields * sizeof(CSSM_FIELD));
	uint32 			numOutFields = 0;
	CSSM_FIELD_PTR 	currOutField;
	uint32 			currOidDex;
	const CSSM_OID 	*currOid;
	CssmAutoData 	aData(alloc);		// for malloc/copy of outgoing data
	
	/* query for each OID we know about */
	for(currOidDex=0; currOidDex<NUM_KNOWN_FIELDS; currOidDex++) {
		const oidToFieldFuncs *fieldFuncs = &fieldFuncTable[currOidDex];
		currOid = fieldFuncs->fieldId;
		uint32 numFields;				// for THIS oid

		/* 
		 * Return false if field not there, which is not an error here. 
		 * Actual exceptions are fatal.
		 */
		if(!fieldFuncs->getFcn(*this, 
				0, 				// index - looking for first one
				numFields, 
				aData)) {
			continue;
		}
		
		/* got some data for this oid - copy it and oid to outgoing CertFields */
		CASSERT(numOutFields < maxFields);
		currOutField = &outFields[numOutFields];
		currOutField->FieldValue = aData.release();
		aData.copy(*currOid);
		currOutField->FieldOid = aData.release();
		numOutFields++;
		
		/* if more fields are available for this OID, snag them too */
		for(uint32 fieldDex=1; fieldDex<numFields; fieldDex++) {
			/* note this should always succeed */
			bool brtn = fieldFuncs->getFcn(*this,
				fieldDex, 			
				numFields, 			// shouldn't change
				aData);
			if(!brtn) {
				errorLog0("getAllParsedCertFields: index screwup\n");
				CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
			}
			CASSERT(numOutFields < maxFields);
			currOutField = &outFields[numOutFields];
			currOutField->FieldValue = aData.release();
			aData.copy(*currOid);
			currOutField->FieldOid = aData.release();
			numOutFields++;
		}	/* multiple fields for currOid */
	}		/* for each known OID */
	
	NumberOfFields = numOutFields;
	CertFields = outFields;
}

void
DecodedCert::describeFormat(
	CssmAllocator &alloc,
	uint32 &NumberOfFields,
	CSSM_OID_PTR &OidList)
{
	/* malloc in app's space, do deep copy (including ->Data) */
	CSSM_OID_PTR oidList = (CSSM_OID_PTR)alloc.malloc(
		NUM_KNOWN_FIELDS * sizeof(CSSM_OID));
	memset(oidList, 0, NUM_KNOWN_FIELDS * sizeof(CSSM_OID));
	for(unsigned i=0; i<NUM_KNOWN_FIELDS; i++) {
		CssmAutoData oidCopy(alloc, *fieldFuncTable[i].fieldId);
		oidList[i] = oidCopy.release();
	}
	NumberOfFields = NUM_KNOWN_FIELDS;
	OidList = oidList;
}
