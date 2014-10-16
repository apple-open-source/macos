/*
 * CSP symmetric encryption performance measurement tool
 * Based on Michael Brouwer's cryptoPerformance.cpp; this one does not 
 * use CssmClient. 
 *
 */
#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <string.h>
#include "cspwrap.h"
#include "common.h"
#include <iomanip>
#include <iostream>
#include <memory>

using namespace std;

/*
 * Default values
 */
#define ALG_DEFAULT				CSSM_ALGID_AES
#define ALG_STR_DEFAULT			"AES"
#define CHAIN_DEFAULT			CSSM_TRUE
#define ALG_MODE_DEFAULT		CSSM_ALGMODE_CBC_IV8
#define ALG_MODE_STR_DEFAULT	"CBC"
#define KEY_SIZE_DEFAULT		128
#define BLOCK_SIZE_DEFAULT		16

static void usage(char **argv)
{
	printf("usage: %s iterations bufsize [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (s=ASC; d=DES; 3=3DES; 2=RC2; 4=RC4; 5=RC5;\n");
	printf("     a=AES; b=Blowfish; c=CAST; n=NULL; default=AES)\n");
	printf("   k=keySizeInBits\n");
	printf("   e (ECB mode; default is CBC)\n");
	printf("   c (re-create context in each loop)\n");
	printf("   v(erbose)\n");
	printf("   h(elp)\n");
	exit(1);
}

static int doEncrypt(
	CSSM_CSP_HANDLE cspHand, 
	CSSM_KEY_PTR symKey,
	CSSM_BOOL resetContext,
	unsigned iterations,
	uint8 blockSizeBytes,
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	const CSSM_DATA *ptext,		// pre-allocd
	CSSM_DATA *ctext)
{
	CSSM_CC_HANDLE ccHand = 0;
	char *someIv = "some Initialization vector";
	CSSM_DATA iv = {blockSizeBytes, (uint8 *)someIv};
	CSSM_DATA remData = {0, NULL};
	CSSM_SIZE moved;
	CSSM_RETURN crtn;
	
	for(unsigned dex=0; dex<iterations; dex++) {
		if(ccHand == 0) {
			/* always the first time, and each loop if resetContext */
			crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				encrAlg,
				encrMode,
				NULL,			// access cred
				symKey,
				&iv,			// InitVector
				CSSM_PADDING_NONE,	
				NULL,			// Params
				&ccHand);
			if(crtn) {
				cssmPerror("CSSM_CSP_CreateSymmetricContext", crtn);
				return -1;
			}
		}
		crtn = CSSM_EncryptData(ccHand, ptext, 1,
			ctext, 1, &moved, &remData);
		if(crtn) {
			cssmPerror("CSSM_EncryptData", crtn);
			return -1;
		}
		if(resetContext) {
			CSSM_DeleteContext(ccHand);
			ccHand = 0;
		}
	}
	if(ccHand != 0) {
		CSSM_DeleteContext(ccHand);
	}
	return 0;
}

static int doDecrypt(
	CSSM_CSP_HANDLE cspHand, 
	CSSM_KEY_PTR symKey,
	CSSM_BOOL resetContext,
	unsigned iterations,
	uint8 blockSizeBytes,
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	const CSSM_DATA *ctext,		// pre-allocd
	CSSM_DATA *rptext)
{
	CSSM_CC_HANDLE ccHand = 0;
	char *someIv = "some Initialization vector";
	CSSM_DATA iv = {blockSizeBytes, (uint8 *)someIv};
	CSSM_DATA remData = {0, NULL};
	CSSM_SIZE moved;
	CSSM_RETURN crtn;
	
	for(unsigned dex=0; dex<iterations; dex++) {
		if(ccHand == 0) {
			/* always the first time, and each loop if resetContext */
			crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				encrAlg,
				encrMode,
				NULL,			// access cred
				symKey,
				&iv,			// InitVector
				CSSM_PADDING_NONE,	
				NULL,			// Params
				&ccHand);
			if(crtn) {
				cssmPerror("CSSM_CSP_CreateSymmetricContext", crtn);
				return -1;
			}
		}
		crtn = CSSM_DecryptData(ccHand, ctext, 1,
			rptext, 1, &moved, &remData);
		if(crtn) {
			cssmPerror("CSSM_DecryptData", crtn);
			return -1;
		}
		if(resetContext) {
			CSSM_DeleteContext(ccHand);
			ccHand = 0;
		}
	}
	CSSM_FreeKey(cspHand, NULL, symKey, CSSM_FALSE);
	if(ccHand != 0) {
		CSSM_DeleteContext(ccHand);
	}
	return 0;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	CSSM_ENCRYPT_MODE	mode = ALG_MODE_DEFAULT;
	char				*modeStr = ALG_MODE_STR_DEFAULT;
	uint32				blockSizeBytes = BLOCK_SIZE_DEFAULT;
	
	/*
	 * User-spec'd params
	 */
	CSSM_BOOL			chainEnable = CHAIN_DEFAULT;
	uint32				keySizeInBits = KEY_SIZE_DEFAULT;
	char				*algStr = ALG_STR_DEFAULT;
	uint32				keyAlg = ALG_DEFAULT;		// CSSM_ALGID_xxx of the key
	uint32				encrAlg = ALG_DEFAULT;		// CSSM_ALGID_xxx for encrypt
	int 				iterations;
	int 				bufSize;
	CSSM_BOOL			resetContext = CSSM_FALSE;
	CSSM_BOOL			verbose = false;
	
	if(argc < 3) {
		usage(argv);
	}
	iterations = atoi(argv[1]);
	bufSize = atoi(argv[2]);
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				/* only set mode and modeStr if not default (CBC) */
				switch(argp[2]) {
					case 's':
						encrAlg = keyAlg = CSSM_ALGID_ASC;
						algStr = "ASC";
						mode = CSSM_ALGMODE_NONE;
						modeStr = "NONE";
						blockSizeBytes = 0;
						break;
					case 'd':
						encrAlg = keyAlg = CSSM_ALGID_DES;
						algStr = "DES";
						keySizeInBits = 64;
						blockSizeBytes = 8;
						break;
					case '3':
						keyAlg  = CSSM_ALGID_3DES_3KEY;
						encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
						algStr = "3DES";
						keySizeInBits = 64 * 3;
						blockSizeBytes = 8;
						break;
					case '2':
						encrAlg = keyAlg = CSSM_ALGID_RC2;
						algStr = "RC2";
						blockSizeBytes = 8;
						break;
					case '4':
						encrAlg = keyAlg = CSSM_ALGID_RC4;
						algStr = "RC4";
						/* not a block cipher */
						chainEnable = CSSM_FALSE;
						mode = CSSM_ALGMODE_NONE;
						modeStr = "NONE";
						blockSizeBytes = 0;
						break;
					case '5':
						encrAlg = keyAlg = CSSM_ALGID_RC5;
						algStr = "RC5";
						blockSizeBytes = 8;
						break;
					case 'a':
						encrAlg = keyAlg = CSSM_ALGID_AES;
						algStr = "AES";
						blockSizeBytes = 16;
						break;
					case 'b':
						encrAlg = keyAlg = CSSM_ALGID_BLOWFISH;
						algStr = "Blowfish";						
						blockSizeBytes = 8;
						break;
					case 'c':
						encrAlg = keyAlg = CSSM_ALGID_CAST;
						algStr = "CAST";
						blockSizeBytes = 8;
						break;
					case 'n':
						encrAlg = keyAlg = CSSM_ALGID_NONE;
						algStr = "NULL";
						blockSizeBytes = 8;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'k':
		    	keySizeInBits = atoi(&argp[2]);
				break;
			case 'e':
				chainEnable = CSSM_FALSE;
				break;
			case 'c':
				resetContext = CSSM_TRUE;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	if(!chainEnable) {
		switch(mode) {
			case CSSM_ALGMODE_CBC_IV8:
				mode = CSSM_ALGMODE_ECB;
				modeStr = "ECB";
				break;
			case CSSM_ALGMODE_NONE:
				/* stream cipher, we weren't running CBC anyway */
			default:
				break;
		}
	}
	
	printf("Algorithm: %s   keySize: %u  mode: %s  iterations: %d  "
		"bufSize %d\n",
		algStr, (unsigned)keySizeInBits, modeStr, iterations, bufSize);
	
	CSSM_CSP_HANDLE cspHand = cspStartup();
	CSSM_DATA ptext;
	CSSM_DATA ctext;
	CSSM_DATA rptext;
	
	appSetupCssmData(&ptext, bufSize);
	appSetupCssmData(&ctext, bufSize);
	appSetupCssmData(&rptext, bufSize);
	simpleGenData(&ptext, ptext.Length, ptext.Length);
	
	CSSM_KEY_PTR symKey = cspGenSymKey(cspHand, keyAlg, 
		"someLabel", 9,
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
		keySizeInBits,
		CSSM_TRUE);
	if(symKey == NULL) {
		printf("***Error generating symmetric key\n");
		exit(1);
	}

	CFAbsoluteTime start, end;
	int rtn;
	
	printf("  %d * cdsaEncrypt %d bytes", iterations, bufSize);
	fflush(stdout);
	start = CFAbsoluteTimeGetCurrent();
	rtn = doEncrypt(cspHand, symKey, resetContext, iterations,
		blockSizeBytes, encrAlg, mode, &ptext, &ctext);
	end = CFAbsoluteTimeGetCurrent();
	if(rtn) {
		exit(1);
	}
	printf(" took: %gs %.1f Kbytes/s\n", end - start,
		(iterations * bufSize) / (end - start) / 1024.0);

	printf("  %d * cdsaDecrypt %d bytes", iterations, bufSize);
	fflush(stdout);
	start = CFAbsoluteTimeGetCurrent();
	rtn = doDecrypt(cspHand, symKey, resetContext, iterations,
		blockSizeBytes, encrAlg, mode, &ctext, &rptext);
	end = CFAbsoluteTimeGetCurrent();
	if(rtn) {
		exit(1);
	}
	printf(" took: %gs %.1f Kbytes/s\n", end - start,
		(iterations * bufSize) / (end - start) / 1024.0);

	return 0;
}
