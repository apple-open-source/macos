/*
 * trustSettings.cpp - multi threaded TP evaluate with Trust Settings enabled 
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <utilLib/common.h>	
#include <utilLib/cspwrap.h>
#include <clAppUtils/BlobList.h>
#include <clAppUtils/certVerify.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <Security/Security.h>

#define HOLD_SEARCH_LIST	0

/* for malloc debug */
#define DO_PAUSE			0

static const char *CERT_FILE0 = "amazon_v3.100.cer";
static const char *CERT_FILE1 = "amazon_v3.101.cer";

/* common data, our known good cert, shared by all threads */
static BlobList blobList;
static BlobList emptyRootList;

/* read in our known good cert file, just once */
int trustSettingsInit(TestParams *testParams)
{
	if(testParams->verbose) {
		printf("trustSettingsInit thread %d: reading cert files %s and %s...\n", 
			testParams->threadNum, CERT_FILE0, CERT_FILE1);
	}
	if(blobList.addFile(CERT_FILE0)) {
		printf("Error reading %s; aborting\n", CERT_FILE0);
		printf("***This test must be run from the clxutils/threadTest directory.\n");
		return 1;
	}
	if(blobList.addFile(CERT_FILE1)) {
		printf("Error reading %s; aborting\n", CERT_FILE1);
		printf("***This test must be run from the clxutils/threadTest directory.\n");
		return 1;
	}
	return 0;
}


int trustSettingsEval(TestParams *testParams)
{
	unsigned			loopNum;
	
	#if HOLD_SEARCH_LIST
	CFArrayRef sl;
	ortn = SecKeychainCopySearchList(&sl);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		return (int)ortn;
	}
	#endif
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("trustSettingsEval loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		int rtn = certVerifySimple(testParams->tpHand, 
			testParams->clHand,
			testParams->cspHand,
			blobList,
			emptyRootList,
			CSSM_TRUE,		/* useSystemAnchors */
			CSSM_FALSE,		/* leafCertIsCA */
			CSSM_FALSE,
			CVP_Basic,
			NULL, CSSM_FALSE, NULL, 
			0,
			NULL, 			/* expectedErrStr */
			0, NULL,		/* certErrors */
			0, NULL,		/* certStatus */
			CSSM_TRUE,		/* TrustSettings */
			CSSM_TRUE, CSSM_FALSE);
		if(rtn) {
			printf("Cert Eval failed\n");
			return rtn;
		}

		#if	DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to continue: ");
		getchar();
		#endif
	}	/* outer loop */
	#if HOLD_SEARCH_LIST
	CFRelease(sl);
	#endif
	return 0;
}
