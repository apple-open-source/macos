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
	CSSM_BOOL				allowExpired)			// to be deleted
{
	CSSM_RETURN			crtn;

	crtn = CSSM_CL_CertVerify(clHand, 
    	CSSM_INVALID_HANDLE, 
    	subjectCert->certData(),
    	issuerCert->certData(),
    	NULL,				// VerifyScope
    	0);					// ScopeSize
	if(crtn == CSSM_OK) {
		#if TP_CERT_CURRENT_CHECK_INLINE
		if(checkIssuerCurrent) {
			/* also verify validity of issuer */
			crtn = issuerCert->isCurrent(allowExpired);
		}
		#endif
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
	CSSM_DL_DB_HANDLE	dlDb,
	const CSSM_DATA		*subjectName,	// DER-encoded
	CSSM_HANDLE_PTR		resultHand,
	CSSM_DATA_PTR		cert)			// RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;	
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	
	cert->Data = NULL;
	cert->Length = 0;
	
	/* SWAG until cert schema nailed down */
	predicate.DbOperator = CSSM_DB_EQUAL;
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = "Subject";
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	predicate.Attribute.Value = const_cast<CSSM_DATA_PTR>(subjectName);
	predicate.Attribute.NumberOfValues = 1;
	
	query.RecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	query.SelectionPredicate = &predicate;
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0;				// FIXME - used?
	
	CSSM_DL_DataGetFirst(dlDb,
		&query,
		resultHand,
		NULL,				// don't fetch attributes
		cert,
		&record);
	return record;
}

/*
 * Search a list of DBs for a cert which verifies specified subject cert. 
 * Just a boolean return - we found it, or not. If we did, we return
 * TPCertInfo associated with the raw cert. 
 *
 * Special case of subject cert expired indicated by *subjectExpired 
 * returned as something other than CSSM_OK.
 */
TPCertInfo *tpFindIssuer(
	CssmAllocator 			&alloc,
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	TPCertInfo				*subjectCert,
	const CSSM_DATA			*issuerName,		// TBD - passed for convenience
	const CSSM_DL_DB_LIST	*dbList,
	const char 				*cssmTimeStr,		// may be NULL
	CSSM_RETURN				*issuerExpired)		// RETURNED
{
	uint32						dbDex;
	CSSM_HANDLE					resultHand;
	CSSM_DATA_PTR				cert;					// we malloc
	CSSM_DL_DB_HANDLE			dlDb;
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	TPCertInfo 					*issuerCert = NULL;
	
	*issuerExpired = CSSM_OK;
	if(dbList == NULL) {
		return NULL;
	}
	cert = (CSSM_DATA_PTR)alloc.malloc(sizeof(CSSM_DATA));
	cert->Data = NULL;
	cert->Length = 0;
	
	for(dbDex=0; dbDex<dbList->NumHandles; dbDex++) {
		dlDb = dbList->DLDBHandle[dbDex];
		record = tpCertLookup(dlDb,
			issuerName,
			&resultHand,
			cert);
		/* remember we have to abort this query regardless...*/
		if(record != NULL) {
			/* Found one. Does it verify the subject cert? */
			issuerCert = new TPCertInfo(cert, clHand, cssmTimeStr, CSSM_TRUE);
			if(tp_VerifyCert(clHand,
					cspHand,
					subjectCert,
					issuerCert,
					CSSM_FALSE,				// check current, ignored 
					CSSM_FALSE)) {			// allowExpired, ignored
					
				delete issuerCert;
				issuerCert = NULL;
				
				/* special case - abort immediately if issuerExpired has expired */
				if((*issuerExpired) != CSSM_OK) {
					CSSM_DL_DataAbortQuery(dlDb, resultHand);
					goto abort;
				}
				
				/*
				 * Verify fail. Continue searching this DB. Break on 
				 * finding the holy grail or no more records found. 
				 */
				for(;;) {
					tpFreeCssmData(alloc, cert, CSSM_FALSE);
					CSSM_RETURN crtn = CSSM_DL_DataGetNext(dlDb, 
						resultHand,
						NULL,		// no attrs 
						cert,
						&record);
					if(crtn) {
						/* no more, done with this DB */
						break;
					}
					
					/* found one - does it verify subject? */
					issuerCert = new TPCertInfo(cert, clHand, cssmTimeStr, 
							CSSM_TRUE);
					if(tp_VerifyCert(clHand,
							cspHand,
							subjectCert,
							issuerCert,
							CSSM_FALSE,
							CSSM_FALSE)) {
						/* yes! */
						break;
					}
					delete issuerCert;
					issuerCert = NULL;
				} /* searching subsequent records */
			}	/* verify fail */
			/* else success! */

			if(issuerCert != NULL) {
				/* successful return */
				CSSM_DL_DataAbortQuery(dlDb, resultHand);
				issuerCert->dlDbHandle(dlDb);
				issuerCert->uniqueRecord(record);
				return issuerCert;
			}
		}	/* tpCertLookup, i.e., CSSM_DL_DataGetFirst, succeeded */
		
		/* in any case, abort the query for this db */
		CSSM_DL_DataAbortQuery(dlDb, resultHand);
		
	}	/* main loop searching dbList */

abort:
	/* issuer not found */
	tpFreeCssmData(alloc, cert, CSSM_TRUE);
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

/*
 * Convert a C string to lower case in place. NULL terminator not needed.
 */
void tpToLower(
	char *str,
	unsigned strLen)
{
	for(unsigned i=0; i<strLen; i++) {
		*str++ = tolower(*str);
	}
}


/*
 * Compare hostname, is presented to the TP in 
 * CSSM_APPLE_TP_SSL_OPTIONS.ServerName, to a server name obtained
 * from the server's cert (i.e., from subjectAltName or commonName).
 * Limited wildcard checking is performed here. 
 *
 * The incoming hostname is assumed to have been processed by tpToLower();
 * we'll perform that processing on serverName here. 
 *
 * Returns CSSM_TRUE on match, else CSSM_FALSE.
 */
CSSM_BOOL tpCompareHostNames(
	const char	 	*hostName,		// spec'd by app, tpToLower'd
	uint32			hostNameLen,
	char			*serverName,	// from cert, we tpToLower
	uint32			serverNameLen)
{
	tpToLower(serverName, serverNameLen);

	/* tolerate optional NULL terminators for both */
	if(hostName[hostNameLen - 1] == '\0') {
		hostNameLen--;
	}
	if(serverName[serverNameLen - 1] == '\0') {
		serverNameLen--;
	}
	
	/* case 1: exact match */
	if((serverNameLen == hostNameLen) &&
	    !memcmp(serverName, hostName, serverNameLen)) {
		return CSSM_TRUE;
	}
	
	/* case 2: handle optional '*' in cert's server name */
	if(serverName[0] == '*') {
		/* last (serverNameLen - 1) chars have to match */
		unsigned effectLen = serverNameLen - 1;		// skip '*' 
		if(serverNameLen < effectLen) {
			errorLog0("tp_verifySslOpts: subject/server name wildcard "
				"mismatch (1)");
			return CSSM_FALSE;
		}
		else if(memcmp(serverName+1,		// skip '*'
		         hostName + hostNameLen - effectLen,
				 effectLen)) {
			errorLog0("tp_verifySslOpts: subject/server name wildcard "
				"mismatch (2)");
			return CSSM_FALSE;
		}
		else {
			/* wildcard match */
			return CSSM_TRUE;
		}
	}
	else {
		/* mismatch */
		errorLog0("tp_verifySslOpts: subject/server name mismatch");
		return CSSM_FALSE;
	}
}
