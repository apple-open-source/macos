/* Copyright (c) 1998,2003-2005,2008 Apple Inc.
 *
 * pbeTest.c - test CSP PBE-style DeriveKey().
 *
 * Revision History
 * ----------------
 *  15 May 2000 Doug Mitchell
 *		Ported to X/CDSA2. 
 *  13 Aug 1998	Doug Mitchell at NeXT
 *		Created.
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <Security/cssm.h>
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
#define MAX_PASSWORD_SIZE	64
#define MAX_SALT_SIZE		32
#define MIN_ITER_COUNT		1000
#define MAX_ITER_COUNT		2000
#define MAX_IV_SIZE			AES_BLOCK_SIZE

/* min values not currently exported by CSP */
#define APPLE_PBE_MIN_PASSWORD	8
#define APPLE_PBE_MIN_SALT		8

/* static IV for derive algorithms which don't create one */
CSSM_DATA staticIv = {MAX_IV_SIZE, (uint8 *)"someIvOrOther..."};

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef unsigned privAlg;
enum {
	/* PBE algs */
	pbe_pbkdf2 = 1,
	// other unsupported for now
	pbe_PKCS12 = 1,
	pbe_MD5,
	pbe_MD2,
	pbe_SHA1,
	
	/* key gen algs */
	pka_ASC,
	pka_RC4,
	pka_DES,
	pka_RC2,
	pka_RC5,
	pka_3DES,
	pka_AES
};

#define PBE_ALG_FIRST			pbe_pbkdf2
#define PBE_ALG_LAST			pbe_pbkdf2
#define ENCR_ALG_FIRST			pka_ASC
#define ENCR_ALG_LAST			pka_AES
#define ENCR_ALG_LAST_EXPORT	pka_RC5

/*
 * Args passed to each test
 */
typedef struct {
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_ALGORITHMS		keyAlg;
	CSSM_ALGORITHMS		encrAlg;
	uint32				keySizeInBits;
	uint32				effectiveKeySizeInBits;	// 0 means not used
	const char 			*keyAlgStr;
	CSSM_ENCRYPT_MODE	encrMode;
	CSSM_PADDING		encrPad;
	CSSM_ALGORITHMS		deriveAlg;
	const char 			*deriveAlgStr;
	CSSM_DATA_PTR		ptext;
	CSSM_DATA_PTR		password;
	CSSM_DATA_PTR 		salt;
	uint32 				iterCount;
	CSSM_BOOL 			useInitVector;		// encrypt needs an IV
	uint32				ivSize;
	CSSM_BOOL			genInitVector;		// DeriveKey generates an IV
	CSSM_BOOL			useRefKey;
	CSSM_BOOL			quiet;
} testArgs;

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   e(xport)\n");
	printf("   r(epeatOnly)\n");
	printf("   z(ero length password)\n");
	printf("   p(ause after each loop)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * Given a privAlg value, return various associated stuff.
 */
static void algInfo(privAlg alg,			// pbe_MD5, etc.
	CSSM_ALGORITHMS		*cdsaAlg,		// CSSM_ALGID_MD5_PBE, etc. RETURNED
										//   key alg for key gen algs
	CSSM_ALGORITHMS		*encrAlg,		// encrypt/decrypt alg for key 
										//   gen algs
	CSSM_ENCRYPT_MODE	*mode,			// RETURNED
	CSSM_PADDING 		*padding,		// RETURNED
	CSSM_BOOL			*useInitVector,	// RETURNED, for encrypt/decrypt
	uint32				*ivSize,		// RETURNED, in bytes
	CSSM_BOOL			*genInitVector,	// RETURNED, for deriveKey
	const char			**algStr)		// RETURNED
{
	/* default or irrelevant fields */
	*mode = CSSM_ALGMODE_NONE;
	*useInitVector = CSSM_FALSE;
	*genInitVector = CSSM_FALSE;		// DeriveKey doesn't do this now
	*padding = CSSM_PADDING_PKCS1;
	*ivSize = 8;						// thje usual size, if needed
	*encrAlg = CSSM_ALGID_NONE;
	
	switch(alg) {
		case pbe_pbkdf2:
			*cdsaAlg = CSSM_ALGID_PKCS5_PBKDF2;
			*algStr = "PBKDF2";
			return;
		/* these are not supported */
		#if 0
		case pbe_PKCS12:
			*cdsaAlg = CSSM_ALGID_SHA1_PBE_PKCS12;
			*algStr = "PKCS12";
			return;
		case pbe_MD5:
			*cdsaAlg = CSSM_ALGID_MD5_PBE;
			*algStr = "MD5";
			return;
		case pbe_MD2:
			*cdsaAlg = CSSM_ALGID_MD2_PBE;
			*algStr = "MD2";
			return;
		case pbe_SHA1:
			*cdsaAlg = CSSM_ALGID_SHA1_PBE;
			*algStr = "SHA1";
			return;
		case pka_ASC:
			*cdsaAlg = CSSM_ALGID_ASC;
			*algStr = "ASC";
			return;
		#endif 
		case pka_DES:
			*cdsaAlg = *encrAlg = CSSM_ALGID_DES;
			*useInitVector = CSSM_TRUE;
			*mode = CSSM_ALGMODE_CBCPadIV8;
			*algStr = "DES";
			return;
		case pka_3DES:
			*cdsaAlg = CSSM_ALGID_3DES_3KEY;
			*encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
			*useInitVector = CSSM_TRUE;
			*mode = CSSM_ALGMODE_CBCPadIV8;
			*algStr = "3DES";
			return;
		case pka_AES:
			*cdsaAlg = *encrAlg = CSSM_ALGID_AES;
			*useInitVector = CSSM_TRUE;
			*mode = CSSM_ALGMODE_CBCPadIV8;
			*padding = CSSM_PADDING_PKCS5;
			*ivSize = AES_BLOCK_SIZE;			// per the default block size
			*algStr = "AES";
			return;
		case pka_RC2:
			*cdsaAlg = *encrAlg = CSSM_ALGID_RC2;
			*useInitVector = CSSM_TRUE;
			*mode = CSSM_ALGMODE_CBCPadIV8;
			*algStr = "RC2";
			return;
		case pka_RC4:
			*cdsaAlg = *encrAlg = CSSM_ALGID_RC4;
			/* initVector false */
			*mode = CSSM_ALGMODE_NONE;
			*algStr = "RC4";
			return;
		case pka_RC5:
			*cdsaAlg = *encrAlg = CSSM_ALGID_RC5;
			*algStr = "RC5";
			*mode = CSSM_ALGMODE_CBCPadIV8;
			*useInitVector = CSSM_TRUE;
			return;
		case pka_ASC:
			*cdsaAlg = *encrAlg = CSSM_ALGID_ASC;
			/* initVector false */
			*algStr = "ASC";
			*mode = CSSM_ALGMODE_NONE;
			return;
		default:
			printf("BRRZZZT! Update algInfo()!\n");
			testError(CSSM_TRUE);
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
 * Writer debug - assertion failure when ctext[1].Data is NULL
 * but length is nonzero
 */
#define SAFE_CTEXT_ARRAY	0

/*
 * Encrypt ptext using specified key, IV, effectiveKeySizeInBits
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
	CSSM_DATA_PTR ctext,			// RETURNED
	CSSM_BOOL quiet)
{
	CSSM_CC_HANDLE cryptHand;
	CSSM_RETURN crtn;
	CSSM_SIZE bytesEncrypted;
	CSSM_DATA remData;
	int rtn;
	#if 	SAFE_CTEXT_ARRAY
	CSSM_DATA safeCtext[2];
	safeCtext[0] = *ctext;
	safeCtext[1].Data = NULL;
	safeCtext[1].Length = 10;		// lie, but shouldn't use this!
	#else
	//	printf("+++ ctext[0] = %d:0x%x; ctext[1] = %d:0x%x\n",
	//		ctext[0].Length, ctext[0].Data,
	//		ctext[1].Length, ctext[1].Data);
	#endif
	
	cryptHand = genCryptHandle(cspHand,
		alg,
		mode,
		padding,		
		key,
		NULL,			// no 2nd key
		iv,				// InitVector
		effectiveKeySizeInBits,
		0);				// rounds
	if(cryptHand == 0) {
		return testError(quiet);
	}
	
	remData.Data = NULL;
	remData.Length = 0;
	crtn = CSSM_EncryptData(cryptHand,
		ptext,
		1,
		#if 	SAFE_CTEXT_ARRAY
		&safeCtext[0],
		#else
		ctext,
		#endif
		1,
		&bytesEncrypted,
		&remData);
	#if 	SAFE_CTEXT_ARRAY
	*ctext = safeCtext[0];
	#endif
	
	if(crtn) {
		printError("CSSM_EncryptData", crtn);
		rtn = testError(quiet);
		goto done;
	}
	if(remData.Length != 0) {
		//printf("***WARNING: nonzero remData on encrypt!\n");
		/* new for CDSA2 - possible remData even if we ask the CSP to 
		 * malloc ctext */
		ctext->Data = (uint8 *)appRealloc(ctext->Data, bytesEncrypted, NULL);
		memmove(ctext->Data + ctext->Length, 
			remData.Data, 
			bytesEncrypted - ctext->Length);
		appFreeCssmData(&remData, CSSM_FALSE);
	}
	/* new for CDSA 2 */
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
 * Decrypt ctext using specified key, IV, effectiveKeySizeInBits
 */
static int decryptCom(CSSM_CSP_HANDLE cspHand,
	const char *testName,
	CSSM_DATA_PTR ctext,
	CSSM_KEY_PTR key,
	CSSM_ALGORITHMS alg,
	CSSM_ENCRYPT_MODE mode,
	CSSM_PADDING padding,		
	CSSM_DATA_PTR iv,				// may be NULL
	uint32 effectiveKeySizeInBits,	// may be 0
	CSSM_DATA_PTR ptext,			// RETURNED
	CSSM_BOOL quiet)
{
	CSSM_CC_HANDLE cryptHand;
	CSSM_RETURN crtn;
	CSSM_SIZE bytesDecrypted;
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
		0);				// rounds
	if(cryptHand == 0) {
		return testError(quiet);
	}
	remData.Data = NULL;
	remData.Length = 0;
	crtn = CSSM_DecryptData(cryptHand,
		ctext,
		1,
		ptext,
		1,
		&bytesDecrypted,
		&remData);
	if(crtn) {
		printError("CSSM_DecryptData", crtn);
		rtn = testError(quiet);
		goto done;
	}
	if(remData.Length != 0) {
		//printf("***WARNING: nonzero remData on decrypt!\n");
		/* new for CDSA2 - possible remData even if we ask the CSP to 
		 * malloc ptext */
		ptext->Data = (uint8 *)appRealloc(ptext->Data, bytesDecrypted, NULL);
		memmove(ptext->Data + ptext->Length, 
			remData.Data, 
			bytesDecrypted - ptext->Length);
		appFreeCssmData(&remData, CSSM_FALSE);
	}
	/* new for CDSA 2 */
	ptext->Length = bytesDecrypted;
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
 *   encrypt ptext with key1, iv1
 *   encrypt ptext with key2, iv2
 *	 compare 2 ctexts; expect failure;
 */
 
#define TRAP_WRITER_ERR	1

static int testCommon(CSSM_CSP_HANDLE cspHand,
	const char *testName,
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	CSSM_PADDING encrPad,
	uint32 effectiveKeySizeInBits,
	CSSM_DATA_PTR ptext,
	CSSM_KEY_PTR key1,
	CSSM_DATA_PTR iv1,
	CSSM_KEY_PTR key2,
	CSSM_DATA_PTR iv2,
	CSSM_BOOL quiet)
{
	CSSM_DATA		ctext1;
	CSSM_DATA		ctext2;
	ctext1.Data = NULL;
	ctext1.Length = 0;
	ctext2.Data = NULL;
	ctext2.Length = 0;
	if(encryptCom(cspHand,
			testName,
			ptext,
			key1,
			encrAlg,
			encrMode,
			encrPad,
			iv1,
			effectiveKeySizeInBits,
			&ctext1,
			quiet)) {
		return 1;
	}
	#if	TRAP_WRITER_ERR
	if(ctext2.Data != NULL){
		printf("Hey! encryptCom(ptext, ctext1 modified ctext2!\n");
		if(testError(quiet)) {
			return 1;
		}
	}
	#endif
	if(encryptCom(cspHand,
			testName,
			ptext,
			key2,
			encrAlg,
			encrMode,
			encrPad,
			iv2,
			effectiveKeySizeInBits,
			&ctext2,
			quiet)) {
		return 1;
	}
	if(compareData(&ctext1, &ctext2)) {
		printf("***%s: Unexpected Data compare!\n", testName);
		return testError(quiet);
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

/* test repeatability - the only test here which actually decrypts */
static int repeatTest(testArgs *targs)
{
	/*
	generate two keys with same params;
	encrypt ptext with key1;
	decrypt ctext with key2;
	compare; expect success;
	*/
	CSSM_KEY_PTR	key1;
	CSSM_KEY_PTR	key2;
	CSSM_DATA		iv1;
	CSSM_DATA		iv2;
	CSSM_DATA_PTR	ivp1;
	CSSM_DATA_PTR	ivp2;
	CSSM_DATA		ctext;
	CSSM_DATA		rptext;
	CSSM_BOOL		gotErr = CSSM_FALSE;
	
	if(targs->useInitVector) {
		if(targs->genInitVector) {
			ivp1 = &iv1;
			ivp2 = &iv2;
		}
		else {
			staticIv.Length = targs->ivSize;
			ivp1 = ivp2 = &staticIv;
		}
	}
	else {
		ivp1 = ivp2 = NULL;
	}
	/* these need to be init'd regardless */
	iv1.Data = NULL;
	iv1.Length = 0;
	iv2.Data = NULL;
	iv2.Length = 0;
	ctext.Data = NULL;
	ctext.Length = 0;
	rptext.Data = NULL;
	rptext.Length = 0;
repeatDerive:
	key1 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL1,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount,
		&iv1);
	if(key1 == NULL) {
		return testError(targs->quiet);
	}
	key2 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL2,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_DECRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount,
		&iv2);
	if(key2 == NULL) {
		return testError(targs->quiet);
	}
repeatEnc:
	if(encryptCom(targs->cspHand,
			"repeatTest",
			targs->ptext,
			key1,
			targs->encrAlg,
			targs->encrMode,
			targs->encrPad,
			ivp1,
			targs->effectiveKeySizeInBits,
			&ctext,
			targs->quiet)) {
		return 1;
	}
	if(decryptCom(targs->cspHand,
			"repeatTest",
			&ctext,
			key2,
			targs->encrAlg,
			targs->encrMode,
			targs->encrPad,
			ivp2,
			targs->effectiveKeySizeInBits,
			&rptext,
			targs->quiet)) {
		return 1;
	}
	if(gotErr || !compareData(targs->ptext, &rptext)) {
		printf("***Data miscompare in repeatTest\n");
		if(REPEAT_ON_ERROR) {
			char str;
			
			gotErr = CSSM_TRUE;
			fpurge(stdin);
			printf("Repeat enc/dec (r), repeat derive (d), continue (c), abort (any)? ");
			str = getchar();
			switch(str) {
				case 'r':
					appFreeCssmData(&ctext, CSSM_FALSE);	
					appFreeCssmData(&rptext, CSSM_FALSE);	
					goto repeatEnc;
				case 'd':
					appFreeCssmData(&ctext, CSSM_FALSE);	
					appFreeCssmData(&rptext, CSSM_FALSE);	
					appFreeCssmData(&iv1, CSSM_FALSE);
					appFreeCssmData(&iv2, CSSM_FALSE);
					cspFreeKey(targs->cspHand, key1);
					cspFreeKey(targs->cspHand, key2);
					goto repeatDerive;
				case 'c':
					break;
				default:
					return 1;
			}
		}
		else {
			return testError(targs->quiet);
		}
	}
	appFreeCssmData(&ctext, CSSM_FALSE);
	appFreeCssmData(&rptext, CSSM_FALSE);
	appFreeCssmData(&iv1, CSSM_FALSE);
	appFreeCssmData(&iv2, CSSM_FALSE);
	cspFreeKey(targs->cspHand, key1);
	cspFreeKey(targs->cspHand, key2);
	CSSM_FREE(key1);
	CSSM_FREE(key2);
	return 0;
}

/* ensure iterCount alters key */
static int iterTest(testArgs *targs)
{
	/*
	generate key1(iterCount), key2(iterCount+1);
	encrypt ptext with key1;
	encrypt ptext with key2;
	compare 2 ctexts; expect failure;
	*/
	CSSM_KEY_PTR	key1;
	CSSM_KEY_PTR	key2;
	CSSM_DATA		iv1;
	CSSM_DATA		iv2;
	CSSM_DATA_PTR	ivp1;
	CSSM_DATA_PTR	ivp2;
	if(targs->useInitVector) {
		if(targs->genInitVector) {
			ivp1 = &iv1;
			ivp2 = &iv2;
		}
		else {
			staticIv.Length = targs->ivSize;
			ivp1 = ivp2 = &staticIv;
		}
	}
	else {
		ivp1 = ivp2 = NULL;
	}
	/* these need to be init'd regardless */
	iv1.Data = NULL;
	iv1.Length = 0;
	iv2.Data = NULL;
	iv2.Length = 0;
	key1 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL1,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount,
		&iv1);
	if(key1 == NULL) {
		return testError(targs->quiet);
	}
	key2 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL2,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount + 1,		// the changed param
		&iv2);
	if(key2 == NULL) {
		return testError(targs->quiet);
	}
	if(testCommon(targs->cspHand,
				"iterTest",
				targs->encrAlg,
				targs->encrMode,
				targs->encrPad,
				targs->effectiveKeySizeInBits,
				targs->ptext,
			 	key1,
				ivp1,
				key2,
				ivp2,
				targs->quiet)) {
		return 1;
	}
	appFreeCssmData(&iv1, CSSM_FALSE);
	appFreeCssmData(&iv2, CSSM_FALSE);
	cspFreeKey(targs->cspHand, key1);
	cspFreeKey(targs->cspHand, key2);
	CSSM_FREE(key1);
	CSSM_FREE(key2);
	return 0;
}

/* ensure password alters key */
static int passwordTest(testArgs *targs)
{
	/*
	generate key1(password), key2(munged password);
	encrypt ptext with key1;
	encrypt ptext with key2;
	compare 2 ctexts; expect failure;
	*/
	CSSM_KEY_PTR	key1;
	CSSM_KEY_PTR	key2;
	CSSM_DATA		iv1;
	CSSM_DATA		iv2;
	CSSM_DATA_PTR	ivp1;
	CSSM_DATA_PTR	ivp2;
	uint32			mungeDex;
	uint32			mungeBits;
	if(targs->useInitVector) {
		if(targs->genInitVector) {
			ivp1 = &iv1;
			ivp2 = &iv2;
		}
		else {
			staticIv.Length = targs->ivSize;
			ivp1 = ivp2 = &staticIv;
		}
	}
	else {
		ivp1 = ivp2 = NULL;
	}
	/* these need to be init'd regardless */
	iv1.Data = NULL;
	iv1.Length = 0;
	iv2.Data = NULL;
	iv2.Length = 0;
	key1 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL1,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount,
		&iv1);
	if(key1 == NULL) {
		return testError(targs->quiet);
	}
	/* munge password */
	mungeDex = genRand(0, targs->password->Length - 1);
	mungeBits = randBit();
	targs->password->Data[mungeDex] ^= mungeBits;
	key2 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL2,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,			// the changed param
		targs->salt,
		targs->iterCount,
		&iv2);
	if(key2 == NULL) {
		return testError(targs->quiet);
	}
	if(testCommon(targs->cspHand,
				"passwordTest",
				targs->encrAlg,
				targs->encrMode,
				targs->encrPad,
				targs->effectiveKeySizeInBits,
				targs->ptext,
			 	key1,
				ivp1,
				key2,
				ivp2,
				targs->quiet)) {
		return 1;
	}
	/* restore  */
	targs->password->Data[mungeDex] ^= mungeBits;
	appFreeCssmData(&iv1, CSSM_FALSE);
	appFreeCssmData(&iv2, CSSM_FALSE);
	cspFreeKey(targs->cspHand, key1);
	cspFreeKey(targs->cspHand, key2);
	CSSM_FREE(key1);
	CSSM_FREE(key2);
	return 0;
}

/* ensure salt alters key */
static int saltTest(testArgs *targs)
{
	/*
	generate key1(seed), key2(munged seed);
	encrypt ptext with key1;
	encrypt ptext with key2;
	compare 2 ctexts; expect failure;
	*/
	CSSM_KEY_PTR	key1;
	CSSM_KEY_PTR	key2;
	CSSM_DATA		iv1;
	CSSM_DATA		iv2;
	CSSM_DATA_PTR	ivp1;
	CSSM_DATA_PTR	ivp2;
	uint32			mungeDex;
	uint32			mungeBits;
	if(targs->useInitVector) {
		if(targs->genInitVector) {
			ivp1 = &iv1;
			ivp2 = &iv2;
		}
		else {
			staticIv.Length = targs->ivSize;
			ivp1 = ivp2 = &staticIv;
		}
	}
	else {
		ivp1 = ivp2 = NULL;
	}
	/* these need to be init'd regardless */
	iv1.Data = NULL;
	iv1.Length = 0;
	iv2.Data = NULL;
	iv2.Length = 0;
	key1 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL1,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount,
		&iv1);
	if(key1 == NULL) {
		return testError(targs->quiet);
	}
	/* munge salt */
	mungeDex = genRand(0, targs->salt->Length - 1);
	mungeBits = randBit();
	targs->salt->Data[mungeDex] ^= mungeBits;
	key2 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL2,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,				// the changed param
		targs->iterCount,
		&iv2);
	if(key2 == NULL) {
		return testError(targs->quiet);
	}
	if(testCommon(targs->cspHand,
				"saltTest",
				targs->encrAlg,
				targs->encrMode,
				targs->encrPad,
				targs->effectiveKeySizeInBits,
				targs->ptext,
			 	key1,
				ivp1,
				key2,
				ivp2,
				targs->quiet)) {
		return 1;
	}
	/* restore  */
	targs->salt->Data[mungeDex] ^= mungeBits;
	appFreeCssmData(&iv1, CSSM_FALSE);
	appFreeCssmData(&iv2, CSSM_FALSE);
	cspFreeKey(targs->cspHand, key1);
	cspFreeKey(targs->cspHand, key2);
	CSSM_FREE(key1);
	CSSM_FREE(key2);
	return 0;
}

/* ensure initVector alters ctext. This isn't testing PBE per se, but
 * it's a handy place to verify this function. */
static int initVectTest(testArgs *targs)
{
	/*
	generate key1;
	encrypt ptext with key1 and initVector;
	encrypt ptext with key1 and munged initVector;
	compare 2 ctexts; expect failure;
	*/
	CSSM_KEY_PTR	key1;
	CSSM_DATA		iv1;
	CSSM_DATA		iv2;
	uint32			mungeDex;
	uint32			mungeBits;
	
	if(targs->genInitVector) {
		iv1.Data = NULL;
		iv1.Length = 0;
	}
	else  {
		iv1 = staticIv;
	}
	key1 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL1,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount,
		&iv1);
	if(key1 == NULL) {
		return testError(targs->quiet);
	}
	
	/* get munged copy of iv */
	iv2.Data = (uint8 *)CSSM_MALLOC(iv1.Length);
	iv2.Length = iv1.Length;
	memmove(iv2.Data, iv1.Data, iv1.Length);
	mungeDex = genRand(0, iv1.Length - 1);
	mungeBits = randBit();
	iv2.Data[mungeDex] ^= mungeBits;
	if(testCommon(targs->cspHand,
				"initVectTest",
				targs->encrAlg,
				targs->encrMode,
				targs->encrPad,
				targs->effectiveKeySizeInBits,
				targs->ptext,
			 	key1,
				&iv1,
				key1,
				&iv2,	// the changed param
				targs->quiet)) {	
		return 1;
	}
	if(targs->genInitVector) {
		appFreeCssmData(&iv1, CSSM_FALSE);
	}
	appFreeCssmData(&iv2, CSSM_FALSE);
	cspFreeKey(targs->cspHand, key1);
	CSSM_FREE(key1);
	return 0;
}

#if 0
/* only one algorithm supported */
/* ensure deriveAlg alters key */
static int deriveAlgTest(testArgs *targs)
{
	/*
	generate key1(deriveAlg), key2(some other deriveAlg);
	encrypt ptext with key1;
	encrypt ptext with key2;
	compare 2 ctexts; expect failure;
	*/
	CSSM_KEY_PTR    key1;
	CSSM_KEY_PTR    key2;
	CSSM_DATA       iv1;
	CSSM_DATA       iv2;
	CSSM_DATA_PTR   ivp1;
	CSSM_DATA_PTR   ivp2;
	uint32          mungeAlg;
	
	if(targs->useInitVector) {
		if(targs->genInitVector) {
			ivp1 = &iv1;
			ivp2 = &iv2;
		}
		else {
			staticIv.Length = targs->ivSize;
			ivp1 = ivp2 = &staticIv;
		}
	}
	else {
		ivp1 = ivp2 = NULL;
	}
	
	/* these need to be init'd regardless */
	iv1.Data = NULL;
	iv1.Length = 0;
	iv2.Data = NULL;
	iv2.Length = 0;
	key1 = cspDeriveKey(targs->cspHand,
		targs->deriveAlg,
		targs->keyAlg,
		KEY_LABEL1,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,
		targs->salt,
		targs->iterCount,
		&iv1);
	if(key1 == NULL) {
		return testError(quiet);
	}
	
	/* munge deriveAlg */
	switch(targs->deriveAlg) {
		case CSSM_ALGID_MD5_PBE:
			mungeAlg = CSSM_ALGID_MD2_PBE;
			break;
		case CSSM_ALGID_MD2_PBE:
			mungeAlg = CSSM_ALGID_SHA1_PBE;
			break;
		case CSSM_ALGID_SHA1_PBE:
			mungeAlg = CSSM_ALGID_SHA1_PBE_PKCS12;
			break;
		case CSSM_ALGID_SHA1_PBE_PKCS12:
			mungeAlg = CSSM_ALGID_MD5_PBE;
			break;
		default:
			printf("BRRRZZZT! Update deriveAlgTest()!\n");
			return testError(quiet);
	}
	key2 = cspDeriveKey(targs->cspHand,
		mungeAlg,                               // the changed param
		targs->keyAlg,
		KEY_LABEL2,
		KEY_LABEL_LEN,
		CSSM_KEYUSE_ENCRYPT,
		targs->keySizeInBits,
		targs->useRefKey,
		targs->password,                        // the changed param
		targs->salt,
		targs->iterCount,
		&iv2);
	if(key2 == NULL) {
		return testError(quiet);
	}
	if(testCommon(targs->cspHand,
			"deriveAlgTest",
			targs->encrAlg,
			targs->encrMode,
			targs->encrPad,
			targs->effectiveKeySizeInBits,
			targs->ptext,
			key1,
			ivp1,
			key2,
			ivp2,
			targs->quiet)) {
		return 1;
	}
	appFreeCssmData(&iv1, CSSM_FALSE);
	appFreeCssmData(&iv2, CSSM_FALSE);
	cspFreeKey(targs->cspHand, key1);
	cspFreeKey(targs->cspHand, key2);
	CSSM_FREE(key1);
	CSSM_FREE(key2);
	return 0;
}
#endif

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	testArgs			targs;
	CSSM_DATA			pwd;
	CSSM_DATA			salt;
	privAlg				pbeAlg;
	privAlg				encrAlg;
	privAlg				lastEncrAlg;
	int 				rtn = 0;
	CSSM_BOOL 			fooBool;
	CSSM_BOOL			refKeysOnly = CSSM_FALSE;
	int					i;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	quiet = CSSM_FALSE;
	CSSM_BOOL	doPause = CSSM_FALSE;
	CSSM_BOOL	doExport = CSSM_FALSE;
	CSSM_BOOL	repeatOnly = CSSM_FALSE;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	CSSM_BOOL	zeroLenPassword = CSSM_FALSE;
	
	#if	macintosh
	argc = ccommand(&argv);
	#endif
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if CSPDL_ALL_KEYS_ARE_REF
				refKeysOnly = CSSM_TRUE;
				#endif
				break;
		    case 'p':
		    	doPause = CSSM_TRUE;
				break;
			case 'e':
				doExport = CSSM_TRUE;
				break;
			case 'r':
				repeatOnly = CSSM_TRUE;
				break;
			case 'z':
				zeroLenPassword = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}

	/* statically allocate ptext, password and seed; data and length
	 * change in test loop */
	pwd.Data = (uint8 *)CSSM_MALLOC(MAX_PASSWORD_SIZE);
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_PTEXT_SIZE);
	salt.Data = (uint8 *)CSSM_MALLOC(MAX_SALT_SIZE);
	printf("Starting pbeTest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	targs.cspHand = cspDlDbStartup(bareCsp, NULL);
	if(targs.cspHand == 0) {
		exit(1);
	}
	targs.ptext 	= &ptext;
	targs.password 	= &pwd;
	targs.salt 		= &salt;
	targs.quiet     = quiet;
	if(doExport) {
		lastEncrAlg = ENCR_ALG_LAST_EXPORT;
	}
	else {
		lastEncrAlg = ENCR_ALG_LAST;
	}
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...loop %d\n", loop);
		}
		/* change once per outer loop */
		simpleGenData(&ptext, MIN_PTEXT_SIZE, MAX_PTEXT_SIZE);
		if(zeroLenPassword) {
			pwd.Length = 0;	// fixed
		}
		else {
			simpleGenData(&pwd, APPLE_PBE_MIN_PASSWORD, MAX_PASSWORD_SIZE);
		}
		simpleGenData(&salt, APPLE_PBE_MIN_SALT, MAX_SALT_SIZE);
		targs.iterCount = genRand(MIN_ITER_COUNT, MAX_ITER_COUNT);
		if(refKeysOnly) {
			targs.useRefKey = CSSM_TRUE;
		}
		else {
			targs.useRefKey = (loop & 1) ? CSSM_FALSE : CSSM_TRUE;
		}
		
		for(encrAlg=ENCR_ALG_FIRST; encrAlg<=lastEncrAlg; encrAlg++) {
			/* Cook up encryption-related args */
			algInfo(encrAlg,
				&targs.keyAlg,
				&targs.encrAlg,
				&targs.encrMode,
				&targs.encrPad,
				&targs.useInitVector,
				&targs.ivSize,
				&fooBool,				// genInitVector
				&targs.keyAlgStr);
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
			for(pbeAlg=PBE_ALG_FIRST; pbeAlg<=PBE_ALG_LAST; pbeAlg++) {
				/* Cook up pbe-related args */
				uint32 foo;
				algInfo(pbeAlg,
					&targs.deriveAlg,
					&foo, 			// encrAlg
					&foo,			// mode
					&foo,
					&fooBool,		// useInitVector
					&foo,			// ivSize
					&targs.genInitVector,
					&targs.deriveAlgStr);
				if(!quiet) {
					printf("      ...PBE alg %s\n", targs.deriveAlgStr);
				}
				/* grind thru the tests */
				if(repeatTest(&targs)) {
					rtn = 1;
					goto testDone;
				}
				if(repeatOnly) {
					continue;
				}
				if(iterTest(&targs)) {
					rtn = 1;
					goto testDone;
				}
				#if 0
				// not supported yet 
				if(deriveAlgTest(&targs)) {
					rtn = 1;
					goto testDone;
				}
				#endif
				if(!zeroLenPassword) {
					/* won't work with zero length password */
					if(passwordTest(&targs)) {
						rtn = 1;
						goto testDone;
					}
				}
				if(saltTest(&targs)) {
					rtn = 1;
					goto testDone;
				}
				if(targs.useInitVector) {
					if(initVectTest(&targs)) {
						rtn = 1;
						goto testDone;
					}
				}
			} /* for pbeAlg */
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
