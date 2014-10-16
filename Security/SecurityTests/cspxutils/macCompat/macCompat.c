/* 
 * macCompat.c - test compatibilty of two different implementations of a
 * given MAC algorithm - one in the standard AppleCSP,
 * one in BSAFE.
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
#include <string.h>
#include "cspdlTesting.h"
#include <openssl/hmac.h>

/*
 * Defaults.
 */
#define LOOPS_DEF		200
#define MIN_EXP			2		/* for data size 10**exp */
#define DEFAULT_MAX_EXP	4
#define MAX_EXP			5

#define MAX_DATA_SIZE	(100000 + 100)		/* bytes */
#define MIN_KEY_SIZE	20					/* bytes - should be smaller */
#define MAX_KEY_SIZE	64					/* bytes */
#define LOOP_NOTIFY		20

/*
 * Enumerate algs our own way to allow iteration.
 */
#define ALG_MD5				1
#define ALG_SHA1			2
#define ALG_SHA1_LEGACY		3
#define ALG_FIRST			ALG_MD5
#define ALG_LAST			ALG_SHA1_LEGACY

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=%d, max=%d)\n", DEFAULT_MAX_EXP, MAX_EXP);
	printf("   k=keySizeInBytes\n");
	printf("   P=plainTextLen\n");
	printf("   z (keys and plaintext all zeroes)\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* 
 * generate MAC using reference BSAFE with either one update
 * (updateSizes == NULL) or specified set of update sizes.
 */
static CSSM_RETURN genMacBSAFE(
	CSSM_ALGORITHMS		macAlg,
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	unsigned			*updateSizes,		// NULL --> random updates
											// else null-terminated list of sizes
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	CSSM_RETURN crtn;
	BU_KEY buKey;
	
	crtn = buGenSymKey(key->Length * 8, key, &buKey);
	if(crtn) {
		return crtn;
	}
	crtn = buGenMac(buKey,
		macAlg,
		inText,
		updateSizes,
		outText);
	buFreeKey(buKey);
	return crtn;
}

/* 
 * Produce HMACMD5 with openssl.
 */
static int doHmacMD5Ref(
	const CSSM_DATA		*key,				// raw key bytes
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	const EVP_MD *md = EVP_md5();	
	unsigned md_len = 16;
	appSetupCssmData(outText, 16);
	HMAC(md, key->Data, (int)key->Length, 
		inText->Data, inText->Length,
		(unsigned char *)outText->Data, &md_len);
	return 0;
}

/*
 * Generate MAC, CSP, specified set of update sizes
 */
static CSSM_RETURN cspGenMacWithSizes(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,		
		CSSM_KEY_PTR key,					// session key
		const CSSM_DATA *text,
		unsigned *updateSizes,				// null-terminated list of sizes
		CSSM_DATA_PTR mac)					// RETURNED
{
	CSSM_CC_HANDLE	macHand;
	CSSM_RETURN		crtn;
	CSSM_DATA		currData = *text;
	
	crtn = CSSM_CSP_CreateMacContext(cspHand,
		algorithm,
		key,
		&macHand);
	if(crtn) {
		printError("CSSM_CSP_CreateMacContext", crtn);
		return crtn;
	}
	crtn = CSSM_GenerateMacInit(macHand);
	if(crtn) {
		printError("CSSM_GenerateMacInit", crtn);
		goto abort;
	}
	/* CSP mallocs */
	mac->Data = NULL;
	mac->Length = 0;
	
	while(*updateSizes) {
		currData.Length = *updateSizes;
		crtn = CSSM_GenerateMacUpdate(macHand,
			&currData,
			1);
		if(crtn) {
			printError("CSSM_GenerateMacUpdate", crtn);
			goto abort;
		}
		currData.Data += *updateSizes;
		updateSizes++;
	}
	crtn = CSSM_GenerateMacFinal(macHand, mac);
	if(crtn) {
		printError("CSSM_GenerateMacFinal", crtn);
	}
abort:
	crtn = CSSM_DeleteContext(macHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
	}
	return crtn;
}

/*
 * Verify MAC, CSP, specified set of update sizes
 */
static CSSM_RETURN cspVfyMacWithSizes(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,		
		CSSM_KEY_PTR key,					// session key
		const CSSM_DATA *text,
		unsigned *updateSizes,				// null-terminated list of sizes
		const CSSM_DATA *mac)	
{
	CSSM_CC_HANDLE	macHand;
	CSSM_RETURN		crtn;
	CSSM_DATA		currData = *text;
	
	crtn = CSSM_CSP_CreateMacContext(cspHand,
		algorithm,
		key,
		&macHand);
	if(crtn) {
		printError("CSSM_CSP_CreateMacContext", crtn);
		return crtn;
	}
	crtn = CSSM_VerifyMacInit(macHand);
	if(crtn) {
		printError("CSSM_VerifyMacInit", crtn);
		goto abort;
	}
	
	while(*updateSizes) {
		currData.Length = *updateSizes;
		crtn = CSSM_VerifyMacUpdate(macHand,
			&currData,
			1);
		if(crtn) {
			printError("CSSM_GenerateMacUpdate", crtn);
			goto abort;
		}
		currData.Data += *updateSizes;
		updateSizes++;
	}
	crtn = CSSM_VerifyMacFinal(macHand, mac);
	if(crtn) {
		printError("CSSM_GenerateMacFinal", crtn);
	}
abort:
	crtn = CSSM_DeleteContext(macHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
	}
	return crtn;
}

/*
 * Generate or verify MAC using CSP with either random-sized staged updates
 * (updateSizes == NULL) or specified set of update sizes. 
 */
static CSSM_RETURN genMacCSSM(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_ALGORITHMS		macAlg,
	CSSM_ALGORITHMS		keyAlg,
	CSSM_BOOL			doGen,
	const CSSM_DATA		*key,				// raw key bytes
	CSSM_BOOL			genRaw,				// first generate raw key (CSPDL)
	const CSSM_DATA		*inText,
	unsigned			*updateSizes,		// NULL --> random updates
											// else null-terminated list of sizes
	CSSM_DATA_PTR 		outText)			// mallocd and returned if doGen
{
	CSSM_KEY_PTR		symKey;
	CSSM_KEY			refKey;				// in case of genRaw
	CSSM_BOOL			refKeyGenerated = CSSM_FALSE;
	CSSM_RETURN			crtn;
	
	if(genRaw) {
		crtn = cspGenSymKeyWithBits(cspHand,
			keyAlg,
			CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY,
			key,
			key->Length,
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
			CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY,
			key->Length * 8,
			CSSM_FALSE);			// ref key
		if(symKey == NULL) {
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
		if(symKey->KeyData.Length != key->Length) {
			printf("***Generated key size error (exp %lu, got %lu)\n",
				key->Length, symKey->KeyData.Length);
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
		memmove(symKey->KeyData.Data, key->Data, key->Length);
	}
	if(doGen) {
		/* CSP mallocs */
		outText->Data = NULL;
		outText->Length = 0;
	}

	/* go for it */
	if(doGen) {
		if(updateSizes) {
			crtn = cspGenMacWithSizes(cspHand,
				macAlg,
				symKey,
				inText, 
				updateSizes,
				outText);
		}
		else {
			crtn = cspStagedGenMac(cspHand,
				macAlg,
				symKey,
				inText, 
				CSSM_TRUE,		// multiUpdates
				CSSM_FALSE,		// mallocMac
				outText);
		}
	}
	else {
		if(updateSizes) {
			crtn = cspVfyMacWithSizes(cspHand,
				macAlg,
				symKey,
				inText,
				updateSizes, 
				outText);
		}
		else {
			crtn = cspMacVerify(cspHand,
				macAlg,
				symKey,
				inText, 
				outText,
				CSSM_OK);
		}
	}
	cspFreeKey(cspHand, symKey);
	if(!refKeyGenerated) {
		/* key itself mallocd by cspGenSymKey */
		CSSM_FREE(symKey);
	}
	return crtn;
}

#define LOG_FREQ			20
#define MAX_FIXED_UPDATES	5

static int doTest(CSSM_CSP_HANDLE cspHand,
	const CSSM_DATA		*ptext,
	const CSSM_DATA		*keyData,
	CSSM_BOOL			genRaw,				// first generate raw key (CSPDL)
	uint32 				macAlg,
	uint32 				keyAlg,
	CSSM_BOOL			fixedUpdates,		// for testing CSSM_ALGID_SHA1HMAC_LEGACY 
	CSSM_BOOL 			quiet)
{
	CSSM_DATA 		macRef = {0, NULL};			// MAC, BSAFE reference
	CSSM_DATA		macTest = {0, NULL};		// MAC, CSP test
	int				rtn = 0;
	CSSM_RETURN		crtn;
	unsigned		updateSizes[MAX_FIXED_UPDATES+1];
	unsigned		*updateSizesPtr;
	
	if(fixedUpdates) {
		/* calculate up to MAX_FIXED_UPDATES update sizes which add up to
		 * ptext->Length */
		int i;
		unsigned bytesToGo = ptext->Length;
		
		memset(updateSizes, 0, sizeof(unsigned) * (MAX_FIXED_UPDATES+1));
		for(i=0; i<MAX_FIXED_UPDATES; i++) {
			updateSizes[i] = genRand(1, bytesToGo);
			bytesToGo -= updateSizes[i];
			if(bytesToGo == 0) {
				break;
			}
		}
		updateSizesPtr = updateSizes;
	}
	else {
		/*
		 * CSP : random updates
		 * BSAFE, openssl: single one-shot update
		 */
		updateSizesPtr = NULL;
	}
	/*
	 * generate with each method;
	 * verify MACs compare;
	 * verify with test code;
	 */
	if(macAlg == CSSM_ALGID_MD5HMAC) {
		doHmacMD5Ref(keyData, ptext, &macRef);
		crtn = CSSM_OK;
	}
	else {
		crtn = genMacBSAFE(macAlg,
			keyData,
			ptext,
			updateSizesPtr,
			&macRef);
	}
	if(crtn) {
		return testError(quiet);
	}
	crtn = genMacCSSM(cspHand,
		macAlg,
		keyAlg,
		CSSM_TRUE,
		keyData,
		genRaw,
		ptext,
		updateSizesPtr,
		&macTest);
	if(crtn) {
		return testError(quiet);
	}

	/* ensure both methods resulted in same MAC */
	if(macRef.Length != macTest.Length) {
		printf("MAC length mismatch (1)\n");
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	if(memcmp(macRef.Data, macTest.Data, macTest.Length)) {
		printf("MAC miscompare\n");
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	
	/* verify with the test method */
	crtn = genMacCSSM(cspHand,
		macAlg,
		keyAlg,
		CSSM_FALSE,
		keyData,
		genRaw,
		ptext,
		updateSizesPtr,
		&macTest);
	if(crtn) {
		printf("***Unexpected MAC verify failure\n");
		rtn = testError(quiet);
	}
	else {
		rtn = 0;
	}
abort:
	if(macTest.Length) {
		CSSM_FREE(macTest.Data);
	}
	if(macRef.Length) {
		CSSM_FREE(macRef.Data);
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
	const char			*algStr;
	uint32				macAlg;			// CSSM_ALGID_xxx 
	uint32				keyAlg;			// CSSM_ALGID_xxx 
	int					i;
	unsigned			currAlg;		// ALG_xxx
	int					rtn = 0;
	CSSM_DATA			keyData;
	CSSM_BOOL			genRaw = CSSM_FALSE;	// first generate raw key (CSPDL)
	
	/*
	 * User-spec'd params
	 */
	unsigned	minAlg = ALG_FIRST;
	unsigned	maxAlg = ALG_LAST;
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	unsigned	minExp = MIN_EXP;
	unsigned	maxExp = DEFAULT_MAX_EXP;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	pauseInterval = 0;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	CSSM_BOOL	fixedUpdates;
	CSSM_BOOL	allZeroes = CSSM_FALSE;
	unsigned	keySizeSpecd = 0;
	unsigned	ptextLenSpecd = 0;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'n':
				minExp = atoi(&argp[2]);
				break;
		    case 'x':
				maxExp = atoi(&argp[2]);
				if(maxExp > MAX_EXP) {
					usage(argv);
				}
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
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);
				break;
			case 'z':
				allZeroes = CSSM_TRUE;
				break;
			case 'k':
				keySizeSpecd = atoi(&argp[2]);
				break;
			case 'P':
				ptextLenSpecd = atoi(&argp[2]);
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	if(minExp > maxExp) {
		printf("***minExp must be <= maxExp\n");
		usage(argv);
	}
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_DATA_SIZE);
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
	/* key length set in test loop */

	printf("Starting macCompat; args: ");
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
		if((currAlg == ALG_SHA1_LEGACY) && !bareCsp && !CSPDL_SHA1HMAC_LEGACY_ENABLE) {
			continue;
		}
		
		/* some default values... */
		switch(currAlg) {
			case ALG_MD5:
				macAlg = CSSM_ALGID_MD5HMAC;
				keyAlg = CSSM_ALGID_MD5HMAC;
				algStr = "MD5";
				fixedUpdates = CSSM_FALSE;
				break;
			case ALG_SHA1:
				macAlg = CSSM_ALGID_SHA1HMAC;
				keyAlg = CSSM_ALGID_SHA1HMAC;
				algStr = "SHA1";
				fixedUpdates = CSSM_FALSE;
				break;
			case ALG_SHA1_LEGACY:
				macAlg = CSSM_ALGID_SHA1HMAC_LEGACY;
				keyAlg = CSSM_ALGID_SHA1HMAC;
				algStr = "SHA1_LEGACY";
				fixedUpdates = CSSM_TRUE;
				break;
			default:
				printf("***Brrzap. Bad alg.\n");
				exit(1);
		}
		
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			/* random ptext and key */
			ptext.Length = genData(ptext.Data, minExp, maxExp, DT_Random);
			if(ptextLenSpecd) {
				ptext.Length = ptextLenSpecd;
			}
			if(allZeroes) {
				memset(ptext.Data, 0, ptext.Length);
			}
			if(macAlg == CSSM_ALGID_SHA1HMAC_LEGACY) {
				simpleGenData(&keyData, 20, 20);
			}
			else {
				simpleGenData(&keyData, MIN_KEY_SIZE, MAX_KEY_SIZE);
				if(keySizeSpecd) {
					keyData.Length = keySizeSpecd;
				}
			}
			if(allZeroes) {
				memset(keyData.Data, 0, keyData.Length);
			}
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %d text size %lu keySize %lu\n",
						loop, ptext.Length, keyData.Length);
				}
			}
			
			if(doTest(cspHand,
					&ptext,
					&keyData,
					genRaw,
					macAlg,
					keyAlg,
					fixedUpdates,
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


