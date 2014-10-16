/* 
 * rawRsaSig.c - Test compatiblity of CSSM_ALGID_{SHA1,MD5}WithRSA and 
 * manual digest followed by raw RSA sign.
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
#define OLOOPS_DEF		10		/* outer loops, one key pair per loop */
#define ILOOPS_DEF		10		/* sig loops */
#define MAX_TEXT_SIZE	1024

#define LOOP_NOTIFY		20
#define DO_MULTI_UPDATE	CSSM_TRUE

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=alg (r=RSA, d=DSA; default=RSA)\n");
	printf("   l=outerloops (default=%d; 0=forever)\n", OLOOPS_DEF);
	printf("   i=innerLoops (default=%d)\n", ILOOPS_DEF);
	printf("   k=keySizeInBits; default is random\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			oloop;
	unsigned			iloop;
	CSSM_DATA			ptext = {0, NULL};
	CSSM_CSP_HANDLE 	cspHand;
	int					i;
	int					rtn = 0;
	uint32				keySizeInBits = 0;
	CSSM_KEY			pubKey;
	CSSM_KEY			privKey;
	CSSM_DATA			sig = {0, NULL};
	CSSM_DATA			digest = {0, NULL};
	CSSM_RETURN			crtn;
	const char			*digestStr;
	
	/*
	 * User-spec'd params
	 */
	CSSM_BOOL			keySizeSpec = CSSM_FALSE;
	unsigned			oloops = OLOOPS_DEF;
	unsigned			iloops = ILOOPS_DEF;
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_BOOL			quiet = CSSM_FALSE;
	unsigned			pauseInterval = 0;
	CSSM_BOOL			bareCsp = CSSM_TRUE;
	CSSM_ALGORITHMS		rawSigAlg = CSSM_ALGID_RSA;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				switch(argp[2]) {
					case 'r':
						rawSigAlg = CSSM_ALGID_RSA;
						break;
					case 'd':
						rawSigAlg = CSSM_ALGID_DSA;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				oloops = atoi(&argp[2]);
				break;
		    case 'i':
				iloops = atoi(&argp[2]);
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
	
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_TEXT_SIZE);
	if(ptext.Data == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	/* ptext length set in inner test loop */
	
	printf("Starting rawRsaSig; args: ");
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
	for(oloop=0; ; oloop++) {
		
		/* key size? */
		if(!keySizeSpec) {
			/* random key size */
			keySizeInBits = randKeySizeBits(rawSigAlg, OT_Sign);
		}

		if(!quiet) {
			if(verbose || ((oloop % LOOP_NOTIFY) == 0)) {
				printf("...oloop %d   keySize %u\n", oloop, (unsigned)keySizeInBits);
			}
		}
		
		/* generate a key pair */
		crtn = cspGenKeyPair(cspHand,
			rawSigAlg,
			"foo",
			3,
			keySizeInBits,
			&pubKey,
			CSSM_TRUE,						// all keys ref for speed
			CSSM_KEYUSE_VERIFY,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			&privKey,
			CSSM_TRUE,
			CSSM_KEYUSE_SIGN,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			CSSM_FALSE);					// genSeed not used 
		if(crtn) {
			return testError(quiet);
		}
		
		for(iloop=0; iloop<iloops; iloop++) {
		
			CSSM_ALGORITHMS		sigAlg;
			CSSM_ALGORITHMS		digestAlg;
			CSSM_CC_HANDLE		sigHand;
			
			/* alternate digest algs for RSA */
			if(rawSigAlg == CSSM_ALGID_RSA) {
				if(iloop & 1) {
					sigAlg = CSSM_ALGID_SHA1WithRSA;
					digestAlg = CSSM_ALGID_SHA1;
					digestStr = "SHA1";
				}
				else {
					sigAlg = CSSM_ALGID_MD5WithRSA;
					digestAlg = CSSM_ALGID_MD5;
					digestStr = "MD5 ";
				}
			}
			else {
				sigAlg = CSSM_ALGID_SHA1WithDSA;
				digestAlg = CSSM_ALGID_SHA1;
				digestStr = "SHA1";
			}
			
			/* new plaintext each inner loop */
			simpleGenData(&ptext, 1, MAX_TEXT_SIZE);
			if(!quiet) {
				if(verbose || ((iloop % LOOP_NOTIFY) == 0)) {
					printf("   ...iloop %d  digest %s  text size %lu\n", 
						iloop, digestStr, ptext.Length);
				}
			}
			
			/*** phase 1 ***/
			
			/* digest+sign */
			crtn = cspStagedSign(cspHand,
				sigAlg,
				&privKey,
				&ptext,
				DO_MULTI_UPDATE,			// multiUpdates
				&sig);
			if(crtn && testError(quiet)) {
				goto abort;
			}
			
			/* digest */
			crtn = cspStagedDigest(cspHand,
				digestAlg,
				CSSM_FALSE,			// mallocDigest
				DO_MULTI_UPDATE,	// multiUpdates
				&ptext, 
				&digest);
			if(crtn && testError(quiet)) {
				goto abort;
			}
			
			/* raw RSA/DSA verify */
			crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				rawSigAlg,
				NULL,				// passPhrase
				&pubKey,
				&sigHand);
			if(crtn) {
				printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
				return crtn;
			}
			crtn = CSSM_VerifyData(sigHand,
				&digest,
				1,
				digestAlg,
				&sig);
			if(crtn) {
				printError("CSSM_VerifyData(raw RSA)", crtn);
				if(testError(quiet)) {
					goto abort;
				}
			}
			
			/* free resources - reuse the digest for raw sign */
			appFreeCssmData(&sig, CSSM_FALSE);
			CSSM_DeleteContext(sigHand);
			
			/*** phase 2 ***/
			
			/* raw RSA/DSA sign */
			crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				rawSigAlg,
				NULL,				// passPhrase
				&privKey,
				&sigHand);
			if(crtn) {
				printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
				return crtn;
			}
			crtn = CSSM_SignData(sigHand,
				&digest,
				1,
				digestAlg,
				&sig);
			if(crtn) {
				printError("CSSM_SignData(raw RSA)", crtn);
				if(testError(quiet)) {
					goto abort;
				}
			}

			/* all-in-one verify */
			crtn = cspStagedSigVerify(cspHand,
				sigAlg,
				&pubKey,
				&ptext,
				&sig,
				DO_MULTI_UPDATE,		// multiUpdates
				CSSM_OK);
			if(crtn && testError(quiet)) {
				goto abort;
			}
			
			/* clean up */
			appFreeCssmData(&sig, CSSM_FALSE);
			appFreeCssmData(&digest, CSSM_FALSE);
			CSSM_DeleteContext(sigHand);
		}	/* end of inner loop */

		/* free keys */
		cspFreeKey(cspHand, &pubKey);
		cspFreeKey(cspHand, &privKey);

		if(oloops && (oloop == oloops)) {
			break;
		}
		if(pauseInterval && ((oloop % pauseInterval) == 0)) {
			fpurge(stdin);
			printf("hit CR to proceed: ");
			getchar();
		}
	}
	
abort:
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
	return rtn;
}


