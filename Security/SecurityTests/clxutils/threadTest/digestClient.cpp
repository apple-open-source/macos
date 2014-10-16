/*
 * digestClient.cpp
 */
#include "testParams.h"
#include <Security/Security.h>
#include <security_cdsa_client/cspclient.h>

/* for malloc debug */
#define DO_PAUSE	0

int digestClientInit(TestParams *testParams)
{
	return 0;
}

using namespace Security;
using namespace CssmClient;

int digestClient(TestParams *testParams)
{
	unsigned loopNum;
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("secTrustEval loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		try {
			CSP *csp = new CSP(gGuidAppleCSP);
			uint8 digData[20];
			Digest *digest = new Digest(*csp, CSSM_ALGID_SHA1);
			CssmData ptext((char *)"test", 4);
			CssmData dig(digData, sizeof(digData));
			digest->digest(ptext, dig);
			if(dig.Length != 20) {
				printf("***digest length error\n");
				return 1;
			}
			delete digest;
			delete csp;
		}
		catch(...) {
			printf("***CSP/Digest client threw exeption\n");
			return 1;
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
