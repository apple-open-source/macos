/* cspRand.cpp */

#include "testParams.h"
#include <utilLib/common.h>	
#include <stdlib.h>
#include <stdio.h>
#include <security_utilities/threading.h>

#define INNER_LOOPS		100
#define MAX_SIZE		256

int cspRandInit(TestParams *testParams)
{
	return 0;
}

int cspRand(TestParams *testParams)
{
	unsigned 		loopNum;
	unsigned 		iLoop;
	unsigned char	randData[MAX_SIZE];
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		
		if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		/* pick a rand size for this outer loop using the global devRand */
		unsigned char randSize;
		CSSM_DATA cdata;
		cdata.Data = &randSize;
		cdata.Length = 1;
		threadGetRandData(testParams, &cdata, 1);
		if(randSize == 0) {
			randSize = 1;
		}
		for(iLoop=0; iLoop<INNER_LOOPS; iLoop++) {
			CSSM_CC_HANDLE	rngHand;
			CSSM_RETURN crtn;
			
			crtn = CSSM_CSP_CreateRandomGenContext(testParams->cspHand,
				CSSM_ALGID_APPLE_YARROW,
				NULL,				/* seed */
				randSize,
				&rngHand);
			if(crtn) {
				printError("CSSM_CSP_CreateRandomGenContext", crtn);
				return 1;
			}
			cdata.Data = randData;
			cdata.Length = randSize;
			crtn = CSSM_GenerateRandom(rngHand, &cdata);
			if(crtn) {
				printError("CSSM_GenerateRandom", crtn);
				return 1;
			}
			CSSM_DeleteContext(rngHand);
		}
	}
	return 0;
}

