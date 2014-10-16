/*
 * Simple test:
 *
 *  -- generate a key
 *  -- generate MAC
 *  -- verify MAC
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#define DATA_SIZE_DEF	100
#define LOOPS_DEF		10

#define KEY_ALG_DEF		CSSM_ALGID_SHA1HMAC
#define MAC_ALG_DEF		CSSM_ALGID_SHA1HMAC

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  d=dataSize (default = %d)\n", DATA_SIZE_DEF);
	printf("  l=loops (0=forever)\n");
	printf("  p=pauseInterval (default=0, no pause)\n");
	printf("  m (HMACMD5; default is HMACSHA1)\n");
	printf("  D (CSP/DL; default = bare CSP)\n");
	printf("  q(uiet)\n");
	printf("  v(erbose))\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int		 				arg;
	char					*argp;
	CSSM_CSP_HANDLE 		cspHand;
	CSSM_CC_HANDLE			macHand;
	CSSM_RETURN				crtn;
	CSSM_DATA				randData;
	CSSM_KEY_PTR			symmKey;
	CSSM_DATA				macData = {0, NULL};
	unsigned				loop;
	int 					i;
	unsigned 				dataSize = DATA_SIZE_DEF;
	unsigned				pauseInterval = 0;
	unsigned				loops = LOOPS_DEF;
	CSSM_BOOL				quiet = CSSM_FALSE;
	CSSM_BOOL				verbose = CSSM_FALSE;
	CSSM_BOOL				bareCsp = CSSM_TRUE;
	CSSM_ALGORITHMS			macAlg = MAC_ALG_DEF;
	CSSM_ALGORITHMS			keyAlg = KEY_ALG_DEF;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
	    switch(argv[arg][0]) {
			case 'd':
				dataSize = atoi(&argv[arg][2]);
				break;
		    case 'l':
				loops = atoi(&argv[arg][2]);
				break;
		    case 'p':
				pauseInterval = atoi(&argv[arg][2]);
				break;
			case 'm':
				keyAlg = macAlg = CSSM_ALGID_MD5HMAC;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	printf("Starting mactest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	symmKey = cspGenSymKey(cspHand,
		keyAlg,
		"noLabel",
		7,
		CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY,
		CSP_KEY_SIZE_DEFAULT,
		CSSM_TRUE);
	if(symmKey == 0) {
		printf("Error generating symmetric key; aborting.\n");
		exit(1);
	}
	randData.Data = (uint8 *)CSSM_MALLOC(dataSize);
	randData.Length = dataSize;
	simpleGenData(&randData, dataSize, dataSize);
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...Loop %d\n", loop);
		}
		crtn = CSSM_CSP_CreateMacContext(cspHand,
			macAlg,
			symmKey,
			&macHand);
		if(crtn) {
			printError("CSSM_CSP_CreateMacContext (1)", crtn);
			exit(1);
		}
		crtn = CSSM_GenerateMac(macHand,
			&randData,
			1,
			&macData);
		if(crtn) {
			printError("CSSM_GenerateMac error", crtn);
			exit(1);
		}
		crtn = CSSM_DeleteContext(macHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			exit(1);
		}
		crtn = CSSM_CSP_CreateMacContext(cspHand,
			macAlg,
			symmKey,
			&macHand);
		if(macHand == 0) {
			printError("CSSM_CSP_CreateMacContext (2)", crtn);
			exit(1);
		}
		crtn = CSSM_VerifyMac(macHand,
			&randData,
			1,
			&macData);
		if(crtn) {
			printError("CSSM_VerifyMac", crtn);
			exit(1);
		}
		crtn = CSSM_DeleteContext(macHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			exit(1);
		}
		if(loops && (loop == loops)) {
			break;
		}
		if(pauseInterval && ((loop % pauseInterval) == 0)) {
			char inch;
			fpurge(stdin);
			printf("Hit CR to proceed or q to quit: ");
			inch = getchar();
			if(inch == 'q') {
				break;
			}
		}
	}
	CSSM_FREE(randData.Data);
	crtn = CSSM_ModuleDetach(cspHand);
	if(crtn) {
		printError("CSSM_CSP_Detach", crtn);
		exit(1);
	}
	if(!quiet) {
		printf("OK\n");
	}
	return 0;
}
