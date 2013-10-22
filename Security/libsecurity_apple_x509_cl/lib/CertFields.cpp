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
 * CertFields.cpp - convert between NSS-based Certificate components and CDSA-style
 *                  fields. A major component of DecodedCert.
 *
 * Created 9/1/2000 by Doug Mitchell.
 * Copyright (c) 2000 by Apple Computer.
 */

#include "DecodedCert.h"
#include "cldebugging.h"
#include "CLCertExtensions.h"
#include "clNssUtils.h"
#include "clNameUtils.h"
#include "CLFieldsCommon.h"
#include <Security/oidscert.h>
#include <Security/x509defs.h>
#include <security_utilities/utilities.h>

/***
 *** Version
 *** Format = DER-encoded int (max of four bytes in this case)
 ***/
static bool getField_Version (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_DATA &vers = cert.mCert.tbs.version;
	if(!tbsGetCheck(vers.Data, index)) {
		/* not present, optional */
		return false;
	}
	fieldValue.copy(vers.Data, vers.Length);
	numFields = 1;
	return true;
}

static void setField_Version (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	CSSM_DATA &vers = cert.mCert.tbs.version;
	tbsSetCheck(vers.Data, fieldValue, 0, "version");
	cert.coder().allocCopyItem(fieldValue, vers);
}


#if	this_is_a_template
/***
 *** Version
 *** Format = DER-encoded int (always four bytes in this case)
 ***/
static bool getField_Version (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	tbsGetCheck(cert.certificateToSign->version, index);
}
static void setField_Version (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
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
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_DATA &sn = cert.mCert.tbs.serialNumber;
	if(!tbsGetCheck(sn.Data, index)) {
		return false;
	}
	fieldValue.copy(sn.Data, sn.Length);
	numFields = 1;
	return true;
}

static void setField_SerialNumber (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	CSSM_DATA &sn = cert.mCert.tbs.serialNumber;
	tbsSetCheck(sn.Data, fieldValue, 0, "SerialNumber");
	cert.coder().allocCopyItem(fieldValue, sn);
}

/*** issuer/subject
 *** Format = CSSM_X509_NAME
 *** class Name from sm_x501if
 ***/
static bool getField_Issuer (
	DecodedItem			&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index != 0) {
		return false;
	}

	bool brtn;

	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	try {
		brtn = getField_RDN_NSS(cert.mCert.tbs.issuer, fieldValue);
		if(brtn) {
			numFields = 1;
		}
	}
	catch (...) {
		freeField_RDN(fieldValue);
		throw;
	}
	return brtn;
}

static void setField_Issuer  (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	const CSSM_X509_NAME *cssmName = (const CSSM_X509_NAME *)fieldValue.Data;
	NSS_Name &nssName = cert.mCert.tbs.issuer;
	tbsSetCheck(nssName.rdns, fieldValue, sizeof(CSSM_X509_NAME),
		"IssuerName");
	CL_cssmNameToNss(*cssmName, nssName, cert.coder());
}

/*** subject ***/
static bool getField_Subject (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index != 0) {
		return false;
	}

	bool brtn;

	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	try {
		brtn = getField_RDN_NSS(cert.mCert.tbs.subject, fieldValue);
		if(brtn) {
			numFields = 1;
		}
	}
	catch (...) {
		freeField_RDN(fieldValue);
		throw;
	}
	return brtn;
}

static void setField_Subject  (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	const CSSM_X509_NAME *cssmName = (const CSSM_X509_NAME *)fieldValue.Data;
	NSS_Name &nssName = cert.mCert.tbs.subject;
	tbsSetCheck(nssName.rdns, fieldValue, sizeof(CSSM_X509_NAME),
		"SubjectName");
	CL_cssmNameToNss(*cssmName, nssName, cert.coder());
}

/***
 *** Issuer Name, Subject Name (normalized and encoded version)
 *** Format = CSSM_DATA containing the DER encoding of the normalized name
 ***/
static bool getFieldSubjectNorm(
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index != 0) {
		return false;
	}
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	return getField_normRDN_NSS(cert.mCert.tbs.derSubject, numFields,
		fieldValue);
}

static bool getFieldIssuerNorm(
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index != 0) {
		return false;
	}
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	return getField_normRDN_NSS(cert.mCert.tbs.derIssuer, numFields, fieldValue);
}

/***
 *** Issuer Name, Subject Name (encoded, NON-normalized version)
 *** Format = CSSM_DATA containing the DER encoding of the name
 ***/
static bool getFieldSubjectStd(
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index != 0) {
		return false;
	}
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	fieldValue.copy(cert.mCert.tbs.derSubject);
	numFields = 1;
	return true;
}

static bool getFieldIssuerStd(
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index != 0) {
		return false;
	}
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	fieldValue.copy(cert.mCert.tbs.derIssuer);
	numFields = 1;
	return true;
}

/***
 *** TBS AlgId, Signature AlgId
 *** Format = CSSM_X509_ALGORITHM_IDENTIFIER
 ***/
/* TBS AlgId */
static bool getField_TbsAlgId (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_X509_ALGORITHM_IDENTIFIER &srcAlgId = cert.mCert.tbs.signature;
	if(!tbsGetCheck(srcAlgId.algorithm.Data, index)) {
		return false;
	}
	getField_AlgIdNSS(srcAlgId, fieldValue);
	numFields = 1;
	return true;
}

static void setField_TbsAlgId (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	CSSM_X509_ALGORITHM_IDENTIFIER &dstAlgId = cert.mCert.tbs.signature;
	tbsSetCheck(dstAlgId.algorithm.Data, fieldValue,
		sizeof(CSSM_X509_ALGORITHM_IDENTIFIER), "TBS_AlgId");
	setField_AlgIdNSS(fieldValue, dstAlgId, cert.coder());
}

/* Cert AlgId - read only */
static bool getField_CertAlgId (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_X509_ALGORITHM_IDENTIFIER &srcAlgId = cert.mCert.signatureAlgorithm;
	if(!tbsGetCheck(srcAlgId.algorithm.Data, index)) {
		return false;
	}
	getField_AlgIdNSS(srcAlgId, fieldValue);
	numFields = 1;
	return true;
}

/***
 *** Validity not before, not after
 *** Format: CSSM_X509_TIME
 ***/

/*** not before ***/
static bool getField_NotBefore (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const NSS_Time &srcTime = cert.mCert.tbs.validity.notBefore;
	return getField_TimeNSS(srcTime, index, numFields, fieldValue);
}

static void setField_NotBefore (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	NSS_Time &dstTime = cert.mCert.tbs.validity.notBefore;
	tbsSetCheck(dstTime.item.Data, fieldValue,
		sizeof(CSSM_X509_TIME), "NotBefore");
	setField_TimeNSS(fieldValue, dstTime, cert.coder());
}

/*** not after ***/
static bool getField_NotAfter (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const NSS_Time &srcTime = cert.mCert.tbs.validity.notAfter;
	return getField_TimeNSS(srcTime, index, numFields, fieldValue);
}

static void setField_NotAfter (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	NSS_Time &dstTime = cert.mCert.tbs.validity.notAfter;
	tbsSetCheck(dstTime.item.Data, fieldValue,
		sizeof(CSSM_X509_TIME), "NotAfter");
	setField_TimeNSS(fieldValue, dstTime, cert.coder());
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
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_DATA &srcBits = cert.mCert.tbs.subjectID;
	if(!tbsGetCheck(srcBits.Data, index)) {
		return false;
	}

	/* That CSSM_DATA is a decoded BITSTRING; its length is in bits */
	CSSM_DATA tmp = srcBits;
	tmp.Length = (tmp.Length + 7) / 8;
	fieldValue.copy(tmp.Data, tmp.Length);
	numFields = 1;
	return true;
}

static void setField_SubjectUniqueId (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	CSSM_DATA &dstBits = cert.mCert.tbs.subjectID;
	tbsSetCheck(dstBits.Data, fieldValue, 0, "SubjectUniqueID");
	cert.coder().allocCopyItem(fieldValue, dstBits);
	dstBits.Length *= 8;
}

static bool getField_IssuerUniqueId (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_DATA &srcBits = cert.mCert.tbs.issuerID;
	if(!tbsGetCheck(srcBits.Data, index)) {
		return false;
	}

	/* That CSSM_DATA is a decoded BITSTRING; its length is in bits */
	CSSM_DATA tmp = srcBits;
	tmp.Length = (tmp.Length + 7) / 8;
	fieldValue.copy(tmp.Data, tmp.Length);
	numFields = 1;
	return true;
}

static void setField_IssuerUniqueId (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	CSSM_DATA &dstBits = cert.mCert.tbs.issuerID;
	tbsSetCheck(dstBits.Data, fieldValue, 0, "IssuerUniqueID");
	cert.coder().allocCopyItem(fieldValue, dstBits);
	dstBits.Length *= 8;
}

/***
 *** Public key info
 *** Format = CSSM_X509_SUBJECT_PUBLIC_KEY_INFO
 ***/
static bool getField_PublicKeyInfo (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_X509_SUBJECT_PUBLIC_KEY_INFO &srcInfo =
		cert.mCert.tbs.subjectPublicKeyInfo;
	if(!tbsGetCheck(srcInfo.subjectPublicKey.Data, index)) {
		return false;
	}

	Allocator &alloc = fieldValue.allocator;
	fieldValue.malloc(sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO));
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *dstInfo =
		(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)fieldValue.data();

	CL_copySubjPubKeyInfo(srcInfo, true,		// length in bits here
		*dstInfo, false,						// length in bytes
		alloc);

	numFields = 1;
	return true;
}

static void setField_PublicKeyInfo (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO &dstKeyInfo =
		cert.mCert.tbs.subjectPublicKeyInfo;
	tbsSetCheck(dstKeyInfo.subjectPublicKey.Data, fieldValue,
		sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO), "PubKeyInfo");

	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *srcKeyInfo =
		(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)fieldValue.Data;
	if((srcKeyInfo->subjectPublicKey.Data == NULL) ||
	   (srcKeyInfo->subjectPublicKey.Length == 0)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}

	ArenaAllocator arenaAlloc(cert.coder());
	CL_copySubjPubKeyInfo(*srcKeyInfo, false,	// length in bytes here
		dstKeyInfo, true,						// length in bits
		arenaAlloc);
}

static void freeField_PublicKeyInfo (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *cssmKeyInfo =
		(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)fieldValue.data();
	if(cssmKeyInfo == NULL) {
		return;
	}
	Allocator &alloc = fieldValue.allocator;
	CL_freeCssmAlgId(&cssmKeyInfo->algorithm, alloc);
	alloc.free(cssmKeyInfo->subjectPublicKey.Data);
	memset(cssmKeyInfo, 0, sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO));}

/***
 *** key info from CSSM_KEY
 *** Format = CSSM_KEY
 ***/
static bool getField_PublicKeyStruct (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	if(!tbsGetCheck(cert.mCert.tbs.subjectPublicKeyInfo.subjectPublicKey.Data,
			index)) {
		return false;
	}
	CSSM_KEY_PTR cssmKey = cert.extractCSSMKey(fieldValue.allocator);
	fieldValue.set(reinterpret_cast<uint8 *>(cssmKey), sizeof(CSSM_KEY));
	numFields = 1;
	return true;
}

static void setField_PublicKeyStruct (
	DecodedItem			&item,
	const CssmData		&fieldValue)
{
	DecodedCert &cert = dynamic_cast<DecodedCert &>(item);
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO &dstKeyInfo =
		cert.mCert.tbs.subjectPublicKeyInfo;
	tbsSetCheck(dstKeyInfo.subjectPublicKey.Data, fieldValue,
		sizeof(CSSM_KEY), "PubKeyStruct");

	CSSM_KEY_PTR cssmKey = (CSSM_KEY_PTR)fieldValue.data();
	if((cssmKey->KeyData.Data == NULL) ||
	   (cssmKey->KeyData.Data == 0)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	CL_CSSMKeyToSubjPubKeyInfoNSS(*cssmKey, dstKeyInfo, cert.coder());
}

static void freeField_PublicKeyStruct (
	CssmOwnedData		&fieldValue)
{
	CSSM_KEY_PTR cssmKey = (CSSM_KEY_PTR)fieldValue.data();
	CL_freeCSSMKey(cssmKey, fieldValue.allocator, false);
}

/***
 *** Signature
 *** Format = raw bytes
 *** read-only
 ***/
static bool getField_Signature (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCert &cert = dynamic_cast<const DecodedCert &>(item);
	const CSSM_DATA &sigBits = cert.mCert.signature;
	if(!tbsGetCheck(sigBits.Data, index)) {
		return false;
	}
	fieldValue.copy(sigBits.Data, (sigBits.Length + 7) / 8);
	numFields = 1;
	return true;
}

/***
 *** end of field-specific triplets
 ***/

/*
 * Table to map OID to {get,set,free}field
 */
typedef struct {
	const CSSM_OID		*fieldId;
	getItemFieldFcn		*getFcn;
	setItemFieldFcn		*setFcn;
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
	{   &CSSMOID_X509V1IssuerNameStd,
		getFieldIssuerStd, &setField_ReadOnly, NULL },
	{   &CSSMOID_X509V1SubjectNameStd,
		getFieldSubjectStd, &setField_ReadOnly, NULL },

	/*
	 * Extensions, implemented in CLCertExtensions.cpp
	 * When adding new ones, also add to:
	 *   -- clOidToNssInfo() in CLFieldsCommon.cpp
	 *   -- get/set/free functions in CLCertExtensions.{cpp,h}
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
		&setFieldSubjIssuerAltName, &freeFieldSubjIssuerAltName } ,
	{	&CSSMOID_IssuerAltName, &getFieldIssuerAltName,
		&setFieldSubjIssuerAltName, &freeFieldSubjIssuerAltName } ,
	{	&CSSMOID_CertificatePolicies, &getFieldCertPolicies,
		&setFieldCertPolicies, &freeFieldCertPolicies } ,
	{	&CSSMOID_NetscapeCertType, &getFieldNetscapeCertType,
		&setFieldNetscapeCertType, &freeFieldSimpleExtension } ,
	{	&CSSMOID_CrlDistributionPoints, &getFieldCrlDistPoints,
		&setFieldCrlDistPoints, &freeFieldCrlDistPoints },
	{   &CSSMOID_X509V3CertificateExtensionCStruct, &getFieldUnknownExt,
		&setFieldUnknownExt, &freeFieldUnknownExt },
	{   &CSSMOID_AuthorityInfoAccess, &getFieldAuthInfoAccess,
		&setFieldAuthInfoAccess, &freeFieldInfoAccess },
	{   &CSSMOID_SubjectInfoAccess, &getFieldSubjInfoAccess,
		&setFieldAuthInfoAccess, &freeFieldInfoAccess },
	{	&CSSMOID_QC_Statements, &getFieldQualCertStatements,
		&setFieldQualCertStatements, &freeFieldQualCertStatements },

	{   &CSSMOID_NameConstraints, &getFieldNameConstraints,
		&setFieldNameConstraints, &freeFieldNameConstraints },
	{   &CSSMOID_PolicyMappings, &getFieldPolicyMappings,
		&setFieldPolicyMappings, &freeFieldPolicyMappings },
	{   &CSSMOID_PolicyConstraints, &getFieldPolicyConstraints,
		&setFieldPolicyConstraints, &freeFieldPolicyConstraints },
	{   &CSSMOID_InhibitAnyPolicy, &getFieldInhibitAnyPolicy,
		&setFieldInhibitAnyPolicy, &freeFieldSimpleExtension },

};

#define NUM_KNOWN_FIELDS		(sizeof(fieldFuncTable) / sizeof(oidToFieldFuncs))
#define NUM_STD_CERT_FIELDS		17		/* not including extensions */

/* map an OID to an oidToFieldFuncs */
static const oidToFieldFuncs *oidToFields(
	const CssmOid			&fieldId)
{
	const oidToFieldFuncs *fieldTable = fieldFuncTable;
	for(unsigned i=0; i<NUM_KNOWN_FIELDS; i++) {
		if(fieldId == CssmData::overlay(*fieldTable->fieldId)) {
			return fieldTable;
		}
		fieldTable++;
	}
#ifndef	NDEBUG
	clErrorLog("oidToFields: unknown OID (len=%d): %s\n",
		(int)fieldId.length(), fieldId.toHex().c_str());
#endif
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
	CssmOwnedData		&fieldValue) 	// RETURNED
{
	switch(mState) {
		case IS_Empty:
		case IS_Building:
			clErrorLog("DecodedCert::getCertField: can't parse undecoded cert!");
			CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
		case IS_DecodedAll:
		case IS_DecodedTBS:
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
	switch(mState) {
		case IS_Empty:			// first time thru
			mState = IS_Building;
			break;
		case IS_Building:		// subsequent passes
			break;
		case IS_DecodedAll:
		case IS_DecodedTBS:
			clErrorLog("DecodedCert::setCertField: can't build on a decoded cert!");
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
	uint32 maxFields = NUM_STD_CERT_FIELDS + mDecodedExtensions.numExtensions();
	CSSM_FIELD_PTR outFields = (CSSM_FIELD_PTR)mAlloc.malloc(maxFields * sizeof(CSSM_FIELD));

	/*
	 * We'll be copying oids and values for fields we find into
	 * outFields; current number of valid fields found in numOutFields.
	 */
	memset(outFields, 0, maxFields * sizeof(CSSM_FIELD));
	uint32 			numOutFields = 0;
	CSSM_FIELD_PTR 	currOutField;
	uint32 			currOidDex;
	const CSSM_OID 	*currOid;
	CssmAutoData 	aData(mAlloc);		// for malloc/copy of outgoing data

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
		assert(numOutFields < maxFields);
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
				clErrorLog("getAllParsedCertFields: index screwup");
				CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
			}
			assert(numOutFields < maxFields);
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
	Allocator &alloc,
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
