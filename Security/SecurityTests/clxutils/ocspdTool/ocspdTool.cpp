/*
 * Copyright (c) 2004-2005,2008 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * ocspdTool.cpp - basic ocspdtool 
 *
 * Created 11 July 2004 by Doug Mitchell.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utilLib/common.h>
#include <Security/Security.h>
#include <security_ocspd/ocspdDbSchema.h>
#include <security_ocspd/ocspdUtils.h>
#include <security_ocspd/ocspdClient.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuTimeStr.h>
#include <Security/SecAsn1Coder.h>
#include <Security/ocspTemplates.h>

static void usage(char **argv)
{
    printf("Usage: op [option..]\n");
    printf("Ops:\n");
	printf("   d   dump database\n");
	printf("   f   flush stale entries\n");
	printf("   F   flush ALL\n");
	printf("Options:\n");
	printf("   TBD\n");
	exit(1);
}

#define OCSP_DB_FILE		"/private/var/db/crls/ocspcache.db"

static CSSM_DL_DB_HANDLE dlDbHandle = {0, 0};

static CSSM_API_MEMORY_FUNCS memFuncs = {
	cuAppMalloc,
	cuAppFree,
	cuAppRealloc,
 	cuAppCalloc,
 	NULL
};

static CSSM_VERSION vers = {2, 0};


static CSSM_RETURN dlAttach()
{
	if(dlDbHandle.DLHandle != 0) {
		return CSSM_OK;
	}
	if(cuCssmStartup() == CSSM_FALSE) {
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}
	CSSM_RETURN crtn = CSSM_ModuleLoad(&gGuidAppleFileDL,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleFileDL,
		&vers,
		&memFuncs,			// memFuncs
		0,					// SubserviceID
		CSSM_SERVICE_DL,	
		0,					// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,				// FunctionTable
		0,					// NumFuncTable
		NULL,				// reserved
		&dlDbHandle.DLHandle);
	if(crtn) {
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}
	return CSSM_OK;
}

static CSSM_RETURN dbAttach(const char *dbFile)
{
	CSSM_RETURN crtn = dlAttach();
	if(crtn) {
		return crtn;
	}
	if(dlDbHandle.DBHandle != 0) {
		return CSSM_OK;
	}
	return CSSM_DL_DbOpen(dlDbHandle.DLHandle,
		dbFile, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&dlDbHandle.DBHandle);
}

static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" 
};
	
static void printTimeStr(const CSSM_DATA *cssmTime)
{
	struct tm tm;
	
	/* ignore cssmTime->timeType for now */
	if(cuTimeStringToTm((char *)cssmTime->Data, cssmTime->Length, &tm)) {
		printf("***Bad time string format***\n");
		return;
	}
	if(tm.tm_mon > 11) {
		printf("***Bad time string format***\n");
		return;
	}
	printf("%02d:%02d:%02d %s %d, %04d\n",
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		months[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);

}

static void freeAttrData(
	CSSM_DB_ATTRIBUTE_DATA *attrData,
	unsigned numAttrs)
{
	if(attrData == NULL) {
		return;
	}
	for(unsigned dex=0; dex<numAttrs; dex++) {
		CSSM_DB_ATTRIBUTE_DATA *a = &attrData[dex];
		if(a->Value == NULL) {
			continue;
		}
		for(unsigned i=0; i<a->NumberOfValues; i++) {
			CSSM_DATA_PTR d = &a->Value[i];
			if(d->Data) {
				APP_FREE(d->Data);
			}
		}
		APP_FREE(a->Value);
	}
}

static void printString(
	const char *title,
	const CSSM_DATA *str)
{
	unsigned i;
	printf("%s: %lu bytes: ", title, str ? str->Length : 0);
	if((str == NULL) || (str->Length == 0)) {
		printf("Empty\n");
		return;
	}
	char *cp = (char *)str->Data;
	for(i=0; i<str->Length; i++) {
		printf("%c", *cp++);
	}
	printf("\n");
}

static void printData(
	const char *title,
	const CSSM_DATA *d)
{
	printf("%s: %lu bytes: ", title, d ? d->Length : 0);
	if((d == NULL) || (d->Length == 0)) {
		printf("Empty\n");
		return;
	}
	unsigned toPrint = 20;
	bool ellipsis = false;
	if(d->Length < toPrint) {
		toPrint = d->Length;
	}	
	else if(d->Length > toPrint) {
		ellipsis = true;
	}
	for(unsigned dex=0; dex<toPrint; dex++) {
		printf("%02X", d->Data[dex]);
		if((dex % 4) == 3) {
			printf(" ");
		}
	}
	if(ellipsis) {
		printf("...\n");
	}
	else {
		printf("\n");
	}
}

static void printCertID(
	const CSSM_DATA &certID)
{
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);
	SecAsn1OCSPCertID asn1CertID;
	memset(&asn1CertID, 0, sizeof(SecAsn1OCSPCertID));
	if(SecAsn1DecodeData(coder, &certID, kSecAsn1OCSPCertIDTemplate, &asn1CertID)) {
		printf("***ERROR decoding stored CertID\n");
		return;
	}
	
	printf("   --- Parsed CertID ---\n");
	printf("   "); printData("issuerNameHash  ", &asn1CertID.issuerNameHash);
	printf("   "); printData("issuerPubKeyHash", &asn1CertID.issuerPubKeyHash);
	printf("   "); printData("serialNumber    ", &asn1CertID.serialNumber);
	SecAsn1CoderRelease(coder);
}
static void printRecord(
	const CSSM_DB_ATTRIBUTE_DATA *attrData, 
	unsigned numAttrs,
	const CSSM_DATA *recordData)
{
	printf("===== printRecord: %u attributes, %lu bytes of data =====\n", numAttrs, 
		recordData ? recordData->Length : 0);
	for(unsigned dex=0; dex<numAttrs; dex++) {
		const CSSM_DB_ATTRIBUTE_DATA *a = &attrData[dex];
		char *attrName = a->Info.Label.AttributeName;
		CSSM_DB_ATTRIBUTE_FORMAT form = a->Info.AttributeFormat;
		if(a->NumberOfValues == 0) {
			printf("++++++++ zero values for attribute %s ++++++++\n", attrName);
		}
		for(unsigned v=0; v<a->NumberOfValues; v++) {
			if(!memcmp(attrName, "Expiration", 10)) {
				const CSSM_DATA *td = &a->Value[v];
				printf("Expiration: ");
				printTimeStr(td);
			}
			else {
				switch(form) {
					case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
						printString(attrName, &a->Value[v]);
						break;
					case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
						printData(attrName, &a->Value[v]);
						break;
					default:
						printf("Help! How do you print format %u\n", (unsigned)form);
						break;
				}
				if(!memcmp(attrName, "CertID", 6)) {
					printCertID(a->Value[v]);
				}
			}
		}
	}
}

static void dumpOcspdDb(char *dbFile)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DATA						recordData = {0, NULL};
	CSSM_DB_ATTRIBUTE_DATA			attrData[OCSPD_NUM_DB_ATTRS];
	CSSM_DB_ATTRIBUTE_INFO			certIDInfo =  OCSPD_DBATTR_CERT_ID ;
	CSSM_DB_ATTRIBUTE_INFO			uriInfo    =  OCSPD_DBATTR_URI ;
	CSSM_DB_ATTRIBUTE_INFO			expireInfo =  OCSPD_DBATTR_EXPIRATION ;
	
	if(dbAttach(dbFile)) {
		printf("***Error opening %s. Aborting.\n", dbFile);
		return;
	}
	
	recordAttrs.DataRecordType = OCSPD_DB_RECORDTYPE;
	recordAttrs.NumberOfAttributes = OCSPD_NUM_DB_ATTRS;
	recordAttrs.AttributeData = attrData;
	attrData[0].Info = certIDInfo;
	attrData[1].Info = uriInfo;
	attrData[2].Info = expireInfo;
	
	/* just search by recordType, no predicates */
	query.RecordType = OCSPD_DB_RECORDTYPE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = CSSM_QUERY_RETURN_DATA;

	crtn = CSSM_DL_DataGetFirst(dlDbHandle,
		&query,
		&resultHand,
		&recordAttrs,
		&recordData,
		&recordPtr);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			printf("***No records found in %s.\n", dbFile);
			/* OK, no certs */
			return;
		default:
			printError("DataGetFirst", crtn);
			return;
	}
	
	printRecord(attrData, OCSPD_NUM_DB_ATTRS, &recordData);
	freeAttrData(attrData, 3);
	CSSM_DL_FreeUniqueRecord(dlDbHandle, recordPtr);
	if(recordData.Data) {
		APP_FREE(recordData.Data);
		recordData.Data = NULL;
	}
	
	/* now the rest of them */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDbHandle,
			resultHand, 
			&recordAttrs,
			NULL,
			&recordPtr);
		switch(crtn) {
			case CSSM_OK:
				printRecord(attrData, OCSPD_NUM_DB_ATTRS, &recordData);
				freeAttrData(attrData, 3);
				CSSM_DL_FreeUniqueRecord(dlDbHandle, recordPtr);
				if(recordData.Data) {
					APP_FREE(recordData.Data);
					recordData.Data = NULL;
				}
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				break;
			default:
				printError("DataGetNext", crtn);
				return;
		}
		if(crtn != CSSM_OK) {
			break;
		}
	}
	CSSM_DL_DataAbortQuery(dlDbHandle, resultHand);
	return;
}

static void cleanOcspdDb(char *dbFile)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	unsigned						numRecords = 0;
	
	if(dbAttach(dbFile)) {
		printf("***Error opening %s. Aborting.\n", dbFile);
		return;
	}
	
	/* just search by recordType, no predicates, no returned attrs, no returned data*/
	query.RecordType = OCSPD_DB_RECORDTYPE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;

	crtn = CSSM_DL_DataGetFirst(dlDbHandle,
		&query,
		&resultHand,
		NULL,
		NULL,
		&recordPtr);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			printf("***No records found in %s.\n", dbFile);
			/* OK, no certs */
			return;
		default:
			printError("DataGetFirst", crtn);
			return;
	}
	
	crtn = CSSM_DL_DataDelete(dlDbHandle, recordPtr);
	if(crtn) {
		cssmPerror("CSSM_DL_DataDelete", crtn);
	}
	CSSM_DL_FreeUniqueRecord(dlDbHandle, recordPtr);
	if(crtn) {
		return;
	}
	numRecords++;
	
	/* now the rest of them */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDbHandle,
			resultHand, 
			NULL,
			NULL,
			&recordPtr);
		switch(crtn) {
			case CSSM_OK:
				crtn = CSSM_DL_DataDelete(dlDbHandle, recordPtr);
				if(crtn) {
					cssmPerror("CSSM_DL_DataDelete", crtn);
				}
				CSSM_DL_FreeUniqueRecord(dlDbHandle, recordPtr);
				if(crtn) {
					return;
				}
				numRecords++;
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				break;
			default:
				printError("DataGetNext", crtn);
				return;
		}
		if(crtn != CSSM_OK) {
			break;
		}
	}
	CSSM_DL_DataAbortQuery(dlDbHandle, resultHand);
	printf("...%u records deleted\n", numRecords);
	return;
}

int main(int argc, char **argv)
{
	if(argc < 2) {
		usage(argv);
	}
	
	extern int optind;
	// extern char *optarg;
	int arg;
    
    optind = 2;
    while ((arg = getopt(argc, argv, "h")) != -1) {
		switch (arg) {
			case 'h':
			case '?':
			default:
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	switch (argv[1][0]) {
		case 'd':
			dumpOcspdDb((char *)OCSP_DB_FILE);
			break;
		case 'f':
			ocspdCacheFlushStale();
			printf("...stale entries flushed\n");
			break;
		case 'F':
			cleanOcspdDb((char *)OCSP_DB_FILE);
			break;
		default:
			usage(argv);
	}
	return 0;
}

