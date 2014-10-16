/* 
 * ccSymCompat.c - test compatibilty of two different implementations of a
 * given symmetric encryption algorithm - one in CommonCrypto (which we
 * might link against directly, not using libSystem, via Makefile tweaking),
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
#include <CommonCrypto/CommonCryptor.h>

/*
 * Defaults.
 */
#define LOOPS_DEF		200

#define MIN_DATA_SIZE	8
#define MAX_DATA_SIZE	10000						/* bytes */
#define MAX_KEY_SIZE	MAX_KEY_SIZE_RC245_BYTES	/* bytes */
#define LOOP_NOTIFY		20

#define NO_PAD_MODE			CSSM_ALGMODE_ECB		/* doesn't work for BSAFE */
#define NO_PAD_MODE_BSAFE	CSSM_ALGMODE_CBC_IV8

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_AES_128 = 1,	/* 128 bit block, 128 bit key */
	ALG_AES_192,		/* 128 bit block, 192 bit key */
	ALG_AES_256,		/* 128 bit block, 256 bit key */
	ALG_DES,
	ALG_3DES,
	ALG_CAST,
	ALG_RC4,
	/* not supported by CommonCrypto */
	ALG_RC2,
	ALG_RC5,
	ALG_BFISH
} SymAlg;
#define ALG_FIRST			ALG_AES_128
#define ALG_LAST			ALG_RC4

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (d=DES; 3=3DES3; a=AES128; n=AES192; A=AES256; c=CAST;\n");
	printf("     4=RC4; default=all)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   k=keySizeInBits\n");
	printf("   e(ncrypt only)\n");
	printf("   m=maxPtextSize (default=%d)\n", MAX_DATA_SIZE);
	printf("   n=minPtextSize (default=%d)\n", MIN_DATA_SIZE);
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   s (all ops single-shot, not staged)\n");
	printf("   o (no padding, disable CBC if possible)\n");
	printf("   z (keys and plaintext all zeroes)\n");
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
	const CSSM_DATA		*iv,				// optional per mode
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
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	switch(encrAlg) {
		case CSSM_ALGID_AES:
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

/* 
 * encrypt/decrypt using CommonCrypto.
 */
static CSSM_RETURN encryptDecryptCC(
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	encrMode,
	CSSM_PADDING 		padding,			// CSSM_PADDING_PKCS1, etc. 
	uint32				cipherBlockSize,
	CSSM_BOOL 			multiUpdates,		// false:single update, true:multi updates
	const CSSM_DATA		*iv,				// optional per mode
	uint32				keySizeInBits,
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	CCCryptorRef cryptor = NULL;
	uint8 *intext = inText->Data;
	uint32 intextLen = inText->Length;
	uint8 *outtext;
	size_t outtextLen;		/* mallocd size of outtext */
	size_t outBytes = 0;	/* bytes actually produced in outtext */
	CCCryptorStatus crtn;
	uint32 blockSize;
	
	/* convert crypt params from CDSA to CommonCrypto */
	CCAlgorithm ccAlg;
	CCOperation ccOp = forEncrypt ? kCCEncrypt : kCCDecrypt;
	CCOptions ccOpts = 0;
	uint8 *ccIv = NULL;
	
	switch(encrAlg) {
		case CSSM_ALGID_DES:
			ccAlg = kCCAlgorithmDES;
			blockSize = kCCBlockSizeDES;
			break;
		case CSSM_ALGID_3DES_3KEY_EDE:
			ccAlg = kCCAlgorithm3DES;
			blockSize = kCCBlockSize3DES;
			break;
		case CSSM_ALGID_AES:
			ccAlg = kCCAlgorithmAES128;
			blockSize = kCCBlockSizeAES128;
			break;
		case CSSM_ALGID_CAST:
			ccAlg = kCCAlgorithmCAST;
			blockSize = kCCBlockSizeCAST;
			break;
		case CSSM_ALGID_RC4:
			ccAlg = kCCAlgorithmRC4;
			blockSize = 0;
			break;
		default:
			printf("***BRRZAP! Unknown algorithm in encryptDecryptCC()\n");
			return -1;
	}
	if(padding != CSSM_PADDING_NONE) {
		ccOpts |= kCCOptionPKCS7Padding;
	}
	switch(encrMode) {
		case CSSM_ALGMODE_CBC_IV8:
			/* testing DES against BSAFE */
			ccOpts = 0;
			ccIv = iv->Data;
			break;
		case CSSM_ALGMODE_ECB:
			ccIv = NULL;
			break;
		case CSSM_ALGMODE_CBCPadIV8:
			/* padding and cbc */
			ccIv = iv->Data;
			break;
		case CSSM_ALGMODE_NONE:
			/* stream cipher */
			ccIv = NULL;
			ccOpts = 0;
			break;
		default:
			printf("***Bad mode (%lu)\n", (unsigned long)encrMode);
			return 1;
	}
	if(ccIv == NULL) {
		ccOpts |= kCCOptionECBMode;
	}
	
	/* alloc outtext - round up to next cipherblock boundary for encrypt */
	if(blockSize) {
		unsigned blocks;
		if(forEncrypt) {
			blocks = (intextLen + blockSize) / blockSize;
		}
		else {
			blocks = intextLen / blockSize;
		}
		outtextLen = blocks * blockSize;
	}
	else {
		outtextLen = intextLen;
	}

	outtext = (uint8 *)CSSM_MALLOC(outtextLen);
	memset(outtext, 0x55, outtextLen);

	if(!multiUpdates) {
		/* one shot */
		crtn = CCCrypt(ccOp, ccAlg, ccOpts,
			key->Data, keySizeInBits / 8, ccIv,
			intext, intextLen,
			outtext, outtextLen, &outtextLen);
		if(crtn) {
			printf("***CCCrypt returned %ld\n", (long)crtn);
			return crtn;
		}
		outText->Data = outtext;
		outText->Length = outtextLen;
		return kCCSuccess;
	}

	/* staged, random sized updates */
	crtn = CCCryptorCreate(ccOp, ccAlg, ccOpts,
		key->Data, keySizeInBits / 8, ccIv,
		&cryptor);
	if(crtn) {
		printf("***CCCryptorInit returned %ld\n", (long)crtn);
		return crtn;
	}
	
	size_t toMove = intextLen;		/* total to go */
	size_t thisMoveOut;				/* output from CCCryptUpdate()/CCCryptFinal() */
	uint8 *outp = outtext;
	uint8 *inp = intext;
	
	while(toMove) {
		uint32 thisMoveIn;			/* input to CCryptUpdate() */
		
		thisMoveIn = genRand(1, toMove);
		crtn = CCCryptorUpdate(cryptor, inp, thisMoveIn,
			outp, outtextLen, &thisMoveOut);
		if(crtn) {
			printf("***CCCryptorUpdate returned %ld\n", (long)crtn);
			goto errOut;
		}
		inp			+= thisMoveIn;
		toMove		-= thisMoveIn;
		outp		+= thisMoveOut;
		outtextLen	-= thisMoveOut;
		outBytes	+= thisMoveOut;
	}
	crtn = CCCryptorFinal(cryptor, outp, outtextLen, &thisMoveOut);
	if(crtn) {
		printf("***CCCryptorFinal returned %ld\n", (long)crtn);
		goto errOut;
	}
	outBytes	   += thisMoveOut;
	outText->Data   = outtext;
	outText->Length = outBytes;
	crtn = kCCSuccess;
errOut:
	if(cryptor) {
		CCCryptorRelease(cryptor);
	}
	return crtn;
}

#define LOG_FREQ	20

static int doTest(
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
	CSSM_BOOL 			stagedEncr,
	CSSM_BOOL 			stagedDecr,
	CSSM_BOOL 			quiet,
	CSSM_BOOL			encryptOnly)
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
		keyData,
		ptext,
		&ctextRef);
	if(crtn) {
		return testError(quiet);
	}
	crtn = encryptDecryptCC(CSSM_TRUE,
		encrAlg,
		encrMode,
		padding,
		cipherBlockSize,
		stagedEncr,
		iv,
		keySizeInBits,
		keyData,
		ptext,
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
	crtn = encryptDecryptCC(CSSM_FALSE,
		encrAlg,
		encrMode,
		padding,
		cipherBlockSize,
		stagedDecr,
		iv,
		keySizeInBits,
		keyData,
		&ctextTest,
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
		const unsigned char *cp = (const unsigned char *)rptext.Data;
		printf("rptext %p: %02X %02X %02X %02X...\n", 
			cp, cp[0], cp[1], cp[2], cp[3]);
		rtn = testError(quiet);
	}
	else {
		rtn = 0;
	}
abort:
	if(ctextTest.Data) {
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
	uint32				blockSize;				// for noPadding case
	CSSM_DATA			keyData;
	CSSM_DATA			initVector;
	uint32				minTextSize;

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
	uint32		mode;
	uint32		padding;
	CSSM_BOOL	noPadding = CSSM_FALSE;
	CSSM_BOOL	encryptOnly = CSSM_FALSE;
	uint32		cipherBlockSize = 0;			// AES only, bits, 0 ==> default
	unsigned 	maxPtextSize = MAX_DATA_SIZE;
	unsigned	minPtextSize = MIN_DATA_SIZE;
	CSSM_BOOL	oneShotOnly = CSSM_FALSE;
	CSSM_BOOL	allZeroes = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					#if 0
					case 's':
						minAlg = maxAlg = ALG_ASC;
						break;
					#endif
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
						minAlg = maxAlg = ALG_AES_128;
						break;
					case 'n':
						minAlg = maxAlg = ALG_AES_192;
						noPadding = CSSM_TRUE;		// current restriction in
													// our reference implementation
						break;
					case 'A':
						minAlg = maxAlg = ALG_AES_256;
						noPadding = CSSM_TRUE;		// current restriction in
													// our reference implementation
						break;
					case 'b':
						minAlg = maxAlg = ALG_BFISH;
						noPadding = CSSM_TRUE;
						break;
					case 'c':
						minAlg = maxAlg = ALG_CAST;
						noPadding = CSSM_TRUE;
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
		    case 'b':
		    	cipherBlockSize = atoi(&argp[2]);
				break;
		    case 's':
		    	oneShotOnly = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
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
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'z':
		    	allZeroes = CSSM_TRUE;
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
				break;
			case 'o':
				noPadding = CSSM_TRUE;
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
	
	printf("Starting ccSymCompat; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	if(pauseInterval) {
		fpurge(stdin);
		printf("Top of test; hit CR to proceed: ");
		getchar();
	}
	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		/* some default values... */
		mode = CSSM_ALGMODE_NONE;
		padding = CSSM_PADDING_NONE;
		const char *padStr = "Off";
		const char *modeStr = "None";
		
		blockSize = 0;			// i.e., don't align
		switch(currAlg) {
			case ALG_DES:
				encrAlg = keyAlg = CSSM_ALGID_DES;
				algStr = "DES";
				if(noPadding) {
					mode = NO_PAD_MODE_BSAFE;
					modeStr = "CBC";
					blockSize = 8;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					modeStr = "CBC";
					padStr = "Off";
					padding = CSSM_PADDING_PKCS1;
				}
				break;
			case ALG_3DES:
				/* currently the only one with different key and encr algs */
				keyAlg  = CSSM_ALGID_3DES_3KEY;
				encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
				algStr = "3DES";
				if(noPadding) {
					mode = NO_PAD_MODE_BSAFE;
					modeStr = "CBC";
					blockSize = 8;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS1;
					modeStr = "CBC";
					padStr = "On";
				}
				break;
			case ALG_RC2:
				encrAlg = keyAlg = CSSM_ALGID_RC2;
				algStr = "RC2";
				if(noPadding) {
					mode = NO_PAD_MODE_BSAFE;
					modeStr = "CBC";
					blockSize = 8;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS1;		// what does padding do here?
					modeStr = "CBC";
					padStr = "On";
				}
				break;
			case ALG_RC4:
				encrAlg = keyAlg = CSSM_ALGID_RC4;
				algStr = "RC4";
				mode = CSSM_ALGMODE_NONE;
				break;
			case ALG_RC5:
				encrAlg = keyAlg = CSSM_ALGID_RC5;
				algStr = "RC5";
				if(noPadding) {
					mode = NO_PAD_MODE_BSAFE;
					modeStr = "CBC";
					blockSize = 8;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS1;		// eh?
					modeStr = "CBC";
					padStr = "On";
				}
				break;
			case ALG_AES_128:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES128";
				/* padding not supported in ref implementation */
				blockSize = 16;
				padStr = "Off";
				if(noPadding) {
					/* also means no CBC */
					mode = CSSM_ALGMODE_ECB;
					modeStr = "ECB";
				}
				else {
					mode = CSSM_ALGMODE_CBC_IV8;
					modeStr = "CBC";
					padStr = "Off";
				}
				effectKeySizeInBits = actKeySizeInBits = kCCKeySizeAES128 * 8;
				break;
			case ALG_AES_192:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES192";
				mode = NO_PAD_MODE;
				/* padding not supported in ref implementation */
				blockSize = 16;
				padStr = "Off";
				if(noPadding) {
					/* also means no CBC */
					mode = CSSM_ALGMODE_ECB;
					modeStr = "ECB";
				}
				else {
					mode = CSSM_ALGMODE_CBC_IV8;
					modeStr = "CBC";
					padStr = "Off";
				}
				effectKeySizeInBits = actKeySizeInBits = kCCKeySizeAES192 * 8;
				break;
			case ALG_AES_256:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES256";
				mode = NO_PAD_MODE;
				/* padding not supported in ref implementation */
				blockSize = 16;
				padStr = "Off";
				if(noPadding) {
					/* also means no CBC */
					mode = CSSM_ALGMODE_ECB;
					modeStr = "ECB";
				}
				else {
					mode = CSSM_ALGMODE_CBC_IV8;
					modeStr = "CBC";
					padStr = "Off";
				}
				effectKeySizeInBits = actKeySizeInBits = kCCKeySizeAES256 * 8;
				break;
			case ALG_BFISH:
				encrAlg = keyAlg = CSSM_ALGID_BLOWFISH;
				algStr = "Blowfish";
				/* libcrypt doesn't do padding */
				mode = CSSM_ALGMODE_CBC_IV8;
				blockSize = 8;
				modeStr = "CBC";
				break;
			case ALG_CAST:
				encrAlg = keyAlg = CSSM_ALGID_CAST;
				algStr = "CAST";
				/* libcrypt doesn't do padding */
				mode = CSSM_ALGMODE_CBC_IV8;
				modeStr = "CBC";
				blockSize = 8;
				break;
		}
		
		/* assume for now all algs require IV */
		initVector.Length = blockSize ? blockSize : 8;
		
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			minTextSize = minPtextSize;	// default
			if((blockSize != 0) && (minTextSize < blockSize)) {
				/* i.e., AES, adjust min ptext size */
				minTextSize = blockSize;
			}
			simpleGenData(&ptext, minTextSize, maxPtextSize);
			if(blockSize) {
				/* i.e., no padding --> align ptext */
				ptext.Length = (ptext.Length / blockSize) * blockSize;
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
				/*
				 * CommonCrypto's AES does one key size only 
				 */
				if(keyAlg != CSSM_ALGID_AES) {
					effectKeySizeInBits = randKeySizeBits(keyAlg, OT_Encrypt);
					/* 
					 * generate keys with well aligned sizes; effectiveKeySize 
					 * differs only if not well aligned 
					 */
					actKeySizeInBits = (effectKeySizeInBits + 7) & ~7;
				}
			}
			/* else constant, spec'd by user, may be 0 (default per alg) */
			/* mix up staging */
			if(oneShotOnly) {
				stagedEncr = CSSM_FALSE;
				stagedDecr = CSSM_FALSE;
			}
			else {
				stagedEncr = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
				stagedDecr = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
			}
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					if(cipherBlockSize) {
						printf("..loop %d text size %lu keySizeBits %u"
							" blockSize %u stagedEncr %d  stagedDecr %d mode %s pad %s\n",
							loop, (unsigned long)ptext.Length, (unsigned)effectKeySizeInBits,
							(unsigned)cipherBlockSize, (int)stagedEncr, (int)stagedDecr,
							modeStr, padStr);
					}
					else {
						printf("..loop %d text size %lu keySizeBits %u"
							" stagedEncr %d  stagedDecr %d mode %s pad %s\n",
							loop, (unsigned long)ptext.Length, (unsigned)effectKeySizeInBits,
							(int)stagedEncr, (int)stagedDecr, modeStr, padStr);
					}
				}
			}
			
			if(doTest(&ptext,
					&keyData,
					&initVector,
					keyAlg,
					encrAlg,
					mode,
					padding,
					actKeySizeInBits,
					actKeySizeInBits,		// FIXME - test effective key size
					cipherBlockSize,
					stagedEncr,
					stagedDecr,
					quiet,
					encryptOnly)) {
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


