/* 
 * sigPerform.c - measure performance of raw sign and verify
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

/*
 * Defaults.
 */
#define SIG_LOOPS_DEF	1000		/* sig loops */
#define KEYSIZE_DEF		512
#define PTEXT_SIZE		20			/* e.g., a SHA1 digest */

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (r=RSA; d=DSA; s=SHA1/RSA; f=FEE/SHA1; F=FEE/MD5; e=ECDSA;\n");
	printf("                E=ECDSA/ANSI; default=RSA)\n");
	printf("   l=numLoop (default=%d)\n", SIG_LOOPS_DEF);
	printf("   k=keySizeInBits; default=%d\n", KEYSIZE_DEF);
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   b (RSA blinding enabled)\n"); 
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}


int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	CSSM_CSP_HANDLE 	cspHand;
	unsigned			i;
	CSSM_KEY			pubKey;
	CSSM_KEY			privKey;
	CSSM_DATA_PTR		ptext;		// different for each sign/vfy
	CSSM_DATA_PTR		sig;		// ditto
	unsigned			sigSize;
	CSSM_RETURN			crtn;
	CFAbsoluteTime 		start, end;
	CSSM_CC_HANDLE		sigHand;
	
	/*
	 * User-spec'd params
	 */
	uint32				keySizeInBits = KEYSIZE_DEF;
	unsigned			sigLoops = SIG_LOOPS_DEF;
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_BOOL			quiet = CSSM_FALSE;
	CSSM_BOOL			bareCsp = CSSM_TRUE;
	CSSM_ALGORITHMS		sigAlg = CSSM_ALGID_RSA;
	CSSM_ALGORITHMS		keyAlg = CSSM_ALGID_RSA;
	CSSM_ALGORITHMS		digestAlg = CSSM_ALGID_SHA1;
	CSSM_BOOL			rsaBlinding = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'r':
						sigAlg = keyAlg = CSSM_ALGID_RSA;
						break;
					case 'd':
						sigAlg = keyAlg = CSSM_ALGID_DSA;
						break;
					case 's':
						sigAlg = CSSM_ALGID_SHA1WithRSA;
						keyAlg = CSSM_ALGID_RSA;
						digestAlg = CSSM_ALGID_NONE;
						break;
					case 'f':
						sigAlg = CSSM_ALGID_FEE_SHA1;
						keyAlg = CSSM_ALGID_FEE;
						digestAlg = CSSM_ALGID_NONE;
						break;
					case 'F':
						sigAlg = CSSM_ALGID_FEE_MD5;
						keyAlg = CSSM_ALGID_FEE;
						digestAlg = CSSM_ALGID_NONE;
						break;
					case 'e':
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						keyAlg = CSSM_ALGID_FEE;
						digestAlg = CSSM_ALGID_NONE;
						break;
					case 'E':
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						keyAlg = CSSM_ALGID_ECDSA;
						digestAlg = CSSM_ALGID_NONE;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				sigLoops = atoi(&argp[2]);
				break;
		    case 'k':
		    	keySizeInBits = atoi(&argp[2]);
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
			case 'b':
				rsaBlinding = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	
	/* malloc sigLoops ptext and data structs and the data they contain */
	ptext = (CSSM_DATA_PTR)CSSM_MALLOC(sigLoops * sizeof(CSSM_DATA));
	sig = (CSSM_DATA_PTR)CSSM_MALLOC(sigLoops * sizeof(CSSM_DATA));
	memset(ptext, 0, sigLoops * sizeof(CSSM_DATA));
	memset(sig, 0, sigLoops * sizeof(CSSM_DATA));
	sigSize = (keySizeInBits + 7) / 8;
	if(sigAlg != CSSM_ALGID_RSA) {
		sigSize *= 3;
	}
	for(i=0; i<sigLoops; i++) {
		appSetupCssmData(&ptext[i], PTEXT_SIZE);
		appSetupCssmData(&sig[i], sigSize);
	}
	
	/* generate random "digests" */
	for(i=0; i<sigLoops; i++) {
		simpleGenData(&ptext[i], PTEXT_SIZE, PTEXT_SIZE);
	}
	
	printf("Generating keys....\n");
	crtn = cspGenKeyPair(cspHand,
		keyAlg,
		"foo",
		3,
		keySizeInBits,
		&pubKey,
		CSSM_TRUE,						// reference key for speed
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
	
	printf("Signing....\n");
	
	/* set up a reusable signature context */
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				sigAlg,
				NULL,				// passPhrase
				&privKey,
				&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
		return 1;
	}
	if(rsaBlinding) {
		CSSM_CONTEXT_ATTRIBUTE	newAttr;	
		newAttr.AttributeType     = CSSM_ATTRIBUTE_RSA_BLINDING;
		newAttr.AttributeLength   = sizeof(uint32);
		newAttr.Attribute.Uint32  = 1;
		crtn = CSSM_UpdateContextAttributes(sigHand, 1, &newAttr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}

	/* go - critical signing loop */
	start = CFAbsoluteTimeGetCurrent();
	for(i=0; i<sigLoops; i++) {
		crtn = CSSM_SignData(sigHand,
			&ptext[i],
			1,
			digestAlg,
			&sig[i]);
		if(crtn) {
			printError("CSSM_SignData", crtn);
			return 1;
		}
	}
	end = CFAbsoluteTimeGetCurrent();
	printf("%d sign ops in %f seconds, %f ms/op\n", sigLoops, end-start, 
		((end - start) * 1000.0) / sigLoops);
	
	CSSM_DeleteContext(sigHand);
	
	/* set up a reusable signature context */
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				sigAlg,
				NULL,				// passPhrase
				&pubKey,
				&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (2)", crtn);
		return 1;
	}

	/* go - critical verifying loop */
	start = CFAbsoluteTimeGetCurrent();
	for(i=0; i<sigLoops; i++) {
		crtn = CSSM_VerifyData(sigHand,
			&ptext[i],
			1,
			digestAlg,
			&sig[i]);
		if(crtn) {
			printError("CSSM_VerifyData", crtn);
			return 1;
		}
	}
	end = CFAbsoluteTimeGetCurrent();
	printf("%d vfy  ops in %f seconds, %f ms/op\n", sigLoops, end-start, 
		((end - start) * 1000.0) / sigLoops);
	CSSM_DeleteContext(sigHand);

	cspFreeKey(cspHand, &privKey);
	cspFreeKey(cspHand, &pubKey);
	for(i=0; i<sigLoops; i++) {
		appFreeCssmData(&ptext[i], CSSM_FALSE);
		appFreeCssmData(&sig[i], CSSM_FALSE);
	}
	CSSM_FREE(ptext);
	CSSM_FREE(sig);
	cspShutdown(cspHand, bareCsp);
	return 0;
}


