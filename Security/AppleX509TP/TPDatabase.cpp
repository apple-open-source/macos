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
 * TPDatabase.cpp - TP's DL/DB access functions.
 *
 * Created 10/9/2002 by Doug Mitchell. 
 */

#include <Security/cssmtype.h>
#include <Security/cssmapi.h> 
#include <Security/Schema.h>				/* private API */
#include <Security/SecCertificatePriv.h>	/* private SecInferLabelFromX509Name() */
#include <Security/oidscert.h>
#include <Security/cssmerrno.h>
#include "TPDatabase.h"
#include "tpdebugging.h"
#include "certGroupUtils.h"
#include "TPCertInfo.h"
#include "TPCrlInfo.h"
#include "tpCrlVerify.h"
#include "tpTime.h"


/*
 * Given a DL/DB, look up cert by subject name. Subsequent
 * certs can be found using the returned result handle. 
 */
static CSSM_DB_UNIQUE_RECORD_PTR tpCertLookup(
	CSSM_DL_DB_HANDLE	dlDb,
	const CSSM_DATA		*subjectName,	// DER-encoded
	CSSM_HANDLE_PTR		resultHand,		// RETURNED
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
 * Search a list of DBs for a cert which verifies specified subject item. 
 * Just a boolean return - we found it, or not. If we did, we return
 * TPCertInfo associated with the raw cert. 
 * A true partialIssuerKey on return indicates that caller must deal
 * with partial public key processing later. 
 */
TPCertInfo *tpDbFindIssuerCert(
	CssmAllocator 			&alloc,
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	const TPClItemInfo		*subjectItem,
	const CSSM_DL_DB_LIST	*dbList,
	const char 				*verifyTime,		// may be NULL
	bool					&partialIssuerKey)	// RETURNED
{
	uint32						dbDex;
	CSSM_HANDLE					resultHand;
	CSSM_DATA					cert;	
	CSSM_DL_DB_HANDLE			dlDb;
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	TPCertInfo 					*issuerCert = NULL;
	bool 						foundIt;

	partialIssuerKey = false;
	if(dbList == NULL) {
		return NULL;
	}
	for(dbDex=0; dbDex<dbList->NumHandles; dbDex++) {
		dlDb = dbList->DLDBHandle[dbDex];
		cert.Data = NULL;
		cert.Length = 0;
		record = tpCertLookup(dlDb,
			subjectItem->issuerName(),
			&resultHand,
			&cert);
		/* remember we have to: 
		 * -- abort this query regardless, and 
		 * -- free the CSSM_DATA cert regardless, and 
		 * -- free the unique record if we don't use it 
		 *    (by placing it in issuerCert)...
		 */
		if(record != NULL) {
			/* Found one */
			assert(cert.Data != NULL);
			issuerCert = new TPCertInfo(clHand, cspHand, &cert, TIC_CopyData, verifyTime);
			/* we're done with raw cert data */
			/* FIXME this assumes that alloc is the same as the 
			 * allocator associated with DlDB...OK? */
			tpFreeCssmData(alloc, &cert, CSSM_FALSE);
			cert.Data = NULL;
			cert.Length = 0;
			
			/* Does it verify the subject cert? */
			CSSM_RETURN crtn = subjectItem->verifyWithIssuer(issuerCert);
			switch(crtn) {
				case CSSM_OK:
					break;
				case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
					partialIssuerKey = true;
					break;
				default:
					delete issuerCert;
					issuerCert = NULL;
					CSSM_DL_FreeUniqueRecord(dlDb, record);
					
					/*
					* Verify fail. Continue searching this DB. Break on 
					* finding the holy grail or no more records found. 
					*/
					for(;;) {
						cert.Data = NULL;
						cert.Length = 0;
						CSSM_RETURN crtn = CSSM_DL_DataGetNext(dlDb, 
							resultHand,
							NULL,		// no attrs 
							&cert,
							&record);
						if(crtn) {
							/* no more, done with this DB */
							assert(cert.Data == NULL);
							break;
						}
						assert(cert.Data != NULL);
						
						/* found one - does it verify subject? */
						issuerCert = new TPCertInfo(clHand, cspHand, &cert, TIC_CopyData, 
								verifyTime);
						/* we're done with raw cert data */
						tpFreeCssmData(alloc, &cert, CSSM_FALSE);
						cert.Data = NULL;
						cert.Length = 0;
	
						/* FIXME - figure out allowExpire, etc. */
						crtn = subjectItem->verifyWithIssuer(issuerCert);
						foundIt = false;
						switch(crtn) {
							case CSSM_OK:
								foundIt = true;
								break;
							case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
								partialIssuerKey = true;
								foundIt = true;
								break;
							default:
								break;
						}
						if(foundIt) {
							/* yes! */
							break;
						}
						delete issuerCert;
						CSSM_DL_FreeUniqueRecord(dlDb, record);
						issuerCert = NULL;
					} /* searching subsequent records */
			}	/* switch verify */

			if(issuerCert != NULL) {
				/* successful return */
				tpDebug("tpDbFindIssuer: found cert record %p", record);
				CSSM_DL_DataAbortQuery(dlDb, resultHand);
				issuerCert->dlDbHandle(dlDb);
				issuerCert->uniqueRecord(record);
				return issuerCert;
			}
		}	/* tpCertLookup, i.e., CSSM_DL_DataGetFirst, succeeded */
		else {
			assert(cert.Data == NULL);
		}
		/* in any case, abort the query for this db */
		CSSM_DL_DataAbortQuery(dlDb, resultHand);
		
	}	/* main loop searching dbList */

	/* issuer not found */
	return NULL;
}

/*
 * Given a DL/DB, look up CRL by issuer name and validity time. 
 * Subsequent CRLs can be found using the returned result handle. 
 */
#define SEARCH_BY_DATE		1

static CSSM_DB_UNIQUE_RECORD_PTR tpCrlLookup(
	CSSM_DL_DB_HANDLE	dlDb,
	const CSSM_DATA		*issuerName,	// DER-encoded
	CSSM_TIMESTRING 	verifyTime,		// may be NULL, implies "now"
	CSSM_HANDLE_PTR		resultHand,		// RETURNED
	CSSM_DATA_PTR		crl)			// RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		pred[3];	
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	char							timeStr[CSSM_TIME_STRLEN + 1];
	
	crl->Data = NULL;
	crl->Length = 0;
	
	/* Three predicates...first, the issuer name */
	pred[0].DbOperator = CSSM_DB_EQUAL;
	pred[0].Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	pred[0].Attribute.Info.Label.AttributeName = "Issuer";
	pred[0].Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	pred[0].Attribute.Value = const_cast<CSSM_DATA_PTR>(issuerName);
	pred[0].Attribute.NumberOfValues = 1;
	
	/* now before/after. Cook up an appropriate time string. */
	if(verifyTime != NULL) {
		/* Caller spec'd tolerate any format */
		int rtn = tpTimeToCssmTimestring(verifyTime, strlen(verifyTime), timeStr);
		if(rtn) {
			tpErrorLog("tpCrlLookup: Invalid VerifyTime string\n");
			return NULL;
		}
	}
	else {
		/* right now */
		StLock<Mutex> _(tpTimeLock());
		timeAtNowPlus(0, TIME_CSSM, timeStr);
	}
	CSSM_DATA timeData;
	timeData.Data = (uint8 *)timeStr;
	timeData.Length = CSSM_TIME_STRLEN;
	
	#if SEARCH_BY_DATE
	pred[1].DbOperator = CSSM_DB_LESS_THAN;
	pred[1].Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	pred[1].Attribute.Info.Label.AttributeName = "NextUpdate";
	pred[1].Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	pred[1].Attribute.Value = &timeData;
	pred[1].Attribute.NumberOfValues = 1;
	
	pred[2].DbOperator = CSSM_DB_GREATER_THAN;
	pred[2].Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	pred[2].Attribute.Info.Label.AttributeName = "ThisUpdate";
	pred[2].Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	pred[2].Attribute.Value = &timeData;
	pred[2].Attribute.NumberOfValues = 1;
	#endif
	
	query.RecordType = CSSM_DL_DB_RECORD_X509_CRL;
	query.Conjunctive = CSSM_DB_AND;
	#if SEARCH_BY_DATE
	query.NumSelectionPredicates = 3;
	#else
	query.NumSelectionPredicates = 1;
	#endif
	query.SelectionPredicate = pred;
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0;				// FIXME - used?
	
	CSSM_DL_DataGetFirst(dlDb,
		&query,
		resultHand,
		NULL,				// don't fetch attributes
		crl,
		&record);
	return record;
}

/*
 * Search a list of DBs for a CRL from the specified issuer and (optional)  
 * TPCrlVerifyContext.verifyTime. 
 * Just a boolean return - we found it, or not. If we did, we return a
 * TPCrlInfo which has been verified with the specified TPCrlVerifyContext.
 */
TPCrlInfo *tpDbFindIssuerCrl(
	TPCrlVerifyContext	&vfyCtx,
	const CSSM_DATA		&issuer,
	TPCertInfo			&forCert)
{
	uint32						dbDex;
	CSSM_HANDLE					resultHand;
	CSSM_DATA					crl;	
	CSSM_DL_DB_HANDLE			dlDb;
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	TPCrlInfo 					*issuerCrl = NULL;
	CSSM_DL_DB_LIST_PTR 		dbList = vfyCtx.dbList;
	CSSM_RETURN					crtn;
	
	if(dbList == NULL) {
		return NULL;
	}
	for(dbDex=0; dbDex<dbList->NumHandles; dbDex++) {
		dlDb = dbList->DLDBHandle[dbDex];
		crl.Data = NULL;
		crl.Length = 0;
		record = tpCrlLookup(dlDb,
			&issuer,
			vfyCtx.verifyTime,
			&resultHand,
			&crl);
		/* remember we have to: 
		 * -- abort this query regardless, and 
		 * -- free the CSSM_DATA crl regardless, and 
		 * -- free the unique record if we don't use it 
		 *    (by placing it in issuerCert)...
		 */
		if(record != NULL) {
			/* Found one */
			assert(crl.Data != NULL);
			issuerCrl = new TPCrlInfo(vfyCtx.clHand, 
				vfyCtx.cspHand,
				&crl, 
				TIC_CopyData, 
				vfyCtx.verifyTime);
			/* we're done with raw CRL data */
			/* FIXME this assumes that vfyCtx.alloc is the same as the 
			 * allocator associated with DlDB...OK? */
			tpFreeCssmData(vfyCtx.alloc, &crl, CSSM_FALSE);
			crl.Data = NULL;
			crl.Length = 0;
			
			/* and we're done with the record */
			CSSM_DL_FreeUniqueRecord(dlDb, record);
			
			/* Does it verify with specified context? */
			crtn = issuerCrl->verifyWithContext(vfyCtx, &forCert);
			if(crtn) {
					
				delete issuerCrl;
				issuerCrl = NULL;
				
				/*
				 * Verify fail. Continue searching this DB. Break on 
				 * finding the holy grail or no more records found. 
				 */
				for(;;) {
					crl.Data = NULL;
					crl.Length = 0;
					crtn = CSSM_DL_DataGetNext(dlDb, 
						resultHand,
						NULL,		// no attrs 
						&crl,
						&record);
					if(crtn) {
						/* no more, done with this DB */
						assert(crl.Data == NULL);
						break;
					}
					assert(crl.Data != NULL);
					
					/* found one - is it any good? */
					issuerCrl = new TPCrlInfo(vfyCtx.clHand, 
						vfyCtx.cspHand,
						&crl, 
						TIC_CopyData, 
						vfyCtx.verifyTime);
					/* we're done with raw CRL data */
					/* FIXME this assumes that vfyCtx.alloc is the same as the 
					* allocator associated with DlDB...OK? */
					tpFreeCssmData(vfyCtx.alloc, &crl, CSSM_FALSE);
					crl.Data = NULL;
					crl.Length = 0;

					CSSM_DL_FreeUniqueRecord(dlDb, record);

					crtn = issuerCrl->verifyWithContext(vfyCtx, &forCert);
					if(crtn == CSSM_OK) {
						/* yes! */
						break;
					}
					delete issuerCrl;
					issuerCrl = NULL;
				} /* searching subsequent records */
			}	/* verify fail */
			/* else success! */

			if(issuerCrl != NULL) {
				/* successful return */
				CSSM_DL_DataAbortQuery(dlDb, resultHand);
				tpDebug("tpDbFindIssuerCrl: found CRL record %p", record);
				return issuerCrl;
			}
		}	/* tpCrlLookup, i.e., CSSM_DL_DataGetFirst, succeeded */
		else {
			assert(crl.Data == NULL);
		}
		/* in any case, abort the query for this db */
		CSSM_DL_DataAbortQuery(dlDb, resultHand);
		
	}	/* main loop searching dbList */

	/* issuer not found */
	return NULL;
}

/*
 * Update an existing DLDB to be CRL-capable.
 */
static CSSM_RETURN tpAddCrlSchema(
	CSSM_DL_DB_HANDLE	dlDbHand)
{
	return CSSM_DL_CreateRelation(dlDbHand,
		CSSM_DL_DB_RECORD_X509_CRL,
		"CSSM_DL_DB_RECORD_X509_CRL",
		Security::KeychainCore::Schema::X509CrlSchemaAttributeCount,
		Security::KeychainCore::Schema::X509CrlSchemaAttributeList,
		Security::KeychainCore::Schema::X509CrlSchemaIndexCount,
		Security::KeychainCore::Schema::X509CrlSchemaIndexList);		
}

/*
 * Search extensions for specified OID, assumed to have underlying
 * value type of uint32; returns the value and true if found.
 */
static bool tpSearchNumericExtension(
	const CSSM_X509_EXTENSIONS	*extens,
	const CSSM_OID				*oid,
	uint32						*val)
{
	for(uint32 dex=0; dex<extens->numberOfExtensions; dex++) {
		const CSSM_X509_EXTENSION *exten = &extens->extensions[dex];
		if(!tpCompareOids(&exten->extnId, oid)) {
			continue;
		}
		if(exten->format != CSSM_X509_DATAFORMAT_PAIR) {
			tpErrorLog("***Malformed CRL extension\n");
			continue;
		}
		*val = *((uint32 *)exten->value.parsedValue);
		return true;
	}
	return false;
}

/*
 * Store a CRL in a DLDB.
 * We store the following attributes:
 *
 *		CrlType
 * 		CrlEncoding
 *		PrintName (Inferred from issuer)
 *		Issuer
 *		ThisUpdate
 *		NextUpdate
 *		URI (if present)
 * 		CrlNumber (if present)
 *		DeltaCrlNumber (if present)
 */
#define MAX_CRL_ATTRS	9

CSSM_RETURN tpDbStoreCrl(
	TPCrlInfo			&crl,
	CSSM_DL_DB_HANDLE	&dlDbHand)
{
	CSSM_DB_ATTRIBUTE_DATA			attrs[MAX_CRL_ATTRS];
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA_PTR		attr = &attrs[0];
	CSSM_DATA						crlTypeData;
	CSSM_DATA						crlEncData;
	CSSM_RETURN						crtn;
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr;
	CSSM_CRL_ENCODING 				crlEnc = CSSM_CRL_ENCODING_DER;
	const CSSM_X509_TBS_CERTLIST 	*tbsCrl;
	CSSM_CRL_TYPE 					crlType;
	CSSM_DATA 						thisUpdateData = {0, NULL};
	CSSM_DATA 						nextUpdateData = {0, NULL};
	char							thisUpdate[CSSM_TIME_STRLEN+1];
	char							nextUpdate[CSSM_TIME_STRLEN+1];
	uint32							crlNumber;
	uint32							deltaCrlNumber;
	CSSM_DATA						crlNumberData;
	CSSM_DATA						deltaCrlNumberData;
	bool							crlNumberPresent = false;
	bool							deltaCrlPresent = false;
	
	tbsCrl = &(crl.x509Crl()->tbsCertList);
	
	/* CrlType inferred from version */
	if(tbsCrl->version.Length == 0) {
		/* should never happen... */
		crlType = CSSM_CRL_TYPE_X_509v1;
	}
	else {
		uint8 vers = tbsCrl->version.Data[tbsCrl->version.Length - 1];
		switch(vers) {
			case 0:
				crlType = CSSM_CRL_TYPE_X_509v1;
				break;
			case 1:
				crlType = CSSM_CRL_TYPE_X_509v2;
				break;
			default:
				tpErrorLog("***Unknown version in CRL (%u)\n", vers);
				crlType = CSSM_CRL_TYPE_X_509v1;
				break;
		}
	}
	crlTypeData.Data = (uint8 *)&crlType;
	crlTypeData.Length = sizeof(CSSM_CRL_TYPE);
	/* encoding more-or-less assumed here */
	crlEncData.Data = (uint8 *)&crlEnc;
	crlEncData.Length = sizeof(CSSM_CRL_ENCODING);
	
	/* printName inferred from issuer */
	CSSM_DATA printName;
	const CSSM_DATA *printNamePtr;
	printNamePtr = SecInferLabelFromX509Name(&tbsCrl->issuer);
	if(printNamePtr) {
		printName = *(const_cast<CSSM_DATA *>(printNamePtr));
	}
	else {
		printName.Data = (uint8 *)"X509 CRL";
		printName.Length = 8;
	}
	
	/* cook up CSSM_TIMESTRING versions of this/next update */
	int rtn = tpTimeToCssmTimestring((const char *)tbsCrl->thisUpdate.time.Data, 
		tbsCrl->thisUpdate.time.Length,
		thisUpdate);
	if(rtn) {
		tpErrorLog("***Badly formatted thisUpdate\n");
	}
	else {
		thisUpdateData.Data = (uint8 *)thisUpdate;
		thisUpdateData.Length = CSSM_TIME_STRLEN;
	}
	if(tbsCrl->nextUpdate.time.Data != NULL) {
		rtn = tpTimeToCssmTimestring((const char *)tbsCrl->nextUpdate.time.Data, 
			tbsCrl->nextUpdate.time.Length,
			nextUpdate);
		if(rtn) {
			tpErrorLog("***Badly formatted nextUpdate\n");
		}
		else {
			nextUpdateData.Data = (uint8 *)nextUpdate;
			nextUpdateData.Length = CSSM_TIME_STRLEN;
		}
	}
	else {
		/*
		 * NextUpdate not present; fake it by using "virtual end of time"
		 */
		tpTimeToCssmTimestring(CSSM_APPLE_CRL_END_OF_TIME, 
			strlen(CSSM_APPLE_CRL_END_OF_TIME),	nextUpdate);
		nextUpdateData.Data = (uint8 *)nextUpdate;
		nextUpdateData.Length = CSSM_TIME_STRLEN;
	}
	
	/* optional CrlNumber and DeltaCrlNumber */
	if(tpSearchNumericExtension(&tbsCrl->extensions,
			&CSSMOID_CrlNumber,
			&crlNumber)) {
		crlNumberData.Data = (uint8 *)&crlNumber;
		crlNumberData.Length = sizeof(uint32);
		crlNumberPresent = true;
	}
	if(tpSearchNumericExtension(&tbsCrl->extensions,
			&CSSMOID_DeltaCrlIndicator,
			&deltaCrlNumber)) {
		deltaCrlNumberData.Data = (uint8 *)&deltaCrlNumber;
		deltaCrlNumberData.Length = sizeof(uint32);
		deltaCrlPresent = true;
	}
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "CrlType";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &crlTypeData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "CrlEncoding";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &crlEncData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "PrintName";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &printName;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "Issuer";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = const_cast<CSSM_DATA *>(crl.issuerName());
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "ThisUpdate";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &thisUpdateData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "NextUpdate";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &nextUpdateData;
	attr++;
	
	/* now the optional attributes */
	CSSM_DATA uri = *crl.uri();
	if(uri.Data != NULL) {
		/* ensure URI string does not contain NULL */
		if(uri.Data[uri.Length - 1] == 0) {
			uri.Length--;
		}
		attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		attr->Info.Label.AttributeName = "URI";
		attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		attr->NumberOfValues = 1;
		attr->Value = &uri;
		attr++;
	}
	if(crlNumberPresent) {
		attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		attr->Info.Label.AttributeName = "CrlNumber";
		attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
		attr->NumberOfValues = 1;
		attr->Value = &crlNumberData;
		attr++;
	}
	if(deltaCrlPresent) {
		attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		attr->Info.Label.AttributeName = "DeltaCrlNumber";
		attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
		attr->NumberOfValues = 1;
		attr->Value = &deltaCrlNumberData;
		attr++;
	}
	
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_X509_CRL;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = attr - attrs;
	recordAttrs.AttributeData = attrs;
	
	crtn = CSSM_DL_DataInsert(dlDbHand,
		CSSM_DL_DB_RECORD_X509_CRL,
		&recordAttrs,
		crl.itemData(),
		&recordPtr);
	if(crtn == CSSMERR_DL_INVALID_RECORDTYPE) {
		/* gross hack of inserting this "new" schema that Keychain 
		 * didn't specify */
		crtn = tpAddCrlSchema(dlDbHand);
		if(crtn == CSSM_OK) {
			/* Retry with a fully capable DLDB */
			crtn = CSSM_DL_DataInsert(dlDbHand,
				CSSM_DL_DB_RECORD_X509_CRL,
				&recordAttrs,
				crl.itemData(),
				&recordPtr);
		}
	}
	if(crtn) {
		tpErrorLog("CSSM_DL_DataInsert: %s", cssmErrorString(crtn).c_str());
	}
	else {
		CSSM_DL_FreeUniqueRecord(dlDbHand, recordPtr);
	}
	
	return crtn;
}
