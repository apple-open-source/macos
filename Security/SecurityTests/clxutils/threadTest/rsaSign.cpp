/*
 * Simple RSA sign/verify threadTest module
 */
/*
 * Simple sign/verify test
 */
#include "testParams.h"
#include <Security/cssmtype.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <cspxutils/cspwrap.h>
#include <cspxutils/common.h>
#include <BSafe/bsafe.h>

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE			0

#define SAFE_RAND_DATA		0

#define KEY_SIZE			CSP_RSA_KEY_SIZE_DEFAULT		/* key size, bits */
#define MAX_SIG_SIZE		((KEY_SIZE / 8) * 2)	/* max signature size, bytes */
#define PTEXT_SIZE			1024
#define NUM_SEED_BYTES		32

static B_ALGORITHM_METHOD *BSAFE_ALGORITHM_CHOOSER[] = {
  &AM_MD5_RANDOM,
  &AM_MD,
  &AM_MD5,
  &AM_MAC,
  &AM_SHA,
  &AM_RSA_CRT_DECRYPT,
  &AM_RSA_CRT_ENCRYPT,
  &AM_RSA_DECRYPT,
  &AM_RSA_ENCRYPT,
  &AM_RSA_KEY_GEN,
  (B_ALGORITHM_METHOD *)NULL_PTR
};

/* generate RSA key pair */
static int RsaGenKeyPair(
	const TestParams 	*testParams,
	unsigned 			keySize,
	B_KEY_OBJ 			*pubKey,
	B_KEY_OBJ 			*privKey)
{
	int 					brtn;
	B_ALGORITHM_OBJ 		keypairGenerator = (B_ALGORITHM_OBJ)NULL_PTR;
	static unsigned char 	f4Data[3] = {0x01, 0x00, 0x01};
	B_ALGORITHM_OBJ 		randomAlgorithm = (B_ALGORITHM_OBJ)NULL_PTR;
	uint8					seedBytes[NUM_SEED_BYTES];
	CSSM_DATA				seedData = {NUM_SEED_BYTES, seedBytes};
	A_RSA_KEY_GEN_PARAMS 	keygenParams;
	
	/* boilerplate RSA key pair generate */
	/* first the random algorithm object */
	brtn = B_CreateAlgorithmObject(&randomAlgorithm);
	if(brtn) {
		printf("***B_CreateAlgorithmObject error (%d)\n", brtn);
		return 1;
	}
	brtn = B_SetAlgorithmInfo(randomAlgorithm,
		AI_MD5Random,
		NULL_PTR);
	if(brtn) {
		printf("***B_SetAlgorithmInfo error (%d)\n", brtn);
		return 1;
	}
	brtn = B_RandomInit(randomAlgorithm,
		BSAFE_ALGORITHM_CHOOSER,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_RandomInit error (%d)\n", brtn);
		return 1;
	}
	#if	SAFE_RAND_DATA
 	threadGetRandData(testParams, &seedData, NUM_SEED_BYTES);
	#else
	simpleGenData(&seedData, NUM_SEED_BYTES,NUM_SEED_BYTES);
	#endif

 	brtn = B_RandomUpdate(randomAlgorithm, seedBytes, NUM_SEED_BYTES, 
			(A_SURRENDER_CTX *)NULL_PTR);
 	if(brtn) {
		printf("***B_RandomUpdate error (%d)\n", brtn);
		return 1;
	}

	/* create a keypair generator */
	brtn = B_CreateAlgorithmObject(&keypairGenerator);
	if(brtn) {
		printf("***B_CreateAlgorithmObject error (%d)\n", brtn);
		return 1;
	}
	keygenParams.modulusBits = keySize;
	keygenParams.publicExponent.data = f4Data;
	keygenParams.publicExponent.len = 3;

	brtn = B_SetAlgorithmInfo(keypairGenerator,
		AI_RSAKeyGen,
		(POINTER)&keygenParams);
	if(brtn) {
		printf("***B_SetAlgorithmInfo error (%d)\n", brtn);
		return 1;
	}
	
	/* go for it */
	brtn = B_GenerateInit(keypairGenerator,
		BSAFE_ALGORITHM_CHOOSER,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_GenerateInit error (%d)\n", brtn);
		return 1;
	}
	brtn = B_CreateKeyObject(pubKey);
	if(brtn) {
		printf("***B_CreateKeyObject error (%d)\n", brtn);
		return 1;
	}
	brtn = B_CreateKeyObject(privKey);
	if(brtn) {
		printf("***B_CreateKeyObject error (%d)\n", brtn);
		return 1;
	}

	brtn = B_GenerateKeypair(keypairGenerator,
		*pubKey,
		*privKey,
		randomAlgorithm,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_GenerateKeypair error (%d)\n", brtn);
		return 1;
	}

	B_DestroyAlgorithmObject (&keypairGenerator);
	B_DestroyAlgorithmObject (&randomAlgorithm);
	return 0;
}

static int rsaSign(
		const TestParams 	*testParams,
		B_KEY_OBJ			privKey,
		const CSSM_DATA		*ptext,
		uint8 				*sigBytes,
		unsigned			maxSigSize,
		unsigned			*actSigSize)		// RETURNED
{
	int 					brtn;
	B_ALGORITHM_OBJ 		signer = (B_ALGORITHM_OBJ)NULL_PTR;

	brtn = B_CreateAlgorithmObject(&signer);
	if(brtn) {
		printf("***B_CreateAlgorithmObject error (%d)\n", brtn);
		return 1;
	}
	
	/* we happen to know that no info is needed for any signing algs */
	brtn = B_SetAlgorithmInfo(signer,
		AI_MD5WithRSAEncryption,
		NULL);
	if(brtn) {
		printf("***B_SetAlgorithmInfo error (%d)\n", brtn);
		return 1;
	}
	brtn = B_SignInit(signer,
		privKey,
		BSAFE_ALGORITHM_CHOOSER,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_SignInit error (%d)\n", brtn);
		return 1;
	}
	brtn = B_SignUpdate(signer,
		ptext->Data,
		ptext->Length,
		NULL);
	if(brtn) {
		printf("***B_SignUpdate error (%d)\n", brtn);
		return 1;
	}
	brtn = B_SignFinal(signer,
		sigBytes,
		actSigSize,
		maxSigSize,
		NULL,				// randAlg
		NULL);
	if(brtn) {
		printf("***B_SignFinal error (%d)\n", brtn);
	}
	B_DestroyAlgorithmObject(&signer);
	return brtn;
}

static int rsaSigVerify(
		const TestParams 	*testParams,
		B_KEY_OBJ			pubKey,
		const CSSM_DATA		*ptext,
		uint8 				*sigBytes,
		unsigned			sigSize)		// RETURNED
{
	int 					brtn;
	B_ALGORITHM_OBJ 		verifier = (B_ALGORITHM_OBJ)NULL_PTR;

	brtn = B_CreateAlgorithmObject(&verifier);
	if(brtn) {
		printf("***B_CreateAlgorithmObject error (%d)\n", brtn);
		return 1;
	}
	
	/* we happen to know that no info is needed for any verifying algs */
	brtn = B_SetAlgorithmInfo(verifier,
		AI_MD5WithRSAEncryption,
		NULL);
	if(brtn) {
		printf("***B_SetAlgorithmInfo error (%d)\n", brtn);
		return 1;
	}
	brtn = B_VerifyInit(verifier,
		pubKey,
		BSAFE_ALGORITHM_CHOOSER,
		(A_SURRENDER_CTX *)NULL_PTR);
	if(brtn) {
		printf("***B_VerifyInit error (%d)\n", brtn);
		return 1;
	}
	brtn = B_VerifyUpdate(verifier,
		ptext->Data,
		ptext->Length,
		NULL);
	if(brtn) {
		printf("***B_VerifyUpdate error (%d)\n", brtn);
		return 1;
	}
	brtn = B_VerifyFinal(verifier,
		sigBytes,
		sigSize,
		NULL,				// randAlg
		NULL);
	if(brtn) {
		printf("***B_VerifyFinal error (%d)\n", brtn);
	}
	B_DestroyAlgorithmObject(&verifier);
	return brtn;
}

/* per-thread info */
typedef struct {
	B_KEY_OBJ	privKey;
	B_KEY_OBJ	pubKey;
	CSSM_DATA	ptext;
} TT_RsaSignParams;

int rsaSignInit(TestParams *testParams)
{
	int 					rtn;
	TT_RsaSignParams		*svParams;
	
	svParams = (TT_RsaSignParams *)CSSM_MALLOC(sizeof(TT_RsaSignParams));
	rtn = RsaGenKeyPair(testParams,
		KEY_SIZE,
		&svParams->pubKey,
		&svParams->privKey);
	if(rtn) {
		printf("***Error generating key pair; aborting\n");
		return 1;
	}
	svParams->ptext.Data = (uint8 *)CSSM_MALLOC(PTEXT_SIZE);
	svParams->ptext.Length = PTEXT_SIZE;
	
	testParams->perThread = svParams;
	return 0;
}

int rsaSignTest(TestParams *testParams)
{
	TT_RsaSignParams	*svParams = (TT_RsaSignParams *)testParams->perThread;
	unsigned 			loop;
	int			 		rtn;
	uint8				sigBytes[MAX_SIG_SIZE];
	unsigned			actSigSize;
	
	for(loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("signVerify thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		#if SAFE_RAND_DATA
		CSSM_RETURN crtn = threadGetRandData(testParams, &svParams->ptext, PTEXT_SIZE);
		if(crtn) {
			return 1;
		}
		#else
		simpleGenData(&svParams->ptext, PTEXT_SIZE,PTEXT_SIZE);
		#endif
		rtn = rsaSign(testParams,
			svParams->privKey,
			&svParams->ptext,
			sigBytes,
			MAX_SIG_SIZE,
			&actSigSize);
		if(rtn) {
			return 1;
		}
		rtn = rsaSigVerify(testParams,
			svParams->pubKey,
			&svParams->ptext,
			sigBytes,
			actSigSize);
		if(rtn) {
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

