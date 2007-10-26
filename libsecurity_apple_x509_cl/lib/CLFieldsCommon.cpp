/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
 * CLFieldsCommon.h - get/set/free routines common to certs and CRLs
 */

#include "CLFieldsCommon.h"
#include "clNameUtils.h"
#include "clNssUtils.h"
#include "AppleX509CLSession.h"
#include <Security/cssmapple.h>
#include <Security/oidscert.h>
#include <Security/nameTemplates.h>
#include <Security/certExtensionTemplates.h>
#include <Security/SecAsn1Templates.h>

/* 
 * Table to map an OID to the info needed to decode the 
 * associated extension 
 */
typedef struct {
	const CSSM_OID 			&oid;
	unsigned				nssObjLen;
	const SecAsn1Template	*templ;
} NssExtenInfo;

static const NssExtenInfo nssExtenInfo[] = {
	{ CSSMOID_KeyUsage,
	  sizeof(CSSM_DATA),
	  kSecAsn1KeyUsageTemplate },
	{ CSSMOID_BasicConstraints,
	  sizeof(NSS_BasicConstraints),
	  kSecAsn1BasicConstraintsTemplate },
	{ CSSMOID_ExtendedKeyUsage,
	  sizeof(NSS_ExtKeyUsage),
	  kSecAsn1ExtKeyUsageTemplate },
	{ CSSMOID_SubjectKeyIdentifier,
	  sizeof(CSSM_DATA),
	  kSecAsn1SubjectKeyIdTemplate },
	{ CSSMOID_AuthorityKeyIdentifier,
	  sizeof(NSS_AuthorityKeyId),
	  kSecAsn1AuthorityKeyIdTemplate },
	{ CSSMOID_SubjectAltName,
	  sizeof(NSS_GeneralNames),
	  kSecAsn1GeneralNamesTemplate },
	{ CSSMOID_IssuerAltName,
	  sizeof(NSS_GeneralNames),
	  kSecAsn1GeneralNamesTemplate },
	{ CSSMOID_CertificatePolicies,
	  sizeof(NSS_CertPolicies),
	  kSecAsn1CertPoliciesTemplate },
	{ CSSMOID_NetscapeCertType,
	  sizeof(CSSM_DATA),
	  kSecAsn1NetscapeCertTypeTemplate },
	{ CSSMOID_CrlDistributionPoints,
	  sizeof(NSS_CRLDistributionPoints),
	  kSecAsn1CRLDistributionPointsTemplate },
	{ CSSMOID_CertIssuer,
	  sizeof(NSS_GeneralNames),
	  kSecAsn1GeneralNamesTemplate },
	{ CSSMOID_AuthorityInfoAccess,
	  sizeof(NSS_AuthorityInfoAccess),
	  kSecAsn1AuthorityInfoAccessTemplate },
	{ CSSMOID_SubjectInfoAccess,
	  sizeof(NSS_AuthorityInfoAccess),
	  kSecAsn1AuthorityInfoAccessTemplate },
	/* CRL extensions */
	{ CSSMOID_CrlNumber,
	  sizeof(CSSM_DATA),
	  kSecAsn1IntegerTemplate },
	{ CSSMOID_IssuingDistributionPoint,
	  sizeof(NSS_IssuingDistributionPoint),
	  kSecAsn1IssuingDistributionPointTemplate },
	{ CSSMOID_HoldInstructionCode,
	  sizeof(CSSM_OID),
	  kSecAsn1ObjectIDTemplate },
	{ CSSMOID_CrlReason,
	  sizeof(CSSM_DATA),
	  kSecAsn1EnumeratedTemplate },
	{ CSSMOID_DeltaCrlIndicator,
	  sizeof(CSSM_DATA),
	  kSecAsn1IntegerTemplate },
	{ CSSMOID_InvalidityDate,
	  sizeof(CSSM_DATA),
	  kSecAsn1GeneralizedTimeTemplate },
	{ CSSMOID_QC_Statements,
	  sizeof(NSS_QC_Statements),
	  kSecAsn1QC_StatementsTemplate }
};

#define NUM_NSS_EXTEN_INFOS	(sizeof(nssExtenInfo) / sizeof(nssExtenInfo[0]))

/* 
 * Returns true if we find the OID.
 */
bool clOidToNssInfo(
	const CSSM_OID			&oid,
	unsigned				&nssObjLen,		// RETURNED
	const SecAsn1Template	*&templ)		// RETURNED
{
	for(unsigned dex=0; dex<NUM_NSS_EXTEN_INFOS; dex++) {
		const NssExtenInfo &info = nssExtenInfo[dex];
		if(clCompareCssmData(&info.oid, &oid)) {
			nssObjLen = info.nssObjLen;
			templ = info.templ;
			return true;
		}
	}
	return false;
}



/*
 * Common code to pass info from a DecodedExten back to app.
 * Called from getField*().
 */
void getFieldExtenCommon(
	void 				*cdsaObj,			// e.g. CE_KeyUsage
											// NULL for berEncoded
	const DecodedExten	&decodedExt, 
	CssmOwnedData		&fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt;
	Allocator &alloc = fieldValue.allocator;
	CssmData &fdata = fieldValue.get();
	
	cssmExt = (CSSM_X509_EXTENSION_PTR)alloc.
		malloc(sizeof(CSSM_X509_EXTENSION));
	fdata.Data = (uint8 *)cssmExt;
	fdata.Length = sizeof(CSSM_X509_EXTENSION);
	decodedExt.convertToCdsa(cdsaObj, cssmExt, alloc);
}

/* 
 * Common code for top of setField* and freeField*().
 */
CSSM_X509_EXTENSION_PTR verifySetFreeExtension(
	const CssmData &fieldValue,
	bool berEncoded)		// false: value in value.parsedValue
							// true : value in BERValue
{
	if(fieldValue.length() != sizeof(CSSM_X509_EXTENSION)) {
		clFieldLog("Set/FreeExtension: bad length : exp %d got %d", 
			(int)sizeof(CSSM_X509_EXTENSION), (int)fieldValue.length());
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);			
	}
	CSSM_X509_EXTENSION_PTR cssmExt = 
		reinterpret_cast<CSSM_X509_EXTENSION_PTR>(fieldValue.data());
	if(berEncoded) {
		if(cssmExt->BERvalue.Data == NULL) {
			CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);		
		}
	}
	else {
		if(cssmExt->value.parsedValue == NULL) {
			CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);		
		}
	}
	return cssmExt;
}

/*
 * Common free code for all extensions. Extension-specific code must
 * free anything beyond cdsaExt->Value.parsedValue, then we free everything 
 * else (except the extension struct itself, which is freed by 
 * DecodedCert::freeCertFieldData()). 
 * The value union may contain a parsed value, or a CSSM_X509EXT_TAGandVALUE;
 * wed ont' care, we just free it. 
 */
void freeFieldExtenCommon(
	CSSM_X509_EXTENSION_PTR	exten,
	Allocator			&alloc)
{
	alloc.free(exten->extnId.Data);
	alloc.free(exten->BERvalue.Data);		// may be NULL
	alloc.free(exten->value.parsedValue);   // may be NULL
}

/*
 * One common free for extensions whose parsed value doesn't go any deeper 
 * than cssmExt->value.parsedValue. 
 */
void freeFieldSimpleExtension (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	freeFieldExtenCommon(cssmExt, fieldValue.allocator);
}


/***
 *** Common code for get/set subject/issuer name (C struct version)
 *** Format = CSSM_X509_NAME
 *** class Name from sm_x501if
 ***/
bool getField_RDN_NSS (
	const NSS_Name 		&nssName,
	CssmOwnedData		&fieldValue)	// RETURNED
{
	/* alloc top-level CSSM_X509_NAME */
	Allocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_NAME));
	CSSM_X509_NAME_PTR cssmName = (CSSM_X509_NAME_PTR)fieldValue.data();
	
	CL_nssNameToCssm(nssName, *cssmName, alloc);
	return true;
}

void freeField_RDN  (
	CssmOwnedData		&fieldValue)
{
	if(fieldValue.data() == NULL) {
		return;
	}
	if(fieldValue.length() != sizeof(CSSM_X509_NAME)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	Allocator &alloc = fieldValue.allocator;
	CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)fieldValue.data();
	CL_freeX509Name(x509Name, alloc);
		
	/* top-level x509Name pointer freed by freeCertFieldData() */
}

/*** 
 *** Common code for Issuer Name, Subject Name (normalized and encoded 
 *** version)
 *** Format = CSSM_DATA containing the DER encoding of the normalized name
 ***/
bool getField_normRDN_NSS (
	const CSSM_DATA		&derName,
	uint32				&numFields,		// RETURNED (if successful, 0 or 1)
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(derName.Data == NULL) {
		/* This can happen during CertGetAllTemplateFields() because
		 * the normalized fields are only set up during cert/CRL decode */
		return false;
	}
	
	/*
	 * First make a temp decoded copy which we'll be manipulating.	
	 */
	SecNssCoder coder;
	NSS_Name decodedName;
	
	memset(&decodedName, 0, sizeof(decodedName));
	PRErrorCode prtn = coder.decodeItem(derName, kSecAsn1NameTemplate, &decodedName);
	if(prtn) {
		/* 
		 * Actually should never happen since this same bag of bits successfully 
		 * decoded when the cert as a whole was decoded... 
		 */
		clErrorLog("getField_normRDN decode error\n");
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
		
	}
	
	/* normalize */
	CL_normalizeX509NameNSS(decodedName, coder);
	
	/* encode result */
	prtn = SecNssEncodeItemOdata(&decodedName, kSecAsn1NameTemplate, fieldValue);
	if(prtn) {
		clErrorLog("getField_normRDN encode error\n");
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	numFields = 1;
	return true;
}

/***
 *** Common code for Time fields - Validity not before, Not After, 
 ***    This Update, Next Update
 *** Format: CSSM_X509_TIME
 ***/
bool getField_TimeNSS (
	const NSS_Time 	&nssTime,
	unsigned		index,			// which occurrence (0 = first)
	uint32			&numFields,		// RETURNED
	CssmOwnedData	&fieldValue)	// RETURNED
{
	if(!tbsGetCheck(nssTime.item.Data, index)) {
		return false;
	}
	Allocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_TIME));
	CSSM_X509_TIME *cssmTime = 
		(CSSM_X509_TIME *)fieldValue.data();
	if(CL_nssTimeToCssm(nssTime, *cssmTime, alloc)) {
		numFields = 1;
		return true;
	}
	else {
		return false;
	}
}

void setField_TimeNSS (
	const CssmData	&fieldValue,
	NSS_Time		&nssTime,
	SecNssCoder		&coder)
{
	CSSM_X509_TIME *cssmTime = 
		(CSSM_X509_TIME *)fieldValue.data();
	CL_cssmTimeToNss(*cssmTime, nssTime, coder);
}

void freeField_Time (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_TIME *cssmTime = (CSSM_X509_TIME *)fieldValue.data();
	if(cssmTime == NULL) {
		return;
	}
	if(fieldValue.length() != sizeof(CSSM_X509_TIME)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	CL_freeCssmTime(cssmTime, fieldValue.allocator);
}

/***
 *** TBS AlgId, Signature AlgId
 *** Format = CSSM_X509_ALGORITHM_IDENTIFIER
 ***/
void getField_AlgIdNSS (
	const CSSM_X509_ALGORITHM_IDENTIFIER 	&srcAlgId,
	CssmOwnedData							&fieldValue)	// RETURNED
{
	Allocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_ALGORITHM_IDENTIFIER));
	CSSM_X509_ALGORITHM_IDENTIFIER *destAlgId = 
		(CSSM_X509_ALGORITHM_IDENTIFIER *)fieldValue.data();
	CL_copyAlgId(srcAlgId, *destAlgId, alloc);
}

void setField_AlgIdNSS (
	const CssmData		&fieldValue,
	CSSM_X509_ALGORITHM_IDENTIFIER &dstAlgId,
	SecNssCoder			&coder)
{
	CSSM_X509_ALGORITHM_IDENTIFIER *srcAlgId = 
		(CSSM_X509_ALGORITHM_IDENTIFIER *)fieldValue.data();
	/* allocator for this coder */
	ArenaAllocator areanAlloc(coder);
	CL_copyAlgId(*srcAlgId, dstAlgId, areanAlloc);
}

void freeField_AlgId (
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
	Allocator &alloc = fieldValue.allocator;
	alloc.free(cssmAlgId->algorithm.Data);
	alloc.free(cssmAlgId->parameters.Data);
	memset(cssmAlgId, 0, sizeof(CSSM_X509_ALGORITHM_IDENTIFIER));
}

/*
 * Routines for common validity checking for certificateToSign fields.
 *
 * Call from setField*: verify field isn't already set, optionally validate
 * input length
 */
void tbsSetCheck(
	void				*fieldToSet,
	const CssmData		&fieldValue,
	uint32				expLength,
	const char			*op)
{
	if(fieldToSet != NULL) {						
		/* can't add another */
		clErrorLog("setField(%s): field already set", op);
		CssmError::throwMe(CSSMERR_CL_INVALID_NUMBER_OF_FIELDS);		
	}										
	if((expLength != 0) && (fieldValue.length() != expLength)) {		
		clErrorLog("setField(%s): bad length : exp %d got %d", 
			op, (int)expLength, (int)fieldValue.length());
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);			
	}
}

/*
 * Call from getField* for unique fields - detect missing field or 
 * index out of bounds.
 */
bool tbsGetCheck(
	const void	*requiredField,
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
 *** unknown extensions 
 *** CDSA format: raw bytes in a CSSM_DATA. This data is the BER-encoding of
 ***              some extension struct we don't know about.
 *** NSS format   CSSM_DATA
 *** OID 		  CSSMOID_X509V3CertificateExtensionCStruct
 ***/

void setFieldUnknownExt(
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, true);
	SecNssCoder &coder = cert.coder();
	CSSM_DATA *rawExtn = (CSSM_DATA *)coder.malloc(sizeof(CSSM_DATA));
	coder.allocCopyItem(cssmExt->BERvalue, *rawExtn);
	cert.addExtension(NULL, cssmExt->extnId, cssmExt->critical, 
		true, NULL /* no template */, rawExtn); 
}

bool getFieldUnknownExt(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	uint8 noOidDataLikeThis[2] = {1, 2};	// a dummy argument
	CSSM_OID noOidLikeThis = {2, noOidDataLikeThis};
	const DecodedExten *decodedExt = 
		cert.DecodedItem::findDecodedExt(noOidLikeThis, 
		true, index, numFields);
	if(decodedExt == NULL) {
		return false;
	}
	getFieldExtenCommon(NULL, *decodedExt, fieldValue);
	return true;
}

void freeFieldUnknownExt (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, true);
	Allocator &alloc = fieldValue.allocator;
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/* setField for read-only OIDs (i.e., the ones in cert/CRL, not TBS) */
void setField_ReadOnly (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	clErrorLog("Attempt to set a read-only field");
	CssmError::throwMe(CSSMERR_CL_UNKNOWN_TAG);
}

bool getField_Unimplemented (
	DecodedItem			&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	clErrorLog("Attempt to get an unimplemented field");
	CssmError::throwMe(CSSMERR_CL_UNKNOWN_TAG);
}



