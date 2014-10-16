/*
 * Simple DES encrypt/decrypt test using raw BSAFE
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <BSafe/bsafe.h>

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE			0
#define SAFE_RAND_DATA		0

#define PTEXT_SIZE		1024
#define KEY_SIZE_BITS	64
#define KEY_SIZE_BYTES	(KEY_SIZE_BITS / 8)
#define DES_BLOCK_SIZE	8

static B_ALGORITHM_METHOD *BSAFE_ALGORITHM_CHOOSER[] = {
	&AM_MD5_RANDOM,
	&AM_MD,
	&AM_MD5,
	&AM_CBC_ENCRYPT,
	&AM_CBC_DECRYPT,
    &AM_OFB_ENCRYPT,
    &AM_OFB_DECRYPT,
	&AM_DES_DECRYPT,
	&AM_DES_ENCRYPT,
	(B_ALGORITHM_METHOD *)NULL_PTR
};

/* generate DES key */
static int desGenKey(
	const TestParams 	*testParams,
	B_KEY_OBJ 			*desKey)
{
	int 					brtn;
	uint8					keyBytes[KEY_SIZE_BYTES];
	CSSM_DATA				keyData = {KEY_SIZE_BYTES, keyBytes};
	
	B_DestroyKeyObject(desKey);
	brtn = B_CreateKeyObject(desKey);
	if(brtn) {
		printf("***Error on B_CreateKeyObject (%d)\n", brtn);
		return 1;
	}
	#if	SAFE_RAND_DATA
 	threadGetRandData(testParams, &keyData, KEY_SIZE_BYTES);
	#else
	simpleGenData(&keyData, KEY_SIZE_BYTES, KEY_SIZE_BYTES);
	#endif
	
	brtn = B_SetKeyInfo(*desKey, KI_DES8, (POINTER)keyBytes);
	if(brtn) {
		printf("***Error on B_SetKeyInfo (%d)\n", brtn);
		return 1;
	}
	
	return 0;
	
}

/* common code to set up encrypt/decrypt */
static int desEncDecSetup(
	ITEM	 						*iv,
    B_BLK_CIPHER_W_FEEDBACK_PARAMS 	*spec,
	B_ALGORITHM_OBJ					*alg)
{
	int brtn;
	
	spec->encryptionMethodName = POINTER("des");
	spec->feedbackMethodName = POINTER("cbc");
	spec->feedbackParams = POINTER(iv);
	spec->paddingParams = NULL_PTR;
	spec->encryptionParams = NULL_PTR;
	spec->paddingMethodName = POINTER("pad");
	
	brtn = B_CreateAlgorithmObject(alg);
	if(brtn) {
		printf("***B_CreateAlgorithmObject error (%d)\n", brtn);
		return 1;
	}
	brtn = B_SetAlgorithmInfo(*alg, AI_FeedbackCipher, (POINTER)spec);
	if(brtn) {
		printf("***B_SetAlgorithmInfo error (%d)\n", brtn);
		return 1;
	}
	return 0;
}

static int desEncrypt(
	TestParams	*testParams,
	B_KEY_OBJ	desKey,
	uint8		*initVector,
	CSSM_DATA	*ptext,
	CSSM_DATA	*ctext)
{
	ITEM	 						iv;
    B_BLK_CIPHER_W_FEEDBACK_PARAMS 	spec;
	B_ALGORITHM_OBJ					alg = NULL;
	int								brtn;
	unsigned						actLen;
	unsigned						remCtext = ctext->Length;
	
	iv.data = initVector;
	iv.len = DES_BLOCK_SIZE;
	brtn = desEncDecSetup(&iv, &spec, &alg);
	if(brtn) {
		return brtn;
	}
	brtn = B_EncryptInit(alg, 
		desKey, 
		BSAFE_ALGORITHM_CHOOSER,  
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_EncryptInit error (%d)\n", brtn);
		return brtn;
	}
	brtn = B_EncryptUpdate(alg,
		ctext->Data,
		&actLen,
		remCtext,
		ptext->Data,
		ptext->Length,
		(B_ALGORITHM_OBJ)NULL_PTR,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_EncryptUpdate error (%d)\n", brtn);
		return brtn;
	}
	remCtext -= actLen;
	ctext->Length = actLen;
	brtn = B_EncryptFinal(alg,
		ctext->Data + actLen,
		&actLen,
		remCtext,
		(B_ALGORITHM_OBJ)NULL_PTR,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_EncryptFinal error (%d)\n", brtn);
		return brtn;
	}
	ctext->Length += actLen;
	B_DestroyAlgorithmObject (&alg);
	return 0;
}

static int desDecrypt(
	TestParams	*testParams,
	B_KEY_OBJ	desKey,
	uint8		*initVector,
	CSSM_DATA	*ctext,
	CSSM_DATA	*ptext)
{
	ITEM	 						iv;
    B_BLK_CIPHER_W_FEEDBACK_PARAMS 	spec;
	B_ALGORITHM_OBJ					alg = NULL;
	int								brtn;
	unsigned						actLen;
	unsigned						remPtext = ptext->Length;
	
	iv.data = initVector;
	iv.len = DES_BLOCK_SIZE;
	brtn = desEncDecSetup(&iv, &spec, &alg);
	if(brtn) {
		return brtn;
	}
	brtn = B_DecryptInit(alg, 
		desKey, 
		BSAFE_ALGORITHM_CHOOSER,  
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_DecryptInit error (%d)\n", brtn);
		return brtn;
	}
	brtn = B_DecryptUpdate(alg,
		ptext->Data,
		&actLen,
		remPtext,
		ctext->Data,
		ctext->Length,
		(B_ALGORITHM_OBJ)NULL_PTR,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_DecryptUpdate error (%d)\n", brtn);
		return brtn;
	}
	remPtext -= actLen;
	ptext->Length = actLen;
	brtn = B_DecryptFinal(alg,
		ptext->Data + actLen,
		&actLen,
		remPtext,
		(B_ALGORITHM_OBJ)NULL_PTR,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_DecryptFinal error (%d)\n", brtn);
		return brtn;
	}
	ptext->Length += actLen;
	B_DestroyAlgorithmObject (&alg);
	return 0;
}

int desInit(TestParams *testParams)
{
	/* nothing for now */
	return 0;
}

int desTest(TestParams *testParams)
{
	unsigned 			loop;
	CSSM_RETURN 		crtn;
	CSSM_DATA			ptext = {0, NULL};
	CSSM_DATA			ctext = {0, NULL};
	CSSM_DATA			rptext = {0, NULL};
	B_KEY_OBJ			desKey = NULL;
	uint8				*initVector = (uint8 *)"someStrangeInitVect";
	int					rtn;
	
	ptext.Data 		= (uint8 *)CSSM_MALLOC(PTEXT_SIZE);
	ptext.Length 	= PTEXT_SIZE;
	rptext.Data 	= (uint8 *)CSSM_MALLOC(PTEXT_SIZE);
	rptext.Length 	= PTEXT_SIZE;
	ctext.Data 		= (uint8 *)CSSM_MALLOC(PTEXT_SIZE + DES_BLOCK_SIZE);
	ctext.Length 	= PTEXT_SIZE + DES_BLOCK_SIZE;
	
	for(loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("symTest thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		/* random plaintext */
		#if	SAFE_RAND_DATA
		crtn = threadGetRandData(testParams, &ptext, PTEXT_SIZE);
		if(crtn) {
			return 1;
		}
		#else
		simpleGenData(&ptext, PTEXT_SIZE, PTEXT_SIZE);
		#endif
		
		/* cook up a key */
		rtn = desGenKey(testParams, &desKey);
		if(rtn) {
			return 1;
		}
		
		/* encrypt, decrypt */
		ctext.Length = PTEXT_SIZE + DES_BLOCK_SIZE;
		rtn = desEncrypt(testParams,
			desKey,
			initVector,
			&ptext,
			&ctext);
		if(rtn) {
			return 1;
		}
		rptext.Length = PTEXT_SIZE + DES_BLOCK_SIZE;
		rtn = desDecrypt(testParams,
			desKey,
			initVector,
			&ctext,
			&rptext);
		if(crtn) {
			return 1;
		}
		
		/* compare */
		if(ptext.Length != rptext.Length) {
			printf("Ptext length mismatch: expect %d, got %d\n", 
				ptext.Length, rptext.Length);
			return 1;
		}
		if(memcmp(ptext.Data, rptext.Data, ptext.Length)) {
			printf("***data miscompare\n");
			return 1;
		}

		#if DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to proceed: ");
		getchar();
		#endif
	}
	return 0;
}

