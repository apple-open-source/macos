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
 * CrlFields.cpp - convert between NSS-based NSS_Crl components and CDSA-style
 *                  fields. A major component of DecodedCrl.
 *
 * Created 8/29/2002 by Doug Mitchell. 
 * Copyright (c) 2002 by Apple Computer. 
 */

#include "DecodedCrl.h"
#include <security_utilities/debugging.h>
#include "cldebugging.h"
#include "CLCrlExtensions.h"
#include "CLCertExtensions.h"
#include "CLFieldsCommon.h"
#include "clNssUtils.h"
#include "clNameUtils.h"
#include <security_utilities/utilities.h>
#include <Security/oidscrl.h>
#include <Security/oidscert.h>
#include <Security/cssmerr.h>
#include <Security/x509defs.h>

static void CL_freeCssmExtensions(
	CSSM_X509_EXTENSIONS	&extens,
	Allocator			&alloc);

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
	const DecodedCrl &crl = dynamic_cast<const DecodedCrl &>(item);
	const CSSM_DATA &vers = crl.mCrl.tbs.version;
	if(!tbsGetCheck(vers.Data, index)) {
		/* not present, optional */
		return false;
	}
	fieldValue.copy(vers.Data, vers.Length);
	numFields = 1;
	return true;
}

static void setField_Version (
	DecodedItem 		&item,
	const CssmData		&fieldValue)
{
	DecodedCrl &crl = dynamic_cast<DecodedCrl &>(item);
	CSSM_DATA &vers = crl.mCrl.tbs.version;
	tbsSetCheck(vers.Data, fieldValue, 0, "version");
	crl.coder().allocCopyItem(fieldValue, vers);
}

/*** issuer
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
	
	const DecodedCrl &crl = dynamic_cast<const DecodedCrl &>(item);
	try {
		brtn = getField_RDN_NSS(crl.mCrl.tbs.issuer, fieldValue);
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
	DecodedItem 		&item,	
	const CssmData		&fieldValue)
{
	DecodedCrl &crl = dynamic_cast<DecodedCrl &>(item);
	const CSSM_X509_NAME *cssmName = (const CSSM_X509_NAME *)fieldValue.Data;
	NSS_Name &nssName = crl.mCrl.tbs.issuer;
	tbsSetCheck(nssName.rdns, fieldValue, sizeof(CSSM_X509_NAME),
		"IssuerName");
	CL_cssmNameToNss(*cssmName, nssName, crl.coder());
}

/***
 *** This/Next update
 *** Format: CSSM_X509_TIME
 ***/
static bool getField_ThisUpdate (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCrl &crl = dynamic_cast<const DecodedCrl &>(item);
	const NSS_Time &srcTime = crl.mCrl.tbs.thisUpdate;
	return getField_TimeNSS(srcTime, index, numFields, fieldValue);
}

static void setField_ThisUpdate (
	DecodedItem 		&item,
	const CssmData		&fieldValue)
{
	DecodedCrl &crl = dynamic_cast<DecodedCrl &>(item);
	NSS_Time &dstTime = crl.mCrl.tbs.thisUpdate;
	tbsSetCheck(dstTime.item.Data, fieldValue, 
		sizeof(CSSM_X509_TIME), "NotBefore");
	setField_TimeNSS(fieldValue, dstTime, crl.coder());
}

static bool getField_NextUpdate (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCrl &crl = dynamic_cast<const DecodedCrl &>(item);
	const NSS_Time &srcTime = crl.mCrl.tbs.nextUpdate;
	return getField_TimeNSS(srcTime, index, numFields, fieldValue);
}

static void setField_NextUpdate (
	DecodedItem 		&item,
	const CssmData		&fieldValue)
{
	DecodedCrl &crl = dynamic_cast<DecodedCrl &>(item);
	NSS_Time &dstTime = crl.mCrl.tbs.nextUpdate;
	tbsSetCheck(dstTime.item.Data, fieldValue, 
		sizeof(CSSM_X509_TIME), "NotBefore");
	setField_TimeNSS(fieldValue, dstTime, crl.coder());
}

/***
 *** Issuer Name (normalized and encoded version)
 *** Format = CSSM_DATA containing the DER encoding of the normalized name
 ***/
static bool getFieldIssuerNorm(
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	if(index != 0) {
		return false;
	}
	const DecodedCrl &crl = dynamic_cast<const DecodedCrl &>(item);
	return getField_normRDN_NSS(crl.mCrl.tbs.derIssuer, numFields, 
		fieldValue);
}

/***
 *** TBS AlgId
 *** Format = CSSM_X509_ALGORITHM_IDENTIFIER
 ***/
static bool getField_CrlTbsAlgId (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	const DecodedCrl &crl = dynamic_cast<const DecodedCrl &>(item);
	const CSSM_X509_ALGORITHM_IDENTIFIER &srcAlgId = 
		crl.mCrl.signatureAlgorithm;
	if(!tbsGetCheck(srcAlgId.algorithm.Data, index)) {
		return false;
	}
	getField_AlgIdNSS(srcAlgId, fieldValue);
	numFields = 1;
	return true;
}

/*
 * Support for entries in revocation list 
 */
static void nssRevokedEntryToCssm(
	NSS_RevokedCert						&nssEntry,
	CSSM_X509_REVOKED_CERT_ENTRY 		&cssmEntry,
	Allocator						&alloc)
{
	clAllocCopyData(alloc, nssEntry.userCertificate, cssmEntry.certificateSerialNumber);
	CL_nssTimeToCssm(nssEntry.revocationDate, cssmEntry.revocationDate, alloc);
	
	/* CSSM_X509_EXTENSIONS extensions */
	NSS_CertExtension 	**nssExtens = nssEntry.extensions;
	if(nssExtens == NULL) {
		/* done */
		return;
	}
	
	/* 
	 * First we have to decode the NSS-style Extensions into a 
	 * DecodedExtensions object. For cert- and CRL-wide extensions, this
	 * is done at the construction of Decoded{Cert,Crl}. However for 
	 * per-CRL-entry entensions, this is (currently) the only place
	 * this decoding is done. 
	 */
	SecNssCoder coder;
	DecodedExtensions decodedExtens(coder, alloc);
	decodedExtens.decodeFromNss(nssExtens);
	
	/* convert to CDSA style */
	decodedExtens.convertToCdsa(cssmEntry.extensions, alloc);
}

static void freeCssmEntry(
	CSSM_X509_REVOKED_CERT_ENTRY_PTR 	cssmEntry,
	Allocator						&alloc)
{
	if(cssmEntry == NULL) {
		return;
	}
	if(cssmEntry->certificateSerialNumber.Data) {
		alloc.free(cssmEntry->certificateSerialNumber.Data);
		cssmEntry->certificateSerialNumber.Data = NULL;
		cssmEntry->certificateSerialNumber.Length = 0;
	}
	CL_freeCssmTime(&cssmEntry->revocationDate, alloc);

	/* CSSM_X509_EXTENSIONS extensions */
	CL_freeCssmExtensions(cssmEntry->extensions, alloc);

	memset(cssmEntry, 0, sizeof(CSSM_X509_REVOKED_CERT_ENTRY));
}

static void nssRevokedListToCssm(
	NSS_RevokedCert					**nssList,		// may be NULL
	CSSM_X509_REVOKED_CERT_LIST_PTR	cssmList,
	Allocator					&alloc)
{
	unsigned numEntries = clNssArraySize((const void **)nssList);
	cssmList->numberOfRevokedCertEntries = numEntries;
	if(numEntries == 0) {
		cssmList->revokedCertEntry = NULL;
		return;
	}
	cssmList->revokedCertEntry = (CSSM_X509_REVOKED_CERT_ENTRY_PTR)alloc.malloc(
		sizeof(CSSM_X509_REVOKED_CERT_ENTRY) * numEntries);
	memset(cssmList->revokedCertEntry, 0, 
		sizeof(CSSM_X509_REVOKED_CERT_ENTRY) * numEntries);
	for(unsigned dex=0; dex<numEntries; dex++) {
		NSS_RevokedCert *nssEntry = nssList[dex];
		assert(nssEntry != NULL);
		CSSM_X509_REVOKED_CERT_ENTRY_PTR cssmEntry = 
			&cssmList->revokedCertEntry[dex];
		nssRevokedEntryToCssm(*nssEntry, *cssmEntry, alloc);
	}
}


static void freeCssmRevokedList(
	CSSM_X509_REVOKED_CERT_LIST_PTR 	cssmList,
	Allocator						&alloc)
{
	if(cssmList == NULL) {
		return;
	}
	for(unsigned dex=0; dex<cssmList->numberOfRevokedCertEntries; dex++) {
		CSSM_X509_REVOKED_CERT_ENTRY_PTR cssmEntry = 
			&cssmList->revokedCertEntry[dex];
		freeCssmEntry(cssmEntry, alloc);
	}
	if(cssmList->revokedCertEntry) {
		alloc.free(cssmList->revokedCertEntry);
	}
	memset(cssmList, 0, sizeof(CSSM_X509_REVOKED_CERT_LIST));
}

/***
 *** SignedCRL
 *** Format: CSSM_X509_SIGNED_CRL (the whole enchilada, parsed)
 ***/
static bool getField_SignedCrl (
	DecodedItem		 	&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue)	// RETURNED
{
	Allocator 			&alloc = fieldValue.allocator;
	
	const DecodedCrl &nssCrl = dynamic_cast<const DecodedCrl &>(item);
	const NSS_TBSCrl &nssTbs = nssCrl.mCrl.tbs;
	fieldValue.malloc(sizeof(CSSM_X509_SIGNED_CRL));
	CSSM_X509_SIGNED_CRL	&cssmCrl = *((CSSM_X509_SIGNED_CRL *)fieldValue.data());

	memset(&cssmCrl, 0, sizeof(CSSM_X509_SIGNED_CRL));
	CSSM_X509_TBS_CERTLIST &cssmTbs = cssmCrl.tbsCertList;

	/* version */
	clAllocCopyData(alloc, nssTbs.version, cssmTbs.version);
	
	/* CSSM_X509_ALGORITHM_IDENTIFIER signature - in TBS and CRL */
	CL_copyAlgId(nssTbs.signature, cssmTbs.signature, alloc);
	CL_copyAlgId(nssCrl.mCrl.signatureAlgorithm, 
			cssmCrl.signature.algorithmIdentifier, alloc);
	
	/* CSSM_X509_NAME issuer */
	CL_nssNameToCssm(nssTbs.issuer, cssmTbs.issuer, alloc);
	
	/* CSSM_X509_TIME thisUpdate, nextUpdate */
	CL_nssTimeToCssm(nssTbs.thisUpdate, cssmTbs.thisUpdate, alloc);
	CL_nssTimeToCssm(nssTbs.nextUpdate, cssmTbs.nextUpdate, alloc);
	
	/* CSSM_X509_REVOKED_CERT_LIST_PTR revokedCertificates */
	if(nssTbs.revokedCerts != NULL) {
		cssmTbs.revokedCertificates = (CSSM_X509_REVOKED_CERT_LIST_PTR)
			alloc.malloc(sizeof(CSSM_X509_REVOKED_CERT_LIST));
		memset(cssmTbs.revokedCertificates, 0, sizeof(CSSM_X509_REVOKED_CERT_LIST));
		nssRevokedListToCssm(nssTbs.revokedCerts,
			cssmTbs.revokedCertificates, alloc);
	}
	
	/* CSSM_X509_EXTENSIONS extensions */
	const DecodedExtensions &decodedExtens = nssCrl.decodedExtens();
	decodedExtens.convertToCdsa(cssmTbs.extensions, alloc);
	
	/* raw signature - stored in bits - note signature.algId set above */
	CSSM_DATA nssSig = nssCrl.mCrl.signature;
	nssSig.Length = (nssSig.Length + 7) / 8;
	clAllocCopyData(alloc, nssSig, cssmCrl.signature.encrypted);
	numFields = 1;
	return true;
}

static void setField_SignedCrl (
	DecodedItem 		&item,
	const CssmData		&fieldValue)
{
	/* TBD - writing CRLs not supported now */
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void freeField_SignedCrl (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_SIGNED_CRL *cssmCrl = 
		(CSSM_X509_SIGNED_CRL *)fieldValue.data();
		
	if(cssmCrl == NULL) {
		return;
	}
	if(fieldValue.length() != sizeof(CSSM_X509_SIGNED_CRL)) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	Allocator &alloc = fieldValue.allocator;
	CSSM_X509_TBS_CERTLIST_PTR cssmTbs = &cssmCrl->tbsCertList;
	if(cssmTbs == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
	}
	
	/* run down the fields */
	if(cssmTbs->version.Data) {
		alloc.free(cssmTbs->version.Data);
	}

	/* CSSM_X509_ALGORITHM_IDENTIFIER signature - in TBS and CRL */
	CL_freeCssmAlgId(&cssmTbs->signature, alloc);
	CL_freeCssmAlgId(&cssmCrl->signature.algorithmIdentifier, alloc);

	/* issuer, thisUpdate, nextUpdate */
	CL_freeX509Name(&cssmTbs->issuer, alloc);
	CL_freeCssmTime(&cssmTbs->thisUpdate, alloc);
	CL_freeCssmTime(&cssmTbs->nextUpdate, alloc);
	
	/* CSSM_X509_REVOKED_CERT_LIST_PTR revokedCertificates */
	freeCssmRevokedList(cssmTbs->revokedCertificates, alloc);
	alloc.free(cssmTbs->revokedCertificates);
	
	/* CSSM_X509_EXTENSIONS extensions */
	CL_freeCssmExtensions(cssmTbs->extensions, alloc);

	/* raw signature - note signature.algId freed above */
	alloc.free(cssmCrl->signature.encrypted.Data);
	
	memset(cssmCrl, 0, sizeof(CSSM_X509_SIGNED_CRL));
}

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


static const oidToFieldFuncs crlFieldTable[] = {
	/* this first one, which returns everything in a parsed format, 
	 * is intended to be normally the only field used */
	{ 	&CSSMOID_X509V2CRLSignedCrlCStruct,
		&getField_SignedCrl, &setField_SignedCrl, &freeField_SignedCrl },
	{ 	&CSSMOID_X509V2CRLVersion, 	
		&getField_Version, &setField_Version, NULL },
	{ 	&CSSMOID_X509V1CRLIssuerNameCStruct, 
		&getField_Issuer, &setField_Issuer, &freeField_RDN },
	{ 	&CSSMOID_X509V1CRLThisUpdate, 
		&getField_ThisUpdate, &setField_ThisUpdate, &freeField_Time },
	{ 	&CSSMOID_X509V1CRLNextUpdate, 
		&getField_NextUpdate, &setField_NextUpdate, &freeField_Time },
	{   &CSSMOID_X509V1IssuerName, 
		getFieldIssuerNorm, &setField_ReadOnly, NULL },
	{	&CSSMOID_X509V1SignatureAlgorithmTBS,
		&getField_CrlTbsAlgId, &setField_ReadOnly, &freeField_AlgId },
		// ...etc..
	/* 
	 * Extensions, implemented in CrlExtensions.cpp 
	 * When adding new ones, also add to:
	 *   -- clOidToNssInfo() in CLFieldsCommon.cpp
	 *   -- get/set/free functions in CrlExtensions.{cpp,h}
	 *   -- DecodedExten::parse in DecodedExtensions.cpp
	 */
	{	&CSSMOID_CrlNumber,
		&getFieldCrlNumber, &setFieldCrlNumber, freeFieldSimpleExtension },
	{	&CSSMOID_DeltaCrlIndicator,
		&getFieldDeltaCrl, &setFieldCrlNumber, freeFieldSimpleExtension },
	{	&CSSMOID_CertIssuer,				// get/set not implemented
		&getField_Unimplemented, &setField_ReadOnly,			
		&freeFieldSubjIssuerAltName},
	{	&CSSMOID_CrlReason,					// get/set not implemented
		&getField_Unimplemented, &setField_ReadOnly, 
		freeFieldSimpleExtension},		
	{	&CSSMOID_IssuingDistributionPoint,	// get/set not implemented
		&getField_Unimplemented, &setField_ReadOnly,			
		&freeFieldIssuingDistPoint},
	{	&CSSMOID_HoldInstructionCode,		// get/set not implemented
		&getField_Unimplemented, &setField_ReadOnly,			
		&freeFieldOidOrData},
	{	&CSSMOID_InvalidityDate,			// get/set not implemented
		&getField_Unimplemented, &setField_ReadOnly,			
		&freeFieldOidOrData},
		
	/* in common with CertExtensions */
	{	&CSSMOID_AuthorityKeyIdentifier, &getFieldAuthorityKeyId,
		&setFieldAuthorityKeyId, &freeFieldAuthorityKeyId } ,
	{   &CSSMOID_X509V3CertificateExtensionCStruct, &getFieldUnknownExt,
		&setFieldUnknownExt, &freeFieldUnknownExt },
	{	&CSSMOID_SubjectAltName, &getFieldSubjAltName,
		&setFieldSubjIssuerAltName, &freeFieldSubjIssuerAltName } ,
	{	&CSSMOID_IssuerAltName, &getFieldIssuerAltName,
		&setFieldSubjIssuerAltName, &freeFieldSubjIssuerAltName } ,

	{	&CSSMOID_CrlDistributionPoints,	// get/set not implemented
		&getField_Unimplemented, &setField_ReadOnly,			
		&freeFieldCrlDistributionPoints},
	// etc..
};

#define NUM_KNOWN_FIELDS		(sizeof(crlFieldTable) / sizeof(oidToFieldFuncs))
#define NUM_STD_CRL_FIELDS		2		/* TBD not including extensions */

/* map an OID to an oidToFieldFuncs */
static const oidToFieldFuncs *oidToFields(
	const CssmOid			&fieldId)
{
	const oidToFieldFuncs *fieldTable = crlFieldTable;
	for(unsigned i=0; i<NUM_KNOWN_FIELDS; i++) {
		if(fieldId == CssmData::overlay(*fieldTable->fieldId)) {
			return fieldTable;
		}
		fieldTable++;
	}
	CssmError::throwMe(CSSMERR_CL_UNKNOWN_TAG);
}

/* 
 * Common routine to free OID-specific field data. Used in the 
 * public DecodedCrl::freeCrlFieldData and when freeing 
 * extensions in a CSSM_X509_TBS_CERTLIST.
 */
static void CL_freeCrlFieldData(
	const CssmOid		&fieldId,
	CssmOwnedData		&fieldValue,
	bool				reset = true)
{
	if((fieldValue.data() == NULL) || (fieldValue.length() == 0)) {
		CssmError::throwMe(CSSM_ERRCODE_INVALID_FIELD_POINTER);
	}
	const oidToFieldFuncs *fieldFuncs = oidToFields(fieldId);
	if(fieldFuncs->freeFcn != NULL) {
		/* optional - simple cases handled below */
		fieldFuncs->freeFcn(fieldValue);
	}
	if(reset) {
		fieldValue.reset();
		fieldValue.release();
	}
}

/*
 * Common routime to free a CSSM_X509_EXTENSIONS. Used to free 
 * CSSM_X509_TBS_CERTLIST.extensions and 
 * CSSM_X509_REVOKED_CERT_ENTRY.extensions. 
 * We just cook up a CssmOid and a CssmOwnedData for each extension
 * and pass to CL_freeCrlFieldData().
 */
static void CL_freeCssmExtensions(
	CSSM_X509_EXTENSIONS	&extens,
	Allocator			&alloc)
{
	for(uint32 dex=0; dex<extens.numberOfExtensions; dex++) {
		CSSM_X509_EXTENSION_PTR exten = &extens.extensions[dex];
		const CSSM_OID *fieldOid;
		
		/* 
		 * The field OID is either the same as the extension's OID (if we parsed
		 * it) or CSSMOID_X509V3CertificateExtensionCStruct (if we didn't).
		 */
		switch(exten->format) {
			case CSSM_X509_DATAFORMAT_ENCODED:
				fieldOid = &CSSMOID_X509V3CertificateExtensionCStruct;
				break;
			case CSSM_X509_DATAFORMAT_PARSED:
			case CSSM_X509_DATAFORMAT_PAIR:
				fieldOid = &exten->extnId;
				break;
			default:
				clErrorLog("CL_freeCssmExtensions: bad exten->format (%d)",
					(int)exten->format);
				CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);
		}
		
		const CssmOid &fieldId = CssmOid::overlay(*fieldOid);
        
        if (exten->extnId.Data != NULL)  // if this is null, something threw when it was instantiated
        {
            CssmData cData((uint8 *)exten, sizeof(CSSM_X509_EXTENSION));
            CssmRemoteData fieldValue(alloc, cData);
            CL_freeCrlFieldData(fieldId, fieldValue, false);
            fieldValue.release();			// but no free (via reset() */
        }
	}
	alloc.free(extens.extensions);
	memset(&extens, 0, sizeof(CSSM_X509_EXTENSIONS));
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
bool DecodedCrl::getCrlFieldData(
	const CssmOid		&fieldId,		// which field
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 	// RETURNED
{ 
	switch(mState) {
		case IS_Empty:		
		case IS_Building:	
			clErrorLog("DecodedCrl::getCrlField: can't parse undecoded CRL!");
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
void DecodedCrl::setCrlField(
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
			clErrorLog("DecodedCrl::setCrlField: can't build on a decoded CRL!");
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
 * No state from DecodedCrl needed; use the routine shared with 
 * CL_freeCssmExtensions().
 */
void DecodedCrl::freeCrlFieldData(
	const CssmOid		&fieldId,
	CssmOwnedData		&fieldValue)
{
	CL_freeCrlFieldData(fieldId, fieldValue);
}


/*
 * Common means to get all fields from a decoded CRL. Used in 
 * CrlGetAllTemplateFields and CrlGetAllFields.
 */
void DecodedCrl::getAllParsedCrlFields(
	uint32 				&NumberOfFields,		// RETURNED
	CSSM_FIELD_PTR 		&CrlFields)				// RETURNED
{
	/* this is the max - some might be missing */
	uint32 maxFields = NUM_STD_CRL_FIELDS + mDecodedExtensions.numExtensions();
	CSSM_FIELD_PTR outFields = (CSSM_FIELD_PTR)mAlloc.malloc(
		maxFields * sizeof(CSSM_FIELD));
	
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
		const oidToFieldFuncs *fieldFuncs = &crlFieldTable[currOidDex];
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
		
		/* got some data for this oid - copy it and oid to outgoing CrlFields */
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
				clErrorLog("getAllParsedCrlFields: index screwup");
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
	CrlFields = outFields;
}

void
DecodedCrl::describeFormat(
	Allocator &alloc,
	uint32 &NumberOfFields,
	CSSM_OID_PTR &OidList)
{
	/* malloc in app's space, do deep copy (including ->Data) */
	CSSM_OID_PTR oidList = (CSSM_OID_PTR)alloc.malloc(
		NUM_KNOWN_FIELDS * sizeof(CSSM_OID));
	memset(oidList, 0, NUM_KNOWN_FIELDS * sizeof(CSSM_OID));
	for(unsigned i=0; i<NUM_KNOWN_FIELDS; i++) {
		CssmAutoData oidCopy(alloc, *crlFieldTable[i].fieldId);
		oidList[i] = oidCopy.release();
	}
	NumberOfFields = NUM_KNOWN_FIELDS;
	OidList = oidList;
}
