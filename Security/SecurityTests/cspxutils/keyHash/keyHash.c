/*
 * keyHash.c - simple test of CSSM_APPLECSP_KEYDIGEST passthrough
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
#define LOOPS_DEF			10

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("  D (CSP/DL; default = bare CSP)\n");
	printf("  p(ause on each loop)\n");
	printf("  q(uiet)\n");
	printf("  v(erbose))\n");
	exit(1);
}

static void dumpBuf(uint8 *buf,
	unsigned len)
{
	unsigned i;
	
	printf("   ");
	for(i=0; i<len; i++) {
		printf("%02X ", buf[i]);
		if((i % 24) == 23) {
			printf("\n      ");
		}
	}
	printf("\n");
}

/*
 * Given a ref key:
 *  NULL wrap to data key;
 *  obtain hash of both keys;
 *  ensure hashes are equal;
 */
static int doTest(
	CSSM_CSP_HANDLE		cspHand,		// raw or CSPDL
	CSSM_CSP_HANDLE		rawCspHand,		// raw, may be same as cspHand
	CSSM_KEY_PTR		refKey,
	CSSM_BOOL			verbose,
	CSSM_BOOL			quiet)
{
	CSSM_KEY		rawKey;
	CSSM_RETURN		crtn;
	CSSM_DATA_PTR	refHash;
	CSSM_DATA_PTR	rawHash;
	
	if(refKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
		printf("Hey! this only works on ref keys!!\n");
		exit(1);
	}
	
	/* get raw key */
	crtn = cspWrapKey(cspHand,
		refKey,
		NULL,			// wrappingKey
		CSSM_ALGID_NONE,
		CSSM_ALGMODE_NONE,
		CSSM_KEYBLOB_WRAPPED_FORMAT_NONE,
		CSSM_PADDING_NONE,
		NULL,			// iv
		NULL,			// descData
		&rawKey);
	if(crtn) {
		return testError(quiet);
	}
	
	/* hash of both keys */
	crtn = cspKeyHash(cspHand, refKey, &refHash);
	if(crtn) {
		return testError(quiet);
	}
	else {
		if(verbose) {
			printf("      ...Ref key hash:\n       ");
			dumpBuf(refHash->Data, refHash->Length);
		}
	}
	crtn = cspKeyHash(rawCspHand, &rawKey, &rawHash);
	if(crtn) {
		return testError(quiet);
	}
	else {
		if(verbose) {
			printf("      ...Raw key hash:\n       ");
			dumpBuf(rawHash->Data, rawHash->Length);
		}
	}
	if(!appCompareCssmData(refHash, rawHash)) {
		printf("***Key Hash Miscompare!\n");
		return testError(quiet);
	}
	appFreeCssmData(refHash, CSSM_TRUE);
	appFreeCssmData(rawHash, CSSM_TRUE);
	cspFreeKey(cspHand, &rawKey);
	return 0;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_CSP_HANDLE 	rawCspHand;
	int					rtn = 0;
	CSSM_KEY			pubKey;
	CSSM_KEY			privKey;
	CSSM_KEY_PTR		symKey;
	int					i;
	
	/*
	 * User-spec'd params
	 */
	unsigned			loops = LOOPS_DEF;
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_BOOL			quiet = CSSM_FALSE;
	CSSM_BOOL			bareCsp = CSSM_TRUE;
	CSSM_BOOL			doPause = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
		    case 'p':
		    	doPause = CSSM_TRUE;
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

	printf("Starting keyHash; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	if(bareCsp) {
		rawCspHand = cspHand;
	}
	else {
		rawCspHand = cspDlDbStartup(CSSM_TRUE, NULL);
		if(rawCspHand == 0) {
			exit(1);
		}
	}
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...loop %d\n", loop);
		}
		
		/* first with symmetric key */
		if(verbose) {
			printf("   ...testing DES key\n");
		}
		symKey = cspGenSymKey(cspHand,
			CSSM_ALGID_DES,
			USAGE_NAME,
			USAGE_NAME_LEN,
			CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
			64,
			CSSM_TRUE);			// refKey
		if(symKey == NULL) {
			if(testError(quiet)) {
				break;
			}
		}
		if(doTest(cspHand, rawCspHand, symKey, verbose, quiet)) {
			break;
		}
		CSSM_FreeKey(cspHand, NULL, symKey, CSSM_TRUE);
		CSSM_FREE(symKey);
		
		/* cook up an RSA key pair */
		rtn = cspGenKeyPair(cspHand,
				CSSM_ALGID_RSA,
				USAGE_NAME,
				USAGE_NAME_LEN,
				CSP_RSA_KEY_SIZE_DEFAULT,
				&pubKey,
				CSSM_TRUE,			// pubIsRef
				CSSM_KEYUSE_ENCRYPT,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				&privKey,
				CSSM_TRUE,			// privIsRef
				CSSM_KEYUSE_DECRYPT,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				CSSM_FALSE);		// genSeed
		if(rtn) {
			if(testError(quiet)) {
				break;
			}
		}
		if(verbose) {
			printf("   ...testing RSA public key\n");
		}
		if(doTest(cspHand, rawCspHand, &pubKey, verbose, quiet)) {
			break;
		}
		if(verbose) {
			printf("   ...testing RSA private key\n");
		}
		if(doTest(cspHand, rawCspHand, &privKey, verbose, quiet)) {
			break;
		}
		CSSM_FreeKey(cspHand, NULL, &pubKey, CSSM_TRUE);
		CSSM_FreeKey(cspHand, NULL, &privKey, CSSM_TRUE);
		if(loops && (loop == loops)) {
			break;
		}
		if(doPause) {
			fpurge(stdin);
			printf("Hit CR to proceed: ");
			getchar();
		}
	}
	return 0;
}
