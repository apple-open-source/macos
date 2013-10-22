/*
 * Copyright (c) 2000-2010 Apple Inc. All Rights Reserved.
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
 * CLCertExtensions.cpp - extensions support. A major component of DecodedCert.
 *
 */
 
#include "DecodedCert.h"
#include "cldebugging.h"
#include "CLCertExtensions.h"
#include "CLFieldsCommon.h"
#include "clNssUtils.h"
#include "clNameUtils.h"
#include <security_utilities/utilities.h>
#include <Security/oidscert.h>
#include <Security/oidsattr.h>
#include <Security/cssmerr.h>
#include <Security/x509defs.h>
#include <Security/certextensions.h>
#include <security_utilities/globalizer.h>
#include <Security/certExtensionTemplates.h>
#include <Security/SecAsn1Templates.h>

/***
 *** get/set/free functions called out from CertFields.cpp
 ***/

/***
 *** KeyUsage 
 *** CDSA format 	CE_KeyUsage
 *** NSS format 	CSSM_DATA, length 2
 *** OID 			CSSMOID_KeyUsage
 ***/
 
void setFieldKeyUsage(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	CE_KeyUsage *cdsaObj = (CE_KeyUsage *)cssmExt->value.parsedValue;
	
	/* Alloc an NSS-style key usage in cert.coder's memory */
	SecNssCoder &coder = cert.coder();
	CSSM_DATA *nssObj = (CSSM_DATA *)coder.malloc(sizeof(CSSM_DATA));
	coder.allocItem(*nssObj, 2);
	
	/* cdsaObj --> nssObj */
	nssObj->Data[0] = (*cdsaObj) >> 8;
	nssObj->Data[1] = *cdsaObj;
	
	/* Adjust length for BIT STRING encoding */
	clCssmBitStringToNss(*nssObj);
	
	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1KeyUsageTemplate); 
}


bool getFieldKeyUsage(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	CSSM_DATA *nssObj;
	CE_KeyUsage *cdsaObj;
	bool brtn;
	
	brtn = cert.GetExtenTop<CSSM_DATA, CE_KeyUsage>(
		index,
		numFields,
		fieldValue.allocator,
		CSSMOID_KeyUsage,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	/* make a copy - can't modify length in place */
	CSSM_DATA bitString = *nssObj;
	clNssBitStringToCssm(bitString);
	size_t toCopy = bitString.Length;
	if(toCopy > 2) {
		/* I hope I never see this... */
		clErrorLog("getFieldKeyUsage: KeyUsage larger than 2 bytes!");
		toCopy = 2;
	}
	unsigned char bits[2] = {0, 0};
	memmove(bits, bitString.Data, toCopy);
	*cdsaObj = (((unsigned)bits[0]) << 8) | bits[1];
	
	/* pass back to caller */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

/***
 *** Basic Constraints 
 *** CDSA format: 	CE_BasicConstraints
 *** NSS format 	CE_BasicConstraints
 *** OID 			CSSMOID_BasicConstraints
 ***/
 
void setFieldBasicConstraints(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_BasicConstraints *cdsaObj = 
		(CE_BasicConstraints *)cssmExt->value.parsedValue;
	
	/* Alloc an NSS-style BasicConstraints in cert.coder's memory */
	SecNssCoder &coder = cert.coder();
	NSS_BasicConstraints *nssObj = 
		(NSS_BasicConstraints *)coder.malloc(sizeof(NSS_BasicConstraints));
	memset(nssObj, 0, sizeof(*nssObj));
	
	/* cdsaObj --> nssObj */
	ArenaAllocator arenaAlloc(coder);
	clCssmBoolToNss(cdsaObj->cA, nssObj->cA, arenaAlloc);
	if(cdsaObj->pathLenConstraintPresent) {
		clIntToData(cdsaObj->pathLenConstraint, 
			nssObj->pathLenConstraint, arenaAlloc);
	}

	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1BasicConstraintsTemplate); 
}


bool getFieldBasicConstraints(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_BasicConstraints *nssObj;
	CE_BasicConstraints *cdsaObj;
	bool brtn;
	
	brtn = cert.GetExtenTop<NSS_BasicConstraints, CE_BasicConstraints>(
		index,
		numFields,
		fieldValue.allocator,
		CSSMOID_BasicConstraints,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}

	if(nssObj->cA.Data == NULL) {
		/* default */
		cdsaObj->cA = CSSM_FALSE;
	}
	else {
		cdsaObj->cA = clNssBoolToCssm(nssObj->cA);
	}
	if(nssObj->pathLenConstraint.Data == NULL) {
		/* optional */
		cdsaObj->pathLenConstraintPresent = CSSM_FALSE;
		cdsaObj->pathLenConstraint = 0;
	}
	else {
		cdsaObj->pathLenConstraintPresent = CSSM_TRUE;
		cdsaObj->pathLenConstraint = clDataToInt(nssObj->pathLenConstraint);
	}
	
	/* pass back to caller */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

/***
 *** Extended Key Usage
 *** CDSA format: 	CE_ExtendedKeyUsage
 *** NSS format: 	NSS_ExtKeyUsage
 *** OID 			CSSMOID_ExtendedKeyUsage
 ***/
void setFieldExtKeyUsage(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	CE_ExtendedKeyUsage *cdsaObj = 
		(CE_ExtendedKeyUsage *)cssmExt->value.parsedValue;
	
	SecNssCoder &coder = cert.coder();
	NSS_ExtKeyUsage *nssObj = 
		(NSS_ExtKeyUsage *)coder.malloc(sizeof(NSS_ExtKeyUsage));
	memset(nssObj, 0, sizeof(*nssObj));
	if(cdsaObj->numPurposes != 0) {
		nssObj->purposes = 
			(CSSM_OID **)clNssNullArray(cdsaObj->numPurposes, coder);
	}
	
	/* cdsaObj --> nssObj, one 'purpose' (OID) at a time */
	for(unsigned dex=0; dex<cdsaObj->numPurposes; dex++) {
		nssObj->purposes[dex] = (CSSM_OID *)coder.malloc(sizeof(CSSM_OID));
		coder.allocCopyItem(cdsaObj->purposes[dex],
			*nssObj->purposes[dex]);
	}

	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1ExtKeyUsageTemplate); 
}

bool getFieldExtKeyUsage(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_ExtKeyUsage *nssObj;
	CE_ExtendedKeyUsage *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;
	
	brtn = cert.GetExtenTop<NSS_ExtKeyUsage, CE_ExtendedKeyUsage>(
		index,
		numFields,
		alloc,
		CSSMOID_ExtendedKeyUsage,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	/* nssObj --> cdsaObj, one purpose at a time */ 
	unsigned numPurposes = clNssArraySize((const void **)nssObj->purposes);
	cdsaObj->numPurposes = numPurposes;
	if(numPurposes) {
		unsigned len = numPurposes * sizeof(CSSM_OID);
		cdsaObj->purposes = (CSSM_OID_PTR)alloc.malloc(len);
		memset(cdsaObj->purposes, 0, len);
	}
	for(unsigned dex=0; dex<numPurposes; dex++) {
		clAllocCopyData(alloc, *nssObj->purposes[dex], cdsaObj->purposes[dex]);
	}

	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldExtKeyUsage(
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_ExtendedKeyUsage *cdsaObj = 
		(CE_ExtendedKeyUsage *)cssmExt->value.parsedValue;
	unsigned oidDex;
	for(oidDex=0; oidDex<cdsaObj->numPurposes; oidDex++) {
		alloc.free(cdsaObj->purposes[oidDex].Data);
	}
	alloc.free(cdsaObj->purposes);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Subject Key Identifier
 *** CDSA format: 	CE_SubjectKeyID, which is just a CSSM_DATA
 *** OID 			CSSMOID_SubjectKeyIdentifier
 ***/
 
void setFieldSubjectKeyId(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	CE_SubjectKeyID *cdsaObj = (CE_SubjectKeyID *)cssmExt->value.parsedValue;
	SecNssCoder &coder = cert.coder();
	CSSM_DATA *nssObj = (CSSM_DATA *)coder.malloc(sizeof(CSSM_DATA));
	coder.allocCopyItem(*cdsaObj, *nssObj);
	
	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1SubjectKeyIdTemplate); 
}

bool getFieldSubjectKeyId(
	DecodedItem	 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	CSSM_DATA *nssObj;
	CE_SubjectKeyID *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;
	
	brtn = cert.GetExtenTop<CSSM_DATA, CE_SubjectKeyID>(
		index,
		numFields,
		alloc,
		CSSMOID_SubjectKeyIdentifier,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	/* if this fails, we're out of sync with nssExtenInfo[] in 
	 * CLFieldsCommon.cpp */
	assert(nssObj != NULL);	
	clAllocCopyData(alloc, *nssObj, *cdsaObj);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldSubjectKeyId (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_SubjectKeyID *cdsaObj = (CE_SubjectKeyID *)cssmExt->value.parsedValue;
	alloc.free(cdsaObj->Data);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Authority Key Identifier
 *** CDSA format: 	CE_AuthorityKeyID
 *** NSS format: 	NSS_AuthorityKeyId
 *** OID 			CSSMOID_AuthorityKeyIdentifier
 ***/
 
void setFieldAuthorityKeyId(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	CE_AuthorityKeyID *cdsaObj = 
		(CE_AuthorityKeyID *)cssmExt->value.parsedValue;
	
	/* Alloc an NSS-style AuthorityKeyId in cert.coder's memory */
	SecNssCoder &coder = cert.coder();
	NSS_AuthorityKeyId *nssObj = 
		(NSS_AuthorityKeyId *)coder.malloc(sizeof(NSS_AuthorityKeyId));
	memset(nssObj, 0, sizeof(*nssObj));
	
	/* convert caller's CDSA-style CE_AuthorityKeyID to NSS */
	CL_cssmAuthorityKeyIdToNss(*cdsaObj, *nssObj, coder);
	
	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1AuthorityKeyIdTemplate); 
}

bool getFieldAuthorityKeyId(
	DecodedItem		 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_AuthorityKeyId *nssObj;
	CE_AuthorityKeyID *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;
	
	brtn = cert.GetExtenTop<NSS_AuthorityKeyId, CE_AuthorityKeyID>(
		index,
		numFields,
		alloc,
		CSSMOID_AuthorityKeyIdentifier,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	
	/* nssObj --> cdsaObj */
	CL_nssAuthorityKeyIdToCssm(*nssObj, *cdsaObj, cert.coder(), alloc);
	
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldAuthorityKeyId (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_AuthorityKeyID *cdsaObj = (CE_AuthorityKeyID *)cssmExt->value.parsedValue;
	CL_freeAuthorityKeyId(*cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Subject/Issuer alternate name
 *** CDSA Format:	CE_GeneralNames
 *** NSS format:	NSS_GeneralNames
 *** OID: 			CSSMOID_SubjectAltName, CSSMOID_IssuerAltName
 ***/
void setFieldSubjIssuerAltName(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_GeneralNames *cdsaObj = (CE_GeneralNames *)cssmExt->value.parsedValue;

	/* Alloc an NSS-style GeneralNames in cert.coder's memory */
	SecNssCoder &coder = cert.coder();
	NSS_GeneralNames *nssObj = 
		(NSS_GeneralNames *)coder.malloc(sizeof(NSS_GeneralNames));
	memset(nssObj, 0, sizeof(*nssObj));
	
	/* cdsaObj --> nssObj */
	CL_cssmGeneralNamesToNss(*cdsaObj, *nssObj, coder);
	
	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1GeneralNamesTemplate); 
}

bool getFieldSubjAltName(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_GeneralNames *nssObj;
	CE_GeneralNames *cdsaObj; 
	bool brtn;
	
	brtn = cert.GetExtenTop<NSS_GeneralNames, CE_GeneralNames>(
		index,
		numFields,
		fieldValue.allocator,
		CSSMOID_SubjectAltName,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	CL_nssGeneralNamesToCssm(*nssObj, *cdsaObj,	
		cert.coder(), fieldValue.allocator);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

bool getFieldIssuerAltName(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_GeneralNames *nssObj;
	CE_GeneralNames *cdsaObj; 
	bool brtn;
	
	brtn = cert.GetExtenTop<NSS_GeneralNames, CE_GeneralNames>(
		index,
		numFields,
		fieldValue.allocator,
		CSSMOID_IssuerAltName,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	CL_nssGeneralNamesToCssm(*nssObj, *cdsaObj,	
		cert.coder(), fieldValue.allocator);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldSubjIssuerAltName (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_GeneralNames *cdsaObj = (CE_GeneralNames *)cssmExt->value.parsedValue;
	CL_freeCssmGeneralNames(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Certificate Policies
 *** CDSA Format:	CE_CertPolicies
 *** NSS format :	NSS_CertPolicies
 *** OID: 			CSSMOID_CertificatePolicies
 ***/
 
#define MAX_IA5_NAME_SIZE	1024

void setFieldCertPolicies(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	SecNssCoder &coder = cert.coder();
	NSS_CertPolicies *nssObj = 
		(NSS_CertPolicies *)coder.malloc(sizeof(NSS_CertPolicies));
	memset(nssObj, 0, sizeof(NSS_CertPolicies));
	CE_CertPolicies *cdsaObj = 
		(CE_CertPolicies *)cssmExt->value.parsedValue;
	
	if(cdsaObj->numPolicies) {
		nssObj->policies = 
			(NSS_PolicyInformation **)clNssNullArray(
				cdsaObj->numPolicies, coder);
	}
	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		NSS_PolicyInformation *nPolInfo = (NSS_PolicyInformation *)
			coder.malloc(sizeof(NSS_PolicyInformation));
		memset(nPolInfo, 0, sizeof(*nPolInfo));
		nssObj->policies[polDex] = nPolInfo;
		
		coder.allocCopyItem(cPolInfo->certPolicyId, nPolInfo->certPolicyId);

		unsigned numQual = cPolInfo->numPolicyQualifiers;
		if(numQual != 0) {
			nPolInfo->policyQualifiers = 
				(NSS_PolicyQualifierInfo **)clNssNullArray(numQual,
					coder);
		}
		for(unsigned qualDex=0; qualDex<numQual; qualDex++) {
			CE_PolicyQualifierInfo *cQualInfo = 
				&cPolInfo->policyQualifiers[qualDex];
			NSS_PolicyQualifierInfo *nQualInfo = 
				(NSS_PolicyQualifierInfo *)coder.malloc(
					sizeof(NSS_PolicyQualifierInfo));
			memset(nQualInfo, 0, sizeof(NSS_PolicyQualifierInfo));
			nPolInfo->policyQualifiers[qualDex] = nQualInfo;
			
			/* 
			 * OK we're at the lowest level. 
			 * policyQualifierId == id_qt_cps: qualifier is 
			 * an IA5 string, incoming data is its contents. 
			 * Else incoming data is an encoded blob we pass on directly.
			 */
			coder.allocCopyItem(cQualInfo->policyQualifierId,
				nQualInfo->policyQualifierId);
				
			if(clCompareCssmData(&cQualInfo->policyQualifierId, 
					&CSSMOID_QT_CPS)) {
				if(coder.encodeItem(&cQualInfo->qualifier,
						kSecAsn1IA5StringTemplate,
						nQualInfo->qualifier)) {
					clErrorLog("setFieldCertPOlicies: IA5 encode error\n");
					CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
				}
			}
			else {
				/* uninterpreted, copy over directly */
				coder.allocCopyItem(cQualInfo->qualifier,
					nQualInfo->qualifier);
			}
		}	/* for each qualifier */
	}	/* for each policy */
	
	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1CertPoliciesTemplate); 
}

bool getFieldCertPolicies( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_CertPolicies *nssObj;
	CE_CertPolicies *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;
	brtn = cert.GetExtenTop<NSS_CertPolicies, CE_CertPolicies>(
		index,
		numFields,
		fieldValue.allocator,
		CSSMOID_CertificatePolicies,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	
	memset(cdsaObj, 0, sizeof(*cdsaObj));
	cdsaObj->numPolicies = 
		clNssArraySize((const void **)nssObj->policies);
	unsigned sz = cdsaObj->numPolicies * sizeof(CE_PolicyInformation);
	if(sz) {
		cdsaObj->policies = (CE_PolicyInformation *)alloc.malloc(sz);
		memset(cdsaObj->policies, 0, sz);
	}

	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		NSS_PolicyInformation *nPolInfo = nssObj->policies[polDex];
		clAllocCopyData(alloc, nPolInfo->certPolicyId,
			cPolInfo->certPolicyId);
		if(nPolInfo->policyQualifiers == NULL) {
			continue;
		}
		
		cPolInfo->numPolicyQualifiers = 
			clNssArraySize((const void **)nPolInfo->policyQualifiers);
		sz = cPolInfo->numPolicyQualifiers * 
			 sizeof(CE_PolicyQualifierInfo);
		cPolInfo->policyQualifiers = (CE_PolicyQualifierInfo *)
			alloc.malloc(sz);
		memset(cPolInfo->policyQualifiers, 0, sz);
		
		for(unsigned qualDex=0; qualDex<cPolInfo->numPolicyQualifiers; 
				qualDex++) {
			NSS_PolicyQualifierInfo *nQualInfo = 
				nPolInfo->policyQualifiers[qualDex];
			CE_PolicyQualifierInfo *cQualInfo = 
				&cPolInfo->policyQualifiers[qualDex];
			
			/* 
			 * leaf. 
			 * policyQualifierId == CSSMOID_QT_CPS : 
			 *  		IA5String - decode and return contents.
			 * Else return whole thing. 
			 */
			clAllocCopyData(alloc, nQualInfo->policyQualifierId,
				cQualInfo->policyQualifierId);
			CSSM_DATA toCopy = nQualInfo->qualifier;
			if(clCompareCssmData(&nQualInfo->policyQualifierId, 
						&CSSMOID_QT_CPS)) {
				/* decode as IA5String to temp memory */
				toCopy.Data = NULL;
				toCopy.Length = 0;
				if(cert.coder().decodeItem(nQualInfo->qualifier,
						kSecAsn1IA5StringTemplate,
						&toCopy)) {
					clErrorLog("***getCertPolicies: bad IA5String!\n");
					CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
				}
			}
			/* else copy out nQualInfo->qualifier */
			clAllocCopyData(alloc, toCopy, cQualInfo->qualifier);
		}	/* for each qualifier */
	}		/* for each policy info */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldCertPolicies (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_CertPolicies *cdsaObj = (CE_CertPolicies *)cssmExt->value.parsedValue;
	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		alloc.free(cPolInfo->certPolicyId.Data);
		for(unsigned qualDex=0; 
		             qualDex<cPolInfo->numPolicyQualifiers; 
					 qualDex++) {
			CE_PolicyQualifierInfo *cQualInfo = 
				&cPolInfo->policyQualifiers[qualDex];
			alloc.free(cQualInfo->policyQualifierId.Data);
			alloc.free(cQualInfo->qualifier.Data);
		}
		alloc.free(cPolInfo->policyQualifiers);
	}
	alloc.free(cdsaObj->policies);
	freeFieldExtenCommon(cssmExt, alloc);	// frees extnId, parsedValue,
											//    BERvalue
}

/***
 *** Netscape cert type
 *** CDSA Format:	CE_NetscapeCertType (a uint16)
 *** NSS format 	CSSM_DATA, length 2
 *** OID: 			CSSMOID_NetscapeCertType
 ***/
void setFieldNetscapeCertType(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	CE_NetscapeCertType *cdsaObj = 
		(CE_NetscapeCertType *)cssmExt->value.parsedValue;
	
	/* Alloc an NSS-style key usage in cert.coder's memory */
	SecNssCoder &coder = cert.coder();
	CSSM_DATA *nssObj = (CSSM_DATA *)coder.malloc(sizeof(CSSM_DATA));
	coder.allocItem(*nssObj, 2);
	
	/* cdsaObj --> nssObj */
	nssObj->Data[0] = (*cdsaObj) >> 8;
	nssObj->Data[1] = *cdsaObj;
	
	/* Adjust length for BIT STRING encoding */
	clCssmBitStringToNss(*nssObj);
	
	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1NetscapeCertTypeTemplate); 
}

bool getFieldNetscapeCertType(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	CSSM_DATA *nssObj;
	CE_NetscapeCertType *cdsaObj;
	bool brtn;
	
	brtn = cert.GetExtenTop<CSSM_DATA, CE_NetscapeCertType>(
		index,
		numFields,
		fieldValue.allocator,
		CSSMOID_NetscapeCertType,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	/* make a copy - can't modify length in place */
	CSSM_DATA bitString = *nssObj;
	clNssBitStringToCssm(bitString);
	size_t toCopy = bitString.Length;
	if(toCopy > 2) {
		/* I hope I never see this... */
		clErrorLog("getFieldKeyUsage: CertType larger than 2 bytes!");
		toCopy = 2;
	}
	unsigned char bits[2] = {0, 0};
	memmove(bits, bitString.Data, toCopy);
	*cdsaObj = (((unsigned)bits[0]) << 8) | bits[1];
	
	/* pass back to caller */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

/***
 *** CRL Distribution points
 *** CDSA Format:	CE_CRLDistPointsSyntax
 *** NSS format:	NSS_CRLDistributionPoints
 *** OID: 			CSSMOID_CrlDistributionPoints
 ***/
void setFieldCrlDistPoints(		
	DecodedItem	&cert, 
	const CssmData &fieldValue)  
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_CRLDistPointsSyntax *cdsaObj = 
		(CE_CRLDistPointsSyntax *)cssmExt->value.parsedValue;
	SecNssCoder &coder = cert.coder();
	NSS_CRLDistributionPoints *nssObj = 
		(NSS_CRLDistributionPoints *)coder.malloc(
				sizeof(NSS_CRLDistributionPoints));
		
	CL_cssmDistPointsToNss(*cdsaObj, *nssObj, coder);
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1CRLDistributionPointsTemplate); 
}

bool getFieldCrlDistPoints( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_CRLDistributionPoints *nssObj;
	CE_CRLDistPointsSyntax *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;

	brtn = cert.GetExtenTop<NSS_CRLDistributionPoints, 
			CE_CRLDistPointsSyntax>(
		index,
		numFields,
		alloc,
		CSSMOID_CrlDistributionPoints,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	CL_nssDistPointsToCssm(*nssObj, *cdsaObj, cert.coder(), alloc);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldCrlDistPoints (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_CRLDistPointsSyntax *cdsaObj = 
		(CE_CRLDistPointsSyntax *)cssmExt->value.parsedValue;
	CL_freeCssmDistPoints(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** {Subject,Authority}InfoAccess
 ***
 *** CDSA Format:	CE_AuthorityInfoAccess
 *** NSS format:	NSS_AuthorityInfoAccess
 *** OID: 			CSSMOID_AuthorityInfoAccess, CSSMOID_SubjectInfoAccess
 ***/
void setFieldAuthInfoAccess(		
	DecodedItem	&cert, 
	const CssmData &fieldValue)  
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_AuthorityInfoAccess *cdsaObj = 
		(CE_AuthorityInfoAccess *)cssmExt->value.parsedValue;
	SecNssCoder &coder = cert.coder();
	NSS_AuthorityInfoAccess *nssObj = 
		(NSS_AuthorityInfoAccess *)coder.malloc(
				sizeof(NSS_AuthorityInfoAccess));
		
	CL_cssmInfoAccessToNss(*cdsaObj, *nssObj, coder);
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1AuthorityInfoAccessTemplate); 
}

bool getFieldAuthInfoAccess( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_AuthorityInfoAccess *nssObj;
	CE_AuthorityInfoAccess *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;

	brtn = cert.GetExtenTop<NSS_AuthorityInfoAccess, 
			CE_AuthorityInfoAccess>(
		index,
		numFields,
		alloc,
		CSSMOID_AuthorityInfoAccess,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	CL_infoAccessToCssm(*nssObj, *cdsaObj, cert.coder(), alloc);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

bool getFieldSubjInfoAccess( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_AuthorityInfoAccess *nssObj;
	CE_AuthorityInfoAccess *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;

	brtn = cert.GetExtenTop<NSS_AuthorityInfoAccess, 
			CE_AuthorityInfoAccess>(
		index,
		numFields,
		alloc,
		CSSMOID_SubjectInfoAccess,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	CL_infoAccessToCssm(*nssObj, *cdsaObj, cert.coder(), alloc);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldInfoAccess (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_AuthorityInfoAccess *cdsaObj = 
		(CE_AuthorityInfoAccess *)cssmExt->value.parsedValue;
	CL_freeInfoAccess(*cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue

}

/***
 *** Qualfied Cert Statements
 ***
 *** CDSA Format:	CE_QC_Statements
 *** NSS format:	NSS_QC_Statements
 *** OID: 			CSSMOID_QC_Statements
 ***/
void setFieldQualCertStatements(		
	DecodedItem	&cert, 
	const CssmData &fieldValue)  
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_QC_Statements *cdsaObj = 
		(CE_QC_Statements *)cssmExt->value.parsedValue;
	SecNssCoder &coder = cert.coder();
	NSS_QC_Statements *nssObj = 
		(NSS_QC_Statements *)coder.malloc(
				sizeof(NSS_QC_Statements));
		
	CL_cssmQualCertStatementsToNss(*cdsaObj, *nssObj, coder);
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1QC_StatementsTemplate); 
}

bool getFieldQualCertStatements( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_QC_Statements *nssObj;
	CE_QC_Statements *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;

	brtn = cert.GetExtenTop<NSS_QC_Statements, 
			CE_QC_Statements>(
		index,
		numFields,
		alloc,
		CSSMOID_QC_Statements,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	CL_qualCertStatementsToCssm(*nssObj, *cdsaObj, cert.coder(), alloc);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldQualCertStatements( 
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_QC_Statements *cdsaObj = 
		(CE_QC_Statements *)cssmExt->value.parsedValue;
	CL_freeQualCertStatements(*cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Name Constraints
 *** CDSA Format:	CE_NameConstraints
 *** NSS format:	NSS_NameConstraints
 *** OID: 			CSSMOID_NameConstraints
 ***/
void setFieldNameConstraints(		
	DecodedItem	&cert, 
	const CssmData &fieldValue)  
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_NameConstraints *cdsaObj = 
		(CE_NameConstraints *)cssmExt->value.parsedValue;
	SecNssCoder &coder = cert.coder();
	NSS_NameConstraints *nssObj = 
		(NSS_NameConstraints *)coder.malloc(
				sizeof(NSS_NameConstraints));
	CL_cssmNameConstraintsToNss(*cdsaObj, *nssObj, coder);
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1NameConstraintsTemplate); 
}

bool getFieldNameConstraints( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_NameConstraints *nssObj;
	CE_NameConstraints *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;

	brtn = cert.GetExtenTop<NSS_NameConstraints, 
			CE_NameConstraints>(
		index,
		numFields,
		alloc,
		CSSMOID_NameConstraints,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	CL_nssNameConstraintsToCssm(*nssObj, *cdsaObj, cert.coder(), alloc);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldNameConstraints (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_NameConstraints *cdsaObj = 
		(CE_NameConstraints *)cssmExt->value.parsedValue;
	CL_freeCssmNameConstraints(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Policy Mappings
 *** CDSA Format:	CE_PolicyMappings
 *** NSS format:	NSS_PolicyMappings
 *** OID: 			CSSMOID_PolicyMappings
 ***/
void setFieldPolicyMappings(		
	DecodedItem	&cert, 
	const CssmData &fieldValue)  
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_PolicyMappings *cdsaObj = 
		(CE_PolicyMappings *)cssmExt->value.parsedValue;
	SecNssCoder &coder = cert.coder();
	NSS_PolicyMappings *nssObj = 
		(NSS_PolicyMappings *)coder.malloc(
				sizeof(NSS_PolicyMappings));
	CL_cssmPolicyMappingsToNss(*cdsaObj, *nssObj, coder);
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1PolicyMappingsTemplate); 
}

bool getFieldPolicyMappings( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_PolicyMappings *nssObj;
	CE_PolicyMappings *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;

	brtn = cert.GetExtenTop<NSS_PolicyMappings, 
			CE_PolicyMappings>(
		index,
		numFields,
		alloc,
		CSSMOID_PolicyMappings,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	CL_nssPolicyMappingsToCssm(*nssObj, *cdsaObj, cert.coder(), alloc);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldPolicyMappings (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_PolicyMappings *cdsaObj = 
		(CE_PolicyMappings *)cssmExt->value.parsedValue;
	CL_freeCssmPolicyMappings(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Policy Constraints
 *** CDSA Format:	CE_PolicyConstraints
 *** NSS format:	NSS_PolicyConstraints
 *** OID: 			CSSMOID_PolicyConstraints
 ***/
void setFieldPolicyConstraints(		
	DecodedItem	&cert, 
	const CssmData &fieldValue)  
{
	CSSM_X509_EXTENSION_PTR cssmExt = 
		verifySetFreeExtension(fieldValue, false);
	CE_PolicyConstraints *cdsaObj = 
		(CE_PolicyConstraints *)cssmExt->value.parsedValue;
	SecNssCoder &coder = cert.coder();
	NSS_PolicyConstraints *nssObj = 
		(NSS_PolicyConstraints *)coder.malloc(
				sizeof(NSS_PolicyConstraints));
	CL_cssmPolicyConstraintsToNss(cdsaObj, nssObj, coder);
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1PolicyConstraintsTemplate); 
}

bool getFieldPolicyConstraints( 
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	NSS_PolicyConstraints *nssObj;
	CE_PolicyConstraints *cdsaObj;
	bool brtn;
	Allocator &alloc = fieldValue.allocator;

	brtn = cert.GetExtenTop<NSS_PolicyConstraints, 
			CE_PolicyConstraints>(
		index,
		numFields,
		alloc,
		CSSMOID_PolicyConstraints,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	assert(nssObj != NULL);
	CL_nssPolicyConstraintsToCssm(nssObj, cdsaObj, cert.coder(), alloc);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldPolicyConstraints (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_PolicyConstraints *cdsaObj = 
		(CE_PolicyConstraints *)cssmExt->value.parsedValue;
	CL_freeCssmPolicyConstraints(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Inhibit Any Policy
 *** CDSA Format:	CE_InhibitAnyPolicy (an integer)
 *** NSS format:	CSSM_DATA, sizeof(uint32)
 *** OID: 			CSSMOID_InhibitAnyPolicy
 ***/
void setFieldInhibitAnyPolicy(		
	DecodedItem	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	CE_InhibitAnyPolicy *cdsaObj = 
		(CE_InhibitAnyPolicy *)cssmExt->value.parsedValue;
	
	/* Alloc in cert.coder's memory */
	SecNssCoder &coder = cert.coder();
	CSSM_DATA *nssObj = (CSSM_DATA *)coder.malloc(sizeof(CSSM_DATA));
	coder.allocItem(*nssObj, sizeof(uint32));
	
	/* cdsaObj --> nssObj */
	nssObj->Data[0] = (*cdsaObj) >> 24;
	nssObj->Data[1] = (*cdsaObj) >> 16;
	nssObj->Data[2] = (*cdsaObj) >> 8;
	nssObj->Data[3] = *cdsaObj;
	
	/* add to mExtensions */
	cert.addExtension(nssObj, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1IntegerTemplate); 
}

bool getFieldInhibitAnyPolicy(
	DecodedItem 		&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	CSSM_DATA *nssObj;
	CE_InhibitAnyPolicy *cdsaObj;
	bool brtn;
	
	brtn = cert.GetExtenTop<CSSM_DATA, CE_InhibitAnyPolicy>(
		index,
		numFields,
		fieldValue.allocator,
		CSSMOID_InhibitAnyPolicy,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	*cdsaObj = *(nssObj->Data); //%%%FIXME check this
	
	/* pass back to caller */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

