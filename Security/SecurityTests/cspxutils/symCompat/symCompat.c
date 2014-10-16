/* 
 * symCompat.c - test compatibilty of two different implementations of a
 * given symmetric encryption algorithm - one in the standard AppleCSP,
 * the other in either libcrypto (for Blowfish and CAST), BSAFE, or the
 * NIST reference port for AES. 
 *
 * Written by Doug Mitchell. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include "bsafeUtils.h"
#include "ssleayUtils.h"
#include "rijndaelApi.h"
#include <string.h>
#include "cspdlTesting.h"

/*
 * Defaults.
 */
#define LOOPS_DEF		200

#define MIN_DATA_SIZE	8
#define MAX_DATA_SIZE	10000							/* bytes */
#define MAX_KEY_SIZE	MAX_KEY_SIZE_RC245_BYTES		/* bytes */
#define LOOP_NOTIFY		20

#define RAW_MODE			CSSM_ALGMODE_ECB		/* doesn't work for BSAFE */
#define RAW_MODE_BSAFE		CSSM_ALGMODE_CBC_IV8

#define COOKED_MODE			CSSM_ALGMODE_CBCPadIV8
#define RAW_MODE_STREAM		CSSM_ALGMODE_NONE

#define RAW_MODE_STR		"ECB"
#define RAW_MODE_BSAFE_STR	"CBC/noPad"
#define COOKED_MODE_STR		"CBC/Pad"
#define RAW_MODE_STREAM_STR	"None"

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	// ALG_ASC = 1,			// not tested - no reference available
	ALG_DES = 1,
	ALG_RC2,
	ALG_RC4,
	ALG_RC5,
	ALG_3DES,
	ALG_AES,
	ALG_AES192,		/* 192 bit block */
	ALG_AES256,		/* 256 bit block */
	ALG_BFISH,
	ALG_CAST
} SymAlg;
#define ALG_FIRST			ALG_DES
#define ALG_LAST			ALG_CAST

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (d=DES; 3=3DES3; 2=RC2; 4=RC4; 5=RC5; a=AES; A=AES192; \n");
	printf("                6=AES256; b=Blowfish; c=CAST; default=all)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   k=keySizeInBits\n");
	printf("   e(ncrypt only)\n");
	printf("   m=maxPtextSize (default=%d)\n", MAX_DATA_SIZE);
	printf("   n=minPtextSize (default=%d)\n", MIN_DATA_SIZE);
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   s (all ops single-shot, not staged)\n");
	printf("   o (raw - no padding or CBC if possible)\n");
	printf("   O (cooked - padding and CBC if possible)\n");
	printf("   z (keys and plaintext all zeroes)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   y (use ssleay EVP; AES128 only)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* 
 * encrypt/decrypt using reference BSAFE.
 */
static CSSM_RETURN encryptDecryptBSAFE(
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	encrMode,
	const CSSM_DATA		*iv,				//Êoptional per mode
	uint32				keySizeInBits,
	uint32				effectiveKeyBits,	// optional per key alg
	uint32				rounds,				// ditto
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	CSSM_RETURN crtn;
	BU_KEY buKey;
	
	crtn = buGenSymKey(keySizeInBits, key, &buKey);
	if(crtn) {
		return crtn;
	}
	crtn = buEncryptDecrypt(buKey,
		forEncrypt,					// forEncrypt
		encrAlg,
		encrMode,
		iv,
		effectiveKeyBits,
		rounds,
		inText,
		outText);
	buFreeKey(buKey);
	return crtn;
}

/* 
 * encrypt/decrypt using reference ssleay.
 */
static CSSM_RETURN encryptDecryptEAY(
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	encrMode,
	const CSSM_DATA		*iv,				//Êoptional per mode
	uint32				keySizeInBits,
	const CSSM_DATA		*key,				// raw key bytes, Length ignored
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	CSSM_RETURN crtn;
	EAY_KEY eayKey;
	CSSM_DATA ckey = *key;
	ckey.Length = keySizeInBits / 8;
	
	crtn = eayGenSymKey(encrAlg, forEncrypt, &ckey, &eayKey);
	if(crtn) {
		return crtn;
	}
	crtn = eayEncryptDecrypt(eayKey,
		forEncrypt,		
		encrAlg,
		encrMode,
		iv,
		inText,
		outText);
	eayFreeKey(eayKey);
	return crtn;
}

/* 
 * encrypt/decrypt using reference AES.
 */
static CSSM_RETURN encryptDecryptAES(
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	encrMode,
	const CSSM_DATA		*iv,				//Êoptional per mode
	uint32				keySizeInBits,
	uint32				effectiveKeyBits,	// optional per key alg
	uint32				cipherBlockSize,
	uint32				rounds,				// ditto
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	keyInstance 	aesKey;
	cipherInstance 	aesCipher;
	BYTE 			mode;
	int 			artn;
	BYTE			*ivPtr;
	
	if(cipherBlockSize == 0) {
		cipherBlockSize = MIN_AES_BLOCK_BITS;
	}
	switch(encrMode) {
		case CSSM_ALGMODE_CBC_IV8:
			mode = MODE_CBC;
			ivPtr = iv->Data;
			break;
		case CSSM_ALGMODE_ECB:
			mode = MODE_ECB;
			ivPtr = NULL;
			break;
		default:
			printf("***AES reference implementation doesn't do padding (yet)\n");
			return CSSM_OK;
	}
	/* fixme - adjust for padding if necessary */
	outText->Data = (uint8 *)CSSM_MALLOC(inText->Length);
	outText->Length = inText->Length;
	artn = _makeKey(&aesKey, 
		forEncrypt ? DIR_ENCRYPT : DIR_DECRYPT,
		keySizeInBits,
		cipherBlockSize,
		key->Data);
	if(artn <= 0) {
		printf("***AES makeKey returned %d\n", artn);
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	artn = _cipherInit(&aesCipher,
		mode,
		cipherBlockSize,
		ivPtr);
	if(artn <= 0) {
		printf("***AES cipherInit returned %d\n", artn);
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	if(forEncrypt) {
		artn = _blockEncrypt(&aesCipher,
			&aesKey,
			(BYTE *)inText->Data,
			inText->Length * 8,
			(BYTE *)outText->Data);
	}
	else {
		artn = _blockDecrypt(&aesCipher,
			&aesKey,
			(BYTE *)inText->Data,
			inText->Length * 8,
			(BYTE *)outText->Data);
	}
	if(artn <= 0) {
		printf("***AES encrypt/decrypt returned %d\n", artn);
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	return CSSM_OK;
}

/*
 * Encrypt/decrypt, one-shot, using one of the various reference implementations. 
 */
static CSSM_RETURN encryptDecryptRef(
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	encrMode,
	const CSSM_DATA		*iv,				// optional per mode
	uint32				keySizeInBits,
	uint32				effectiveKeyBits,	// optional per key alg
	uint32				cipherBlockSize,
	uint32				rounds,				// ditto
	CSSM_BOOL			useEvp,				// AES only 
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	switch(encrAlg) {
		case CSSM_ALGID_AES:
			if(useEvp && (cipherBlockSize == 128)) {
				return (CSSM_RETURN)evpEncryptDecrypt(encrAlg, forEncrypt,
					key, keySizeInBits, encrMode, iv, inText, outText);
			}
			else {
				return encryptDecryptAES(
					forEncrypt,
					encrAlg,
					encrMode,
					iv,
					keySizeInBits,
					effectiveKeyBits,
					cipherBlockSize,
					rounds,
					key,
					inText,
					outText);
			}
		case CSSM_ALGID_BLOWFISH:
		case CSSM_ALGID_CAST:
			return encryptDecryptEAY(
				forEncrypt,
				encrAlg,
				encrMode,
				iv,
				keySizeInBits,
				key,
				inText,
				outText);
		default:
			if(useEvp && (encrAlg == CSSM_ALGID_DES)) {
				return (CSSM_RETURN)evpEncryptDecrypt(encrAlg, forEncrypt,
					key, keySizeInBits, encrMode, iv, inText, outText);
			}
			else {
				return encryptDecryptBSAFE(
					forEncrypt,
					encrAlg,
					encrMode,
					iv,
					keySizeInBits,
					effectiveKeyBits,
					rounds,
					key,
					inText,
					outText);
			}
	}
}

/* 
 * encrypt/decrypt using CSSM.
 */
static CSSM_RETURN encryptDecryptCSSM(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		keyAlg,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	encrMode,
	CSSM_PADDING 		padding,			// CSSM_PADDING_PKCS1, etc. 

	CSSM_BOOL 			multiUpdates,		// false:single update, true:multi updates
	const CSSM_DATA		*iv,				//Êoptional per mode
	uint32				keySizeInBits,
	uint32				effectiveKeyBits,	// optional per key alg
	uint32				cipherBlockSize,
	uint32				rounds,				// ditto
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	CSSM_BOOL			genRaw,				// first generate raw key (CSPDL)
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	CSSM_KEY_PTR		symKey;				// mallocd by cspGenSymKey or a ptr
											// to refKey
	CSSM_KEY			refKey;				// in case of genRaw
	CSSM_BOOL			refKeyGenerated = CSSM_FALSE;
	unsigned			keyBytes = (keySizeInBits + 7) / 8;
	CSSM_RETURN			crtn;
	
	if(genRaw) {
		crtn = cspGenSymKeyWithBits(cspHand,
			keyAlg,
			CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
			key,
			keyBytes,
			&refKey);
		if(crtn) {
			return crtn;
		}
		symKey = &refKey;
		refKeyGenerated = CSSM_TRUE;
	}
	else {
		/* cook up a raw symmetric key */
		symKey = cspGenSymKey(cspHand,
			keyAlg,
			"noLabel",
			8,
			CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
			keySizeInBits,
			CSSM_FALSE);			// ref key
		if(symKey == NULL) {
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
		if(symKey->KeyData.Length != keyBytes) {
			printf("***Generated key size error (exp %d, got %lu)\n",
				keyBytes, symKey->KeyData.Length);
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
		memmove(symKey->KeyData.Data, key->Data, keyBytes);
	}
	outText->Data = NULL;
	outText->Length = 0;

	if(keySizeInBits == effectiveKeyBits) {
		effectiveKeyBits = 0;
	}
	
	/* go for it */
	if(forEncrypt) {
		crtn = cspStagedEncrypt(cspHand,
			encrAlg,
			encrMode,
			padding,
			symKey,
			NULL,			// no second key
			effectiveKeyBits,
			cipherBlockSize / 8,
			rounds,
			iv,
			inText,
			outText,
			multiUpdates);
	}
	else {
		crtn = cspStagedDecrypt(cspHand,
			encrAlg,
			encrMode,
			padding,
			symKey,
			NULL,			// no second key
			effectiveKeyBits,
			cipherBlockSize / 8,
			rounds,
			iv,
			inText,
			outText,
			multiUpdates);
	}
	cspFreeKey(cspHand, symKey);
	if(!refKeyGenerated) {
		/* key itself mallocd by cspGenSymKey */
		CSSM_FREE(symKey);
	}
	return crtn;
}

#define LOG_FREQ	20

static int doTest(CSSM_CSP_HANDLE cspHand,
	const CSSM_DATA		*ptext,
	const CSSM_DATA		*keyData,
	const CSSM_DATA		*iv,
	uint32 				keyAlg,						// CSSM_ALGID_xxx of the key
	uint32 				encrAlg,						// encrypt/decrypt
	uint32 				encrMode,
	uint32 				padding,
	uint32				keySizeInBits,
	uint32 				efectiveKeySizeInBits,
	uint32				cipherBlockSize,
	CSSM_BOOL			useEvp,					// AES only 
	CSSM_BOOL 			stagedEncr,
	CSSM_BOOL 			stagedDecr,
	CSSM_BOOL 			quiet,
	CSSM_BOOL			encryptOnly,
	CSSM_BOOL			genRaw)					// first generate raw key (CSPDL)
{
	CSSM_DATA 		ctextRef = {0, NULL};		// ciphertext, reference
	CSSM_DATA		ctextTest = {0, NULL};		// ciphertext, test
	CSSM_DATA		rptext = {0, NULL};			// recovered plaintext
	int				rtn = 0;
	CSSM_RETURN		crtn;
	uint32			rounds = 0;
	
	if(encrAlg == CSSM_ALGID_RC5) {
		/* roll the dice, pick one of three values for rounds */
		unsigned die = genRand(1,3);
		switch(die) {
			case 1:
				rounds = 8;
				break;
			case 2:
				rounds = 12;
				break;
			case 3:
				rounds = 16;
				break;
		}
	}
	
	/*
	 * encrypt with each method;
	 * verify ciphertexts compare;
	 * decrypt with test code;
	 * verify recovered plaintext and incoming plaintext compare;
	 */
	crtn = encryptDecryptRef(CSSM_TRUE,
		encrAlg,
		encrMode,
		iv,
		keySizeInBits,
		efectiveKeySizeInBits, 
		cipherBlockSize,
		rounds,
		useEvp,
		keyData,
		ptext,
		&ctextRef);
	if(crtn) {
		return testError(quiet);
	}
	crtn = encryptDecryptCSSM(cspHand,
		CSSM_TRUE,
		keyAlg,
		encrAlg,
		encrMode,
		padding,
		stagedEncr,
		iv,
		keySizeInBits,
		efectiveKeySizeInBits, 
		cipherBlockSize,
		rounds,
		keyData,
		ptext,
		genRaw,
		&ctextTest);
	if(crtn) {
		return testError(quiet);
	}

	/* ensure both methods resulted in same ciphertext */
	if(ctextRef.Length != ctextTest.Length) {
		printf("Ctext length mismatch (1)\n");
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	if(memcmp(ctextRef.Data, ctextTest.Data, ctextTest.Length)) {
		printf("Ctext miscompare\n");
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	
	if(encryptOnly) {
		rtn = 0;
		goto abort;
	}
	
	/* decrypt with the test method */
	crtn = encryptDecryptCSSM(cspHand,
		CSSM_FALSE,
		keyAlg,
		encrAlg,
		encrMode,
		padding,
		stagedDecr,
		iv,
		keySizeInBits,
		efectiveKeySizeInBits, 
		cipherBlockSize,
		rounds,
		keyData,
		&ctextTest,
		genRaw,
		&rptext);
	if(crtn) {
		return testError(quiet);
	}
	if(rptext.Length != ptext->Length) {
		printf("ptext length mismatch (1)\n");
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	if(memcmp(rptext.Data, ptext->Data, ptext->Length)) {
		printf("ptext miscompare\n");
		rtn = testError(quiet);
	}
	else {
		rtn = 0;
	}
abort:
	if(ctextTest.Length) {
		CSSM_FREE(ctextTest.Data);
	}
	if(ctextRef.Length) {
		CSSM_FREE(ctextRef.Data);
	}
	if(rptext.Length) {
		CSSM_FREE(rptext.Data);
	}
	return rtn;
}


int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_BOOL			stagedEncr;
	CSSM_BOOL 			stagedDecr;
	const char			*algStr;
	uint32				keyAlg;					// CSSM_ALGID_xxx of the key
	uint32				encrAlg;				// CSSM_ALGID_xxx of encr/decr
	int					i;
	unsigned			currAlg;				// ALG_xxx
	uint32				actKeySizeInBits;
	uint32				effectKeySizeInBits;
	int					rtn = 0;
	CSSM_DATA			keyData;
	CSSM_DATA			initVector;
	CSSM_BOOL			genRaw = CSSM_FALSE;	// first generate raw key (CSPDL)
	uint32				minTextSize;
	uint32				rawMode;
	uint32				cookedMode;
	const char			*rawModeStr;
	const char			*cookedModeStr;
	uint32				algBlockSize;
	
	/*
	 * User-spec'd params
	 */
	CSSM_BOOL	keySizeSpec = CSSM_FALSE;		// false: use rand key size
	unsigned	minAlg = ALG_FIRST;
	unsigned	maxAlg = ALG_LAST;
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	pauseInterval = 0;
	uint32		padding;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	CSSM_BOOL	encryptOnly = CSSM_FALSE;
	unsigned 	maxPtextSize = MAX_DATA_SIZE;
	unsigned	minPtextSize = MIN_DATA_SIZE;
	CSSM_BOOL	oneShotOnly = CSSM_FALSE;
	CSSM_BOOL	allZeroes = CSSM_FALSE;
	CSSM_BOOL	rawCookedSpecd = CSSM_FALSE;	// when true, use allRaw only
	CSSM_BOOL	allRaw = CSSM_FALSE;
	CSSM_BOOL	useEvp = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'd':
						minAlg = maxAlg = ALG_DES;
						break;
					case '3':
						minAlg = maxAlg = ALG_3DES;
						break;
					case '2':
						minAlg = maxAlg = ALG_RC2;
						break;
					case '4':
						minAlg = maxAlg = ALG_RC4;
						break;
					case '5':
						minAlg = maxAlg = ALG_RC5;
						break;
					case 'a':
						minAlg = maxAlg = ALG_AES;
						break;
					case 'A':
						minAlg = maxAlg = ALG_AES192;
						break;
					case '6':
						minAlg = maxAlg = ALG_AES256;
						break;
					case 'b':
						minAlg = maxAlg = ALG_BFISH;
						break;
					case 'c':
						minAlg = maxAlg = ALG_CAST;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'k':
		    	actKeySizeInBits = effectKeySizeInBits = atoi(&argp[2]);
		    	keySizeSpec = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if CSPDL_ALL_KEYS_ARE_REF
				genRaw = CSSM_TRUE;
				#endif
				break;
		    case 'e':
		    	encryptOnly = CSSM_TRUE;
				break;
			case 'm':
				maxPtextSize = atoi(&argp[2]);
				break;
			case 'n':
				minPtextSize = atoi(&argp[2]);
				break;
		    case 'z':
		    	allZeroes = CSSM_TRUE;
				break;
		    case 's':
		    	oneShotOnly = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
				break;
			case 'o':
				rawCookedSpecd = CSSM_TRUE;
				allRaw = CSSM_TRUE;
				break;
			case 'O':
				rawCookedSpecd = CSSM_TRUE;
				allRaw = CSSM_FALSE;		// i.e., use cooked 
				break;
			case 'y':
				useEvp = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext.Data = (uint8 *)CSSM_MALLOC(maxPtextSize);
	if(ptext.Data == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	/* ptext length set in test loop */

	keyData.Data = (uint8 *)CSSM_MALLOC(MAX_KEY_SIZE);
	if(keyData.Data == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	keyData.Length = MAX_KEY_SIZE;

	initVector.Data = (uint8 *)"someStrangeInitVect";	
	
	printf("Starting symCompat; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	if(pauseInterval) {
		fpurge(stdin);
		printf("Top of test; hit CR to proceed: ");
		getchar();
	}
	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		/* some default values... */
		padding = CSSM_PADDING_PKCS1;
		switch(currAlg) {
			case ALG_DES:
				encrAlg = keyAlg = CSSM_ALGID_DES;
				algStr        = "DES";
				algBlockSize  = 8;
				if(useEvp) {
					/* this one requires padding */
					rawMode       = RAW_MODE;
					cookedMode    = COOKED_MODE;
					rawModeStr	  = RAW_MODE_STR;
					cookedModeStr = COOKED_MODE_STR;
					padding = CSSM_PADDING_PKCS5;
				}
				else {
					rawMode       = RAW_MODE_BSAFE;
					cookedMode    = CSSM_ALGMODE_CBCPadIV8;
					rawModeStr	  = RAW_MODE_BSAFE_STR;
					cookedModeStr = COOKED_MODE_STR;
				}
				break;
			case ALG_3DES:
				/* currently the only one with different key and encr algs */
				keyAlg  = CSSM_ALGID_3DES_3KEY;
				encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
				algStr = "3DES";
				algBlockSize  = 8;
				rawMode       = RAW_MODE_BSAFE;
				cookedMode    = CSSM_ALGMODE_CBCPadIV8;
				rawModeStr	  = RAW_MODE_BSAFE_STR;
				cookedModeStr = COOKED_MODE_STR;
				break;
			case ALG_RC2:
				encrAlg = keyAlg = CSSM_ALGID_RC2;
				algStr = "RC2";
				algBlockSize  = 8;
				rawMode       = RAW_MODE_BSAFE;
				cookedMode    = CSSM_ALGMODE_CBCPadIV8;
				rawModeStr	  = RAW_MODE_BSAFE_STR;
				cookedModeStr = COOKED_MODE_STR;
				break;
			case ALG_RC4:
				encrAlg = keyAlg = CSSM_ALGID_RC4;
				algStr = "RC4";
				algBlockSize  = 0;
				rawMode       = RAW_MODE_STREAM;
				cookedMode    = RAW_MODE_STREAM;
				rawModeStr	  = RAW_MODE_STREAM_STR;
				cookedModeStr = RAW_MODE_STREAM_STR;
				break;
			case ALG_RC5:
				encrAlg = keyAlg = CSSM_ALGID_RC5;
				algStr = "RC5";
				algBlockSize  = 8;
				rawMode       = RAW_MODE_BSAFE;
				cookedMode    = CSSM_ALGMODE_CBCPadIV8;
				rawModeStr	  = RAW_MODE_BSAFE_STR;
				cookedModeStr = COOKED_MODE_STR;
				break;
			case ALG_AES:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES";
				algBlockSize  = 16;
				if(useEvp) {
					rawMode       = RAW_MODE;
					cookedMode    = COOKED_MODE;
					rawModeStr	  = RAW_MODE_STR;
					cookedModeStr = COOKED_MODE_STR;
					padding = CSSM_PADDING_PKCS7;
				}
				else {
					/* padding not supported in ref implementation */
					rawMode       = RAW_MODE;
					cookedMode    = RAW_MODE_BSAFE;
					rawModeStr	  = RAW_MODE_STR;
					cookedModeStr = RAW_MODE_BSAFE_STR;
				}
				break;
			case ALG_AES192:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES192";
				/* padding not supported in ref implementation */
				algBlockSize  = 24;
				rawMode       = RAW_MODE;
				cookedMode    = RAW_MODE_BSAFE;
				rawModeStr	  = RAW_MODE_STR;
				cookedModeStr = RAW_MODE_BSAFE_STR;
				break;
			case ALG_AES256:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES";
				/* padding not supported in ref implementation */
				algBlockSize  = 32;
				rawMode       = RAW_MODE;
				cookedMode    = RAW_MODE_BSAFE;
				rawModeStr	  = RAW_MODE_STR;
				cookedModeStr = RAW_MODE_BSAFE_STR;
				break;
			case ALG_BFISH:
				encrAlg = keyAlg = CSSM_ALGID_BLOWFISH;
				algStr = "Blowfish";
				algBlockSize = 8;
				/* libcrypt doesn't do padding or ECB */
				rawMode       = RAW_MODE_BSAFE;
				cookedMode    = RAW_MODE_BSAFE;
				rawModeStr	  = RAW_MODE_BSAFE_STR;
				cookedModeStr = RAW_MODE_BSAFE_STR;
				break;
			case ALG_CAST:
				encrAlg = keyAlg = CSSM_ALGID_CAST;
				algStr = "CAST";
				algBlockSize = 8;
				/* libcrypt doesn't do padding or ECB */
				rawMode       = RAW_MODE_BSAFE;
				cookedMode    = RAW_MODE_BSAFE;
				rawModeStr	  = RAW_MODE_BSAFE_STR;
				cookedModeStr = RAW_MODE_BSAFE_STR;
				break;
		}
		
		/* assume for now all algs require IV */
		initVector.Length = algBlockSize ? algBlockSize : 8;
		
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			/* mix up raw/cooked */
			uint32 mode;
			const char *modeStr;
			CSSM_BOOL paddingEnabled;
			
			if(rawCookedSpecd) {
				if(allRaw) {
					mode = rawMode;
					modeStr = rawModeStr;
				}
				else {
					mode = cookedMode;
					modeStr = cookedModeStr;
				}
			}
			else {
				if(loop & 1) {
					mode = rawMode;
					modeStr = rawModeStr;
				}
				else {
					mode = cookedMode;
					modeStr = cookedModeStr;
				}
			}
			switch(mode) {
				case CSSM_ALGMODE_CBCPadIV8:
					paddingEnabled = CSSM_TRUE;
					break;
				default:
					/* all others - right? */
					paddingEnabled = CSSM_FALSE;
					break;
			}
			minTextSize = minPtextSize;	// default
			if(!paddingEnabled && algBlockSize && (minTextSize < algBlockSize)) {
				/* i.e., no padding, adjust min ptext size */
				minTextSize = algBlockSize;
			}
			simpleGenData(&ptext, minTextSize, maxPtextSize);
			if(!paddingEnabled && algBlockSize) {
				/* align ptext */
				ptext.Length = (ptext.Length / algBlockSize) * algBlockSize;
			}
			
			/* mix up staging */
			if(oneShotOnly) {
				stagedEncr = CSSM_FALSE;
				stagedDecr = CSSM_FALSE;
			}
			else {
				stagedEncr = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
				stagedDecr = (loop & 4) ? CSSM_TRUE : CSSM_FALSE;
			}

			if(allZeroes) {
				memset(ptext.Data, 0, ptext.Length);
				memset(keyData.Data, 0, MAX_KEY_SIZE);
				keyData.Length = MAX_KEY_SIZE;
			}
			else {
				simpleGenData(&keyData, MAX_KEY_SIZE, MAX_KEY_SIZE);
			}

			if(!keySizeSpec) {
				effectKeySizeInBits = randKeySizeBits(keyAlg, OT_Encrypt);
				/* 
				 * generate keys with well aligned sizes; effectiveKeySize 
				 * differs only if not well aligned 
				 */
				actKeySizeInBits = (effectKeySizeInBits + 7) & ~7;
			}
			/* else constant, spec'd by user, may be 0 (default per alg) */
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					if(algBlockSize) {
						printf("..loop %d text size %lu keySizeBits %u"
							" blockSize %u stageEncr %d  stageDecr %d mode %s\n",
							loop, (unsigned long)ptext.Length, (unsigned)effectKeySizeInBits,
							(unsigned)algBlockSize, (int)stagedEncr, (int)stagedDecr,
							modeStr);
					}
					else {
						printf("..loop %d text size %lu keySizeBits %u"
							" stageEncr %d  stageDecr %d mode %s\n",
							loop, (unsigned long)ptext.Length, (unsigned)effectKeySizeInBits,
							(int)stagedEncr, (int)stagedDecr, modeStr);
					}
				}
			}
			
			if(doTest(cspHand,
					&ptext,
					&keyData,
					&initVector,
					keyAlg,
					encrAlg,
					mode,
					padding,
					actKeySizeInBits,
					actKeySizeInBits,		// FIXME - test effective key size
					algBlockSize * 8,
					useEvp,
					stagedEncr,
					stagedDecr,
					quiet,
					encryptOnly,
					genRaw)) {
				rtn = 1;
				break;
			}
			if(pauseInterval && ((loop % pauseInterval) == 0)) {
				char c;
				fpurge(stdin);
				printf("Hit CR to proceed, q to abort: ");
				c = getchar();
				if(c == 'q') {
					goto testDone;
				}
			}
			if(loops && (loop == loops)) {
				break;
			}
		}	/* main loop */
		if(rtn) {
			break;
		}
		
	}	/* for algs */
	
testDone:
	cspShutdown(cspHand, bareCsp);
	if(pauseInterval) {
		fpurge(stdin);
		printf("ModuleDetach/Unload complete; hit CR to exit: ");
		getchar();
	}
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	CSSM_FREE(ptext.Data);
	CSSM_FREE(keyData.Data);
	return rtn;
}


