/*
 * mdsdump.cpp - dump contents of system MDS databases 
 */
 
 /**** FIXME this uses a private API not currently exported in any way from
  **** Security project
  ****/
  
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <stdio.h>
#include <Security/mds.h>
#include <common.h>
#include <Security/mds_schema.h>
#include "MDSSchema.h"
#include <strings.h>

#define MAX_MDS_ATTRS	32

static CSSM_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };
 
static void showInfoTypes()
{
	printf("   o   Object records\n");
	printf("   C   CSSM records\n");
	printf("   p   Plugin Common records\n");
	printf("   c   CSP records\n");
	printf("   l   CL records\n");
	printf("   t   TP records\n");
	printf("   d   DL records\n");
	printf("   a   All records from Object DB\n");
	printf("   A   All records from CDSA directory DB\n");
}

static void usage(char **argv)
{
	printf("Usage: %s info_type [options...]\n", argv[0]);
	printf("info_type values:\n");
	showInfoTypes();
	printf("   h   help\n");
	printf("Options:\n");
	printf("   i   perform MDS_Install()\n");
	printf("   v   verbose\n");
	printf("   k   keep connected and go again\n");
	exit(1);
}

#define NORM_KEY_LEN	20

/* print a key name, padding out to NORM_KEY_LEN columns */
static void printName(
	const char *attrName)
{
	printf("      %s", attrName);
	int len = strlen(attrName);
	if(len > NORM_KEY_LEN) {
		return;
	}
	int numSpaces = NORM_KEY_LEN - len;
	for(int i=0; i<numSpaces; i++) {
		putchar(' ');
	}

}

#if 0
/*
 * Attempt to print a numeric value as a string, per a MDSNameValuePair table.
 * Of course this can not deal with OR'd together values; the MDSNameValuePair
 * mechanism does not indicate on a per-field basis which fields are OR-able. 
 * If the value is in fact a collection of legal values (per the nameValues
 * array), the value will just be printed in hex.  
 */
static void printValueAsString(
	unsigned val, 
	const Security::MDSNameValuePair *nameValues)		// optional 
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
#endif

/* print value string, surrounded by single quotes, then a newline */
static void printValue(
	const CSSM_DATA *attrValue)
{
	printf("'");
	for(uint32 dex=0; dex<attrValue->Length; dex++) {
		printf("%c", attrValue->Data[dex]);
	} 
	printf("'\n");
}

/* Print one attribute value */
static void dumpAttr(
	CSSM_DB_ATTRIBUTE_FORMAT attrForm,
	const CSSM_DATA *attrData)
{
	if((attrData == NULL) || (attrData->Data == NULL)) {
		printf("<NULL DATA>\n");
		return;
	}
	void *data = attrData->Data;
	switch(attrForm) {
		case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
			printValue(attrData);
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:	// not really supported in MDS
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
		{
			unsigned val = *(unsigned *)data;
			printf("0x%x\n", val);
			break;
		}
		case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
		{
			printf("BLOB length %u : ", (unsigned)attrData->Length);
			for(unsigned i=0; i<attrData->Length; i++) {
				unsigned dat = attrData->Data[i];
				printf("%02X ", dat);
			}
			printf("\n");
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
				printf("0x%x", (unsigned)(*uip++));
			}
			printf("]\n");
			break;
		}
		default:
			printf("***UNKNOWN FORMAT (%u), Length %u\n",
				(unsigned)attrForm, (unsigned)attrData->Length);
			break;
	}
}

/*
 * Vanilla "dump one record" routine. Assumes format of all attribute labels 
 * as string. Uses a MDSNameValuePair ptr array in parallel to the attributes 
 * themselves to facilitate displaying numeric values as strings (e.g. 
 * "CSSM_ALGID_SHA1") where possible. 
 */
static void dumpRecord(
	const CSSM_DB_RECORD_ATTRIBUTE_DATA *recordAttrs)
{
	unsigned dex;
	for(dex=0; dex<recordAttrs->NumberOfAttributes; dex++) {
		const CSSM_DB_ATTRIBUTE_DATA *attrData = &recordAttrs->AttributeData[dex];
		if(attrData->Info.AttributeNameFormat != CSSM_DB_ATTRIBUTE_NAME_AS_STRING) {
			printf("***BAD ATTR_NAME FORMAT (%u)\n", 
				(unsigned)attrData->Info.AttributeNameFormat);
				continue;
		}
		const char *attrName = attrData->Info.Label.AttributeName;
		printName(attrName);
		printf(": ");
		/* note currently in MDS NumberOfValues is always one or zero */
		for(unsigned attrNum=0; attrNum<attrData->NumberOfValues; attrNum++) {
			dumpAttr(attrData->Info.AttributeFormat, 	
				&attrData->Value[attrNum]);
		}
		if(attrData->NumberOfValues == 0) {
			printf("<<no values present>>\n");
		}
	}
}

/* free attribute(s) allocated by MDS */
static void freeAttrs(
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR	recordAttrs)
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
			appFree(data->Data, NULL);
			data->Data = NULL;
			data->Length = 0;
		}
		appFree(attrData->Value, NULL);
		attrData->Value = NULL;
	}
}

/* 
 * Fetch and display all records of specified CSSM_DB_RECORDTYPE.
 */
static void fetchAllAttrs(
	MDS_FUNCS *mdsFuncs,
	MDS_DB_HANDLE dlDb,
	CSSM_DB_RECORDTYPE recordType)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	CSSM_DB_ATTRIBUTE_DATA			attrs[MAX_MDS_ATTRS];
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	const RelationInfo				*relInfo;
	
	relInfo = MDSRecordTypeToRelation(recordType);
	if(relInfo == NULL) {
		printf("***UNKNOWN recordType %d\n", (int)recordType);
		return;
	}
	
	/* build an attr array from schema so we get all known attrs */
	memset(attrs, 0, sizeof(CSSM_DB_ATTRIBUTE_DATA) * MAX_MDS_ATTRS);
	unsigned attrDex;
	for(attrDex=0; attrDex<relInfo->NumberOfAttributes; attrDex++) {
		attrs[attrDex].Info = relInfo->AttributeInfo[attrDex];
	}
	recordAttrs.DataRecordType = recordType;
	recordAttrs.NumberOfAttributes = relInfo->NumberOfAttributes;
	recordAttrs.AttributeData = attrs;
	
	/* just search by recordType, no predicates */
	query.RecordType = recordType;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	crtn = mdsFuncs->DataGetFirst(dlDb,
		&query,
		&resultHand,
		&recordAttrs,
		NULL,			// No data
		&record);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			printf("%s: no record found\n", relInfo->relationName);
			return;
		default:
			printError("DataGetFirst", crtn);
			return;
	}
	unsigned recNum = 0;
	printf("%s:\n", relInfo->relationName);
	printf("   record %d; numAttrs %d:\n", 
		recNum++, (int)recordAttrs.NumberOfAttributes);
		
	dumpRecord(&recordAttrs);
	mdsFuncs->FreeUniqueRecord(dlDb, record);
	freeAttrs(&recordAttrs);
	
	/* now the rest of them */
	/* hopefully we don't have to re-init the recordAttr array */
	for(;;) {
		crtn = mdsFuncs->DataGetNext(dlDb,
			resultHand, 
			&recordAttrs,
			NULL,
			&record);
		switch(crtn) {
			case CSSM_OK:
				printf("   record %d; numAttrs %d:\n", 
					recNum++, (int)recordAttrs.NumberOfAttributes);
				dumpRecord(&recordAttrs);
				mdsFuncs->FreeUniqueRecord(dlDb, record);
				freeAttrs(&recordAttrs);
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

/*
 * This is different - it's schema-independent. Fetch all records from specified
 * DlDb which contain a ModuleID attribute.
 */
static void fetchAllRecords(
	MDS_FUNCS *mdsFuncs,
	MDS_DB_HANDLE dlDb)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA			theAttr;
	CSSM_DB_ATTRIBUTE_INFO_PTR		attrInfo = &theAttr.Info;
	CSSM_DATA						attrValue = {0, NULL};
	
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_ANY;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 1;
	recordAttrs.AttributeData = &theAttr;
	
	attrInfo->AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attrInfo->Label.AttributeName = (char *)"ModuleID";
	attrInfo->AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
	
	theAttr.NumberOfValues = 1;
	theAttr.Value = &attrValue;
	
	/* just search by recordType, no predicates */
	query.RecordType = CSSM_DL_DB_RECORD_ANY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	crtn = mdsFuncs->DataGetFirst(dlDb,
		&query,
		&resultHand,
		&recordAttrs,
		NULL,			// No data
		&record);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			printf("no record found\n");
			return;
		default:
			printError("DataGetFirst", crtn);
			return;
	}
	unsigned recNum = 0;
	printf("Records containing a ModuleID attribute:\n");
	printf("   record %d:\n", recNum++);
		
	dumpRecord(&recordAttrs);
	mdsFuncs->FreeUniqueRecord(dlDb, record);
	freeAttrs(&recordAttrs);
	
	/* now the rest of them */
	/* hopefully we don't have to re-init the recordAttr array */
	for(;;) {
		crtn = mdsFuncs->DataGetNext(dlDb,
			resultHand, 
			&recordAttrs,
			NULL,
			&record);
		switch(crtn) {
			case CSSM_OK:
				printf("   record %d:\n", recNum++);
				dumpRecord(&recordAttrs);
				mdsFuncs->FreeUniqueRecord(dlDb, record);
				freeAttrs(&recordAttrs);
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

static void doInstall(	
	MDS_HANDLE mdsHand)
{
	CFAbsoluteTime start, end;

	start = CFAbsoluteTimeGetCurrent();
	CSSM_RETURN crtn = MDS_Install(mdsHand);
	end = CFAbsoluteTimeGetCurrent();
	if(crtn) {
		printError("MDS_Install", crtn);
	}
	else {
		printf("MDS_Install took %gs\n", end - start);
	}
}

int main(int argc, char **argv)
{
	MDS_FUNCS 			mdsFuncs;
	MDS_HANDLE 			mdsHand;
	CSSM_RETURN 		crtn;
	int 				arg;
	char 				op;
	char 				*dbName;
	CSSM_DB_HANDLE		dbHand = 0;
	MDS_DB_HANDLE		dlDb;
	bool 				verbose = 0;
	bool				keepConnected = false;
	bool				install = false;
	
	if(argc < 2) {
		usage(argv);
	}
	op = argv[1][0];
	if(op == 'h') {
		usage(argv);
	}
	for(arg=2; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'v':
				verbose = true;
				break;
			case 'i':
				install = true;
				break;
			case 'k':
				keepConnected = true;
				break;
			default:
				usage(argv);
		}
	}
	if(verbose) {
		printf("..calling MDS_Initialize\n");
	}
	crtn = MDS_Initialize(NULL,		// callerGuid
		&memFuncs,
		&mdsFuncs,
		&mdsHand);
	if(crtn) {
		printError("MDS_Initialize", crtn);
		exit(1);
	}
	if(install) {
		doInstall(mdsHand);
	}
	do {
		/* open one or the other DB */
		switch(op) {
			case 'o':
			case 'a':
				dbName = (char *)MDS_OBJECT_DIRECTORY_NAME;
				break;
			default:
				dbName = (char *)MDS_CDSA_DIRECTORY_NAME;
				break;
		}
		crtn = mdsFuncs.DbOpen(mdsHand,
			dbName,
			NULL,				// DbLocation
			CSSM_DB_ACCESS_READ,
			NULL,				// AccessCred - hopefully optional 
			NULL,				// OpenParameters
			&dbHand);
		if(crtn) {
			printError("DbOpen", crtn);
			exit(1);
		}
		dlDb.DLHandle = mdsHand;
		dlDb.DBHandle = dbHand;
		
		/* go for it */
		switch(op) {
			case 'o':
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_OBJECT_RECORDTYPE);
				break;
			case 'C':
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_CSSM_RECORDTYPE);
				break;
			case 'p':
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_COMMON_RECORDTYPE);
				break;
			case 'c':
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_CSP_PRIMARY_RECORDTYPE);
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE);
				if(verbose) {
					fetchAllAttrs(&mdsFuncs, dlDb, 
							MDS_CDSADIR_CSP_ENCAPSULATED_PRODUCT_RECORDTYPE);
					fetchAllAttrs(&mdsFuncs, dlDb, 
							MDS_CDSADIR_CSP_SC_INFO_RECORDTYPE);
				}
				break;
			case 'l':
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_CL_PRIMARY_RECORDTYPE);
				break;
			case 't':
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_TP_PRIMARY_RECORDTYPE);
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_TP_OIDS_RECORDTYPE);
				if(verbose) {
					fetchAllAttrs(&mdsFuncs, dlDb, 
						MDS_CDSADIR_TP_ENCAPSULATED_PRODUCT_RECORDTYPE);
				}
				break;
			case 'd':
				fetchAllAttrs(&mdsFuncs, dlDb, MDS_CDSADIR_DL_PRIMARY_RECORDTYPE);
				break;
			case 'a':
			case 'A':
				fetchAllRecords(&mdsFuncs, dlDb);
				break;
			default:
				usage(argv);
		}
		
		crtn = mdsFuncs.DbClose(dlDb);
		if(crtn) {
			printError("DbClose", crtn);
		}
		if(keepConnected) {
			printf("\n");
			showInfoTypes();
			fpurge(stdin);
			printf("Enter new info type: ");
			op = getchar();
		}
	} while(keepConnected);
	crtn = MDS_Terminate(mdsHand);
	if(crtn) {
		printError("MDS_Terminate", crtn);
	}
	return 0;
}
