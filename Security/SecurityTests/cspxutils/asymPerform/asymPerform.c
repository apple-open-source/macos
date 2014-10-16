/* 
 * asymPerform.c - measure performance of RSA and FEE encrypt and decrypt
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
#define ENC_LOOPS_DEF	1000	
#define KEYSIZE_DEF		1024
#define PTEXT_SIZE		20			/* e.g., a SHA1 digest */

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (r=RSA; f=FEED; F=FEEDExp; default=RSA)\n");
	printf("   l=numLoop (default=%d)\n", ENC_LOOPS_DEF);
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
	CSSM_DATA_PTR		ptext;		// different for each loop
	CSSM_DATA_PTR		ctext;		// ditto
	CSSM_DATA_PTR		rptext;		// ditto
	CSSM_RETURN			crtn;
	CFAbsoluteTime 		start, end;
	CSSM_CC_HANDLE		ccHand;
	unsigned			ctextSize;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_SIZE			processed;
	CSSM_DATA			remData;
	
	/*
	 * User-spec'd params
	 */
	uint32				keySizeInBits = KEYSIZE_DEF;
	unsigned			encLoops = ENC_LOOPS_DEF;
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_BOOL			quiet = CSSM_FALSE;
	CSSM_BOOL			bareCsp = CSSM_TRUE;
	CSSM_BOOL			rsaBlinding = CSSM_FALSE;
	CSSM_ALGORITHMS		keyAlg = CSSM_ALGID_RSA;
	CSSM_ALGORITHMS		encrAlg = CSSM_ALGID_RSA;
	CSSM_PADDING		padding = CSSM_PADDING_PKCS1;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'r':
						encrAlg = keyAlg = CSSM_ALGID_RSA;
						break;
					case 'f':
						encrAlg = CSSM_ALGID_FEED;
						keyAlg = CSSM_ALGID_FEE;
						padding = CSSM_PADDING_NONE;
						break;
					case 'F':
						encrAlg = CSSM_ALGID_FEEDEXP;
						keyAlg = CSSM_ALGID_FEE;
						padding = CSSM_PADDING_NONE;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				encLoops = atoi(&argp[2]);
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
	
	/* malloc encLoops ptext and ctext structs and the data they contain */
	ptext = (CSSM_DATA_PTR)CSSM_MALLOC(encLoops * sizeof(CSSM_DATA));
	ctext = (CSSM_DATA_PTR)CSSM_MALLOC(encLoops * sizeof(CSSM_DATA));
	rptext = (CSSM_DATA_PTR)CSSM_MALLOC(encLoops * sizeof(CSSM_DATA));
	memset(ptext, 0, encLoops * sizeof(CSSM_DATA));
	memset(ctext, 0, encLoops * sizeof(CSSM_DATA));
	memset(rptext, 0, encLoops * sizeof(CSSM_DATA));
	ctextSize = (keySizeInBits + 7) / 8;
	if(keyAlg != CSSM_ALGID_RSA) {
		ctextSize *= 8;
	}
	for(i=0; i<encLoops; i++) {
		appSetupCssmData(&ptext[i], PTEXT_SIZE);
		appSetupCssmData(&ctext[i], ctextSize);
	}
	
	/* generate random "digests" */
	for(i=0; i<encLoops; i++) {
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
		CSSM_KEYUSE_ANY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&privKey,
		CSSM_TRUE,
		CSSM_KEYUSE_ANY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);					// genSeed not used 
	if(crtn) {
		return testError(quiet);
	}
	
	printf("Encrypting....\n");
	
	/* set up a reusable crypt context */
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
				encrAlg,
				&creds,
				(encrAlg == CSSM_ALGID_FEED) ? &privKey : &pubKey,
				padding,
				&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateAsymmetricContext (1)", crtn);
		return 1;
	}
	if(encrAlg == CSSM_ALGID_FEED) {
		AddContextAttribute(ccHand, CSSM_ATTRIBUTE_PUBLIC_KEY, 
			sizeof(CSSM_KEY), CAT_Ptr, &pubKey, 0);
	}
	
	/* go - critical encrypt loop */
	start = CFAbsoluteTimeGetCurrent();
	for(i=0; i<encLoops; i++) {
		crtn = CSSM_EncryptData(ccHand,
			&ptext[i],
			1,
			&ctext[i],
			1,
			&processed,
			&remData);
		ctext[i].Length = processed;
		if(crtn) {
			printError("CSSM_EncryptData", crtn);
			return 1;
		}
	}
	end = CFAbsoluteTimeGetCurrent();
	printf("%d encr ops in %f seconds, %f ms/op\n", encLoops, end-start, 
		((end - start) * 1000.0) / encLoops);
	
	CSSM_DeleteContext(ccHand);
	
	/* set up a reusable encryption context */
	crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
				encrAlg,
				&creds,
				&privKey,
				padding,
				&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateAsymmetricContext (2)", crtn);
		return 1;
	}
	if(rsaBlinding) {
		CSSM_CONTEXT_ATTRIBUTE	newAttr;	
		newAttr.AttributeType     = CSSM_ATTRIBUTE_RSA_BLINDING;
		newAttr.AttributeLength   = sizeof(uint32);
		newAttr.Attribute.Uint32  = 1;
		crtn = CSSM_UpdateContextAttributes(ccHand, 1, &newAttr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	if(encrAlg == CSSM_ALGID_FEED) {
		AddContextAttribute(ccHand, CSSM_ATTRIBUTE_PUBLIC_KEY, 
			sizeof(CSSM_KEY), CAT_Ptr, &pubKey, 0);
	}

	/* go - critical decrypt loop */
	start = CFAbsoluteTimeGetCurrent();
	for(i=0; i<encLoops; i++) {
		crtn = CSSM_DecryptData(ccHand,
			&ctext[i],
			1,
			&rptext[i],
			1,
			&processed,
			&remData);
		if(crtn) {
			printError("CSSM_DecryptData", crtn);
			return 1;
		}
	}
	end = CFAbsoluteTimeGetCurrent();
	printf("%d decr ops in %f seconds, %f ms/op\n", encLoops, end-start, 
		((end - start) * 1000.0) / encLoops);
	CSSM_DeleteContext(ccHand);

	cspFreeKey(cspHand, &privKey);
	cspFreeKey(cspHand, &pubKey);
	for(i=0; i<encLoops; i++) {
		appFreeCssmData(&ptext[i], CSSM_FALSE);
		appFreeCssmData(&ctext[i], CSSM_FALSE);
		appFreeCssmData(&rptext[i], CSSM_FALSE);
	}
	CSSM_FREE(ptext);
	CSSM_FREE(ctext);
	CSSM_FREE(rptext);
	cspShutdown(cspHand, bareCsp);
	return 0;
}


