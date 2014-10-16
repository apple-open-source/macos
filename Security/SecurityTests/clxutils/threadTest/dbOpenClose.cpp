/* 
 * dbOpenClose.cpp - multi-threaded DB open/close test
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <strings.h>
#include <utilLib/common.h>

/* One create at init time, multi threads opening and closing this DB */
#define DB_NAME		"/tmp/dbOpenCLose.db"

static CSSM_DL_HANDLE dlHand = 0;

int dbOpenCloseInit(TestParams *testParams)
{
	dlHand = cuDlStartup();
	if(dlHand == 0) {
		printf("***dbOpenCloseInit: Error connecting to DL\n");
		return -1;
	}
	
	int ourRtn = 0;
	
	/* Create the DB, deleting existing */
	CSSM_RETURN crtn;
	CSSM_DB_HANDLE dbHand = 0;
	crtn = dbCreateOpen(dlHand, DB_NAME, 
		CSSM_TRUE,		// doCreate
		CSSM_TRUE,		// delete exist
		"foobar",
		&dbHand);
	if(crtn) {
		printf("***Error creating %s. Aborting.\n", DB_NAME);
		ourRtn = -1;
	}
	return ourRtn;
}

int dbOpenCloseEval(TestParams *testParams)
{
	int ourRtn = 0;
	for(unsigned loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("dbOpenClose thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
	
		/* attach to existing DB - don't create */
		CSSM_DB_HANDLE dbHand = cuDbStartupByName(dlHand, (char *)DB_NAME,
			CSSM_FALSE,		// don't create
			testParams->quiet);
		if(dbHand == 0) {
			printf("***dbOpenClose: error attaching to db %s\n", DB_NAME);
			ourRtn = -1;
			break;
		}
		
		CSSM_DL_DB_HANDLE dlDbHand = {dlHand, dbHand};
		CSSM_RETURN crtn = CSSM_DL_DbClose(dlDbHand);
		if(crtn) {
			cssmPerror("CSSM_DL_DbClose", crtn);
			printf("***Error closing %s\n", DB_NAME);
			ourRtn = -1;
			break;
		}
	}
	return ourRtn;
}

