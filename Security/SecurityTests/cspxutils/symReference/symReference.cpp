/* 
 * symReference.c - write keys and ciphertext blobs, read them back
 *                  and decrypt on (possibly) a different platfrom.
 *					Intended for use in testing multiplatform
 *					compatibility (e.g. encrypt on 32 bit G4, decrypt
 *					on 64-bit G5). 
 *
 * Created by Doug Mitchell 10/31/05. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include <security_cdsa_utils/cuFileIo.h>
#include "common.h"
#include <string.h>
#include "cspdlTesting.h"
#include <unistd.h>

/*
 * Defaults.
 */
#define LOOPS_DEF		200
#define PTEXT_SIZE_DEF	256
#define BLOCK_SIZE_MAX	32		/* bytes */

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_ASC = 0,		/* first must be 0 */
	ALG_DES,
	ALG_RC2,
	ALG_RC4,
	ALG_RC5,
	ALG_3DES,
	ALG_AES,
	ALG_AES192,
	ALG_AES256,
	ALG_BFISH,
	ALG_CAST
} SymAlg;

#define ALG_FIRST			ALG_ASC
#define ALG_LAST			ALG_CAST

static void usage(char **argv)
{
	printf("usage: %s e|d dirName [options]\n", argv[0]);
	printf("  e=encrypt, d=decrypt; blobs read/written in dirName\n");
	printf("   Options:\n");
	printf("   a=algorithm (d=DES; 3=3DES3; 2=RC2; 4=RC4; 5=RC5; a=AES; b=Blowfish; \n");
	printf("                c=CAST; s=ASC, default=all)\n");
	printf("   p=ptextSize (default=%d)\n", PTEXT_SIZE_DEF);
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * map SymAlg to test params
 */
typedef struct {
	SymAlg				alg;
	const char			*algStr;
	CSSM_ALGORITHMS		cssmAlg;
	CSSM_ENCRYPT_MODE	mode;
	CSSM_PADDING		padding;
	CSSM_SIZE			keySizeBits;
	CSSM_SIZE			ivLen;		// in bytes
} SymAlgParams;

static const SymAlgParams symAlgParams[] = 
{
	{ ALG_ASC, "ASC", CSSM_ALGID_ASC, CSSM_ALGMODE_NONE, CSSM_PADDING_NONE, 
		CSP_ASC_KEY_SIZE_DEFAULT, 0 },
	{ ALG_DES, "DES", CSSM_ALGID_DES, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		CSP_DES_KEY_SIZE_DEFAULT, 8 },
	{ ALG_RC2, "RC2", CSSM_ALGID_RC2, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		CSP_RC2_KEY_SIZE_DEFAULT, 8 },
	{ ALG_RC4, "RC4", CSSM_ALGID_RC4, CSSM_ALGMODE_NONE, CSSM_PADDING_NONE, 
		CSP_RC4_KEY_SIZE_DEFAULT, 0 },
	{ ALG_RC5, "RC5", CSSM_ALGID_RC5, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		CSP_RC5_KEY_SIZE_DEFAULT, 8 },
	{ ALG_3DES, "3DES", CSSM_ALGID_3DES_3KEY_EDE, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		CSP_DES3_KEY_SIZE_DEFAULT, 8 },
	{ ALG_AES, "AES", CSSM_ALGID_AES, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		CSP_AES_KEY_SIZE_DEFAULT, 16 },
	{ ALG_AES192, "AES192", CSSM_ALGID_AES, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		192, 24 },
	{ ALG_AES256, "AES256", CSSM_ALGID_AES, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		256, 32 },
	{ ALG_BFISH, "Blowfish", CSSM_ALGID_BLOWFISH, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5,
		CSP_BFISH_KEY_SIZE_DEFAULT,  8 },
	{ ALG_CAST, "CAST", CSSM_ALGID_CAST, CSSM_ALGMODE_CBCPadIV8, CSSM_PADDING_PKCS5, 
		CSP_CAST_KEY_SIZE_DEFAULT,  8 }
};

static void genFileNames(
	const char	*algStr,
	char		*keyFile,
	char		*ptextFile,
	char		*ctextFile,
	char		*ivFile)
{
	sprintf(keyFile,	"key_%s", algStr);
	sprintf(ptextFile,	"ptext_%s", algStr);
	sprintf(ctextFile,	"ctext_%s", algStr);
	sprintf(ivFile,		"iv_%s", algStr);
}
	
/* encrypt, write blobs (key, plaintext, ciphertext, optional IV) to disk */
static int doEncrypt(
	CSSM_CSP_HANDLE		cspHand,
	const SymAlgParams	*algParams,
	CSSM_DATA			*ptext,		// mallocd, length valid, we fill data
	CSSM_BOOL			quiet,
	CSSM_BOOL			verbose)
{
	CSSM_KEY_PTR	symKey = NULL;
	CSSM_KEY		rawKey;
	CSSM_RETURN		crtn;
	CSSM_DATA		ctext = {0, NULL};
	uint8			iv[BLOCK_SIZE_MAX];
	CSSM_DATA		ivd = {BLOCK_SIZE_MAX, iv};
	CSSM_DATA		*ivp = NULL;
	uint32			blockSize = 0;
	char			keyFile[FILENAME_MAX];
	char			ptextFile[FILENAME_MAX];
	char			ctextFile[FILENAME_MAX];
	char			ivFile[FILENAME_MAX];
	
	if(!quiet) {
		printf("...encrypting, alg %s\n", algParams->algStr);
	}

	/* generate reference key (works with CSPDL) */
	symKey = cspGenSymKey(cspHand, algParams->cssmAlg,
		"noLabel", 7,
		CSSM_KEYUSE_ANY, algParams->keySizeBits, CSSM_TRUE);
	if(symKey == NULL) {
		printf("***Error generating key for alg %s size %u bits\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}
	
	/* get key in raw format (to get the raw blob we write to disk) */
	crtn = cspRefKeyToRaw(cspHand, symKey, &rawKey);
	if(crtn) {
		printf("***Error generating raw key for alg %s size %u bits\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}
	
	appGetRandomBytes(ptext->Data, (unsigned)ptext->Length);
	
	/* 
	 * Hack: we only need to specify block size for AES192 and AES256, which 
	 * we detect by their having an ivLen of greater than 16.
	 */
	if(algParams->ivLen > 16) {
		blockSize = algParams->ivLen;
	}
	if(algParams->ivLen) {
		appGetRandomBytes(iv, algParams->ivLen);
		ivd.Length = algParams->ivLen;
		ivp = &ivd;
	}
	
	crtn = cspStagedEncrypt(cspHand,
		algParams->cssmAlg, algParams->mode, algParams->padding,
		symKey, NULL,
		0, blockSize, 0,
		ivp, ptext,
		&ctext, 
		CSSM_FALSE);
	if(crtn) {
		printf("***Error encrypting for alg %s size %u bits\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}

	/* write: key, IV, ptext, ctext */
	genFileNames(algParams->algStr, keyFile, ptextFile, ctextFile, ivFile);
	if(writeFile(keyFile, rawKey.KeyData.Data, (unsigned)rawKey.KeyData.Length) ||
	   writeFile(ptextFile, ptext->Data, (unsigned)ptext->Length) ||
	   writeFile(ctextFile, ctext.Data, (unsigned)ctext.Length)) {
		printf("***Error writing result of alg %s size %u bits\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}
	if(ivp != NULL) {
		if(writeFile(ivFile, ivp->Data, (unsigned)ivp->Length)) {
			printf("***Error writing IV for alg %s size %u bits\n",
				algParams->algStr, (unsigned)algParams->keySizeBits);
			return testError(quiet);
		}
	}
	
	/* Free resources */
	CSSM_FreeKey(cspHand, NULL, symKey, CSSM_FALSE);
	CSSM_FreeKey(cspHand, NULL, &rawKey, CSSM_FALSE);
	CSSM_FREE(ctext.Data);
	return 0;
}

/* read blobs (key, plaintext, ciphertext, optional IV) from disk, decrypt, compare plaintext */
static int doDecrypt(
	CSSM_CSP_HANDLE		cspHand,
	const SymAlgParams	*algParams,
	CSSM_BOOL			quiet,
	CSSM_BOOL			verbose)
{
	CSSM_KEY		symKey;
	uint8			*symKeyBits;
	unsigned		symKeyLen;				// in bytes
	CSSM_DATA		symKeyData;
	CSSM_RETURN		crtn;
	uint8			*ctextChars;
	unsigned		ctextLen = 0;
	CSSM_DATA		ctext;
	CSSM_DATA		rptext = {0, NULL};		// recovered/decrytped
	uint8			*refPTextChars;
	unsigned		refPtextLen;
	CSSM_DATA		refPtext = {0, NULL};	// expected
	uint8			*iv = NULL;
	unsigned		ivLen;
	CSSM_DATA		ivd = {BLOCK_SIZE_MAX, iv};
	CSSM_DATA		*ivp = NULL;
	uint32			blockSize = 0;
	char			keyFile[FILENAME_MAX];
	char			ptextFile[FILENAME_MAX];
	char			ctextFile[FILENAME_MAX];
	char			ivFile[FILENAME_MAX];
	
	if(!quiet) {
		printf("...decrypting, alg %s\n", algParams->algStr);
	}
	
	/* 
	 * Hack: we only need to specify block size for AES192 and AES256, which 
	 * we detect by their having an ivLen of greater than 16.
	 */
	if(algParams->ivLen > 16) {
		blockSize = algParams->ivLen;
	}
	if(algParams->ivLen) {
		ivp = &ivd;
		ivd.Length = algParams->ivLen;
	}

	/* read: key, IV, ptext, ctext */
	genFileNames(algParams->algStr, keyFile, ptextFile, ctextFile, ivFile);
	if(readFile(keyFile, &symKeyBits, &symKeyLen) ||
	   readFile(ptextFile, &refPTextChars, &refPtextLen) ||
	   readFile(ctextFile, &ctextChars, &ctextLen)) {
		printf("***Error reading reference blobs for alg %s size %u bits\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}
	if(ivp != NULL) {
		if(readFile(ivFile, &iv, &ivLen)) {
			printf("***Error writing IV for alg %s size %u bits\n",
				algParams->algStr, (unsigned)algParams->keySizeBits);
			return testError(quiet);
		}
		if(ivLen != algParams->ivLen) {
			printf("***Unexpected IV length: expect %u found %u\n",
				(unsigned)algParams->ivLen, (unsigned)ivLen);
			if(testError(quiet)) {
				return 1;
			}
		}
		ivd.Data = iv;
	}
	ctext.Data = ctextChars;
	ctext.Length = ctextLen;
	refPtext.Data = refPTextChars;
	refPtext.Length = refPtextLen;
	
	/* generate key */
	symKeyData.Data = symKeyBits;
	symKeyData.Length = symKeyLen;
	
	crtn = cspGenSymKeyWithBits(cspHand, algParams->cssmAlg,
		CSSM_KEYUSE_ANY, &symKeyData, symKeyLen, &symKey);
	if(crtn) {
		printf("***Error creating key for alg %s keySize %u\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}
	
	crtn = cspStagedDecrypt(cspHand,
		algParams->cssmAlg, algParams->mode, algParams->padding,
		&symKey, NULL,
		0, blockSize, 0,
		ivp, &ctext,
		&rptext, 
		CSSM_FALSE);
	if(crtn) {
		printf("***Error decrypting for alg %s size %u bits\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}

	/* moment of truth */
	if(!appCompareCssmData(&rptext, &refPtext)) {
		printf("***DATA MISCOMPARE AFTER DECRYPT alg %s size %u bits\n",
			algParams->algStr, (unsigned)algParams->keySizeBits);
		return testError(quiet);
	}
	
	/* Free resources */
	CSSM_FreeKey(cspHand, NULL, &symKey, CSSM_FALSE);
	free(symKeyBits);		// mallocd by readFile()
	free(refPTextChars);
	free(ctextChars);
	CSSM_FREE(rptext.Data);	// mallocd by CSP
	if(iv) {
		free(iv);
	}
	return 0;
}


int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	unsigned			currAlg;				// ALG_xxx
	int					rtn = 0;

	/*
	 * User-spec'd params
	 */
	unsigned	minAlg = ALG_FIRST;
	unsigned	maxAlg = ALG_LAST;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	quiet = CSSM_FALSE;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	bool		encrypt = false;
	unsigned	ptextSize = PTEXT_SIZE_DEF;
	char		*dirName;
		
	if(argc < 3) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'e':
			encrypt = true;
			break;
		case 'd':
			encrypt = false;
			break;
		default:
			usage(argv);
	}
	dirName = argv[2];
	
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 's':
						minAlg = maxAlg = ALG_ASC;
						break;
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
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
			case 'p':
				ptextSize = atoi(&argp[2]);
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext.Data = (uint8 *)CSSM_MALLOC(ptextSize);
	if(ptext.Data == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	ptext.Length = ptextSize;

	testStartBanner("symReference", argc, argv);

	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}

	if(chdir(dirName)) {
		perror(dirName);
		printf("Error accessing directory %s. Aborting.\n", dirName);
		exit(1);
	}
	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		const SymAlgParams *algParams = &symAlgParams[currAlg];

		if(encrypt) {
			rtn = doEncrypt(cspHand, algParams, &ptext, quiet, verbose);
		}
		else {
			rtn = doDecrypt(cspHand, algParams, quiet, verbose);
		}
		if(rtn) {
			break;
		}
	}	/* for algs */
	
	cspShutdown(cspHand, bareCsp);
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	CSSM_FREE(ptext.Data);
	return rtn;
}


