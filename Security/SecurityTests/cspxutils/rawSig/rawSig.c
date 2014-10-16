/* 
 * rawSig.c - Test compatiblity of (hash+sign) ops vs. manual digest followed 
 * by raw sign.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include "bsafeUtils.h"
#include "cspdlTesting.h"
#include <string.h>

/*
 * Defaults.
 */
#define OLOOPS_DEF		10		/* outer loops, one key pair per loop */
#define ILOOPS_DEF		10		/* sig loops */
#define MAX_TEXT_SIZE	1024

#define LOOP_NOTIFY		20

/*
 * Enumerate algs our own way to allow iteration.
 */
#define ALG_RSA_MD5			1
#define ALG_RSA_SHA1		2
#define ALG_RSA_MD2			3
#define ALG_FEE_MD5			4
#define ALG_FEE_SHA1		5
#define ALG_ECDSA_SHA1		6
#define ALG_DSA_SHA1		7
#define ALG_ANSI_ECDSA_SHA1	8
#define ALG_ECDSA_SHA256	9
#define ALG_ECDSA_SHA384	10
#define ALG_ECDSA_SHA512	11
#define ALG_FIRST			ALG_RSA_MD5
#define ALG_LAST			ALG_ECDSA_SHA512

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=alg (5=RSA/MD5; 2=RSA/MD2; R=RSA/SHA1; d=DSA/SHA1; f=FEE/MD5; F=FEE/SHA1; \n");
	printf("   e=ECDSA/SHA1; E=ANSI_ECDSA/SHA1; 7=ECDSA/SHA256; 8=ECDSA/SHA384; 9=ECDSA/512; default=all)\n");
	printf("   l=outerloops (default=%d; 0=forever)\n", OLOOPS_DEF);
	printf("   i=innerLoops (default=%d)\n", ILOOPS_DEF);
	printf("   k=keySizeInBits; default is random\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   s(mall keys)\n");
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
	unsigned			currAlg;
	
	/* current alg (e.g. ALG_FEE_SHA1) parsed to come up with these */
	CSSM_ALGORITHMS		digestAlg;
	CSSM_ALGORITHMS		rawSigAlg;
	CSSM_ALGORITHMS		sigAlg;
	CSSM_ALGORITHMS		keyGenAlg;
	const char			*sigAlgStr;
	
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
	unsigned			minAlg = ALG_FIRST;
	uint32				maxAlg = ALG_LAST;
	CSSM_BOOL			smallKeys = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				switch(argp[2]) {
					case 'f':
						minAlg = maxAlg = ALG_FEE_MD5;
						break;
					case 'F':
						minAlg = maxAlg = ALG_FEE_SHA1;
						break;
					case 'e':
						minAlg = maxAlg = ALG_ECDSA_SHA1;
						break;
					case '5':
						minAlg = maxAlg = ALG_RSA_MD5;
						break;
					case '2':
						minAlg = maxAlg = ALG_RSA_MD2;
						break;
					case 'R':
						minAlg = maxAlg = ALG_RSA_SHA1;
						break;
					case 'd':
						minAlg = maxAlg = ALG_DSA_SHA1;
						break;
					case 'E':
						minAlg = maxAlg = ALG_ANSI_ECDSA_SHA1;
						break;
					case '7':
						minAlg = maxAlg = ALG_ECDSA_SHA256;
						break;
					case '8':
						minAlg = maxAlg = ALG_ECDSA_SHA384;
						break;
					case '9':
						minAlg = maxAlg = ALG_ECDSA_SHA512;
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
		    case 's':
		    	smallKeys = CSSM_TRUE;
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
	
	printf("Starting rawSig; args: ");
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
		/* get current algs */
		switch(currAlg) {
			case ALG_RSA_MD5:
				digestAlg = CSSM_ALGID_MD5;
				rawSigAlg = CSSM_ALGID_RSA;
				sigAlg    = CSSM_ALGID_MD5WithRSA;
				keyGenAlg = CSSM_ALGID_RSA;
				sigAlgStr = "MD5WithRSA";
				break;
			case ALG_RSA_MD2:
				digestAlg = CSSM_ALGID_MD2;
				rawSigAlg = CSSM_ALGID_RSA;
				sigAlg    = CSSM_ALGID_MD2WithRSA;
				keyGenAlg = CSSM_ALGID_RSA;
				sigAlgStr = "MD2WithRSA";
				break;
			case ALG_RSA_SHA1:
				digestAlg = CSSM_ALGID_SHA1;
				rawSigAlg = CSSM_ALGID_RSA;
				sigAlg    = CSSM_ALGID_SHA1WithRSA;
				keyGenAlg = CSSM_ALGID_RSA;
				sigAlgStr = "SHA1WithRSA";
				break;
			case ALG_DSA_SHA1:
				digestAlg = CSSM_ALGID_SHA1;
				rawSigAlg = CSSM_ALGID_DSA;
				sigAlg    = CSSM_ALGID_SHA1WithDSA;
				keyGenAlg = CSSM_ALGID_DSA;
				sigAlgStr = "SHA1WithDSA";
				break;
			case ALG_FEE_MD5:
				digestAlg = CSSM_ALGID_MD5;
				rawSigAlg = CSSM_ALGID_FEE;
				sigAlg    = CSSM_ALGID_FEE_MD5;
				keyGenAlg = CSSM_ALGID_FEE;
				sigAlgStr = "MD5WithFEE";
				break;
			case ALG_FEE_SHA1:
				digestAlg = CSSM_ALGID_SHA1;
				rawSigAlg = CSSM_ALGID_FEE;
				sigAlg    = CSSM_ALGID_FEE_SHA1;
				keyGenAlg = CSSM_ALGID_FEE;
				sigAlgStr = "SHA1WithFEE";
				break;
			case ALG_ECDSA_SHA1:
				digestAlg = CSSM_ALGID_SHA1;
				rawSigAlg = CSSM_ALGID_ECDSA;
				sigAlg    = CSSM_ALGID_SHA1WithECDSA;
				keyGenAlg = CSSM_ALGID_FEE;
				sigAlgStr = "SHA1WithECDSA";
				break;
			case ALG_ANSI_ECDSA_SHA1:
				digestAlg = CSSM_ALGID_SHA1;
				rawSigAlg = CSSM_ALGID_ECDSA;
				sigAlg    = CSSM_ALGID_SHA1WithECDSA;
				keyGenAlg = CSSM_ALGID_ECDSA;
				sigAlgStr = "ANSI ECDSA";
				break;
			case ALG_ECDSA_SHA256:
				digestAlg = CSSM_ALGID_SHA256;
				rawSigAlg = CSSM_ALGID_ECDSA;
				sigAlg    = CSSM_ALGID_SHA256WithECDSA;
				keyGenAlg = CSSM_ALGID_ECDSA;
				sigAlgStr = "ECDSA/SHA256";
				break;
			case ALG_ECDSA_SHA384:
				digestAlg = CSSM_ALGID_SHA384;
				rawSigAlg = CSSM_ALGID_ECDSA;
				sigAlg    = CSSM_ALGID_SHA384WithECDSA;
				keyGenAlg = CSSM_ALGID_ECDSA;
				sigAlgStr = "ECDSA/SHA384";
				break;
			case ALG_ECDSA_SHA512:
				digestAlg = CSSM_ALGID_SHA512;
				rawSigAlg = CSSM_ALGID_ECDSA;
				sigAlg    = CSSM_ALGID_SHA512WithECDSA;
				keyGenAlg = CSSM_ALGID_ECDSA;
				sigAlgStr = "ECDSA/SHA512";
				break;
			default:
				printf("BRRZAP!\n");
				exit(1);
		}
		if(!quiet) {
			printf("Testing alg %s...\n", sigAlgStr);
		}
		for(oloop=0; ; oloop++) {
			
			/* key size? */
			if(smallKeys) {
				keySizeInBits = cspDefaultKeySize(keyGenAlg);
			}
			else if(!keySizeSpec) {
				/* random key size */
				keySizeInBits = randKeySizeBits(rawSigAlg, OT_Sign);
			}
	
			if(!quiet) {
				if(verbose || ((oloop % LOOP_NOTIFY) == 0)) {
					printf("   ...oloop %d   keySize %u\n", oloop, (unsigned)keySizeInBits);
				}
			}
			
			/* generate a key pair */
			if(currAlg == ALG_DSA_SHA1) {
				CSSM_BOOL doGenParams;
				
				if(bareCsp || CSPDL_DSA_GEN_PARAMS) {
					doGenParams = CSSM_TRUE;
				}
				else {
					/* CSPDL - no gen params */
					doGenParams = CSSM_FALSE;
				}
				crtn = cspGenDSAKeyPair(cspHand,
					"foo",
					3,
					keySizeInBits,
					&pubKey,
					CSSM_TRUE,				// all keys ref for speed
					CSSM_KEYUSE_VERIFY,
					CSSM_KEYBLOB_RAW_FORMAT_NONE,
					&privKey,
					CSSM_TRUE,
					CSSM_KEYUSE_SIGN,
					CSSM_KEYBLOB_RAW_FORMAT_NONE,
					doGenParams,			// genParams
					NULL);					// params 
			}
			else {
				crtn = cspGenKeyPair(cspHand,
					keyGenAlg,
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
			}
			if(crtn) {
				return testError(quiet);
			}
			
			for(iloop=0; iloop<iloops; iloop++) {
				CSSM_CC_HANDLE		sigHand;
								
				/* new plaintext each inner loop */
				simpleGenData(&ptext, 1, MAX_TEXT_SIZE);
				if(!quiet) {
					if(verbose || ((iloop % LOOP_NOTIFY) == 0)) {
						printf("      ...iloop %d  text size %lu\n", 
							iloop, ptext.Length);
					}
				}
				
				/*** phase 1 ***/
				
				/* digest+sign */
				crtn = cspStagedSign(cspHand,
					sigAlg,
					&privKey,
					&ptext,
					CSSM_TRUE,			// multiUpdates
					&sig);
				if(crtn && testError(quiet)) {
					goto abort;
				}
				
				/* manual digest */
				crtn = cspStagedDigest(cspHand,
					digestAlg,
					CSSM_FALSE,			// mallocDigest
					CSSM_TRUE,			// multiUpdates
					&ptext, 
					&digest);
				if(crtn && testError(quiet)) {
					goto abort;
				}
				
				/* raw verify of the digest */
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
					printError("CSSM_VerifyData(raw)", crtn);
					if(testError(quiet)) {
						goto abort;
					}
				}
				
				/* free resources - reuse the digest for raw sign */
				appFreeCssmData(&sig, CSSM_FALSE);
				CSSM_DeleteContext(sigHand);
				
				/*** phase 2 ***/
				
				/* raw sign the digest */
				crtn = CSSM_CSP_CreateSignatureContext(cspHand,
					rawSigAlg,
					NULL,				// passPhrase
					&privKey,
					&sigHand);
				if(crtn) {
					printError("CSSM_CSP_CreateSignatureContext (2)", crtn);
					return crtn;
				}
				crtn = CSSM_SignData(sigHand,
					&digest,
					1,
					digestAlg,
					&sig);
				if(crtn) {
					printError("CSSM_SignData(raw)", crtn);
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
					CSSM_TRUE,			// multiUpdates
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
		}	/* oloop */
	}		/* for alg */
	
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


