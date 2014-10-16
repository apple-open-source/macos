/*
 * hashClone.c - test CSSM_DigestDataClone function
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"

/*
 * Defaults
 */
#define LOOPS_DEF		50
#define MAX_PTEXT		(8 * 1024)
#define MIN_PTEXT		16
#define LOOP_NOTIFY		20

/*
 * Enumerated algs
 */
typedef unsigned privAlg;
enum  {
	ALG_MD5 = 1,
	ALG_SHA1,
	ALG_MD2,
	ALG_SHA224,
	ALG_SHA256,
	ALG_SHA384,
	ALG_SHA512
};

#define ALG_FIRST	ALG_MD5
#define ALG_LAST	ALG_SHA512

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (s=SHA1; m=MD5; M=MD2; 4=SHA224; 2=SHA256; 3=SHA384; 5=SHA512; "
					"default=all\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_ALGORITHMS alg,
	const char *algStr,
	CSSM_DATA_PTR ptext,
	CSSM_BOOL verbose,
	CSSM_BOOL quiet)
{
	CSSM_CC_HANDLE		digHand1 = 0;	// reference
	CSSM_CC_HANDLE		digHand2 = 0;	// to be cloned
	CSSM_CC_HANDLE		digHand3 = 0;	// cloned from digHand2
	CSSM_DATA 			dig1 = {0, NULL};
	CSSM_DATA 			dig2 = {0, NULL};
	CSSM_DATA 			dig3 = {0, NULL};
	CSSM_RETURN			crtn;
	unsigned			thisMove;		// this update
	unsigned			toMove;			// total to go
	unsigned			totalRequest;	// originally requested
	CSSM_DATA			thisText;		// actually passed to update
		
	/* cook up two digest contexts */
	crtn = CSSM_CSP_CreateDigestContext(cspHand,
		alg,
		&digHand1);
	if(crtn) {
		printError("CSSM_CSP_CreateDigestContext (1)", crtn);
		return testError(quiet);
	}
	crtn = CSSM_CSP_CreateDigestContext(cspHand,
		alg,
		&digHand2);
	if(crtn) {
		printError("CSSM_CSP_CreateDigestContext (2)", crtn);
		return testError(quiet);
	}
	crtn = CSSM_DigestDataInit(digHand1);
	if(crtn) {
		printError("CSSM_DigestDataInit (1)", crtn);
		return testError(quiet);
	}
	crtn = CSSM_DigestDataInit(digHand2);
	if(crtn) {
		printError("CSSM_DigestDataInit (2)", crtn);
		return testError(quiet);
	}
	
	/* do some random updates to first two digests, until we've digested
	 * at least half of the requested data */
	totalRequest = ptext->Length;
	toMove = ptext->Length;
	thisText.Data = ptext->Data;
	while(toMove > (totalRequest / 2)) {
		thisMove = genRand((MIN_PTEXT / 2), toMove);
		thisText.Length = thisMove;
		if(verbose) {
			printf("  ..updating digest1, digest2 with %d bytes\n", thisMove);
		}
		crtn = CSSM_DigestDataUpdate(digHand1, &thisText, 1);
		if(crtn) {
			printError("CSSM_DigestDataUpdate (1)", crtn);
			return testError(quiet);
		}
		crtn = CSSM_DigestDataUpdate(digHand2, &thisText, 1);
		if(crtn) {
			printError("CSSM_DigestDataUpdate (2)", crtn);
			return testError(quiet);
		}
		thisText.Data += thisMove;
		toMove -= thisMove;
	}
	
	/* digest3 := clone(digest2) */
	crtn = CSSM_DigestDataClone(digHand2, &digHand3);
	if(crtn) {
		printError("CSSM_DigestDataClone", crtn);
		return testError(quiet);
	}
	
	/* finish off remaining ptext, updating all 3 digests identically */
	while(toMove) {
		thisMove = genRand(1, toMove);
		thisText.Length = thisMove;
		if(verbose) {
			printf("  ..updating all three digests with %d bytes\n", thisMove);
		}
		crtn = CSSM_DigestDataUpdate(digHand1, &thisText, 1);
		if(crtn) {
			printError("CSSM_DigestDataUpdate (3)", crtn);
			return testError(quiet);
		}
		crtn = CSSM_DigestDataUpdate(digHand2, &thisText, 1);
		if(crtn) {
			printError("CSSM_DigestDataUpdate (4)", crtn);
			return testError(quiet);
		}
		crtn = CSSM_DigestDataUpdate(digHand3, &thisText, 1);
		if(crtn) {
			printError("CSSM_DigestDataUpdate (5)", crtn);
			return testError(quiet);
		}
		thisText.Data += thisMove;
		toMove -= thisMove;
	}
	
	/* obtain all three digests */
	crtn = CSSM_DigestDataFinal(digHand1, &dig1);
	if(crtn) {
		printError("CSSM_DigestDataFinal (1)", crtn);
		return testError(quiet);
	}
	crtn = CSSM_DigestDataFinal(digHand2, &dig2);
	if(crtn) {
		printError("CSSM_DigestDataFinal (2)", crtn);
		return testError(quiet);
	}
	crtn = CSSM_DigestDataFinal(digHand3, &dig3);
	if(crtn) {
		printError("CSSM_DigestDataFinal (3)", crtn);
		return testError(quiet);
	}
	
	/* ensure all three digests identical */
	if(!appCompareCssmData(&dig1, &dig2)) {
		printf("***Digest miscompare(dig1, dig2)***\n");
		if(testError(quiet)) {
			return 1;
		}
	}
	if(!appCompareCssmData(&dig2, &dig3)) {
		printf("***Digest miscompare(dig2, dig3)***\n");
		if(testError(quiet)) {
			return 1;
		}
	}
	
	/* free resources */
	appFreeCssmData(&dig1, CSSM_FALSE);
	appFreeCssmData(&dig2, CSSM_FALSE);
	appFreeCssmData(&dig3, CSSM_FALSE);
	CSSM_DeleteContext(digHand1);
	CSSM_DeleteContext(digHand2);
	CSSM_DeleteContext(digHand3);
	return 0;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	const char 			*algStr;
	privAlg				alg;		// ALG_MD5, etc.
	CSSM_ALGORITHMS		cssmAlg;	// CSSM_ALGID_MD5, etc.
	int					j;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	minAlg = ALG_FIRST;
	unsigned	maxAlg = ALG_LAST;
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
					case 'm':
						minAlg = maxAlg = ALG_MD5;
						break;
					case 'M':
						minAlg = maxAlg = ALG_MD2;
						break;
					case '4':
						minAlg = maxAlg = ALG_SHA224;
						break;
					case '2':
						minAlg = maxAlg = ALG_SHA256;
						break;
					case '3':
						minAlg = maxAlg = ALG_SHA384;
						break;
					case '5':
						minAlg = maxAlg = ALG_SHA512;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_PTEXT);
	/* length set in test loop */
	if(ptext.Data == NULL) {
		printf("Insufficient heap\n");
		exit(1);
	}
	
	printf("Starting hashClone; args: ");
	for(j=1; j<argc; j++) {
		printf("%s ", argv[j]);
	}
	printf("\n");
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	
	for(alg=minAlg; alg<=maxAlg; alg++) {
		switch(alg) {
			case ALG_MD5:
				algStr = "MD5";
				cssmAlg = CSSM_ALGID_MD5;
				break;
			case ALG_MD2:
				algStr = "MD2";
				cssmAlg = CSSM_ALGID_MD2;
				break;
			case ALG_SHA1:
				algStr = "SHA1";
				cssmAlg = CSSM_ALGID_SHA1;
				break;
			case ALG_SHA224:
				algStr = "SHA224";
				cssmAlg = CSSM_ALGID_SHA224;
				break;
			case ALG_SHA256:
				algStr = "SHA256";
				cssmAlg = CSSM_ALGID_SHA256;
				break;
			case ALG_SHA384:
				algStr = "SHA384";
				cssmAlg = CSSM_ALGID_SHA384;
				break;
			case ALG_SHA512:
				algStr = "SHA512";
				cssmAlg = CSSM_ALGID_SHA512;
				break;
		}
		if(!quiet) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			simpleGenData(&ptext, MIN_PTEXT, MAX_PTEXT);
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %d text size %lu\n", loop, ptext.Length);
				}
			}
			if(doTest(cspHand,
					cssmAlg,
					algStr,
					&ptext,
					verbose,
					quiet)) {
				exit(1);
			}
			if(loops && (loop == loops)) {
				break;
			}
			if(pauseInterval && ((loop % pauseInterval) == 0)) {
				fpurge(stdin);
				printf("Hit CR to proceed: ");
				getchar();
			}
		}
	}
	cspShutdown(cspHand, bareCsp);
	if(!quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return 0;
}
