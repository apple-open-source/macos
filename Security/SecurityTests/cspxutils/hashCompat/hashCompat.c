/* 
 * hashCompat.c - test compatibilty of two different implementations of a
 * various digest algorithms - one in the standard AppleCSP, one in BSAFE.
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

/*
 * Defaults.
 */
#define LOOPS_DEF		200
#define MIN_EXP			2		/* for data size 10**exp */
#define DEFAULT_MAX_EXP	4
#define MAX_EXP			5

#define MAX_DATA_SIZE	(100000 + 100)		/* bytes */
#define LOOP_NOTIFY		20

/*
 * Enumerate algs our own way to allow iteration.
 */
enum {
	ALG_SHA1	= 1,
	ALG_MD5,
	ALG_MD2
};

#define ALG_FIRST			ALG_SHA1
#define ALG_LAST			ALG_MD2

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (s=SHA1; 5=MD5; 2=MD2; default=all\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=%d, max=%d)\n", DEFAULT_MAX_EXP, MAX_EXP);
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* 
 * generate digest using reference BSAFE.
 */
static CSSM_RETURN genDigestBSAFE(
	CSSM_ALGORITHMS		hashAlg,
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned
{
	CSSM_RETURN crtn;

	crtn = buGenDigest(hashAlg,
		inText,
		outText);
	return crtn;
}

/*
 * Generate digest using CSP.
 */
static CSSM_RETURN genDigestCSSM(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_ALGORITHMS		hashAlg,
	const CSSM_DATA		*inText,
	CSSM_DATA_PTR 		outText)			// mallocd and returned if doGen
{

	outText->Data = NULL;
	outText->Length = 0;
	return cspStagedDigest(cspHand,
		hashAlg,
		CSSM_TRUE,		// mallocDigest
		CSSM_TRUE,		// multiUpdates
		inText, 
		outText);
}

#define LOG_FREQ	20

static int doTest(CSSM_CSP_HANDLE cspHand,
	const CSSM_DATA		*ptext,
	uint32 				hashAlg,
	CSSM_BOOL 			quiet)
{
	CSSM_DATA 		hashRef = {0, NULL};		// digest, BSAFE reference
	CSSM_DATA		hashTest = {0, NULL};		// digest, CSP test
	int				rtn = 0;
	CSSM_RETURN		crtn;
		
	/*
	 * generate with each method;
	 * verify digests compare;
	 */
	crtn = genDigestBSAFE(hashAlg,
		ptext,
		&hashRef);
	if(crtn) {
		return testError(quiet);
	}
	crtn = genDigestCSSM(cspHand,
		hashAlg,
		ptext,
		&hashTest);
	if(crtn) {
		return testError(quiet);
	}

	/* ensure both methods resulted in same hash */
	if(hashRef.Length != hashTest.Length) {
		printf("hash length mismatch (1)\n");
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	if(memcmp(hashRef.Data, hashTest.Data, hashTest.Length)) {
		printf("hash miscompare\n");
		rtn = testError(quiet);
	}
	else {
		rtn = 0;
	}
abort:
	if(hashTest.Length) {
		CSSM_FREE(hashTest.Data);
	}
	if(hashRef.Length) {
		CSSM_FREE(hashRef.Data);
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
	uint32				hashAlg;		// CSSM_ALGID_xxx 
	int					i;
	unsigned			currAlg;		// ALG_xxx
	int					rtn = 0;
	
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
	
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 's':
						minAlg = maxAlg = ALG_SHA1;
						break;
					case '5':
						minAlg = maxAlg = ALG_MD5;
						break;
					case '2':
						minAlg = maxAlg = ALG_MD2;
						break;
					default:
						usage(argv);
				}
				break;
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

	printf("Starting hashCompat; args: ");
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
		switch(currAlg) {
			case ALG_SHA1:
				hashAlg = CSSM_ALGID_SHA1;
				algStr = "SHA1";
				break;
			case ALG_MD5:
				hashAlg = CSSM_ALGID_MD5;
				algStr = "MD5";
				break;
			case ALG_MD2:
				hashAlg = CSSM_ALGID_MD2;
				algStr = "MD2";
				break;
			default:
				printf("***Brrzap. Bad alg.\n");
				exit(1);
		}
		
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			/* random ptext length and data */
			ptext.Length = genData(ptext.Data, minExp, maxExp, DT_Random);
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %d text size %lu \n", loop, ptext.Length);
				}
			}
			
			if(doTest(cspHand,
					&ptext,
					hashAlg,
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
		printf("%s complete\n", argv[0]);
	}
	CSSM_FREE(ptext.Data);
	return rtn;
}


