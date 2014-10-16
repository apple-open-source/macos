/* cssmErrorString per-thread basher */
#include <time.h>
#include <stdio.h>
#include "testParams.h"
#include <Security/SecBasePriv.h>
#include <Security/cssmerr.h>

/* nothing here for now */
int cssmErrStrInit(TestParams *tp)
{
	return 0;
}

CSSM_RETURN variousErrors[] = {
	CSSMERR_CSP_INVALID_CONTEXT,
	CSSMERR_CSP_INVALID_ALGORITHM,
	CSSMERR_CSP_INVALID_ATTR_KEY,
	CSSMERR_CSP_MISSING_ATTR_KEY,
	CSSMERR_CSP_INVALID_ATTR_INIT_VECTOR,
	CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR,
	CSSMERR_CSP_INVALID_ATTR_SALT,
	CSSMERR_CSP_MISSING_ATTR_SALT,
	CSSMERR_CSP_INVALID_ATTR_PADDING,
	CSSMERR_CSP_MISSING_ATTR_PADDING,
	CSSMERR_CSP_INVALID_ATTR_RANDOM,
	CSSMERR_CSP_MISSING_ATTR_RANDOM,
	CSSMERR_CSP_INVALID_ATTR_SEED,
	CSSMERR_CSP_MISSING_ATTR_SEED,
	CSSMERR_CSP_INVALID_ATTR_PASSPHRASE,
	CSSMERR_CSP_MISSING_ATTR_PASSPHRASE,
	CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH,
	CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH,
	CSSMERR_CSP_INVALID_ATTR_BLOCK_SIZE,
	CSSMERR_CSP_MISSING_ATTR_BLOCK_SIZE,
	CSSMERR_CSP_INVALID_ATTR_OUTPUT_SIZE,
	CSSMERR_CSP_MISSING_ATTR_OUTPUT_SIZE,
	CSSMERR_CSP_INVALID_ATTR_ROUNDS
};
#define NUM_ERRORS	(sizeof(variousErrors) / sizeof(variousErrors[0]))

int cssmErrStr(TestParams *testParams)
{
	for(unsigned loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("cssmErrStr loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);    
		}
		
		for(unsigned dex=0; dex<NUM_ERRORS; dex++) {
			unsigned whichErr = (dex + testParams->threadNum) % NUM_ERRORS;
			const char *es = cssmErrorString(variousErrors[whichErr]);
			if(testParams->verbose) {
				printf("..%s..", es);
			}
		}
	}
	return 0;
}

