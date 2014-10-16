/*
 * Simple symmetric encrypt/decrypt test
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <strings.h>

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE	0

#define PTEXT_SIZE	1024
#define USAGE_DEF	"noUsage"

int symTestInit(TestParams *testParams)
{
	/* nothing for now */
	return 0;
}

int symTest(TestParams *testParams)
{
	unsigned 			loop;
	CSSM_RETURN 		crtn;
	CSSM_DATA			ptext = {0, NULL};
	CSSM_DATA			ctext = {0, NULL};
	CSSM_DATA			rptext = {0, NULL};
	CSSM_KEY_PTR		symKey;
	CSSM_PADDING		padding;
	CSSM_ALGORITHMS		keyAlg;
	CSSM_ALGORITHMS		encrAlg;
	CSSM_ENCRYPT_MODE	encrMode;
	CSSM_BOOL			keyIsRef;
	CSSM_DATA			initVector;
	
	initVector.Data = (uint8 *)"someStrangeInitVect";
	ptext.Data = (uint8 *)CSSM_MALLOC(PTEXT_SIZE);
	ptext.Length = PTEXT_SIZE;
	
	for(loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("symTest thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		/* random plaintext */
		crtn = threadGetRandData(testParams, &ptext, PTEXT_SIZE);
		if(crtn) {
			return 1;
		}
		
		/* pick algorithm and params */
		if(loop & 1) {
			keyAlg   = CSSM_ALGID_DES;
			encrAlg  = CSSM_ALGID_DES;
			encrMode = CSSM_ALGMODE_CBCPadIV8;
			padding  = CSSM_PADDING_PKCS1;
			initVector.Length = 8;
		}
		else {
			keyAlg   = CSSM_ALGID_AES;
			encrAlg  = CSSM_ALGID_AES;
			encrMode = CSSM_ALGMODE_CBCPadIV8;
			padding  = CSSM_PADDING_PKCS5;
			initVector.Length = 16;
		}
		if(loop & 2) {
			keyIsRef = CSSM_TRUE;
		}
		else {
			keyIsRef = CSSM_FALSE;
		}
		
		/* cook up a key */
		symKey = cspGenSymKey(testParams->cspHand,
			keyAlg,
			USAGE_DEF,
			strlen(USAGE_DEF),
			CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
			CSP_KEY_SIZE_DEFAULT,
			keyIsRef);
		if(symKey == NULL) {
			return 1;
		}
		
		/* encrypt, decrypt */
		crtn = cspEncrypt(testParams->cspHand,
			encrAlg,
			encrMode,
			padding,
			symKey,
			NULL,		// second key unused
			0,			// efectiveKeySizeInBits,
			0,			// rounds
			&initVector,
			&ptext,
			&ctext,
			CSSM_TRUE);	// mallocCtext
		if(crtn) {
			return 1;
		}
		crtn = cspDecrypt(testParams->cspHand,
			encrAlg,
			encrMode,
			padding,
			symKey,
			NULL,		// second key unused
			0,			// efectiveKeySizeInBits
			0,			// rounds
			&initVector,
			&ctext,
			&rptext,
			CSSM_TRUE);	// mallocCtext
		if(crtn) {
			return 1;
		}
		
		/* compare */
		if(ptext.Length != rptext.Length) {
			printf("Ptext length mismatch: expect %lu, got %lu\n", 
				ptext.Length, rptext.Length);
			return 1;
		}
		if(memcmp(ptext.Data, rptext.Data, ptext.Length)) {
			printf("***data miscompare\n");
			return 1;
		}

		/* free everything */
		appFreeCssmData(&ctext, CSSM_FALSE);
		appFreeCssmData(&rptext, CSSM_FALSE);
		cspFreeKey(testParams->cspHand, symKey);
		CSSM_FREE(symKey);
		#if DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to proceed: ");
		getchar();
		#endif
	}
	return 0;
}

