/*
 * crlUtils.cpp - CRL CL/TP/DL utilities.
 */
#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include <utilLib/common.h>
#include <clAppUtils/timeStr.h>
#include <clAppUtils/crlUtils.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/oidscrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <security_cdsa_utilities/Schema.h>				/* private API */

/* crlAddCrlToDb() removed due to build problem; obsoleted in favor of 
 * cuAddCrlToDb() in security_cdsa_utils 
 */
#define CRL_ADD_TO_DB		0

#if		CRL_ADD_TO_DB
#include <Security/SecCertificatePriv.h>	/* private */
											/* SecInferLabelFromX509Name() */

/*
 * Update an existing DLDB to be CRL-capable.
 */
static CSSM_RETURN crlAddCrlSchema(
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

static void crlInferCrlLabel(
	const CSSM_X509_NAME 	*x509Name,
	CSSM_DATA				*label)	 // not mallocd; contents are
									 //    from the x509Name
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
static bool crlSearchNumericExtension(
	const CSSM_X509_EXTENSIONS	*extens,
	const CSSM_OID				*oid,
	uint32						*val)
{
	for(uint32 dex=0; dex<extens->numberOfExtensions; dex++) {
		const CSSM_X509_EXTENSION *exten = &extens->extensions[dex];
		if(!appCompareCssmData(&exten->extnId, oid)) {
			continue;
		}
		if(exten->format != CSSM_X509_DATAFORMAT_PARSED) {
			printf("***Malformed extension\n");
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
#define MAX_CRL_ATTRS			8

CSSM_RETURN crlAddCrlToDb(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	const CSSM_DATA		*crl)
{
	CSSM_DB_ATTRIBUTE_DATA			attrs[MAX_CRL_ATTRS];
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA_PTR		attr = &attrs[0];
	CSSM_DATA						crlTypeData;
	CSSM_DATA						crlEncData;
	CSSM_DATA						printNameData;
	CSSM_RETURN						crtn;
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr;
	CSSM_DATA_PTR					issuer;		// mallocd by CL
	CSSM_DATA_PTR					crlValue;	// ditto
	uint32							numFields;
	CSSM_HANDLE						result;
	CSSM_CRL_ENCODING 				crlEnc = CSSM_CRL_ENCODING_DER;
	const CSSM_X509_SIGNED_CRL 		*signedCrl;
	const CSSM_X509_TBS_CERTLIST 	*tbsCrl;
	CSSM_CRL_TYPE 					crlType;
	CSSM_DATA 						thisUpdateData = {0, NULL};
	CSSM_DATA 						nextUpdateData = {0, NULL};
	char							*thisUpdate, *nextUpdate;
	unsigned						timeLen;
	uint32							crlNumber;
	uint32							deltaCrlNumber;
	CSSM_DATA						crlNumberData;
	CSSM_DATA						deltaCrlNumberData;
	bool							crlNumberPresent = false;
	bool							deltaCrlPresent = false;
	
	/* get normalized issuer name as Issuer attr */
	crtn = CSSM_CL_CrlGetFirstFieldValue(clHand,
		crl,
		&CSSMOID_X509V1IssuerName,
		&result,
		&numFields,
		&issuer);
	if(crtn) {
		printError("CSSM_CL_CrlGetFirstFieldValue(Issuer)", crtn);
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
		printError("CSSM_CL_CrlGetFirstFieldValue(Issuer)", crtn);
		return crtn;
	}
	CSSM_CL_CrlAbortQuery(clHand, result);
	if(crlValue == NULL) {
		printf("***CSSM_CL_CrlGetFirstFieldValue: value error (1)\n");
		return CSSMERR_CL_INVALID_CRL_POINTER;
	}
	if((crlValue->Data == NULL) || 
	   (crlValue->Length != sizeof(CSSM_X509_SIGNED_CRL))) {
		printf("***CSSM_CL_CrlGetFirstFieldValue: value error (2)\n");
		return CSSMERR_CL_INVALID_CRL_POINTER;
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
				printf("***Unknown version in CRL (%u)\n", vers);
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
	crlInferCrlLabel(&tbsCrl->issuer, &printNameData);
	
	/* cook up CSSM_TIMESTRING versions of this/next update */
	thisUpdate = x509TimeToCssmTimestring(&tbsCrl->thisUpdate, &timeLen);
	if(thisUpdate == NULL) {
		printf("***Badly formatted thisUpdate\n");
	}
	else {
		thisUpdateData.Data = (uint8 *)thisUpdate;
		thisUpdateData.Length = timeLen;
	}
	nextUpdate = x509TimeToCssmTimestring(&tbsCrl->nextUpdate, &timeLen);
	if(nextUpdate == NULL) {
		printf("***Badly formatted nextUpdate\n");
	}
	else {
		nextUpdateData.Data = (uint8 *)nextUpdate;
		nextUpdateData.Length = timeLen;
	}

	/* optional CrlNumber and DeltaCrlNumber */
	if(crlSearchNumericExtension(&tbsCrl->extensions,
			&CSSMOID_CrlNumber,
			&crlNumber)) {
		crlNumberData.Data = (uint8 *)&crlNumber;
		crlNumberData.Length = sizeof(uint32);
		crlNumberPresent = true;
	}
	if(crlSearchNumericExtension(&tbsCrl->extensions,
			&CSSMOID_DeltaCrlIndicator,
			&deltaCrlNumber)) {
		deltaCrlNumberData.Data = (uint8 *)&deltaCrlNumber;
		deltaCrlNumberData.Length = sizeof(uint32);
		deltaCrlPresent = true;
	}
	/* we spec six attributes, skipping alias and URI */
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
	attr->Value = &printNameData;
	attr++;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "Issuer";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = issuer;
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
		crl,
		&recordPtr);
	if(crtn == CSSMERR_DL_INVALID_RECORDTYPE) {
		/* gross hack of inserting this "new" schema that 
		 * Keychain didn't specify */
		crtn = crlAddCrlSchema(dlDbHand);
		if(crtn == CSSM_OK) {
			/* Retry with a fully capable DLDB */
			crtn = CSSM_DL_DataInsert(dlDbHand,
				CSSM_DL_DB_RECORD_X509_CRL,
				&recordAttrs,
				crl,
				&recordPtr);
		}
	}
	if(crtn) {
		printError("CSSM_DL_DataInsert", crtn);
	}
	else {
		CSSM_DL_FreeUniqueRecord(dlDbHand, recordPtr);
	}
	
	/* free all the stuff we allocated to get here */
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1IssuerName, issuer);
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V2CRLSignedCrlCStruct, 
			crlValue);
	free(thisUpdate);
	free(nextUpdate);
	return crtn;
}

#endif  /* CRL_ADD_TO_DB */
