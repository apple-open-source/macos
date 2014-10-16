/*
 * getFields.cpp
 *
 * do a "GetAllFields" 'n' times on a known good cert; verify same results
 * each time
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

/* read in our known good cert file, just once */
int getFieldsInit(TestParams *testParams)
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
	const CSSM_FIELD	*fieldPtr1,
	const CSSM_FIELD	*fieldPtr2)
{
	const CSSM_DATA *val1 = &fieldPtr1->FieldValue;
	const CSSM_DATA *val2 = &fieldPtr2->FieldValue;
	
	/* OIDs must match exactly */
	if(!appCompareCssmData(&fieldPtr1->FieldOid, &fieldPtr2->FieldOid)) {
		printf("***FieldOid miscompare\n");
		return 1;
	}
	
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
	const CSSM_OID  *thisOid = &fieldPtr1->FieldOid;
	if(appCompareCssmData(thisOid, &CSSMOID_X509V1Version)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1Version mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(thisOid, &CSSMOID_X509V1SerialNumber)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1SerialNumber mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(thisOid, &CSSMOID_X509V1ValidityNotBefore)) {
		CSSM_X509_TIME *cssmTime1 = (CSSM_X509_TIME *)val1->Data;
		CSSM_X509_TIME *cssmTime2 = (CSSM_X509_TIME *)val2->Data;
		if(!appCompareCssmData(&cssmTime1->time, &cssmTime2->time)) {
			printf("***CSSMOID_X509V1ValidityNotBefore mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(thisOid, &CSSMOID_X509V1ValidityNotAfter)) {
		CSSM_X509_TIME *cssmTime1 = (CSSM_X509_TIME *)val1->Data;
		CSSM_X509_TIME *cssmTime2 = (CSSM_X509_TIME *)val2->Data;
		if(!appCompareCssmData(&cssmTime1->time, &cssmTime2->time)) {
			printf("***CSSMOID_X509V1ValidityNotAfter mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(thisOid, &CSSMOID_X509V1CertificateIssuerUniqueId)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1CertificateIssuerUniqueId mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(thisOid, &CSSMOID_X509V1CertificateSubjectUniqueId)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1CertificateSubjectUniqueId mismatch\n");
			return 1;
		}
	}
	else if(appCompareCssmData(thisOid, &CSSMOID_X509V1Signature)) {
		if(!appCompareCssmData(val1, val2)) {
			printf("***CSSMOID_X509V1Signature mismatch\n");
			return 1;
		}
	}
	return 0;
}

int getFields(TestParams *testParams)
{
	CSSM_RETURN		crtn;
	CSSM_FIELD_PTR	fieldPtr1;		// reference - mallocd by CL
	CSSM_FIELD_PTR	fieldPtr2;		// mallocd by CL
	uint32			i;
	uint32			numFields1;
	uint32			numFields2;
	unsigned		loopNum;
	CSSM_DATA		cert;
	unsigned		dex;
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("getFields loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		/* get reference fields */
		cert.Data = certData;
		cert.Length = certLength;
		crtn = CSSM_CL_CertGetAllFields(testParams->clHand,
			&cert,
			&numFields1,
			&fieldPtr1);
		if(crtn) {
			printError("CSSM_CL_CertGetAllFields(1)", crtn);
			return 1;
		}
	
		for(dex=0; dex<NUM_INNER_LOOPS; dex++) {
			/* get all fields again */
			crtn = CSSM_CL_CertGetAllFields(testParams->clHand,
				&cert,
				&numFields2,
				&fieldPtr2);
			if(crtn) {
				printError("CSSM_CL_CertGetAllFields(2)", crtn);
				return 1;
			}
			
			/* compare to reference fields */
			if(numFields1 != numFields2) {
				printf("***CSSM_CL_CertGetAllFields returned differing numFields "
					"(%u, %u)\n", (unsigned)numFields1, (unsigned)numFields2);
					return 1;
			}
			for(i=0; i<numFields1; i++) {
				if(compareFields(&fieldPtr1[i], &fieldPtr2[i])) {
					return 1;
				}
			}
			crtn = CSSM_CL_FreeFields(testParams->clHand, numFields1, &fieldPtr2);
			if(crtn) {
				printError("CSSM_CL_FreeFields", crtn);
				return 1;
			}
			/* leak debug */
			#if	DO_PAUSE
			fpurge(stdin);
			printf("Hit CR to continue: ");
			getchar();
			#endif
		}	/* inner loop */
		
		crtn = CSSM_CL_FreeFields(testParams->clHand, numFields1, &fieldPtr1);
		if(crtn) {
			printError("CSSM_CL_FreeFields", crtn);
			return 1;
		}
	}	/* outer loop */
	return 0;
}
