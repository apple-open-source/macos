/* 
 * contextReuse.cpp 
 *
 * Verify proper operation of symmetric CSP algorithms when CSSM_CC_HANDLE 
 * (crypto context) is reused. Tests specifically for Radar 4551700, which 
 * dealt with a problem with the Gladman AES implementation handling the 
 * same context for an encrypt followed by a decrypt; other situations
 * are tested here (e.g. encrypt followed by another encrypt including CBC)
 * as well as all CSP symmetric algorithms. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
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
#define RAW_MODE_STREAM		CSSM_ALGMODE_NONE
#define COOKED_MODE			CSSM_ALGMODE_CBCPadIV8

#define RAW_MODE_STR		"ECB"
#define RAW_MODE_STREAM_STR	"None"
#define COOKED_MODE_STR		"CBC/Pad"

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_ASC = 1,			// not tested - no reference available
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
#define ALG_FIRST			ALG_ASC
#define ALG_LAST			ALG_CAST

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (d=DES; 3=3DES3; 2=RC2; 4=RC4; 5=RC5; a=AES; A=AES192; \n");
	printf("                6=AES256; b=Blowfish; c=CAST; s=ASC; default=all)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   k=keySizeInBits\n");
	printf("   m=maxPtextSize (default=%d)\n", MAX_DATA_SIZE);
	printf("   n=minPtextSize (default=%d)\n", MIN_DATA_SIZE);
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

#define LOG_STAGED_OPS				0
#if		LOG_STAGED_OPS
#define soprintf(s)	printf s
#else
#define soprintf(s)
#endif

/* 
 * Multipurpose encrypt. Like cspStagedEncrypt(), but it takes a
 * context handle and doesn't have as many options. 
 */
static CSSM_RETURN stagedEncrypt(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_CC_HANDLE 	cryptHand,
	uint32			algorithm,			// CSSM_ALGID_FEED, etc.
	uint32			cipherBlockSizeBytes,// optional
	const CSSM_DATA *iv,				// init vector, optional
	const CSSM_DATA *ptext,
	CSSM_DATA_PTR	ctext,				// mallocd by caller, must be big enough!
	CSSM_BOOL		multiUpdates)		// false:single update, true:multi updates
{
	CSSM_RETURN		crtn;
	CSSM_SIZE		bytesEncrypted;			// per update
	CSSM_SIZE		bytesEncryptedTotal = 0;
	CSSM_RETURN		ocrtn = CSSM_OK;		// 'our' crtn
	unsigned		toMove;					// remaining
	unsigned		thisMove;				// bytes to encrypt on this update
	CSSM_DATA		thisPtext;				// running ptr into ptext
	CSSM_DATA		thisCtext;				// running ptr into ctext
	CSSM_BOOL		restoreErr = CSSM_FALSE;
	CSSM_RETURN		savedErr = CSSM_OK;
	CSSM_SIZE		ctextLen;
	
	if(cipherBlockSizeBytes) {
		crtn = AddContextAttribute(cryptHand,
			CSSM_ATTRIBUTE_BLOCK_SIZE,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			cipherBlockSizeBytes);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			ocrtn = crtn;
			goto abort;
		}
	}
	
	thisPtext = *ptext;
	thisCtext = *ctext;
	memset(ctext->Data, 0, ctext->Length);
	ctextLen = ctext->Length;
	
	crtn = CSSM_EncryptDataInit(cryptHand);
	if(crtn) {
		printError("CSSM_EncryptDataInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	
	toMove = ptext->Length;
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			/* just do one pass thru this loop */
			thisMove = toMove;
		}
		thisPtext.Length = thisMove;
		crtn = CSSM_EncryptDataUpdate(cryptHand,
			&thisPtext,
			1,
			&thisCtext,
			1,
			&bytesEncrypted);
		if(crtn) {
			printError("CSSM_EncryptDataUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		soprintf(("*** EncryptDataUpdate: ptextLen 0x%x  bytesEncrypted 0x%x\n",
			(unsigned)thisMove, (unsigned)bytesEncrypted));

		// NOTE: We return the proper length in ctext....
		ctextLen            -= bytesEncrypted;		// bump out ptr
		thisCtext.Length     = ctextLen;
		thisCtext.Data      += bytesEncrypted;
		bytesEncryptedTotal += bytesEncrypted;
		thisPtext.Data      += thisMove;			// bump in ptr
		toMove              -= thisMove;
	}
	/* OK, one more */
	crtn = CSSM_EncryptDataFinal(cryptHand, &thisCtext);
	if(crtn) {
		printError("CSSM_EncryptDataFinal", crtn);
		savedErr = crtn;
		restoreErr = CSSM_TRUE;
		goto abort;
	}
	soprintf(("*** EncryptDataFinal: bytesEncrypted 0x%x\n",
		(unsigned)thisCtext.Length));
	bytesEncryptedTotal += thisCtext.Length;
	ctext->Length = bytesEncryptedTotal;
abort:
	if(restoreErr) {
		/* give caller the error from the encrypt */
		ocrtn = savedErr;
	}
	return ocrtn;
}

/* 
 * Multipurpose decrypt. Like cspStagedDecrypt(), but it takes a
 * context handle and doesn't have as many options. 
 */
CSSM_RETURN stagedDecrypt(
	CSSM_CSP_HANDLE cspHand,
	CSSM_CC_HANDLE 	cryptHand,
	uint32			algorithm,			// CSSM_ALGID_FEED, etc.
	uint32			cipherBlockSizeBytes,// optional
	const CSSM_DATA *iv,				// init vector, optional
	const CSSM_DATA *ctext,
	CSSM_DATA_PTR	ptext,				// mallocd by caller, must be big enough!
	CSSM_BOOL		multiUpdates)		// false:single update, true:multi updates
{
	CSSM_RETURN		crtn;
	CSSM_SIZE		bytesDecrypted;			// per update
	CSSM_SIZE		bytesDecryptedTotal = 0;
	CSSM_RETURN		ocrtn = CSSM_OK;		// 'our' crtn
	unsigned		toMove;					// remaining
	unsigned		thisMove;				// bytes to decrypt on this update
	CSSM_DATA		thisCtext;				// running ptr into ptext
	CSSM_DATA		thisPtext;				// running ptr into ctext
	CSSM_SIZE		ptextLen;
	
	if(cipherBlockSizeBytes) {
		crtn = AddContextAttribute(cryptHand,
			CSSM_ATTRIBUTE_BLOCK_SIZE,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			cipherBlockSizeBytes);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			ocrtn = crtn;
			goto abort;
		}
	}
	
	thisCtext = *ctext;
	thisPtext = *ptext;
	memset(ptext->Data, 0, ptext->Length);
	ptextLen = ptext->Length;
	
	crtn = CSSM_DecryptDataInit(cryptHand);
	if(crtn) {
		printError("CSSM_DecryptDataInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	
	toMove = ctext->Length;
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			/* just do one pass thru this loop */
			thisMove = toMove;
		}
		thisCtext.Length = thisMove;
		crtn = CSSM_DecryptDataUpdate(cryptHand,
			&thisCtext,
			1,
			&thisPtext,
			1,
			&bytesDecrypted);
		if(crtn) {
			printError("CSSM_DecryptDataUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		soprintf(("*** DecryptDataUpdate: ctextLen 0x%x  bytesDecrypted 0x%x\n",
			(unsigned)thisMove, (unsigned)bytesDecrypted));

		// NOTE: We return the proper length in ptext....
		ptextLen            -= bytesDecrypted;		// bump out ptr
		thisPtext.Length     = ptextLen;
		thisPtext.Data      += bytesDecrypted;
		bytesDecryptedTotal += bytesDecrypted;
		thisCtext.Data      += thisMove;			// bump in ptr
		toMove              -= thisMove;
	}
	/* OK, one more */
	crtn = CSSM_DecryptDataFinal(cryptHand, &thisPtext);
	if(crtn) {
		printError("CSSM_DecryptDataFinal", crtn);
		ocrtn = crtn;
		goto abort;
	}
	soprintf(("*** DecryptDataFinal: bytesEncrypted 0x%x\n",
		(unsigned)thisPtext.Length));
	bytesDecryptedTotal += thisPtext.Length;
	ptext->Length = bytesDecryptedTotal;
abort:
	return ocrtn;
}

static int doTest(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		*ptext,
	const CSSM_DATA		*ctext1,
	const CSSM_DATA		*ctext2,
	const CSSM_DATA		*rptext,
	const CSSM_DATA		*keyData,
	const CSSM_DATA		*iv,
	uint32 				keyAlg,					// CSSM_ALGID_xxx of the key
	uint32 				encrAlg,				// encrypt/decrypt
	uint32 				encrMode,
	uint32 				padding,
	uint32				keySizeInBits,
	uint32				cipherBlockSizeBytes,
	CSSM_BOOL 			quiet)
{
	CSSM_DATA		lctext1;
	CSSM_DATA		lctext2;
	CSSM_DATA		lrptext;
	int				rtn = 0;
	CSSM_RETURN		crtn;
	CSSM_CC_HANDLE	ccHand1 = 0;
	CSSM_CC_HANDLE	ccHand2 = 0;
	CSSM_KEY		key1;
	CSSM_KEY		key2;
	uint8			dummy[cipherBlockSizeBytes];
	CSSM_DATA		dummyData = {cipherBlockSizeBytes, dummy};
	
	/* 
	 * generate two equivalent keys key1 and key2;
	 * generate two CC handles ccHand1, ccHand2;
	 * encrypt dummy data with ccHand1 to get it cooked;
	 * encrypt ptext with ccHand1 ==> ctext1;
	 * encrypt ptext with ccHand2 ==> ctext2;
	 * Compare ctext1 and ctext2;
	 * decrypt ctext1 with ccHand1, compare with ptext;
	 */
	crtn = cspGenSymKeyWithBits(cspHand, keyAlg, CSSM_KEYUSE_ANY,
		keyData, keySizeInBits / 8, &key1);
	if(crtn) {
		return crtn;
	}
	crtn = cspGenSymKeyWithBits(cspHand, keyAlg, CSSM_KEYUSE_ANY,
		keyData, keySizeInBits / 8, &key2);
	if(crtn) {
		return crtn;
	}
	ccHand1 = genCryptHandle(cspHand, 
		encrAlg, 
		encrMode, 
		padding,
		&key1, 
		NULL,		// pubKey
		iv,
		0,			// effectiveKeySizeInBits
		0);			// rounds
	if(ccHand1 == 0) {
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	ccHand2 = genCryptHandle(cspHand, 
		encrAlg, 
		encrMode, 
		padding,
		&key2, 
		NULL,		// pubKey
		iv,
		0,			// effectiveKeySizeInBits
		0);			// rounds
	if(ccHand2 == 0) {
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	 
	/* dummy encrypt to heat up ccHand1 */
	appGetRandomBytes(dummy, sizeof(dummy));
	lctext1 = *ctext1;
	crtn = stagedEncrypt(cspHand, ccHand1, encrAlg, cipherBlockSizeBytes, 
		iv, &dummyData, &lctext1, CSSM_FALSE);
	if(crtn) {
		return crtn;
	}
	
	/* encrypt ptext with ccHand1 and ccHand2, compare ctext */
	lctext1 = *ctext1;
	crtn = stagedEncrypt(cspHand, ccHand1, encrAlg, cipherBlockSizeBytes, 
		iv, ptext, &lctext1, CSSM_TRUE);
	if(crtn) {
		return crtn;
	}
	lctext2 = *ctext2;
	crtn = stagedEncrypt(cspHand, ccHand2, encrAlg, cipherBlockSizeBytes, 
		iv, ptext, &lctext2, CSSM_TRUE);
	if(crtn) {
		return crtn;
	}
	if(!appCompareCssmData(&lctext1, &lctext2)) {
		printf("***Ciphertext miscompare\n");
		if(testError(quiet)) {
			return 1;
		}
	}

	/* decrypt with ccHand1, compare with ptext */
	lrptext = *rptext;
	crtn = stagedDecrypt(cspHand, ccHand1, encrAlg, cipherBlockSizeBytes, 
		iv, &lctext1, &lrptext, CSSM_TRUE);
	if(crtn) {
		return crtn;
	}
	if(!appCompareCssmData(&lctext1, &lctext2)) {
		printf("***Plaintext miscompare\n");
		if(testError(quiet)) {
			return 1;
		}
	}
	
	if(ccHand1) {
		CSSM_DeleteContext(ccHand1);
	}
	if(ccHand2) {
		CSSM_DeleteContext(ccHand2);
	}
	return rtn;
}


int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_DATA			ctext1;
	CSSM_DATA			ctext2;
	CSSM_DATA			rptext;
	CSSM_CSP_HANDLE 	cspHand;
	const char			*algStr;
	uint32				keyAlg;					// CSSM_ALGID_xxx of the key
	uint32				encrAlg;				// CSSM_ALGID_xxx of encr/decr
	unsigned			currAlg;				// ALG_xxx
	uint32				keySizeInBits;
	int					rtn = 0;
	CSSM_DATA			keyData;
	CSSM_DATA			initVector;
	uint32				minTextSize;
	uint32				rawMode;
	uint32				cookedMode;
	const char			*rawModeStr;
	const char			*cookedModeStr;
	uint32				algBlockSizeBytes;
	
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
	unsigned 	maxPtextSize = MAX_DATA_SIZE;
	unsigned	minPtextSize = MIN_DATA_SIZE;
	
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
					case 's':
						minAlg = maxAlg = ALG_ASC;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'k':
		    	keySizeInBits = atoi(&argp[2]);
		    	keySizeSpec = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
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
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
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
	appSetupCssmData(&ctext1, 2 * maxPtextSize);
	appSetupCssmData(&ctext2, 2 * maxPtextSize);
	appSetupCssmData(&rptext, 2 * maxPtextSize);
	
	keyData.Data = (uint8 *)CSSM_MALLOC(MAX_KEY_SIZE);
	if(keyData.Data == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	keyData.Length = MAX_KEY_SIZE;

	initVector.Data = (uint8 *)"someStrangeInitVect";	
	
	testStartBanner("contextReuse", argc, argv);

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
		padding		  = CSSM_PADDING_PKCS5;
		rawMode       = RAW_MODE;
		cookedMode    = COOKED_MODE;
		rawModeStr	  = RAW_MODE_STR;
		cookedModeStr = COOKED_MODE_STR;
		padding		  = CSSM_PADDING_PKCS5;
		
		switch(currAlg) {
			case ALG_DES:
				encrAlg = keyAlg = CSSM_ALGID_DES;
				algStr = "DES";
				algBlockSizeBytes  = 8;
				break;
			case ALG_3DES:
				/* currently the only one with different key and encr algs */
				keyAlg  = CSSM_ALGID_3DES_3KEY;
				encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
				algStr = "3DES";
				algBlockSizeBytes = 8;
				break;
			case ALG_RC2:
				encrAlg = keyAlg = CSSM_ALGID_RC2;
				algStr = "RC2";
				algBlockSizeBytes  = 8;
				break;
			case ALG_RC4:
				encrAlg = keyAlg = CSSM_ALGID_RC4;
				algStr = "RC4";
				algBlockSizeBytes = 0;
				rawMode       = RAW_MODE_STREAM;
				cookedMode    = RAW_MODE_STREAM;
				rawModeStr	  = RAW_MODE_STREAM_STR;
				cookedModeStr = RAW_MODE_STREAM_STR;
				break;
			case ALG_RC5:
				encrAlg = keyAlg = CSSM_ALGID_RC5;
				algStr = "RC5";
				algBlockSizeBytes = 8;
				break;
			case ALG_AES:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES";
				algBlockSizeBytes = 16;
				break;
			case ALG_AES192:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES192";
				algBlockSizeBytes = 24;
				break;
			case ALG_AES256:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES256";
				algBlockSizeBytes = 32;
				break;
			case ALG_BFISH:
				encrAlg = keyAlg = CSSM_ALGID_BLOWFISH;
				algStr = "Blowfish";
				algBlockSizeBytes = 8;
				break;
			case ALG_CAST:
				encrAlg = keyAlg = CSSM_ALGID_CAST;
				algStr = "CAST";
				algBlockSizeBytes = 8;
				break;
		}
		
		/* assume for now all algs require IV */
		initVector.Length = algBlockSizeBytes ? algBlockSizeBytes : 8;
		
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			/* mix up raw/cooked */
			uint32 mode;
			const char *modeStr;
			CSSM_BOOL paddingEnabled;
			
			if(loop & 1) {
				mode = rawMode;
				modeStr = rawModeStr;
			}
			else {
				mode = cookedMode;
				modeStr = cookedModeStr;
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
			if(!paddingEnabled && algBlockSizeBytes && (minTextSize < algBlockSizeBytes)) {
				/* i.e., no padding, adjust min ptext size */
				minTextSize = algBlockSizeBytes;
			}
			simpleGenData(&ptext, minTextSize, maxPtextSize);
			if(!paddingEnabled && algBlockSizeBytes) {
				/* align ptext */
				ptext.Length = (ptext.Length / algBlockSizeBytes) * algBlockSizeBytes;
			}
			
			simpleGenData(&keyData, MAX_KEY_SIZE, MAX_KEY_SIZE);

			if(!keySizeSpec) {
				/* random but byte-aligned */
				keySizeInBits = randKeySizeBits(keyAlg, OT_Encrypt);
				keySizeInBits = (keySizeInBits + 7) & ~7;
			}
			/* else constant, spec'd by user, may be 0 (default per alg) */
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					if(algBlockSizeBytes) {
						printf("..loop %d text size %lu keySizeBits %u"
							" blockSize %u mode %s\n",
							loop, (unsigned long)ptext.Length, (unsigned)keySizeInBits,
							(unsigned)algBlockSizeBytes, modeStr);
					}
					else {
						printf("..loop %d text size %lu keySizeBits %u"
							" mode %s\n",
							loop, (unsigned long)ptext.Length, (unsigned)keySizeInBits,
							modeStr);
					}
				}
			}
			
			if(doTest(cspHand,
					&ptext,
					&ctext1,
					&ctext2,
					&rptext,
					&keyData,
					&initVector,
					keyAlg,
					encrAlg,
					mode,
					padding,
					keySizeInBits,
					algBlockSizeBytes,
					quiet)) {
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


