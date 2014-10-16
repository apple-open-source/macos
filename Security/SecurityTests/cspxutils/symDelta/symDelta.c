/* Copyright (c) 1998,2003-2005,2008 Apple Inc.
 *
 * symDelta.c - Ensure that varying each parameter in a symmetric
 *              encryption op does in fact change the ciphertext.
 *
 * Revision History
 * ----------------
 *  July 18 2000	Doug Mitchell at Apple
 *		Created.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

/* we need to know a little bit about AES for this test.... */
#define AES_BLOCK_SIZE		16		/* bytes */

/*
 * Defaults.
 */
#define LOOPS_DEF			10
#define MIN_PTEXT_SIZE		AES_BLOCK_SIZE		/* for non-padding tests */
#define MAX_PTEXT_SIZE		1000
#define MAX_IV_SIZE			AES_BLOCK_SIZE

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef unsigned privAlg;
enum {
	pka_ASC,
	pka_RC4,
	pka_DES,
	pka_RC2,
	pka_RC5,
	pka_3DES,
	pka_AES
};

/*
 * Ditto for modes. ALGMODE_NONE not iterated, it's a special case for 
 * RC4 and ASC. 
 */
typedef unsigned privMode;
enum {
	pma_CBC_PadIV8,
	pma_CBC_IV8,			// no pad - requires well-aligned ptext
	pma_ECB,				// no IV, no pad - requires well-aligned ptext
};

#define ENCR_ALG_FIRST			pka_ASC
#define ENCR_ALG_LAST			pka_AES

#define ENCR_MODE_FIRST			pma_CBC_PadIV8
#define ENCR_MODE_LAST			pma_ECB

/*
 * Args passed to each test and to testCommon; these completely define
 * the paramters for one encryption op. 
 */
typedef struct {
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_ALGORITHMS		keyAlg;
	CSSM_ALGORITHMS		encrAlg;
	uint32				keySizeInBits;
	uint32				effectiveKeySizeInBits;	// 0 means not used
	uint32				rounds;					// ditto
	const char 			*keyAlgStr;
	CSSM_ENCRYPT_MODE	encrMode;
	const char			*encrModeStr;
	CSSM_PADDING		encrPad;
	CSSM_DATA_PTR		ptext;
	CSSM_BOOL 			useInitVector;		// encrypt needs an IV
	CSSM_BOOL			useRefKey;
	CSSM_DATA			initVector;			// Data mallocd and init in main()
	CSSM_KEY_PTR		key;				// gen'd in main
	CSSM_BOOL			verbose;
	CSSM_BOOL			quiet;
} testArgs;

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   e(xport)\n");
	printf("   r(epeatOnly)\n");
	printf("   p(ause after each loop)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * Given a privAlg value, return various associated stuff.
 */
static void algInfo(privAlg alg,		// pka_DES, etc.
	CSSM_ALGORITHMS		*keyAlg,		// CSSM_ALGID_DES, etc. RETURNED
										//   key alg for key gen algs
	CSSM_ALGORITHMS		*encrAlg,		// encrypt/decrypt alg for key 
										//   gen algs
	const char			**algStr,		// RETURNED
	CSSM_SIZE			*ivSize)		// RETURNED
{
	*ivSize = 8;
	switch(alg) {
		case pka_DES:
			*keyAlg = *encrAlg = CSSM_ALGID_DES;
			*algStr = "DES";
			return;
		case pka_3DES:
			*keyAlg = CSSM_ALGID_3DES_3KEY;
			*encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
			*algStr = "3DES";
			return;
		case pka_RC2:
			*keyAlg = *encrAlg = CSSM_ALGID_RC2;
			*algStr = "RC2";
			return;
		case pka_RC4:
			*keyAlg = *encrAlg = CSSM_ALGID_RC4;
			/* initVector false */
			*ivSize = 0;
			*algStr = "RC4";
			return;
		case pka_RC5:
			*keyAlg = *encrAlg = CSSM_ALGID_RC5;
			*algStr = "RC5";
			return;
		case pka_AES:
			*keyAlg = *encrAlg = CSSM_ALGID_AES;
			*algStr = "AES";
			*ivSize = AES_BLOCK_SIZE;
			return;
		case pka_ASC:
			*keyAlg = *encrAlg = CSSM_ALGID_ASC;
			/* initVector false */
			*ivSize = 0;
			*algStr = "ASC";
			return;
		default:
			printf("BRRZZZT! Update algInfo()!\n");
			testError(CSSM_TRUE);
	}
}

/* given a privMode, return related info */
static void modeInfo(
	CSSM_ALGORITHMS		alg,
	privMode 			mode,
	CSSM_ENCRYPT_MODE	*cdsaMode,
	const char			**modeStr,
	CSSM_PADDING		*pad,			// PKCS5 or NONE
	CSSM_BOOL			*useInitVector)	// RETURNED, for encrypt/decrypt
{
	*useInitVector = CSSM_FALSE;
	
	/* first deal with modeless algorithms */
	switch(alg) {
		case CSSM_ALGID_RC4:
		case CSSM_ALGID_ASC:
			*cdsaMode = CSSM_ALGMODE_NONE;
			*modeStr = "NONE";
			*pad = CSSM_PADDING_NONE;
			return;
		default:
			break;
	}
	
	switch(mode) {
		case pma_CBC_PadIV8:
			*cdsaMode = CSSM_ALGMODE_CBCPadIV8;
			*modeStr = "CBCPadIV8";
			*useInitVector = CSSM_TRUE;
			*pad = CSSM_PADDING_PKCS5;
			return;
		case pma_CBC_IV8:
			*cdsaMode = CSSM_ALGMODE_CBC_IV8;
			*modeStr = "CBC_IV8";
			*useInitVector = CSSM_TRUE;
			*pad = CSSM_PADDING_NONE;
			return;
		case pma_ECB:
			*cdsaMode = CSSM_ALGMODE_ECB;
			*modeStr = "ECB";
			*pad = CSSM_PADDING_NONE;
			return;
		default:
			printf("BRRZZZT! Update modeInfo()!\n");
			testError(CSSM_TRUE);
	}
}

/*
 * Given alg and mode, determine alignment of ptext size. 0 means no 
 * alignment necessary.
 */
uint32 alignInfo(
	CSSM_ALGORITHMS		alg,
	CSSM_ENCRYPT_MODE	mode)
{
	switch(alg) {
		case CSSM_ALGID_RC4:
		case CSSM_ALGID_ASC:
			return 0;
		default:
			break;
	}
	
	switch(mode) {
		case CSSM_ALGMODE_CBC_IV8: 
		case CSSM_ALGMODE_ECB:
			if(alg == CSSM_ALGID_AES) {
				return AES_BLOCK_SIZE;
			}
			else {
				return 8;
			}
		default:
			return 0;
	}
}

/* a handy "compare two CSSM_DATAs" ditty */
static CSSM_BOOL compareData(const CSSM_DATA_PTR d1,
	const CSSM_DATA_PTR d2)
{
	if(d1->Length != d2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(d1->Data, d2->Data, d1->Length)) {
		return CSSM_FALSE;
	}
	return CSSM_TRUE;
}

/* generate random one-bit byte */
static uint8 randBit()
{
	return 1 << genRand(0, 7);
}

/*
 * Copy a key.
 */
static void copyCssmKey(
	const CSSM_KEY_PTR	key1,
	CSSM_KEY_PTR		key2)
{
	key2->KeyHeader = key1->KeyHeader;
	key2->KeyData.Data = NULL;
	key2->KeyData.Length = 0;
	appCopyCssmData(&key1->KeyData, &key2->KeyData);
}

/*
 * Encrypt ptext using specified parameters
 */
static int encryptCom(CSSM_CSP_HANDLE cspHand,
	const char *testName,
	CSSM_DATA_PTR ptext,
	CSSM_KEY_PTR key,
	CSSM_ALGORITHMS alg,
	CSSM_ENCRYPT_MODE mode,
	CSSM_PADDING padding,			// CSSM_PADDING_PKCS1, etc. 
	CSSM_DATA_PTR iv,				// may be NULL
	uint32 effectiveKeySizeInBits,	// may be 0
	uint32 rounds,					// ditto
	CSSM_DATA_PTR ctext,			// RETURNED
	CSSM_BOOL quiet)
{
	CSSM_CC_HANDLE cryptHand;
	CSSM_RETURN crtn;
	CSSM_SIZE bytesEncrypted;
	CSSM_DATA remData;
	int rtn;
	
	cryptHand = genCryptHandle(cspHand,
		alg,
		mode,
		padding,		
		key,
		NULL,			// no 2nd key
		iv,				// InitVector
		effectiveKeySizeInBits,
		rounds);
	if(cryptHand == 0) {
		return testError(quiet);
	}
	
	remData.Data = NULL;
	remData.Length = 0;
	crtn = CSSM_EncryptData(cryptHand,
		ptext,
		1,
		ctext,
		1,
		&bytesEncrypted,
		&remData);
	
	if(crtn) {
		printError("CSSM_EncryptData", crtn);
		rtn = testError(quiet);
		goto done;
	}
	if(remData.Length != 0) {
		ctext->Data = (uint8 *)appRealloc(ctext->Data, bytesEncrypted, NULL);
		memmove(ctext->Data + ctext->Length, 
			remData.Data, 
			bytesEncrypted - ctext->Length);
		appFreeCssmData(&remData, CSSM_FALSE);
	}
	ctext->Length = bytesEncrypted;
	rtn = 0;
done:
	if(CSSM_DeleteContext(cryptHand)) {
		printError("CSSM_DeleteContext", 0);
		rtn = 1;
	}
	return rtn;
}

/*
 * Common test portion
 *   encrypt ptext with args in targ1;
 *   encrypt ptext with args in targ2;
 *	 compare 2 ctexts; expect failure;
 */
static int testCommon(const char *testName,
	testArgs *targs1,
	testArgs *targs2)
{
	CSSM_DATA		ctext1 = {0, NULL};
	CSSM_DATA		ctext2 = {0, NULL};

	if(encryptCom(targs1->cspHand,
			testName,
			targs1->ptext,
			targs1->key,
			targs1->encrAlg,
			targs1->encrMode,
			targs1->encrPad,
			&targs1->initVector,
			targs1->effectiveKeySizeInBits,
			targs1->rounds,
			&ctext1,
			targs1->quiet)) {
		return 1;
	}
	if(encryptCom(targs2->cspHand,
			testName,
			targs2->ptext,
			targs2->key,
			targs2->encrAlg,
			targs2->encrMode,
			targs2->encrPad,
			&targs2->initVector,
			targs2->effectiveKeySizeInBits,
			targs2->rounds,
			&ctext2,
			targs2->quiet)) {
		return 1;
	}
	if(compareData(&ctext1, &ctext2)) {
		printf("***%s: Unexpected Data compare!\n", testName);
		return testError(targs1->quiet);
	}
	appFreeCssmData(&ctext1, CSSM_FALSE);
	appFreeCssmData(&ctext2, CSSM_FALSE);
	return 0;
}

/**
 ** inidividual tests.
 **/
#define KEY_LABEL1		"noLabel1"
#define KEY_LABEL2		"noLabel2"
#define KEY_LABEL_LEN	strlen(KEY_LABEL1)
#define REPEAT_ON_ERROR	1

/*
 * Ensure initVector alters ctext. 
 */
static int initVectTest(testArgs *targs)
{
	uint32			mungeDex;
	uint32			mungeBits;
	testArgs		mungeArgs = *targs;
	CSSM_DATA		mungeIV;
	
	if(targs->verbose) {
		printf("         ...modifying init vector\n");
	}
	
	/* get munged copy of iv */
	mungeIV.Length = targs->initVector.Length;
	mungeIV.Data = (uint8 *)CSSM_MALLOC(mungeIV.Length);
	memmove(mungeIV.Data, targs->initVector.Data, mungeIV.Length);
	mungeArgs.initVector = mungeIV;
	mungeDex = genRand(0, mungeIV.Length - 1);
	mungeBits = randBit();
	mungeIV.Data[mungeDex] ^= mungeBits;
	if(testCommon("initVectTest", targs, &mungeArgs))  {
		return 1;
	}
	appFreeCssmData(&mungeIV, CSSM_FALSE);
	return 0;
}

/* 
 * Ensure effectiveKeySizeInBits alters ctext. RC2 only.
 */
static int effectSizeTest(testArgs *targs)
{
	testArgs		mungeArgs = *targs;
	
	if(targs->verbose) {
		printf("         ...modifying effective key size\n");
	}
	mungeArgs.effectiveKeySizeInBits -= 8;
	return testCommon("effectSizeTest", targs, &mungeArgs);
}

/* 
 * Ensure rounds alters ctext. RC5 only.
 */
static int roundsTest(testArgs *targs)
{
	testArgs		mungeArgs = *targs;
	
	if(targs->verbose) {
		printf("         ...modifying rounds\n");
	}
	switch(targs->rounds) {
		case 8:
			mungeArgs.rounds = 12;
			break;
		case 12:
			mungeArgs.rounds = 16;
			break;
		case 16:
			mungeArgs.rounds = 8;
			break;
		default:
			printf("***ACK! roundsTest needs work!\n");
			return 1;
	}
	return testCommon("roundsTest", targs, &mungeArgs);
}


/* 
 * ensure encryption algorithm alters ctext. 
 */
static int encrAlgTest(testArgs *targs)
{
	testArgs		mungeArgs = *targs;
	CSSM_KEY		mungeKey = *targs->key;
	
	/* come up with different encrypt alg - not all work */
	switch(targs->encrAlg) {
		case CSSM_ALGID_DES:		// fixed size key
		case CSSM_ALGID_3DES_3KEY_EDE:
		default:
			return 0;
		case CSSM_ALGID_RC4:		// no IV
			mungeArgs.encrAlg = CSSM_ALGID_ASC;
			break;
		case CSSM_ALGID_ASC:		// no IV
			mungeArgs.encrAlg = CSSM_ALGID_RC4;
			break;
		case CSSM_ALGID_RC2:
			mungeArgs.encrAlg = CSSM_ALGID_RC5;
			break;
		case CSSM_ALGID_RC5:
			mungeArgs.encrAlg = CSSM_ALGID_RC2;
			break;
		case CSSM_ALGID_AES:
			mungeArgs.encrAlg = CSSM_ALGID_RC5;
			mungeArgs.initVector.Length = 8;
			break;
	}

	/* we're assuming this is legal - a shallow copy of a key, followed by a blind
	 * reassignment of its algorithm... */
	mungeKey.KeyHeader.AlgorithmId = mungeArgs.encrAlg;
	mungeArgs.key = &mungeKey;
	
	if(targs->verbose) {
		printf("         ...modifying encryption alg\n");
	}
	
	return testCommon("encrAlgTest", targs, &mungeArgs);
}

/* 
 * ensure encryption mode alters ctext. 
 */
static int encrModeTest(testArgs *targs)
{
	testArgs		mungeArgs = *targs;
	
	/* come up with different encrypt mode - not all work */
	switch(targs->encrMode) {
		case CSSM_ALGMODE_NONE:			// i.e., RC4, ASC
		case CSSM_ALGMODE_CBCPadIV8:	// others, only one which does
										//   padding
			return 0;
		case CSSM_ALGMODE_CBC_IV8:
			mungeArgs.encrMode = CSSM_ALGMODE_ECB;
			mungeArgs.useInitVector = CSSM_FALSE;
			break;
		case CSSM_ALGMODE_ECB:
			mungeArgs.encrMode = CSSM_ALGMODE_CBC_IV8;
			mungeArgs.useInitVector = CSSM_TRUE;
			break;
		default:
			printf("Update encrModeTest\n");
			return 1;
	}
	if(targs->verbose) {
		printf("         ...modifying encryption mode\n");
	}
	
	return testCommon("encrModeTest", targs, &mungeArgs);
}

/*
 * Ensure ptext alters ctext. 
 */
static int ptextTest(testArgs *targs)
{
	uint32			mungeDex;
	uint32			mungeBits;
	testArgs		mungeArgs = *targs;
	CSSM_DATA		mungePtext;
	
	if(targs->verbose) {
		printf("         ...modifying plaintext\n");
	}
	
	/* get munged copy of ptext */
	mungePtext.Length = targs->ptext->Length;
	mungePtext.Data = (uint8 *)CSSM_MALLOC(mungePtext.Length);
	memmove(mungePtext.Data, targs->ptext->Data, mungePtext.Length);
	mungeArgs.ptext = &mungePtext;
	mungeDex = genRand(0, mungePtext.Length - 1);
	mungeBits = randBit();
	mungePtext.Data[mungeDex] ^= mungeBits;
	if(testCommon("ptextTest", targs, &mungeArgs))  {
		return 1;
	}
	appFreeCssmData(&mungePtext, CSSM_FALSE);
	return 0;
}

/*
 * Ensure key alters ctext. Requires raw key, of course.
 */
static int keyTest(testArgs *targs)
{
	uint32			mungeDex;
	uint32			mungeBits;
	testArgs		mungeArgs = *targs;
	CSSM_KEY		mungeKey;
	unsigned		minBit;
	unsigned		maxByte;
	
	if(targs->verbose) {
		printf("         ...modifying key\n");
	}
	
	/* get munged copy of key */
	copyCssmKey(targs->key, &mungeKey);
	mungeArgs.key = &mungeKey;

	maxByte = mungeKey.KeyData.Length - 1;
	if(targs->effectiveKeySizeInBits) {
		/* skip MS byte - partially used */
		maxByte--;
	}
	mungeDex = genRand(0, maxByte);

	minBit = 0;
	switch(targs->keyAlg) {
		case CSSM_ALGID_DES:
		case CSSM_ALGID_DESX:
		case CSSM_ALGID_3DES_3KEY:
			/* skip lsb - DES parity bit */
			minBit++;
			break;
		default:
			break;
	}
	mungeBits = 1 << genRand(minBit, 7);
	mungeKey.KeyData.Data[mungeDex] ^= mungeBits;
	if(testCommon("keyTest", targs, &mungeArgs))  {
		return 1;
	}
	appFreeCssmData(&mungeKey.KeyData, CSSM_FALSE);
	return 0;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_DATA			initVector;
	testArgs			targs;
	privAlg				encrAlg;
	int 				rtn = 0;
	privMode			pmode;
	uint32				origLen;
	uint32				alignRequired;
	CSSM_BOOL			refKeys = CSSM_FALSE;
	int					i;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	quiet = CSSM_FALSE;
	CSSM_BOOL	doPause = CSSM_FALSE;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	repeatOnly = CSSM_FALSE;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if 	CSPDL_ALL_KEYS_ARE_REF
				refKeys = CSSM_TRUE;
				#endif
				break;
		    case 'p':
		    	doPause = CSSM_TRUE;
				break;
			case 'r':
				repeatOnly = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	
	/* statically allocate ptext and initVector; data and length 
	 * change in test loop */
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_PTEXT_SIZE);
	initVector.Data = (uint8 *)CSSM_MALLOC(MAX_IV_SIZE);
	targs.ptext = &ptext;
	targs.initVector = initVector;
	targs.verbose = verbose;
	targs.quiet = quiet;
	
	printf("Starting symDelta; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	targs.cspHand = cspDlDbStartup(bareCsp, NULL);
	if(targs.cspHand == 0) {
		exit(1);
	}
	
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...loop %d\n", loop);
		}
		/* change once per outer loop */
		simpleGenData(&ptext, MIN_PTEXT_SIZE, MAX_PTEXT_SIZE);
		origLen = ptext.Length;
		simpleGenData(&initVector, MAX_IV_SIZE, MAX_IV_SIZE);
		targs.useRefKey = refKeys;
		
		for(encrAlg=ENCR_ALG_FIRST; encrAlg<=ENCR_ALG_LAST; encrAlg++) {
			/* Cook up encryption-related args */
			algInfo(encrAlg,
				&targs.keyAlg,
				&targs.encrAlg,
				&targs.keyAlgStr,
				&targs.initVector.Length);

			/* random key size */
			targs.effectiveKeySizeInBits = randKeySizeBits(targs.keyAlg, OT_Encrypt);
			targs.keySizeInBits = (targs.effectiveKeySizeInBits + 7) & ~7;
			if(targs.keySizeInBits == targs.effectiveKeySizeInBits) {
				/* same size, ignore effective */
				targs.effectiveKeySizeInBits = 0;
			}
			if(!quiet) {
				printf("   ...Encrypt alg %s keySizeInBits %u effectKeySize %u\n",
					targs.keyAlgStr, (unsigned)targs.keySizeInBits,
					(unsigned)targs.effectiveKeySizeInBits);
			}
			
			/* generate raw key for this keyAlg */
			targs.key = cspGenSymKey(targs.cspHand,
				targs.keyAlg,
				KEY_LABEL1,
				KEY_LABEL_LEN,
				CSSM_KEYUSE_ENCRYPT,
				targs.keySizeInBits,
				targs.useRefKey);
			if(targs.key == NULL) {
				if(testError(quiet)) {
					goto testDone;		// abort
				}
				else {
					continue;			// next key alg
				}
			}

			/* 
			 * Inner loop: iterate thru modes for algorithms which 
			 * support them 
			 */
			for(pmode=ENCR_MODE_FIRST; 
			    pmode<=ENCR_MODE_LAST; 
				pmode++) {
				
				/* Cook up mode-related args */
				modeInfo(targs.keyAlg,
					pmode,
					&targs.encrMode,
					&targs.encrModeStr,
					&targs.encrPad,
					&targs.useInitVector);
				
				if(targs.keyAlg == CSSM_ALGID_RC5) {
					/* roll the dice, pick one of three values for rounds */
					unsigned die = genRand(1,3);
					switch(die) {
						case 1:
							targs.rounds = 8;
							break;
						case 2:
							targs.rounds = 12;
							break;
						case 3:
							targs.rounds = 16;
							break;
					}
				}
				else {
					targs.rounds = 0;
				}
				if(!quiet) {
					printf("      ...mode %s\n", targs.encrModeStr);
				}
				
				alignRequired = alignInfo(targs.encrAlg, targs.encrMode);
				if(alignRequired) {
					/* truncate ptext - we'll restore it at end of loop */
					ptext.Length &= (~(alignRequired - 1));
				}
				if(targs.useInitVector) {
					if(initVectTest(&targs)) {
						rtn = 1;
						goto testDone;
					}
				}

				if(targs.effectiveKeySizeInBits != 0) {
					if(effectSizeTest(&targs)) {
						rtn = 1;
						goto testDone;
					}
				}
				
				if(targs.rounds != 0) {
					if(roundsTest(&targs)) {
						rtn = 1;
						goto testDone;
					}
				}
				
				if(!targs.useRefKey) {
					/* can't do this with ref keys due to key/encr alg mismatch */
					if(encrAlgTest(&targs)) {
						rtn = 1;
						goto testDone;
					}
				}
				
				if(encrModeTest(&targs)) {
					rtn = 1;
					goto testDone;
				}
				if(ptextTest(&targs)) {
					rtn = 1;
					goto testDone;
				}
				if(!targs.useRefKey) {
					if(keyTest(&targs)) {
						rtn = 1;
						goto testDone;
					}
				}

				
				/* restore possible rounded ptext length */
				ptext.Length = origLen;
				
				if(targs.encrMode == CSSM_ALGMODE_NONE) {
					/* one mode, we're done */
					break;
				}

			} /* for mode */
			
			/* free key */
			cspFreeKey(targs.cspHand, targs.key);
			CSSM_FREE(targs.key);

		} /* for encrAlg */
		if(doPause) {
			if(testError(quiet)) {
				break;
			}
		}
		if(loops && (loop == loops)) {
			break;
		}
	} /* for loop */
	
testDone:
	CSSM_ModuleDetach(targs.cspHand);
	if(!quiet && (rtn == 0)) {
		printf("%s test complete\n", argv[0]);
	}
	return rtn;
}
