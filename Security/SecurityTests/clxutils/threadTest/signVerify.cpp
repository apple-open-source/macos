/*
 * Simple sign/verify test
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <strings.h>

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE		0

#define SIG_ALG		CSSM_ALGID_SHA1WithRSA
#define KEY_GEN_ALG	CSSM_ALGID_RSA
#define KEY_SIZE	CSP_RSA_KEY_SIZE_DEFAULT
#define PTEXT_SIZE	1024
#define USAGE_DEF	"noUsage"

/* per-thread info */
typedef struct {
	CSSM_KEY	privKey;
	CSSM_KEY	pubKey;
	CSSM_DATA	ptext;
} TT_SignVfyParams;

int signVerifyInit(TestParams *testParams)
{
	CSSM_BOOL 			pubIsRef;
	CSSM_BOOL 			privIsRef;
	CSSM_RETURN			crtn;
	TT_SignVfyParams	*svParams;
	
	/* flip coin for ref/blob key forms */
	if(testParams->threadNum & 1) {
		pubIsRef  = CSSM_TRUE;
		privIsRef = CSSM_FALSE;
	}
	else {
		pubIsRef  = CSSM_FALSE;
		privIsRef = CSSM_TRUE;
	}
	svParams = (TT_SignVfyParams *)CSSM_MALLOC(sizeof(TT_SignVfyParams));
	crtn = cspGenKeyPair(testParams->cspHand,
		KEY_GEN_ALG,
		USAGE_DEF,
		strlen(USAGE_DEF),
		KEY_SIZE,
		&svParams->pubKey,
		pubIsRef,	
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&svParams->privKey,
		privIsRef,
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		printf("***Error generating key pair; aborting\n");
		return 1;
	}
	svParams->ptext.Data = (uint8 *)CSSM_MALLOC(PTEXT_SIZE);
	svParams->ptext.Length = PTEXT_SIZE;
	
	testParams->perThread = svParams;
	return 0;
}

int signVerify(TestParams *testParams)
{
	TT_SignVfyParams	*svParams = (TT_SignVfyParams *)testParams->perThread;
	unsigned 			loop;
	CSSM_RETURN 		crtn;
	CSSM_DATA			sig;
	
	for(loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("signVerify thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		crtn = threadGetRandData(testParams, &svParams->ptext, PTEXT_SIZE);
		if(crtn) {
			return 1;
		}
		sig.Data = NULL;
		sig.Length = 0;
		crtn = cspSign(testParams->cspHand,
			SIG_ALG,
			&svParams->privKey,
			&svParams->ptext,
			&sig);
		if(crtn) {
			return 1;
		}
		crtn = cspSigVerify(testParams->cspHand,
			SIG_ALG,
			&svParams->pubKey,
			&svParams->ptext,
			&sig,
			CSSM_OK);
		if(crtn) {
			return 1;
		}
		appFree(sig.Data, NULL);
		#if DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to proceed: ");
		getchar();
		#endif
	}
	return 0;
}

