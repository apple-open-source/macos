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
	certGroupUtils.cpp

	Created 10/9/2000 by Doug Mitchell. 
*/

#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include <Security/x509defs.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <Security/cssmapple.h>

#include "certGroupUtils.h" 
#include "cldebugging.h"
#include "tpTime.h"

#include <string.h>				/* for memcmp */

#if 0
void *tpCalloc(CssmAllocator &alloc, uint32 num, uint32 size)
{
	void *p = alloc.malloc(num * size);
	memset(p, 0, num* size);
	return p;
}  
#endif

/*
 * Copy one CSSM_DATA to another, mallocing destination. 
 */
void tpCopyCssmData(
	CssmAllocator	&alloc,
	const CSSM_DATA	*src,
	CSSM_DATA_PTR	dst)
{
	dst->Data = (uint8 *)alloc.malloc(src->Length);
	dst->Length = src->Length;
	memmove(dst->Data, src->Data, src->Length);
}

/*
 * Malloc a CSSM_DATA, copy another one to it.
 */
CSSM_DATA_PTR tpMallocCopyCssmData(
	CssmAllocator	&alloc,
	const CSSM_DATA	*src)
{
	CSSM_DATA_PTR dst = (CSSM_DATA_PTR)alloc.malloc(sizeof(CSSM_DATA));
	tpCopyCssmData(alloc, src, dst);
	return dst;
}

/*
 * Free the data referenced by a CSSM data, and optionally, the struct itself.
 */
void tpFreeCssmData(
	CssmAllocator	&alloc,
	CSSM_DATA_PTR 	data,
	CSSM_BOOL 		freeStruct)
{
	if(data == NULL) {
		return;
	}
	if(data->Length != 0) {
		tpFree(alloc, data->Data);
	}
	if(freeStruct) {
		tpFree(alloc, data);
	}
	else {
		data->Length = 0;
		data->Data = NULL;
	}
}

/*
 * Compare two CSSM_DATAs, return CSSM_TRUE if identical.
 */
CSSM_BOOL tpCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return CSSM_FALSE;
	}
	if(data1->Length != data2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
		return CSSM_TRUE;
	}
	else {
		return CSSM_FALSE;
	}
}

/*
 * Compare two OIDs, return CSSM_TRUE if identical.
 */
CSSM_BOOL tpCompareOids(
	const CSSM_OID *oid1,
	const CSSM_OID *oid2)
{	
	/*
	 * This should break if/when CSSM_OID is not the same as
	 * CSSM_DATA, which is exactly what we want.
	 */
	return tpCompareCssmData(oid1, oid2);
}

/*
 * Obtain the public key blob from a cert.
 */
CSSM_DATA_PTR tp_CertGetPublicKey( 
    TPCertInfo *cert,
	CSSM_DATA_PTR *valueToFree)			// used in tp_CertFreePublicKey
{
	CSSM_RETURN crtn;
	CSSM_DATA_PTR val;
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *keyInfo;
	
	*valueToFree = NULL;
	crtn = cert->fetchField(&CSSMOID_X509V1SubjectPublicKeyCStruct, &val);
	if(crtn) {
		errorLog0("Error on CSSM_CL_CertGetFirstFieldValue(PublicKeyCStruct)\n");
		return NULL;
	}
	*valueToFree = val;
	keyInfo = (CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)val->Data;
	return &keyInfo->subjectPublicKey;
}

void tp_CertFreePublicKey(
	CSSM_CL_HANDLE	clHand,
	CSSM_DATA_PTR	value)
{
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SubjectPublicKeyCStruct, value);
}

/*
 * Obtain signature algorithm info from a cert.
 */
CSSM_X509_ALGORITHM_IDENTIFIER_PTR tp_CertGetAlgId( 
    TPCertInfo	 	*cert,
	CSSM_DATA_PTR 	*valueToFree)			// used in tp_CertFreeAlgId
{
	CSSM_RETURN crtn;
	CSSM_DATA_PTR val;
	
	*valueToFree = NULL;
	crtn = cert->fetchField(&CSSMOID_X509V1SignatureAlgorithm, &val);
	if(crtn) {
		errorLog0("Error on fetchField(CSSMOID_X509V1SignatureAlgorithm)\n");
		return NULL;
	}
	*valueToFree = val;
	return (CSSM_X509_ALGORITHM_IDENTIFIER_PTR)val->Data;
}

void tp_CertFreeAlgId(
	CSSM_CL_HANDLE	clHand,
	CSSM_DATA_PTR	value)
{
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SignatureAlgorithm, value);
}

/* 
 * Compare two DER-encoded normalized names.
 */
CSSM_BOOL tpIsSameName( 
	const CSSM_DATA *name1,
	const CSSM_DATA *name2)
{
	return tpCompareCssmData(name1, name2);
}


/*
 * Given a TP handle, a CSP handle, a CL handle, and two certs, verify
 * subjectCert with issuerCert. If checkIssuerExpired is CSSM_TRUE, 
 * we'll do a not before/after check of the issuer only if the 
 * signature verify  passes. The rationale is that we're not interested 
 * in this condition for potential issuers which fail the sig verify. 
 *
 * Returns:
 *		CSSM_OK
 *		CSSMERR_TP_VERIFICATION_FAILURE		-- sig verify failure
 *		CSSMERR_TP_CERT_EXPIRED
 *		CSSMERR_TP_CERT_NOT_VALID_YET
 */
CSSM_RETURN tp_VerifyCert(
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	TPCertInfo				*subjectCert,
	TPCertInfo				*issuerCert,
	CSSM_BOOL				checkIssuerCurrent,
	CSSM_BOOL				allowExpired)
{
	CSSM_RETURN			crtn;

	crtn = CSSM_CL_CertVerify(clHand, 
    	CSSM_INVALID_HANDLE, 
    	subjectCert->certData(),
    	issuerCert->certData(),
    	NULL,				// VerifyScope
    	0);					// ScopeSize
	if(crtn == CSSM_OK) {
		if(checkIssuerCurrent) {
			/* also verify validity of issuer */
			crtn = issuerCert->isCurrent(allowExpired);
		}
	}
	else {
		/* general cert verify failure */
		crtn = CSSMERR_TP_VERIFICATION_FAILURE;
	}
	return crtn;
}

/*
 * Determine if two certs - passed in encoded form - are equivalent. 
 */
CSSM_BOOL tp_CompareCerts(
	const CSSM_DATA			*cert1,
	const CSSM_DATA			*cert2)
{
	return tpCompareCssmData(cert1, cert2);
}

#if		TP_DL_ENABLE
/*
 * Given a DL/DB, look up cert by subject name. Subsequent
 * certs can be found using the returned result handle. 
 */
static CSSM_DB_UNIQUE_RECORD_PTR tpCertLookup(
	CSSM_TP_HANDLE		tpHand,
	CSSM_DL_DB_HANDLE	dlDb,
	const CSSM_DATA_PTR	subjectName,	// DER-encoded
	CSSM_HANDLE_PTR		resultHand,
	CSSM_DATA_PTR		cert)			// RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;	
	CSSM_BOOL  						EndOfDataStore;
	CSSM_DB_UNIQUE_RECORD_PTR		record;
	
	cert->Data = NULL;
	cert->Length = 0;
	
	predicate.DbOperator = CSSM_DB_EQUAL;
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_NUMBER;		// may not be needed
	predicate.Attribute.Info.Attr.AttributeNumber = kSubjectKCItemAttr;
	predicate.Attribute.Value = *subjectName;
	
	query.RecordType = CSSM_DL_DB_RECORD_CERT;
	query.NumSelectionPredicates = 1;
	query.Conjunctive = CSSM_DB_NONE;
	
	query.SelectionPredicate = &predicate;
	
	record = CSSM_DL_DataGetFirst(dlDb,
		&query,
		resultHand,
		&EndOfDataStore,
		NULL,				// don't fetch attributes
		cert);
	return record;
}

/*
 * Search a list of DBs for a cert which verifies specified subject cert. 
 * Just a boolean return - we found it, or not. If we did, we return
 * a pointer to the raw cert. 
 *
 * Special case of subject cert expired indicated by *subjectExpired 
 * returned as something other than CSSM_OK.
 */
CSSM_DATA_PTR tpFindIssuer(
	CSSM_TP_HANDLE			tpHand,
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	const CSSM_DATA_PTR		subjectCert,
	const CSSM_DATA_PTR		issuerName,			// passed for convenience
	const CSSM_DB_LIST_PTR	dbList,
	CSSM_RETURN				*issuerExpired)		// RETURNED
{
	uint32						dbDex;
	CSSM_HANDLE					resultHand;
	CSSM_DATA_PTR				cert;					// we malloc
	CSSM_DL_DB_HANDLE			dlDb;
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	
	*subjectExpired = CSSM_OK;
	if(dbList == NULL) {
		return NULL;
	}
	cert = (CSSM_DATA_PTR)tpMalloc(tpHand, sizeof(CSSM_DATA));
	cert->Data = NULL;
	cert->Length = 0;
	
	for(dbDex=0; dbDex<dbList->NumHandles; dbDex++) {
		dlDb = dbList->DLDBHandle[dbDex];
		record = tpCertLookup(tpHand,
			dlDb,
			issuerName,
			&resultHand,
			cert);
		/* remember we have to abort this query regardless...*/
		if(record != NULL) {
			/* Found one. Does it verify the subject cert? */
			if(!tp_VerifyCert(tpHand,
					clHand,
					cspHand,
					subjectCert,
					cert,
					issuerExpired)) {
					
				/* special case - abort immediately if issuerExpired has expired */
				if((*issuerExpired) != CSSM_OK) {
					CSSM_DL_AbortQuery(dlDb, resultHand);
					goto abort;
				}
				
				/*
				 * Verify fail. Continue searching this DB. Break on 
				 * finding the holy grail or no more records found. 
				 */
				for(;;) {
					CSSM_BOOL eod;
					
					tpFreeCssmData(tpHand, cert, CSSM_FALSE);
					record = CSSM_DL_DataGetNext(dlDb, 
						resultHand,
						&eod,
						NULL,		// no attrs 
						cert);
					if(record == NULL) {
						/* no more, done with this DB */
						break;
					}
					
					/* found one - does it verify subject? */
					if(tp_VerifyCert(tpHand,
							clHand,
							cspHand,
							subjectCert,
							cert,
							issuerExpired)) {
						/* yes! */
						break;
					}
					else if((*issuerExpired) != CSSM_OK) {
						/* abort immediately */
						CSSM_DL_AbortQuery(dlDb, resultHand);
						goto abort;
					}
				} /* searching subsequent records */
			}	/* verify fail */
			/* else success! */

			if(record != NULL) {
				/* successful return */
				CSSM_DL_AbortQuery(dlDb, resultHand);
				return cert;
			}
		}	/* tpCertLookup, i.e., CSSM_DL_DataGetFirst, succeeded */
		
		/* in any case, abort the query for this db */
		CSSM_DL_AbortQuery(dlDb, resultHand);
		
	}	/* main loop searching dbList */

abort:
	/* issuer not found */
	tpFreeCssmData(tpHand, cert, CSSM_TRUE);
	return NULL;
}

#endif	/* TP_DL_ENABLE */

/*
 * Given a aignature OID, return the corresponding CSSM_ALGID for the 
 * signature the required key.
 */
CSSM_ALGORITHMS tpOidToAldId(
	const CSSM_OID *oid,
	CSSM_ALGORITHMS *keyAlg)			// RETURNED
{
	*keyAlg = CSSM_ALGID_RSA;			// default
	if(tpCompareOids(oid, &CSSMOID_MD2WithRSA)) {
		return CSSM_ALGID_MD2WithRSA;
	}
	else if(tpCompareOids(oid, &CSSMOID_MD5WithRSA)) {
		return CSSM_ALGID_MD5WithRSA;
	}
	else if(tpCompareOids(oid, &CSSMOID_SHA1WithRSA)) {
		return CSSM_ALGID_SHA1WithRSA;
	}
	else if(tpCompareOids(oid, &CSSMOID_SHA1WithDSA)) {
		*keyAlg = CSSM_ALGID_DSA;
		return CSSM_ALGID_SHA1WithDSA;
	}
	else if(tpCompareOids(oid, &CSSMOID_APPLE_FEE_MD5)) {
		*keyAlg = CSSM_ALGID_FEE;
		return CSSM_ALGID_FEE_MD5;
	}
	else if(tpCompareOids(oid, &CSSMOID_APPLE_FEE_SHA1)) {
		*keyAlg = CSSM_ALGID_FEE;
		return CSSM_ALGID_FEE_SHA1;
	}
	else if(tpCompareOids(oid, &CSSMOID_APPLE_ECDSA)) {
		*keyAlg = CSSM_ALGID_FEE;
		return CSSM_ALGID_SHA1WithECDSA;
	}
	else {
		*keyAlg = CSSM_ALGID_NONE;
		return CSSM_ALGID_NONE;
	}
}
