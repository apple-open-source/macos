/*
 * getCachedFields.cpp
 *
 * do a "CSSM_CL_CertGetFirstCachedFieldValue" 'n' times on a known good 
 * cert; with a variety of fields; verify same results each time.
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <utilLib/common.h>	
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <Security/oidscert.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>

#define DO_PAUSE	0

//static const char *CERT_FILE = "serverpremium.crt";
static const char *CERT_FILE = "mypage.apple_v3.100.cer";

#define NUM_INNER_LOOPS			10

/* common data, our known good cert, shared by all threads */
static unsigned char *certData = NULL;
static unsigned certLength = 0;

/*
 * Hard coded list of field OIDs to fetch 
 */
static const CSSM_OID *fieldOids[] = 
{
	&CSSMOID_X509V1Version,
	&CSSMOID_X509V1SubjectName,
	&CSSMOID_X509V1IssuerName,
	&CSSMOID_X509V1SerialNumber,
	&CSSMOID_X509V1ValidityNotBefore,
	&CSSMOID_X509V1ValidityNotAfter,
	&CSSMOID_X509V1Signature
	/* etc. */
};
#define NUM_FIELD_OIDS 	(sizeof(fieldOids) / sizeof(CSSM_OID *))


/* read in our known good cert file, just once */
int getCachedFieldsInit(TestParams *testParams)
{
	if(certData != NULL) {
		return 0;
	}
	if(testParams->verbose) {
		printf("getFields thread %d: reading cert file %s...\n", 
			testParams->threadNum, CERT_FILE);
	}
	if(readFile(CERT_FILE, &certData, &certLength)) {
		printf("Error reading %s; aborting\n", CERT_FILE);
		printf("***This test must be run from the clxutils/threadTest directory.\n");
		return 1;
	}
	return 0;
}

static int compareFields(
	const CSSM_OID *oid,
	const CSSM_DATA	*val1,
	const CSSM_DATA	*val2)
{
	/* data length must match */
	if(val1->Length != val2->Length) {
		printf("***FieldValue.Length miscompare\n");
		return 1;
	}
	
	/*
	 * The hard part. Most OIDs have some kind of C struct pointer in their
	 * FieldValue.Data pointers, so comparison is on an oid-by-oid basis.
	 * We'll just do the easy ones, and the ones we suspect may be causing
	 * trouble.
	 */
	if(appCompareCssmData(oid, &CSSMOID_X509V1Version)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1Version mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(oid, &CSSMOID_X509V1SerialNumber)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1SerialNumber mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(oid, &CSSMOID_X509V1ValidityNotBefore)) {
		CSSM_X509_TIME *cssmTime1 = (CSSM_X509_TIME *)val1->Data;
		CSSM_X509_TIME *cssmTime2 = (CSSM_X509_TIME *)val2->Data;
		if(!appCompareCssmData(&cssmTime1->time, &cssmTime2->time)) {
			printf("***CSSMOID_X509V1ValidityNotBefore mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(oid, &CSSMOID_X509V1ValidityNotAfter)) {
		CSSM_X509_TIME *cssmTime1 = (CSSM_X509_TIME *)val1->Data;
		CSSM_X509_TIME *cssmTime2 = (CSSM_X509_TIME *)val2->Data;
		if(!appCompareCssmData(&cssmTime1->time, &cssmTime2->time)) {
			printf("***CSSMOID_X509V1ValidityNotAfter mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(oid, &CSSMOID_X509V1CertificateIssuerUniqueId)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1CertificateIssuerUniqueId mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(oid, &CSSMOID_X509V1CertificateSubjectUniqueId)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1CertificateSubjectUniqueId mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(oid, &CSSMOID_X509V1Signature)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1Signature mismatch\n");
			return 1;
		}
	}
	return 0;
}

static int checkOneField(
	CSSM_CL_HANDLE		clHand,
	CSSM_HANDLE 		cacheHand1,
	CSSM_HANDLE 		cacheHand2,
	const CSSM_OID		*fieldOid)
{
	CSSM_DATA_PTR	fieldData1 = NULL;
	CSSM_DATA_PTR	fieldData2 = NULL;
	CSSM_RETURN		crtn;
	CSSM_HANDLE 	resultHand1 = 0;
	CSSM_HANDLE 	resultHand2 = 0;
	uint32 			numFields = 0;
	int				rtn;
	
	crtn = CSSM_CL_CertGetFirstCachedFieldValue(
		clHand,
		cacheHand1,
	    fieldOid,
	    &resultHand1,
	    &numFields,
		&fieldData1);
	if(crtn) {
		return crtn;
	}
	if(numFields != 1) {
		printf("Fiedl not present; try another cert\n");
		return 1;
	}
	crtn = CSSM_CL_CertGetFirstCachedFieldValue(
		clHand,
		cacheHand2,
	    fieldOid,
	    &resultHand2,
	    &numFields,
		&fieldData2);
	if(crtn) {
		return crtn;
	}
	rtn = compareFields(fieldOid, fieldData1, fieldData2);
  	CSSM_CL_CertAbortQuery(clHand, resultHand1);
  	CSSM_CL_CertAbortQuery(clHand, resultHand2);
	CSSM_CL_FreeFieldValue(clHand, fieldOid, fieldData1);
	CSSM_CL_FreeFieldValue(clHand, fieldOid, fieldData2);
	return rtn;
}

int getCachedFields(TestParams *testParams)
{
	CSSM_RETURN		crtn;
	CSSM_HANDLE 	cacheHand1;
	CSSM_HANDLE 	cacheHand2;
	unsigned		fieldNum;
	unsigned		loopNum;
	CSSM_DATA		cert;
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("getCachedFields loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		/* get two cached certs */
		cert.Data = certData;
		cert.Length = certLength;
		crtn = CSSM_CL_CertCache(testParams->clHand, &cert, &cacheHand1);
		if(crtn) {
			printError("CSSM_CL_CertCache(1)", crtn);
			return 1;
		}
		crtn = CSSM_CL_CertCache(testParams->clHand, &cert, &cacheHand2);
		if(crtn) {
			printError("CSSM_CL_CertCache(2)", crtn);
			return 1;
		}
	
		/* grind thru the known OIDs */
		for(fieldNum=0; fieldNum<NUM_FIELD_OIDS; fieldNum++) {
			int rtn = checkOneField(testParams->clHand,
				cacheHand1, 
				cacheHand2,
				fieldOids[fieldNum]);
			if(rtn) {
				return 1;
			}
		}
		CSSM_CL_CertAbortCache(testParams->clHand, cacheHand1);
		CSSM_CL_CertAbortCache(testParams->clHand, cacheHand2);
		/* leak debug */
		#if	DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to continue: ");
		getchar();
		#endif
	}	/* outer loop */
	return 0;
}
