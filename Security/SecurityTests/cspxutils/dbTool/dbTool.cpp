/* Copyright (c) 2002-2006 Apple Computer, Inc.
 *
 * dbTool.cpp - DL/DB tool.
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <strings.h>
#include <ctype.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include <Security/cssmapplePriv.h>
#include "cspwrap.h"
#include "common.h"
#include "dbAttrs.h"
#include "dbCert.h"
#include "cspdlTesting.h"


static void usage(char **argv)
{
	printf("usage: %s dbFileName command [options]\n", argv[0]);
	printf("Commands:\n");
	printf("   r   Dump Schema Relations\n");
	printf("   k   Dump all keys\n");
	printf("   c   Dump certs\n");
	printf("   a   Dump all records\n");
	printf("   d   Delete records (interactively)\n");
	printf("   D   Delete records (noninteractively, requires really arg)\n");
	printf("   i   Import bad cert and its (good) private key\n");
	printf("Options:\n");
	printf("   v   verbose\n");
	printf("   q   quiet\n");
	printf("   R   really! (for D command)\n");
	printf("   d   dump data\n");
	printf("   c=certFile\n");
	printf("   k=keyFile\n");
	exit(1);
}


static unsigned indentVal = 0;
static void indentIncr()
{
	indentVal += 3;
}

static void indentDecr()
{
	if(indentVal) {
		indentVal -= 3;
	}
}

static void doIndent()
{
	unsigned i;
	for(i=0; i<indentVal; i++) {
		printf(" ");
	}
}

#define NORM_KEY_LEN	20

/* print an attribute name, padding out to NORM_KEY_LEN columns */
static void printName(
	const CSSM_DB_ATTRIBUTE_INFO *attrInfo)
{
	switch(attrInfo->AttributeNameFormat) {
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		{
			char *attrName = attrInfo->Label.AttributeName;
			printf("%s", attrName);
			int len = strlen(attrName);
			if(len > NORM_KEY_LEN) {
				return;
			}
			int numSpaces = NORM_KEY_LEN - len;
			for(int i=0; i<numSpaces; i++) {
				putchar(' ');
			}
			break;
		}
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		{
			/* OSType, endian dependent... */
			char *cp = (char *)&(attrInfo->Label.AttributeID);
			for(unsigned i=0; i<4; i++) {
				putchar(*cp++);
			}
			printf("                ");
			break;
		}
		default:
			printf("Unknown attribute name format (%u)\n", 
				(unsigned)attrInfo->AttributeNameFormat);
			break;
	}
}

/*
 * Attempt to print a numeric value as a string, per a NameValuePair table.
 * If the value is in fact a collection of legal values (per the nameValues
 * array), the value will just be printed in hex.  
 */
static void printValueAsString(
	unsigned val, 
	const NameValuePair *nameValues)
{
	if(nameValues != NULL) {
		while(nameValues->name != NULL) {
			if(nameValues->value == val) {
				printf("%s", nameValues->name);
				return;
			}
			nameValues++;
		}
	}
	/* Oh well */
	printf("0x%x", val);
}

static void safePrint(
	uint8 *cp, 
	uint32 len)
{
	for(unsigned i=0; i<len; i++) {
		printf("%c", *cp++);
	}
}

/* See if a blob is printable. Used for BLOB and UINT32 types, the latter of 
 * which is sometimes used for OSType representation of attr name. */
bool isPrintable(
	const CSSM_DATA *dp)
{
	bool printable = true;
	uint8 *cp = dp->Data;
	for(unsigned i=0; i<dp->Length; i++) {
		if(*cp == 0) {
			if(i != (dp->Length - 1)) {
				/* data contains NULL character before end */
				printable = false;
			}
			/* else end of string */
			break;
		}
		if(!isprint(*cp)) {
			printable = false;
			break;
		}
		cp++;
	}
	return printable;
}

#define MAX_BLOB_TO_PRINT	12
static void printBlob(
	const CSSM_DATA *data)
{
	unsigned toPrint = data->Length;
	if(toPrint > MAX_BLOB_TO_PRINT) {
		toPrint = MAX_BLOB_TO_PRINT;
	}
	for(unsigned i=0; i<toPrint; i++) {
		unsigned dat = data->Data[i];
		printf("%02X ", dat);
	}
	if(toPrint < data->Length) {
		printf("...");
	}
}

static void printAttrData(
	const CSSM_DB_ATTRIBUTE_INFO *attrInfo,
	const CSSM_DATA *attrData,
	const NameValuePair *nameValues)		// optional
{
	void *data = attrData->Data;
	
	switch(attrInfo->AttributeFormat) {
	
		case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
			putchar('\'');
			safePrint(attrData->Data, attrData->Length);
			putchar('\'');
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
		{
			unsigned val = *(unsigned *)data;
			printValueAsString(val, nameValues);
			break;
		}
		case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
		{
			printf("BLOB length %u : ", (unsigned)attrData->Length);
			/* see if it happens to be a printable string */
			if(isPrintable(attrData)) {
				putchar('\'');
				safePrint(attrData->Data, attrData->Length);
				putchar('\'');
			}
			else {
				printBlob(attrData);
			}
			break;
		}
		case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
		{
			printf("multi_int[");
			uint32 numInts = attrData->Length / sizeof(uint32);
			uint32 *uip = (uint32 *)data;
			for(unsigned i=0; i<numInts; i++) {
				if(i > 0) {
					printf(", ");
				}
				printValueAsString(*uip++, nameValues);
			}
			printf("]");
			break;
		}
		case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
			putchar('\'');
			safePrint(attrData->Data, attrData->Length);
			putchar('\'');
			break;
			
		default:
			printf("***UNKNOWN FORMAT (%u), Length %u",
				(unsigned)attrInfo->AttributeFormat, (unsigned)attrData->Length);
			break;
	}
}

/* free attribute(s) allocated by DL */
static void freeAttrs(
	CSSM_DB_RECORD_ATTRIBUTE_DATA *recordAttrs)
{
	unsigned i;
	
	for(i=0; i<recordAttrs->NumberOfAttributes; i++) {
		CSSM_DB_ATTRIBUTE_DATA_PTR attrData = &recordAttrs->AttributeData[i];
		if(attrData == NULL) {
			/* fault of caller, who allocated the CSSM_DB_ATTRIBUTE_DATA */
			printf("***freeAttrs screwup: NULL attrData\n");
			return;
		}
		unsigned j;
		for(j=0; j<attrData->NumberOfValues; j++) {
			CSSM_DATA_PTR data = &attrData->Value[j];
			if(data == NULL) {
				/* fault of MDS, who said there was a value here */
				printf("***freeAttrs screwup: NULL data\n");
				return;
			}
			CSSM_FREE(data->Data);
			data->Data = NULL;
			data->Length = 0;
		}
		CSSM_FREE(attrData->Value);
		attrData->Value = NULL;
	}
}

static void dumpDataBlob(
	const CSSM_DATA *datap)
{
	doIndent();
	printf("Record data length %lu ", datap->Length);
	if(datap->Length != 0) {
		printf(" : ");
		printBlob(datap);
	}
	printf("\n");
}

static void dumpRecordAttrs(
	const CSSM_DB_RECORD_ATTRIBUTE_DATA *recordAttrs,
	const NameValuePair 				**nameValues,		// parallel to recordAttrs
	const CSSM_DATA						*recordData = NULL)	// optional data
{
	unsigned valNum;
	unsigned dex;

	for(dex=0; dex<recordAttrs->NumberOfAttributes; dex++) {
		const CSSM_DB_ATTRIBUTE_DATA *attrData = &recordAttrs->AttributeData[dex];
		doIndent();
		printName(&attrData->Info);
		printf(": ");
		if(attrData->NumberOfValues == 0) {
			printf("<<not present>>\n");
			continue;
		}
		for(valNum=0; valNum<attrData->NumberOfValues; valNum++) {
			printAttrData(&attrData->Info, &attrData->Value[valNum], nameValues[dex]);
			if(valNum < (attrData->NumberOfValues - 1)) {
				printf(", ");
			}
		}
		printf("\n");
	}
	if(recordData) {
		dumpDataBlob(recordData);
	}
}

static void dumpRelation(
	CSSM_DL_DB_HANDLE 		dlDbHand, 
	const RelationInfo		*relInfo,
	CSSM_BOOL				dumpData)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	CSSM_DB_ATTRIBUTE_DATA			*attrs;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	unsigned 						attrDex;
	unsigned 						recNum = 0;
	uint32							numAttrs = relInfo->NumberOfAttributes;
	CSSM_DATA						data = {0, NULL};
	CSSM_DATA_PTR					datap = NULL;
	
	if(dumpData) {
		datap = &data;
	}
	
	/* build an attr array from known schema */
	attrs = (CSSM_DB_ATTRIBUTE_DATA *)CSSM_MALLOC(
		sizeof(CSSM_DB_ATTRIBUTE_DATA) * numAttrs);
	memset(attrs, 0, sizeof(CSSM_DB_ATTRIBUTE_DATA) * numAttrs);
	for(attrDex=0; attrDex<numAttrs; attrDex++) {
		attrs[attrDex].Info = relInfo->AttributeInfo[attrDex];
	}
	recordAttrs.DataRecordType = relInfo->DataRecordType;
	recordAttrs.NumberOfAttributes = numAttrs;
	recordAttrs.AttributeData = attrs;
	
	/* just search by recordType, no predicates */
	query.RecordType = relInfo->DataRecordType;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		datap,
		&record);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			printf("%s: no record found\n", relInfo->relationName);
			CSSM_FREE(attrs);
			return;
		default:
			printError("DataGetFirst", crtn);
			CSSM_FREE(attrs);
			return;
	}
	printf("%s:\n", relInfo->relationName);
	printf("   record %d; numAttrs %d:\n", 
		recNum++, (int)recordAttrs.NumberOfAttributes);
	indentIncr();
	
	dumpRecordAttrs(&recordAttrs, relInfo->nameValues, datap);
	CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	freeAttrs(&recordAttrs);
	if(datap) {
		CSSM_FREE(datap->Data);
	}
	
	/* now the rest of them */
	/* hopefully we don't have to re-init the recordAttr array */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDbHand,
			resultHand, 
			&recordAttrs,
			datap,
			&record);
		switch(crtn) {
			case CSSM_OK:
				printf("   record %d; numAttrs %d:\n", 
					recNum++, (int)recordAttrs.NumberOfAttributes);
				dumpRecordAttrs(&recordAttrs, relInfo->nameValues, datap);
				CSSM_DL_FreeUniqueRecord(dlDbHand, record);
				freeAttrs(&recordAttrs);
				if(datap) {
					CSSM_FREE(datap->Data);
				}
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				break;
			default:
				printError("DataGetNext", crtn);
				break;
		}
		if(crtn != CSSM_OK) {
			break;
		}
	}
	indentDecr();
	CSSM_FREE(attrs);
}

/*
 * Given a record type and a CSSM_DB_UNIQUE_RECORD, fetch and parse all the 
 * attributes we can.
 */
static void fetchParseRecord(
	CSSM_DL_DB_HANDLE				dlDbHand,
	CSSM_DB_RECORD_ATTRIBUTE_DATA	*inRecordAttrs,
	CSSM_DB_UNIQUE_RECORD_PTR		record,
	const CSSM_DATA_PTR				datap,
	CSSM_BOOL						dumpData)
{
	const RelationInfo *relInfo = NULL;
	
	/* infer RelationInfo from recordType */
	switch(inRecordAttrs->DataRecordType) {
		case CSSM_DL_DB_RECORD_PUBLIC_KEY:
		case CSSM_DL_DB_RECORD_PRIVATE_KEY:
		case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
			relInfo = &allKeysRelation;
			break;
		case CSSM_DL_DB_RECORD_GENERIC_PASSWORD:
		case CSSM_DL_DB_RECORD_INTERNET_PASSWORD:
		case CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD:
			relInfo = &genericKcRelation;
			break;
		case CSSM_DL_DB_RECORD_CERT:
			relInfo = &certRecordRelation;
			break;
		case CSSM_DL_DB_RECORD_X509_CERTIFICATE:
			relInfo = &x509CertRecordRelation;
			break;
		case CSSM_DL_DB_RECORD_X509_CRL:
			relInfo = &x509CrlRecordRelation;
			break;
		case CSSM_DL_DB_RECORD_USER_TRUST:
			relInfo = &userTrustRelation;
			break;
		case CSSM_DL_DB_RECORD_UNLOCK_REFERRAL:
			relInfo = &referralRecordRelation;
			break;
		case CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE:
			relInfo = &extendedAttrRelation;
			break;
		case DBBlobRelationID:
			relInfo = NULL;
			doIndent();
			printf("--- No attributes ---\n");
			if(dumpData) {
				dumpDataBlob(datap);
			}
			return;
		default:
			doIndent();
			printf("<<unparsed>>\n");
			if(dumpData) {
				doIndent();
				printf("Record blob (length %ld): ", datap->Length);
				printBlob(datap);
				printf("\n");
			}
			return;
	}
	
	CSSM_DB_ATTRIBUTE_DATA			*attrs = NULL;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	unsigned 						attrDex;
	uint32							numAttrs = relInfo->NumberOfAttributes;
	CSSM_RETURN						crtn;
	CSSM_DATA 						recordData = {0, NULL};
	CSSM_DATA_PTR					recordDataP = dumpData ? &recordData : NULL;
	
	/* build an attr array from known schema */
	attrs = (CSSM_DB_ATTRIBUTE_DATA *)CSSM_MALLOC(
		sizeof(CSSM_DB_ATTRIBUTE_DATA) * numAttrs);
	memset(attrs, 0, sizeof(CSSM_DB_ATTRIBUTE_DATA) * numAttrs);
	for(attrDex=0; attrDex<numAttrs; attrDex++) {
		attrs[attrDex].Info = relInfo->AttributeInfo[attrDex];
	}

	/* from inRecordAttrs, not the relInfo, which could be a typeless template */
	recordAttrs.DataRecordType = relInfo->DataRecordType;
	recordAttrs.NumberOfAttributes = numAttrs;
	recordAttrs.AttributeData = attrs;

	crtn = 	CSSM_DL_DataGetFromUniqueRecordId(dlDbHand,
		record,
		&recordAttrs,
		recordDataP);
	if(crtn) {
		printError("CSSM_DL_DataGetFromUniqueRecordId", crtn);
		goto abort;
	}
	dumpRecordAttrs(&recordAttrs, relInfo->nameValues, recordDataP);
	freeAttrs(&recordAttrs);
	if(recordData.Data) {
		CSSM_FREE(recordData.Data);
	}
abort:
	if(attrs) {
		CSSM_FREE(attrs);
	}
	return;
}
	
static void deleteRecord(
	CSSM_DL_DB_HANDLE 			dlDbHand,
	CSSM_DB_UNIQUE_RECORD_PTR	record,
	CSSM_BOOL					interact)
{
	if(interact) {
		fpurge(stdin);
		printf("\nDelete this record [y/anything] ? ");
		char resp = getchar();
		if(resp != 'y') {
			return;
		}
	}
	CSSM_RETURN crtn;
	crtn = CSSM_DL_DataDelete(dlDbHand, record);
	if(crtn) {
		printError("CSSM_DL_DataDelete", crtn);
	}
	else if(interact) {
		printf("...record deleted\n\n");
	}
}

/*
 * In this case we search for CSSM_DL_DB_RECORD_ANY. The current schema results
 * in no single attribute which all interesting records have in common, so we
 * can't grab any attributes at GetFirst/GetNext time. Instead we have
 * to deal with the returned record per its record type. 
 */
static void dumpAllRecords(
	CSSM_DL_DB_HANDLE 		dlDbHand,
	CSSM_BOOL				deleteAll,
	CSSM_BOOL				interact,
	CSSM_BOOL				dumpData)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DATA						data = {0, NULL};
	CSSM_DATA_PTR					datap = NULL;
	
	if(dumpData) {
		datap = &data;
	}
	
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_ANY;
	recordAttrs.NumberOfAttributes = 0;
	recordAttrs.AttributeData = NULL;
	
	/* just search by recordType, no predicates */
	query.RecordType = CSSM_DL_DB_RECORD_ANY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		datap,	
		&record);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			printf("CSSM_DL_DB_RECORD_ANY: no record found\n");
			return;
		default:
			printError("DataGetFirst", crtn);
			return;
	}

	/* could be anything; check it out */
	if(interact) {
		doIndent();
		printValueAsString(recordAttrs.DataRecordType, recordTypeNames);
		printf("\n");
		indentIncr();
		fetchParseRecord(dlDbHand, &recordAttrs, record, datap, dumpData);
		indentDecr();
	}
	if(deleteAll && (recordAttrs.DataRecordType != DBBlobRelationID)) {
		/* NEVER delete a DBBlob */
		deleteRecord(dlDbHand, record, interact);
	}
	CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	
	/* now the rest of them */
	/* hopefully we don't have to re-init the recordAttr array */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDbHand,
			resultHand, 
			&recordAttrs,
			datap,
			&record);
		switch(crtn) {
			case CSSM_OK:
				if(interact) {
					doIndent();
					printValueAsString(recordAttrs.DataRecordType, recordTypeNames);
					printf("\n");
					indentIncr();
					fetchParseRecord(dlDbHand, &recordAttrs, record, datap, dumpData);
					indentDecr();
				}
				if(deleteAll && (recordAttrs.DataRecordType != DBBlobRelationID)) {
					/* NEVER delete a DBBlob */
					deleteRecord(dlDbHand, record, interact);
				}
				CSSM_DL_FreeUniqueRecord(dlDbHand, record);
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				break;
			default:
				printError("DataGetNext", crtn);
				break;
		}
		if(crtn != CSSM_OK) {
			break;
		}
	}
}

int main(
	int argc, 
	char **argv)
{
	int					arg;
	char				*argp;
	char				*dbFileName;
	char				cmd;
	CSSM_DL_DB_HANDLE	dlDbHand;
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_BOOL			quiet = CSSM_FALSE;
	char				*certFile = NULL;
	char				*keyFile = NULL;
	CSSM_BOOL			interact = CSSM_TRUE;
	CSSM_BOOL			dumpData = CSSM_FALSE;
	
	/* should be cmd line opts */
	CSSM_ALGORITHMS		keyAlg = CSSM_ALGID_RSA;
	CSSM_BOOL			pemFormat = CSSM_FALSE;
	CSSM_KEYBLOB_FORMAT	keyFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	CSSM_RETURN 		crtn = CSSM_OK;
	
	if(argc < 3) {
		usage(argv);
	}
	dbFileName = argv[1];
	cmd = argv[2][0];
	
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'R':
				if(cmd == 'D') {
					interact = CSSM_FALSE;
				}
				break;
			case 'd':
				dumpData = CSSM_TRUE;
				break;
			case 'c':
				certFile = &argp[2];
				break;
			case 'k':
				keyFile = &argp[2];
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	
	dlDbHand.DLHandle = dlStartup();
	if(dlDbHand.DLHandle == 0) {
		exit(1);
	}
	if(cmd == 'i') {
		crtn = importBadCert(dlDbHand.DLHandle, dbFileName, certFile, 
			keyFile, keyAlg, pemFormat, keyFormat, verbose);
		goto done;
	}
	crtn = dbCreateOpen(dlDbHand.DLHandle, dbFileName, 
		CSSM_FALSE, CSSM_FALSE, NULL, &dlDbHand.DBHandle);
	if(crtn) {
		exit(1);
	}
	switch(cmd) {
		case 'r':
			dumpRelation(dlDbHand, &schemaInfoRelation, dumpData);
			break;
		case 'k':
			dumpRelation(dlDbHand, &allKeysRelation, dumpData);
			break;
		case 'c':
			dumpRelation(dlDbHand, &x509CertRecordRelation, dumpData);
			break;
		case 'a':
			dumpAllRecords(dlDbHand, CSSM_FALSE, CSSM_TRUE, dumpData);
			break;
		case 'd':
		case 'D':
			dumpAllRecords(dlDbHand, CSSM_TRUE, interact, dumpData);
			if(!interact) {
				/* we ignored errors.... */
				if(!quiet) {
					printf("...DB %s wiped clean\n", dbFileName);
				}
			}
			break;
		default:
			usage(argv);
	}
	CSSM_DL_DbClose(dlDbHand);
done:
	CSSM_ModuleDetach(dlDbHand.DLHandle);
	return crtn;
}
