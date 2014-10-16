/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * CertParser.h - cert parser with autorelease of fetched fields
 *
 * Created 24 October 2003 by Doug Mitchell
 */
 
#include "CertParser.h"
#import <AvailabilityMacros.h>

#define CP_DEBUG	1
#if		CP_DEBUG
#define dprintf(args...)  printf(args)
#else
#define dprintf(args...)
#endif

#pragma mark --- CP_FetchedField ---

class CP_FetchedField
{
public:
	/* construct one fetched field (which will be stored in CertParser's 
	 * mFetchedFields) */
	CP_FetchedField(
		const CSSM_OID	&fieldOid,
		CSSM_DATA_PTR   fieldData,
		CSSM_CL_HANDLE  clHand);
	
	/* Free the field via CL */
	~CP_FetchedField();
private:
	CSSM_OID		mFieldOid;
	CSSM_DATA_PTR   mFieldData;
	CSSM_CL_HANDLE  mClHand;
};

CP_FetchedField::CP_FetchedField(
	const CSSM_OID	&fieldOid,
	CSSM_DATA_PTR   fieldData,
	CSSM_CL_HANDLE  clHand)
	: mFieldOid(fieldOid), mFieldData(fieldData), mClHand(clHand)
{
}

/* Free the field via CL */
CP_FetchedField::~CP_FetchedField()
{
	CSSM_CL_FreeFieldValue(mClHand, &mFieldOid, mFieldData);
}

#pragma mark --- CertParser implementation ---

/* Construct with or without data - you can add the data later with 
 * initWithData() to parse without exceptions */
CertParser::CertParser()
{
	initFields();
}

CertParser::CertParser(
	CSSM_CL_HANDLE		clHand)
{
	initFields();
	mClHand = clHand;
}

CertParser::CertParser(
	CSSM_CL_HANDLE		clHand,
	const CSSM_DATA 	&certData)
{
	initFields();
	mClHand = clHand;
	CSSM_RETURN crtn = initWithData(certData);
	if(crtn) {
		throw ((int)crtn);
	}
}

CertParser::CertParser(
	SecCertificateRef 	secCert)
{
	initFields();
	OSStatus ortn = initWithSecCert(secCert);
	if(ortn) {
		throw ((int)ortn);
	}
}

/* frees all the fields we fetched */
CertParser::~CertParser()
{
	if(mClHand && mCacheHand) {
		CSSM_RETURN crtn = CSSM_CL_CertAbortCache(mClHand, mCacheHand);
		if(crtn) {
			/* almost certainly a bug */
			printf("Internal Error: CertParser error on free.");
			cssmPerror("CSSM_CL_CertAbortCache", crtn);
		}
	}
	vector<CP_FetchedField *>::iterator iter;
	for(iter=mFetchedFields.begin(); iter!=mFetchedFields.end(); iter++) {
		delete *iter;
	}
}

/* common init for all constructors */
void CertParser::initFields()
{
	mClHand = 0;
	mCacheHand = 0;
}

/*** NO MORE EXCEPTIONS ***/

/*
 * No cert- or CDSA-related exceptions thrown by remainder.
 * This is the core initializer: have the CL parse and cache the cert. 
 */
CSSM_RETURN CertParser::initWithData(
	const CSSM_DATA 	&certData)
{
	assert(mClHand != 0);
	CSSM_RETURN crtn = CSSM_CL_CertCache(mClHand, &certData, &mCacheHand);
	#if CP_DEBUG
	if(crtn) {
		cssmPerror("CSSM_CL_CertCache", crtn);
	}
	#endif
	return crtn;
}

OSStatus CertParser::initWithSecCert(
	SecCertificateRef 	secCert)
{
	OSStatus ortn;
	CSSM_DATA certData;
	
	assert(mClHand == 0);
	ortn = SecCertificateGetCLHandle(secCert, &mClHand);
	if(ortn) {
		return ortn;
	}
	ortn = SecCertificateGetData(secCert, &certData);
	if(ortn) {
		return ortn;
	}
	return (OSStatus)initWithData(certData);
}

CSSM_RETURN CertParser::initWithCFData(
	CFDataRef			cfData)
{
	CSSM_DATA   cdata;
	
	cdata.Data = (uint8 *)CFDataGetBytePtr(cfData);
	cdata.Length = CFDataGetLength(cfData);
	return initWithData(cdata);
}

/*
 * Obtain atrbitrary field from cached cert. This class takes care of freeing
 * the field in its destructor. 
 *
 * Returns NULL if field not found (not exception). 
 *
 * Caller optionally specifies field length to check - specifying zero means
 * "don't care, don't check". Actual field length always returned in fieldLength. 
 */
const void *CertParser::fieldForOid(
	const CSSM_OID		&oid,
	CSSM_SIZE			&fieldLength)		// IN/OUT
{
	CSSM_RETURN crtn;
	
	uint32 NumberOfFields = 0;
	CSSM_HANDLE resultHand = 0;
	CSSM_DATA_PTR fieldData = NULL;

	assert(mClHand != 0);
	assert(mCacheHand != 0);
	crtn = CSSM_CL_CertGetFirstCachedFieldValue(
		mClHand,
		mCacheHand,
	    &oid,
	    &resultHand,
	    &NumberOfFields,
		&fieldData);
	if(crtn) {
		/* not an error; just means that the cert doesn't have this field */
		return NULL;
	}
	assert(NumberOfFields == 1);
  	CSSM_CL_CertAbortQuery(mClHand, resultHand);
	
	if(fieldLength) {
		if(fieldLength != fieldData->Length) {
			/* FIXME what's a good way to log in this situation? */
			printf("***CertParser::fieldForOid: field length mismatch\n");
			return NULL;
		}
	}
	/* Store the OID and the field for autorelease */
	CP_FetchedField *cpField = new CP_FetchedField(oid, fieldData, mClHand);
	mFetchedFields.push_back(cpField);
	fieldLength = fieldData->Length;
	return fieldData->Data;
}

/*
 * Conveneince routine to fetch an extension we "know" the CL can parse.
 * The return value gets cast to one of the CE_Data types.
 */
const void *CertParser::extensionForOid(
	const CSSM_OID		&oid)
{
	CSSM_SIZE len = sizeof(CSSM_X509_EXTENSION);
	CSSM_X509_EXTENSION *cssmExt = 
		(CSSM_X509_EXTENSION *)fieldForOid(oid,	len);
	if(cssmExt) {
		if(cssmExt->format != CSSM_X509_DATAFORMAT_PARSED) {
			printf("***Badly formatted extension");
			return NULL;
		}
		return cssmExt->value.parsedValue;
	}
	else {
		return NULL;
	}
}

