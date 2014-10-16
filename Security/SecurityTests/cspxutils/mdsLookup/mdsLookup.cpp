/*
 * mdsLookup.cpp - demonstrate some MDS lookups
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <Security/mds.h>
#include <Security/mds_schema.h>
#include <Security/oidsalg.h>		// for TP OIDs
#include "common.h"
#include <strings.h>

/* the memory functions themselves are in utilLib/common.c. */
static CSSM_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };
 
static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("   k   keep connected and go again\n");
	exit(1);
}

#define NORM_KEY_LEN	10

/* print a key name, padding out to NORM_KEY_LEN columns */
static void printName(
	const char *attrName)
{
	printf("   %s", attrName);
	int len = strlen(attrName);
	if(len > NORM_KEY_LEN) {
		return;
	}
	int numSpaces = NORM_KEY_LEN - len;
	for(int i=0; i<numSpaces; i++) {
		putchar(' ');
	}

}

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
				printf("0x%x", (unsigned)*uip++);
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
 * as string.  
 */
static void dumpRecord(
	const CSSM_DB_RECORD_ATTRIBUTE_DATA *recordAttrs)
{
	unsigned dex;
	for(dex=0; dex<recordAttrs->NumberOfAttributes; dex++) {
		const CSSM_DB_ATTRIBUTE_DATA *attrData = &recordAttrs->AttributeData[dex];
		if(attrData->Info.AttributeNameFormat != 
				CSSM_DB_ATTRIBUTE_NAME_AS_STRING) {
			printf("***BAD ATTR_NAME FORMAT (%u)\n", 
				(unsigned)attrData->Info.AttributeNameFormat);
				continue;
		}
		const char *attrName = attrData->Info.Label.AttributeName;
		printName(attrName);
		printf(": ");
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
 * Core MDS lookup routine. Used in two situations. It's called by main() to perform
 * a lookup in the CDSA Directory Database based one one key/value pair; this
 * call fetches one attribute from the associated record - the GUID ("ModuleID" 
 * in MDS lingo). Then the function calls itself to do a lookup in the Object DB,
 * based on that GUID, in order to fetch the path of the module associated with
 * that GUID. The first call (from main()) corresponds to an application's
 * typical use of MDS. The recursive call, which does a lookup in the Object
 * DB, corresponds to CSSM's typical use of MDS, which is to map a GUID to a 
 * bundle path.
 *
 * The ModuleID and Path of all modules satisfying the initial search criteria
 * are displayed on stdout. 
 *
 * Caller specifies one search attribute, by name, value,Êand value format. 
 * Whether this is the first or second (recursive) call is indicated by the 
 * cdsaLookup argument. That determines both the DB to search and the attribute
 * to fetch (ModuleID or Path).
 */
static void doLookup(
	MDS_FUNCS 					*mdsFuncs,
	
	/* Two DBs and a flag indicating which one to use */
	MDS_DB_HANDLE 				objDlDb,
	MDS_DB_HANDLE 				cdsaDlDb,
	bool						cdsaLookup,	// true - use cdsaDlDb; false - objDlDb
	
	/* Record type, a.k.a. Relation, e.g. MDS_CDSADIR_CSP_PRIMARY_RECORDTYPE */
	CSSM_DB_RECORDTYPE			recordType,	
	
	/* key, value, valForm, and valOp are the thing we search on */
	/* Note CSSM_DB_ATTRIBUTE_NAME_FORMAT - the format of the attribute name - 
	 *    is always CSSM_DB_ATTRIBUTE_NAME_AS_STRING for MDS. */
	const char					*key,		// e.g. "AlgType"
	const void					*valPtr,	
	unsigned					valLen,
	CSSM_DB_ATTRIBUTE_FORMAT	valForm,	// CSSM_DB_ATTRIBUTE_FORMAT_STRING, etc.
	CSSM_DB_OPERATOR			valOp,		// normally CSSM_DB_EQUAL
	
	/* for display only */
	const char					*srchStr)
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DATA						predData;
	CSSM_DB_ATTRIBUTE_DATA			outAttr;
	CSSM_DB_ATTRIBUTE_INFO_PTR		attrInfo;
	CSSM_RETURN						crtn;
	MDS_DB_HANDLE					dlDb;
	const char 						*attrName;
	
	if(cdsaLookup) {
		/* first call, fetching guid from the CDSA Directory DB */
		dlDb = cdsaDlDb;
		attrName = "ModuleID";
	}
	else {
		/* recursive call, fetching path from Object DB */
		dlDb = objDlDb;
		attrName = "Path";
	}
	
	/* We want one attributes back, name and format specified by caller */
	recordAttrs.DataRecordType = recordType;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 1;
	recordAttrs.AttributeData = &outAttr;

	memset(&outAttr, 0, sizeof(CSSM_DB_ATTRIBUTE_DATA));
	attrInfo = &outAttr.Info;
	attrInfo->AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attrInfo->Label.AttributeName = (char *)attrName;
	attrInfo->AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_STRING;

	/* one predicate - the caller's key and CSSM_DB_OPERATOR */
	predicate.DbOperator = valOp;
	predicate.Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = (char *)key;
	predicate.Attribute.Info.AttributeFormat = valForm;
	predData.Data = (uint8 *)valPtr;
	predData.Length = valLen;
	predicate.Attribute.Value = &predData;
	predicate.Attribute.NumberOfValues = 1;
	
	query.RecordType = recordType;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	query.SelectionPredicate = &predicate;
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
			printf("%s: no record found\n", srchStr);
			return;
		default:
			printError("DataGetFirst", crtn);
			return;
	}
	/* dump this record, one attribute */
	if(srchStr) {
		/* not done on recursive call */
		printf("%s found:\n", srchStr);		
	}
	dumpRecord(&recordAttrs);
	mdsFuncs->FreeUniqueRecord(dlDb, record);
	
	if(srchStr != NULL) {
		/* 
		 * Now do a lookup in Object DB of this guid, looking for path. 
		 * Apps normally don't do this; this is what CSSM does when given
		 * the GUID of a module.
		 */
		if(outAttr.Value == NULL) {
			printf("***Screwup: DataGetFirst worked, but no outAttr\n");
			return;
		}
		doLookup(mdsFuncs,
			objDlDb,
			cdsaDlDb,
			false,			 		// use objDlDb
			MDS_OBJECT_RECORDTYPE,
			"ModuleID",				// key
			outAttr.Value->Data,	// valPtr, ModuleID, as string
			outAttr.Value->Length,	// valLen
			CSSM_DB_ATTRIBUTE_FORMAT_STRING,
			CSSM_DB_EQUAL,
			NULL);					// srchStr
	}
	freeAttrs(&recordAttrs);

	/* now the rest of them */
	for(;;) {
		crtn = mdsFuncs->DataGetNext(dlDb,
			resultHand, 
			&recordAttrs,
			NULL,
			&record);
		switch(crtn) {
			case CSSM_OK:
				dumpRecord(&recordAttrs);
				mdsFuncs->FreeUniqueRecord(cdsaDlDb, record);
				if(srchStr != NULL) {
					if(outAttr.Value == NULL) {
						printf("***Screwup: DataGetNext worked, but no outAttr\n");
						return;
					}
					doLookup(mdsFuncs,
						objDlDb,
						cdsaDlDb,
						false,			 		// use objDlDb
						MDS_OBJECT_RECORDTYPE,
						"ModuleID",				// key
						outAttr.Value->Data,	// valPtr, ModuleID, as string
						outAttr.Value->Length,	// valLen
						CSSM_DB_ATTRIBUTE_FORMAT_STRING,
						CSSM_DB_EQUAL,
						NULL);					// srchStr
				}
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

int main(int argc, char **argv)
{
	MDS_FUNCS 			mdsFuncs;
	MDS_HANDLE 			mdsHand;
	CSSM_RETURN 		crtn;
	int 				arg;
	CSSM_DB_HANDLE		dbHand = 0;
	MDS_DB_HANDLE		objDlDb;
	MDS_DB_HANDLE		cdsaDlDb;
	bool				keepConnected = false;
	uint32				val;
	
	for(arg=2; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'k':
				keepConnected = true;
				break;
			default:
				usage(argv);
		}
	}
	crtn = MDS_Initialize(NULL,		// callerGuid
		&memFuncs,
		&mdsFuncs,
		&mdsHand);
	if(crtn) {
		printError("MDS_Initialize", crtn);
		exit(1);
	}

	do {
		/* 
		 * Open both MDS DBs - apps normally only have to open 
		 * MDS_CDSA_DIRECTORY_NAME.
		 */
		crtn = mdsFuncs.DbOpen(mdsHand,
			MDS_OBJECT_DIRECTORY_NAME,
			NULL,				// DbLocation
			CSSM_DB_ACCESS_READ,
			NULL,				// AccessCred - hopefully optional 
			NULL,				// OpenParameters
			&dbHand);
		if(crtn) {
			printError("DbOpen(MDS_OBJECT_DIRECTORY_NAME)", crtn);
			exit(1);
		}
		objDlDb.DLHandle = mdsHand;
		objDlDb.DBHandle = dbHand;
		
		crtn = mdsFuncs.DbOpen(mdsHand,
			MDS_CDSA_DIRECTORY_NAME,
			NULL,				// DbLocation
			CSSM_DB_ACCESS_READ,
			NULL,				// AccessCred - hopefully optional 
			NULL,				// OpenParameters
			&dbHand);
		if(crtn) {
			printError("DbOpen(MDS_CDSA_DIRECTORY_NAME)", crtn);
			exit(1);
		}
		cdsaDlDb.DLHandle = mdsHand;
		cdsaDlDb.DBHandle = dbHand;
		
		/* 
		 * Do some typical lookups.
		 */

		/* a CSP which can do SHA1 digest */
		val = CSSM_ALGID_SHA1;
		doLookup(&mdsFuncs,
			objDlDb,
			cdsaDlDb,
			true,
			MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE,
			"AlgType",
			&val,
			sizeof(uint32),
			CSSM_DB_ATTRIBUTE_FORMAT_UINT32,
			CSSM_DB_EQUAL,
			"CSP for SHA1 digest");
		
		/* a TP which can do iSign verification */
		doLookup(&mdsFuncs,
			objDlDb,
			cdsaDlDb,
			true,
			MDS_CDSADIR_TP_OIDS_RECORDTYPE,
			"OID",
			CSSMOID_APPLE_ISIGN.Data,
			CSSMOID_APPLE_ISIGN.Length,
			CSSM_DB_ATTRIBUTE_FORMAT_BLOB,
			CSSM_DB_EQUAL,
			"TP for CSSMOID_APPLE_ISIGN policy");

		/* an X509-savvy CL */
		/* Very weird data form - two fields in one 32-bit word */
		val = (CSSM_CERT_X_509v3 << 16) | CSSM_CERT_ENCODING_DER;
		doLookup(&mdsFuncs,
			objDlDb,
			cdsaDlDb,
			true,
			MDS_CDSADIR_CL_PRIMARY_RECORDTYPE,
			"CertTypeFormat",
			&val,
			sizeof(uint32),
			CSSM_DB_ATTRIBUTE_FORMAT_UINT32,
			CSSM_DB_EQUAL,
			"X509 CL");

		/* A DL which can do CSSM_DB_AND */
		val = CSSM_DB_AND;
		doLookup(&mdsFuncs,
			objDlDb,
			cdsaDlDb,
			true,
			MDS_CDSADIR_DL_PRIMARY_RECORDTYPE,
			"ConjunctiveOps",
			&val,
			sizeof(uint32),
			CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32,
			/* This is a multi-uint32, meaning we want to search for a
			 * ConjunctiveOps which contains the specified value */
			CSSM_DB_CONTAINS,
			"DL with ConjunctiveOp CSSM_DB_AND");

		/* a CSP which can do CSSM_ALGID_IDEA, should fail */
		val = CSSM_ALGID_IDEA;
		doLookup(&mdsFuncs,
			objDlDb,
			cdsaDlDb,
			true,
			MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE,
			"AlgType",
			&val,
			sizeof(uint32),
			CSSM_DB_ATTRIBUTE_FORMAT_UINT32,
			CSSM_DB_EQUAL,
			"CSP for CSSM_ALGID_BLOWFISH, expect failure");

		/* a TP which can obtain a .mac signing certificate */
		doLookup(&mdsFuncs,
			objDlDb,
			cdsaDlDb,
			true,
			MDS_CDSADIR_TP_OIDS_RECORDTYPE,
			"OID",
			CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN.Data,
			CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN.Length,
			CSSM_DB_ATTRIBUTE_FORMAT_BLOB,
			CSSM_DB_EQUAL,
			"TP for .mac signing certificate policy");

		crtn = mdsFuncs.DbClose(objDlDb);
		if(crtn) {
			printError("DbClose(objDlDb)", crtn);
		}
		crtn = mdsFuncs.DbClose(cdsaDlDb);
		if(crtn) {
			printError("DbClose(cdsaDlDb)", crtn);
		}
		if(keepConnected) {
			printf("\n");
			fpurge(stdin);
			printf("Enter CR to go again: ");
			getchar();
		}
	} while(keepConnected);
	crtn = MDS_Terminate(mdsHand);
	if(crtn) {
		printError("MDS_Terminate", crtn);
	}
	return 0;
}
