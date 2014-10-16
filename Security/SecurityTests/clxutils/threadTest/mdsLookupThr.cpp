/* MDS per-thread basher */
#include <time.h>
#include <stdio.h>
#include "testParams.h"
#include <security_cdsa_client/mdsclient.h>

/* for malloc debug */
#define DO_PAUSE			0

using namespace Security;

/* most of this is cribbed from cspxutils/mdsLookup/ */

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
			/* we're using cdsa_client, what's the appFree equivalent? */
			free(data->Data);
			data->Data = NULL;
			data->Length = 0;
		}
		free(attrData->Value);
		attrData->Value = NULL;
	}
}

static int doLookup(
	MDSClient::Directory		&mds,
	
	/* Record type, a.k.a. Relation, e.g. MDS_CDSADIR_CSP_PRIMARY_RECORDTYPE */
	CSSM_DB_RECORDTYPE			recordType,	
	
	/* key, value, valForm, and valOp are the thing we search on */
	/* Note CSSM_DB_ATTRIBUTE_NAME_FORMAT - the format of the attribute name - 
	 *    is always CSSM_DB_ATTRIBUTE_NAME_AS_STRING for MDS. */
	const char					*key,		// e.g. "AlgType"
	const void					*valPtr,	
	unsigned					valLen,
	CSSM_DB_ATTRIBUTE_FORMAT	valForm,	// CSSM_DB_ATTRIBUTE_FORMAT_STRING, etc.
	CSSM_DB_OPERATOR			valOp)		// normally CSSM_DB_EQUAL
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_HANDLE						resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DATA						predData;
	CSSM_DB_ATTRIBUTE_DATA			outAttr;
	CSSM_DB_ATTRIBUTE_INFO_PTR		attrInfo;
	const char 						*attrName = "ModuleID";
		
	/* We want one attribute back, name and format specified by caller */
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

	/* until this API settles down bag this test */
	#if 0
		printf("doLookup: mds.dlGetFirst unstable' skipping!\n");
		return -1;
	#else
	try {
		/* 8A268 has this */
		#if 0
		resultHand = mds.dlGetFirst(query, recordAttrs, record);
		#else
		/* TOT */
		resultHand = mds.dlGetFirst(query, recordAttrs, NULL, record);
		#endif
	}
	catch(...) {
		printf("doLookup: dlGetFirst threw exception!\n");
		return -1;
	}
	#endif
	if(resultHand == 0) {
		printf("doLookup: no record found\n");
		return -1;
	}
	
	/* we could examine the record here of we wanted to */
	mds.dlFreeUniqueId(record);
	freeAttrs(&recordAttrs);
	mds.dlAbortQuery(resultHand);
	return 0;
}

/* nothing here for now */
int mdsLookupInit(TestParams *tp)
{
	return 0;
}

int mdsLookup(TestParams *testParams)
{
	for(unsigned loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("mdsLookup loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);    
		}
		try {
			MDSClient::Directory &mds = MDSClient::mds();
			uint32 val = CSSM_ALGID_SHA1;
			if(doLookup(mds, MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE, 
					"AlgType", &val, sizeof(uint32), 
					CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_EQUAL)) {
				printf("***thread %u aborting\n", testParams->threadNum);
				return -1;
			}
		}
		catch(CssmError &err) {
			cssmPerror("MDS init", err.error);
			printf("mdsLookup: MDSClient::mds() threw CssmError!\n");
			return -1;
		}
		catch(...) {
			printf("mdsLookup: MDSClient::mds() threw exception!\n");
			return -1;
		}
		#if	DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to continue: ");
		getchar();
		#endif
	}
	return 0;
}

