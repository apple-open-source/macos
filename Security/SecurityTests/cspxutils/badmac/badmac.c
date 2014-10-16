/* Copyright (c) 1997,2003-2005,2008 Apple Inc.
 *
 * badmac.c - Verify bad MAC detect.
 *
 * Revision History
 * ----------------
 *   4 May 2000  Doug Mitchell
 *		Ported to X/CDSA2. 
 *  21 Dec 1998	Doug Mitchell at Apple
 *		Created.
 */
/*
 * text size =       {random, from 100 bytes to 1 megabyte, in
 *                   geometrical steps, i.e. the number of
 *                   bytes would be 10^r, where r is random out of
 *                   {2,3,4,5,6}, plus a random integer in {0,..99}};
 *
 * for loop_count
 *     text contents = {random data, random size as specified above};
 *     generate random MAC key;
 *     generate MAC, validate;
 *     for various bytes of ptext {
 *        corrupt text byte;
 *        verify bad MAC;
 *        restore corrupted byte;
 *     }
 *  }
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

#define USAGE_NAME		"noUsage"
#define USAGE_NAME_LEN	(strlen(USAGE_NAME))

/*
 * HMAC/SHA1 can not do multiple updates with BSAFE (though the CSP's current 
 * internal implementation can.)
 * Fixed in Puma; the bug was in BSAFE. 
 */
#define HMACSHA_MULTI_UPDATES	1

/*
 * Defaults.
 */
#define LOOPS_DEF			10
#define MIN_EXP				2		/* for data size 10**exp */
#define DEFAULT_MAX_EXP		2
#define MAX_EXP				5
#define INCR_DEFAULT		0		/* munge every incr bytes - zero means
									 * "adjust per ptext size" */

/*
 * Enumerate algs our own way to allow iteration.
 */
#define ALG_SHA1HMAC		1
#define ALG_MD5HMAC			2
#define ALG_FIRST			ALG_SHA1HMAC
#define ALG_LAST			ALG_MD5HMAC
#define MAX_DATA_SIZE		(100000 + 100)	/* bytes */

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=%d, max=%d)\n", DEFAULT_MAX_EXP, MAX_EXP);
	printf("   i=increment (default=%d)\n", INCR_DEFAULT);
	printf("   r(eference keys only)\n");
	printf("   m (CSM mallocs MAC)\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

#define LOG_FREQ	20
static int doTest(CSSM_CSP_HANDLE cspHand,
	uint32 macAlg,					// CSSM_ALGID_xxx mac algorithm
	CSSM_DATA_PTR ptext,
	CSSM_BOOL verbose,
	CSSM_BOOL quiet,
	unsigned keySize,
	unsigned incr,
	CSSM_BOOL stagedGen,
	CSSM_BOOL stagedVerify,
	CSSM_BOOL mallocMac,
	CSSM_BOOL refKey)
{
	CSSM_KEY_PTR	symmKey;
	CSSM_DATA 		mac = {0, NULL};
	unsigned		length;
	unsigned		byte;
	unsigned char	*data;
	unsigned char	origData;
	unsigned char	bits;
	int				rtn = 0;
	CSSM_RETURN		crtn;
	uint32			keyGenAlg;
	unsigned		loop = 0;
	
	switch(macAlg) {
		case CSSM_ALGID_SHA1HMAC:
			keyGenAlg = CSSM_ALGID_SHA1HMAC;
			break;
		case CSSM_ALGID_MD5HMAC:
			keyGenAlg = CSSM_ALGID_MD5HMAC;
			break;
		default:
			printf("bogus algorithm\n");
			return 1;
	}
	symmKey = cspGenSymKey(cspHand,
		keyGenAlg,
		"noLabel",
		7,
		CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY,
		keySize,
		refKey);
	if(symmKey == NULL) {
		rtn = testError(quiet);
		goto abort;
	}
	if(stagedGen) {
		crtn = cspStagedGenMac(cspHand,
			macAlg,
			symmKey,
			ptext,
			mallocMac,
			CSSM_TRUE,			// multi
			&mac);
	}
	else {
		crtn = cspGenMac(cspHand,
			macAlg,
			symmKey,
			ptext,
			&mac);
	}
	if(crtn) {
		rtn = 1;
		goto abort;
	}
	if(stagedVerify) {
		crtn = cspStagedMacVerify(cspHand,
			macAlg,
			symmKey,
			ptext,
			&mac,
			CSSM_TRUE,			// multi
			CSSM_OK);			// expectedResult
	}
	else {
		crtn = cspMacVerify(cspHand,
			macAlg,
			symmKey,
			ptext,
			&mac,
			CSSM_OK);
	}
	if(crtn) {
		printf("**Unexpected BAD MAC\n");
		return testError(quiet);
	}
	data = (unsigned char *)ptext->Data;
	length = ptext->Length;
	for(byte=0; byte<length; byte += incr) {
		if(verbose && ((loop++ % LOG_FREQ) == 0)) {
			printf("  ..byte %d\n", byte);
		}
		origData = data[byte];
		/*
		 * Generate random non-zero byte
		 */
		do {
			bits = genRand(1, 0xff) & 0xff;
		} while(bits == 0);
		data[byte] ^= bits;
		if(stagedVerify) {
			crtn = cspStagedMacVerify(cspHand,
				macAlg,
				symmKey,
				ptext,
				&mac,
				CSSM_TRUE,					// multi
				CSSMERR_CSP_VERIFY_FAILED);	// expect failure
		}
		else {
			crtn = cspMacVerify(cspHand,
				macAlg,
				symmKey,
				ptext,
				&mac,
				CSSMERR_CSP_VERIFY_FAILED);
		}
		if(crtn) {
			return testError(quiet);
		}
		data[byte] = origData;
	}
abort:
	/* free key */
	if(cspFreeKey(cspHand, symmKey)) {
		printf("Error freeing symmKey\n");
		rtn = 1;
	}
	CSSM_FREE(mac.Data);
	return rtn;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	CSPHandle;
	CSSM_BOOL			stagedSign;
	CSSM_BOOL 			stagedVfy;
	CSSM_BOOL 			mallocMac;
	CSSM_BOOL 			refKey;
	const char 			*algStr;
	unsigned			actualIncr;
	uint32				macAlg;			// CSSM_ALGID_xxx
	unsigned			currAlg;		// ALG_xxx
	int					i;
	int 				rtn = 0;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	unsigned	minExp = MIN_EXP;
	unsigned	maxExp = DEFAULT_MAX_EXP;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	unsigned	incr = INCR_DEFAULT;
	unsigned	minAlg = ALG_FIRST;
	uint32		maxAlg = ALG_LAST;
	unsigned	pauseInterval = 0;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	CSSM_BOOL	refKeysOnly = CSSM_FALSE;
	CSSM_BOOL	cspMallocs = CSSM_FALSE;
	
	#if	macintosh
	argc = ccommand(&argv);
	#endif
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			/* no Alg or keySizeInBits spec for now */
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
		    case 'i':
				incr = atoi(&argp[2]);
				break;
		    case 'p':
				pauseInterval = atoi(&argp[2]);
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if CSPDL_ALL_KEYS_ARE_REF
				refKeysOnly = CSSM_TRUE;
				#endif
				break;
			case 'r':
				refKeysOnly = CSSM_TRUE;
				break;
			case 'm':
				cspMallocs = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_DATA_SIZE);
	/* length set in test loop */
	if(ptext.Data == NULL) {
		printf("Insufficient heap\n");
		exit(1);
	}
	printf("Starting badmac; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	CSPHandle = cspDlDbStartup(bareCsp, NULL);
	if(CSPHandle == 0) {
		exit(1);
	}
	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		switch(currAlg) {
			case ALG_SHA1HMAC:
				macAlg = CSSM_ALGID_SHA1HMAC;
				algStr = "SHA1HMAC";
				break;
			case ALG_MD5HMAC:
				macAlg = CSSM_ALGID_MD5HMAC;
				algStr = "MD5HMAC";
				break;
		}
		if(!quiet) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			ptext.Length = genData(ptext.Data, minExp, maxExp, DT_Random);
			if(!quiet) {
				printf("..loop %d text size %lu\n", loop, ptext.Length);
			}
			if(incr == 0) {
				/* adjust increment as appropriate */
				actualIncr = (ptext.Length / 50) + 1;
			}
			else {
				actualIncr = incr;
			}
			/* mix up staging & ref key format*/
			stagedSign = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
			stagedVfy  = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
			if(refKeysOnly) {
				refKey = CSSM_TRUE;
			}
			else {
				refKey     = (loop & 4) ? CSSM_TRUE : CSSM_FALSE;
			}
			if(cspMallocs) {
				mallocMac = CSSM_FALSE;
			}
			else {
				mallocMac  = (loop & 8) ? CSSM_TRUE : CSSM_FALSE;
			}

			#if		!HMACSHA_MULTI_UPDATES
			if(macAlg == CSSM_ALGID_SHA1HMAC) {
				stagedSign = stagedVfy = CSSM_FALSE;
			}
			#endif	/* HMACSHA_MULTI_UPDATES */
			
			if(!quiet) {
				printf("  stagedSign %d  stagedVfy %d refKey %d mallocMac %d\n",
					 (int)stagedSign, (int)stagedVfy, (int)refKey, (int)mallocMac);
			}
			if(doTest(CSPHandle,
					macAlg,
					&ptext,
					verbose,
					quiet,
					keySizeInBits,
					actualIncr,
					stagedSign,
					stagedVfy,
					mallocMac,
					refKey)) {
				rtn = 1;
				goto testDone;
			}
			if(loops && (loop == loops)) {
				break;
			}
			if(pauseInterval && ((loop % pauseInterval) == 0)) {
				fpurge(stdin);
				printf("Hit CR to proceed: ");
				getchar();
			}
		}	/* for loop */
	}		/* for alg */
testDone:
	CSSM_ModuleDetach(CSPHandle);
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return rtn;
}
