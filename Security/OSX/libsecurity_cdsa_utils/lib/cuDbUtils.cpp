/*
 * Copyright (c) 2002-2003,2011-2012,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. 
 * Please obtain a copy of the License at http://www.apple.com/publicsource
 * and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights 
 * and limitations under the License.
 */

/*
	File:		 cuDbUtils.cpp
	
	Description: CDSA DB access utilities

	Author:		 dmitch
*/

#include "cuCdsaUtils.h"
#include "cuTimeStr.h"
#include "cuDbUtils.h"
#include "cuPrintCert.h"
#include <stdlib.h>
#include <stdio.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>	/* private SecInferLabelFromX509Name() */
#include <Security/cssmapple.h>				/* for cssmPerror() */
#include <Security/oidscert.h>
#include <Security/oidscrl.h>
#include <Security/oidsattr.h>
#include <strings.h>
#include <security_cdsa_utilities/Schema.h>			/* private API */

#ifndef	NDEBUG
#define dprintf(args...) printf(args)
#else
#define dprintf(args...)
#endif

/*
 * Add a certificate to an open DLDB.
 */
CSSM_RETURN cuAddCertToDb(
	CSSM_DL_DB_HANDLE	dlDbHand,
	const CSSM_DATA		*cert,
	CSSM_CERT_TYPE		certType,
	CSSM_CERT_ENCODING	certEncoding,
	const char			*printName,		// C string
	const CSSM_DATA		*publicKeyHash)		
{
	CSSM_DB_ATTRIBUTE_DATA			attrs[6];
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA_PTR		attr = &attrs[0];
	CSSM_DATA						certTypeData;
	CSSM_DATA						certEncData;
	CSSM_DATA						printNameData;
	CSSM_RETURN						crtn;
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr;
	
	/* issuer and serial number required, fake 'em */
	CSSM_DATA						issuer = {6, (uint8 *)"issuer"};
	CSSM_DATA						serial = {6, (uint8 *)"serial"};
	
	/* we spec six attributes, skipping alias */
	certTypeData.Data = (uint8 *)&certType;
	certTypeData.Length = sizeof(CSSM_CERT_TYPE);
	certEncData.Data = (uint8 *)&certEncoding;
	certEncData.Length = sizeof(CSSM_CERT_ENCODING);
	printNameData.Data = (uint8 *)printName;
	printNameData.Length = strlen(printName) + 1;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "CertType";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &certTypeData;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "CertEncoding";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &certEncData;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "PrintName";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &printNameData;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "PublicKeyHash";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = (CSSM_DATA_PTR)publicKeyHash;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "Issuer";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &issuer;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "SerialNumber";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &serial;
	
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 6;
	recordAttrs.AttributeData = attrs;
	
	crtn = CSSM_DL_DataInsert(dlDbHand,
		CSSM_DL_DB_RECORD_X509_CERTIFICATE,
		&recordAttrs,
		cert,
		&recordPtr);
	if(crtn) {
		cuPrintError("CSSM_DL_DataInsert", crtn);
	}
	else {
		CSSM_DL_FreeUniqueRecord(dlDbHand, recordPtr);
	}
	return crtn;
}

static CSSM_RETURN cuAddCrlSchema(
	CSSM_DL_DB_HANDLE	dlDbHand);
	
static void cuInferCrlLabel(
	const CSSM_X509_NAME 	*x509Name,
	CSSM_DATA				*label)	 // not mallocd; contents are from the x509Name
{
	/* use private API for common "infer label" logic */
	const CSSM_DATA *printValue = SecInferLabelFromX509Name(x509Name);
	if(printValue == NULL) {
		/* punt! */
		label->Data = (uint8 *)"X509 CRL";
		label->Length = 8;
	}
	else {
		*label = *printValue;
	}
}

/*
 * Search extensions for specified OID, assumed to have underlying
 * value type of uint32; returns the value and true if found.
 */
static bool cuSearchNumericExtension(
	const CSSM_X509_EXTENSIONS	*extens,
	const CSSM_OID				*oid,
	uint32						*val)
{
	for(uint32 dex=0; dex<extens->numberOfExtensions; dex++) {
		const CSSM_X509_EXTENSION *exten = &extens->extensions[dex];
		if(!cuCompareOid(&exten->extnId, oid)) {
			continue;
		}
		if(exten->format != CSSM_X509_DATAFORMAT_PARSED) {
			dprintf("***Malformed extension\n");
			continue;
		}
		*val = *((uint32 *)exten->value.parsedValue);
		return true;
	}
	return false;
}

/*
 * Add a CRL to an existing DL/DB.
 */
#define MAX_CRL_ATTRS			9

CSSM_RETURN cuAddCrlToDb(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	const CSSM_DATA		*crl,
	const CSSM_DATA		*URI)
{
	CSSM_DB_ATTRIBUTE_DATA			attrs[MAX_CRL_ATTRS];
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA_PTR		attr = &attrs[0];
	CSSM_DATA						crlTypeData;
	CSSM_DATA						crlEncData;
	CSSM_DATA						printNameData;
	CSSM_RETURN						crtn;
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr;
	CSSM_DATA_PTR					issuer = NULL;		// mallocd by CL
	CSSM_DATA_PTR					crlValue = NULL;	// ditto
	uint32							numFields;
	CSSM_HANDLE						result;
	CSSM_CRL_ENCODING 				crlEnc = CSSM_CRL_ENCODING_DER;
	const CSSM_X509_SIGNED_CRL 		*signedCrl;
	const CSSM_X509_TBS_CERTLIST 	*tbsCrl;
	CSSM_CRL_TYPE 					crlType;
	CSSM_DATA 						thisUpdateData = {0, NULL};
	CSSM_DATA 						nextUpdateData = {0, NULL};
	char							*thisUpdate = NULL;
	char							*nextUpdate = NULL;
	unsigned						timeLen;
	uint32							crlNumber;
	uint32							deltaCrlNumber;
	CSSM_DATA						crlNumberData;
	CSSM_DATA						deltaCrlNumberData;
	bool							crlNumberPresent = false;
	bool							deltaCrlPresent = false;
	CSSM_DATA						attrUri;
	
	/* get normalized issuer name as Issuer attr */
	crtn = CSSM_CL_CrlGetFirstFieldValue(clHand,
		crl,
		&CSSMOID_X509V1IssuerName,
		&result,
		&numFields,
		&issuer);
	if(crtn) {
		cuPrintError("CSSM_CL_CrlGetFirstFieldValue(Issuer)", crtn);
		return crtn;
	}
	CSSM_CL_CrlAbortQuery(clHand, result);
	
	/* get parsed CRL from the CL */
	crtn = CSSM_CL_CrlGetFirstFieldValue(clHand,
		crl,
		&CSSMOID_X509V2CRLSignedCrlCStruct,
		&result,
		&numFields,
		&crlValue);
	if(crtn) {
		cuPrintError("CSSM_CL_CrlGetFirstFieldValue(Issuer)", crtn);
		goto errOut;
	}
	CSSM_CL_CrlAbortQuery(clHand, result);
	if(crlValue == NULL) {
		dprintf("***CSSM_CL_CrlGetFirstFieldValue: value error (1)\n");
		crtn = CSSMERR_CL_INVALID_CRL_POINTER;
		goto errOut;
	}
	if((crlValue->Data == NULL) || 
	   (crlValue->Length != sizeof(CSSM_X509_SIGNED_CRL))) {
		dprintf("***CSSM_CL_CrlGetFirstFieldValue: value error (2)\n");
		crtn = CSSMERR_CL_INVALID_CRL_POINTER;
		goto errOut;
	}
	signedCrl = (const CSSM_X509_SIGNED_CRL *)crlValue->Data;
	tbsCrl = &signedCrl->tbsCertList;
	
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
				dprintf("***Unknown version in CRL (%u)\n", vers);
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
	cuInferCrlLabel(&tbsCrl->issuer, &printNameData);
	
	/* cook up CSSM_TIMESTRING versions of this/next update */
	thisUpdate = cuX509TimeToCssmTimestring(&tbsCrl->thisUpdate, &timeLen);
	if(thisUpdate == NULL) {
		dprintf("***Badly formatted thisUpdate\n");
	}
	else {
		thisUpdateData.Data = (uint8 *)thisUpdate;
		thisUpdateData.Length = timeLen;
	}
	if(tbsCrl->nextUpdate.time.Data != NULL) {
		nextUpdate = cuX509TimeToCssmTimestring(&tbsCrl->nextUpdate, &timeLen);
		if(nextUpdate == NULL) {
			dprintf("***Badly formatted nextUpdate\n");
		}
		else {
			nextUpdateData.Data = (uint8 *)nextUpdate;
			nextUpdateData.Length = timeLen;
		}
	}
	else {
		/*
		 * NextUpdate not present; fake it by using "virtual end of time"
		 */
		CSSM_X509_TIME tempTime = {	0,		// timeType, not used
			{ strlen(CSSM_APPLE_CRL_END_OF_TIME), 
			  (uint8 *)CSSM_APPLE_CRL_END_OF_TIME} };
		nextUpdate = cuX509TimeToCssmTimestring(&tempTime, &timeLen);
		nextUpdateData.Data = (uint8 *)nextUpdate;
		nextUpdateData.Length = CSSM_TIME_STRLEN;
	}
	
	/* optional CrlNumber and DeltaCrlNumber */
	if(cuSearchNumericExtension(&tbsCrl->extensions,
			&CSSMOID_CrlNumber,
			&crlNumber)) {
		crlNumberData.Data = (uint8 *)&crlNumber;
		crlNumberData.Length = sizeof(uint32);
		crlNumberPresent = true;
	}
	if(cuSearchNumericExtension(&tbsCrl->extensions,
			&CSSMOID_DeltaCrlIndicator,
			&deltaCrlNumber)) {
		deltaCrlNumberData.Data = (uint8 *)&deltaCrlNumber;
		deltaCrlNumberData.Length = sizeof(uint32);
		deltaCrlPresent = true;
	}

	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "CrlType";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &crlTypeData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "CrlEncoding";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &crlEncData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "PrintName";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &printNameData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "Issuer";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = issuer;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "ThisUpdate";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &thisUpdateData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = (char*) "NextUpdate";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &nextUpdateData;
	attr++;

    /* ensure URI string does not contain NULL */
    attrUri = *URI;
    if((attrUri.Length != 0) &&
       (attrUri.Data[attrUri.Length - 1] == 0)) {
        attrUri.Length--;
    }
    attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
    attr->Info.Label.AttributeName = (char*) "URI";
    attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
    attr->NumberOfValues = 1;
    attr->Value = &attrUri;
    attr++;

	/* now the optional attributes */
	if(crlNumberPresent) {
		attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		attr->Info.Label.AttributeName = (char*) "CrlNumber";
		attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
		attr->NumberOfValues = 1;
		attr->Value = &crlNumberData;
		attr++;
	}
	if(deltaCrlPresent) {
		attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		attr->Info.Label.AttributeName = (char*) "DeltaCrlNumber";
		attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
		attr->NumberOfValues = 1;
		attr->Value = &deltaCrlNumberData;
		attr++;
	}
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_X509_CRL;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = (uint32)(attr - attrs);
	recordAttrs.AttributeData = attrs;
	
	crtn = CSSM_DL_DataInsert(dlDbHand,
		CSSM_DL_DB_RECORD_X509_CRL,
		&recordAttrs,
		crl,
		&recordPtr);
	if(crtn == CSSMERR_DL_INVALID_RECORDTYPE) {
		/* gross hack of inserting this "new" schema that Keychain didn't specify */
		crtn = cuAddCrlSchema(dlDbHand);
		if(crtn == CSSM_OK) {
			/* Retry with a fully capable DLDB */
			crtn = CSSM_DL_DataInsert(dlDbHand,
				CSSM_DL_DB_RECORD_X509_CRL,
				&recordAttrs,
				crl,
				&recordPtr);
		}
	}
	if(crtn == CSSM_OK) {
		CSSM_DL_FreeUniqueRecord(dlDbHand, recordPtr);
	}
	
errOut:
	/* free all the stuff we allocated to get here */
	if(issuer) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1IssuerName, issuer);
	}
	if(crlValue) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V2CRLSignedCrlCStruct, crlValue);
	}
	if(thisUpdate) {
		free(thisUpdate);
	}
	if(nextUpdate) {
		free(nextUpdate);
	}
	return crtn;
}


/*
 * Update an existing DLDB to be CRL-capable.
 */
static CSSM_RETURN cuAddCrlSchema(
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
 * Search DB for all records of type CRL or cert, calling appropriate
 * parse/print routine for each record. 
 */ 
CSSM_RETURN cuDumpCrlsCerts(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	CSSM_BOOL			isCert,
	unsigned			&numItems,		// returned
	CSSM_BOOL			verbose)
{
	CSSM_QUERY					query;
	CSSM_DB_UNIQUE_RECORD_PTR	record = NULL;
	CSSM_HANDLE					resultHand;
	CSSM_RETURN					crtn;
	CSSM_DATA					certCrl;
	const char					*itemStr;
	
	numItems = 0;
	itemStr = isCert ? "Certificate" : "CRL";
	
	/* just search by recordType, no predicates, no attributes */
	if(isCert) {
		query.RecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	}
	else {
		query.RecordType = CSSM_DL_DB_RECORD_X509_CRL;
	}
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	certCrl.Data = NULL;
	certCrl.Length = 0;
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		NULL,			// no attrs 
		&certCrl,
		&record);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			/* no data, otherwise OK */
			return CSSM_OK;
		case CSSMERR_DL_INVALID_RECORDTYPE:
			/* invalid record type just means "this hasn't been set up
			* for certs yet". */
			return crtn;
		default:
			cuPrintError("DataGetFirst", crtn);
			return crtn;
	}

	/* got one; print it */
	dprintf("%s %u:\n", itemStr, numItems);
	if(isCert) {
		printCert(certCrl.Data, (unsigned)certCrl.Length, verbose);
	}
	else {
		printCrl(certCrl.Data, (unsigned)certCrl.Length, verbose);
	}
	CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	APP_FREE(certCrl.Data);
	certCrl.Data = NULL;
	certCrl.Length = 0;
	numItems++;
	
	/* get the rest */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDbHand,
			resultHand, 
			NULL,
			&certCrl,
			&record);
		switch(crtn) {
			case CSSM_OK:
				dprintf("%s %u:\n", itemStr, numItems);
				if(isCert) {
					printCert(certCrl.Data, (unsigned)certCrl.Length, verbose);
				}
				else {
					printCrl(certCrl.Data, (unsigned)certCrl.Length, verbose);
				}
				CSSM_DL_FreeUniqueRecord(dlDbHand, record);
				APP_FREE(certCrl.Data);
				certCrl.Data = NULL;
				certCrl.Length = 0;
				numItems++;
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				return CSSM_OK;
			default:
				cuPrintError("DataGetNext", crtn);
				return crtn;
		}
	}
	/* NOT REACHED */
}
	
