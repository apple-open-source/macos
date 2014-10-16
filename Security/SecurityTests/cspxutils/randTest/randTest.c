/*
 * Simple interactive CSP RNG exerciser. 
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"

#define RAND_ALG		CSSM_ALGID_APPLE_YARROW
#define BUFSIZE			32

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  D (CSP/DL; default = bare CSP)\n");
	exit(1);
}

static void dumpBuf(uint8 *buf,
	unsigned len)
{
	unsigned i;
	
	printf("   ");
	for(i=0; i<len; i++) {
		printf("%02X  ", buf[i]);
		if((i % 8) == 7) {
			printf("\n   ");
		}
	}
	printf("\n");
}

void doGenRand(
	CSSM_CSP_HANDLE			cspHand,
	CSSM_CC_HANDLE			ccHand, 	// 0 ==> gen a new one
										// else use this
	CSSM_CRYPTO_DATA_PTR	seed,		// optional
	unsigned				len,
	CSSM_BOOL				weMalloc)
{
	CSSM_RETURN				crtn;
	CSSM_DATA 				data = {0, NULL};
	
	/* optional existing context */
	if(ccHand == 0) {
		crtn = CSSM_CSP_CreateRandomGenContext(
			cspHand,
			RAND_ALG,
			seed,
			len,
			&ccHand);
		if(crtn) {
			printError("CSSM_CSP_CreateRandomGenContext", crtn);
			return;
		}
	}
	
	/* who mallocs the data? */
	if(weMalloc) {
		data.Data = (uint8 *)appMalloc(len, NULL);
		if(data.Data == NULL) {
			printf("***malloc failure\n");
			return;
		}
		data.Length = len;
	}
	
	/* go for it */
	crtn = CSSM_GenerateRandom(ccHand, &data);
	if(crtn) {
		printError("CSSM_GenerateRandom", crtn);
		return;
	}
	
	dumpBuf(data.Data, data.Length);
	appFree(data.Data, NULL);
	return;
}

#define SEED_SIZE	32

/*
 * CryptoData callback for optional random seed.
 */
CSSM_RETURN seedCallback(
	CSSM_DATA_PTR OutData, 
	void *CallerCtx)
{
	int i, j;
	static unsigned char	seed[SEED_SIZE];
	
	OutData->Length = SEED_SIZE;
	OutData->Data = seed;
	for(i=SEED_SIZE, j=0; i>0; i--, j++) {
		seed[j] = i;
	}
	return CSSM_OK;
}

int main(int argc, char **argv)
{
	int		 				arg;
	char					*argp;
	CSSM_CSP_HANDLE 		cspHand;
	CSSM_CC_HANDLE			ccHand = 0;
	CSSM_RETURN				crtn;
	CSSM_BOOL				bareCsp = CSSM_TRUE;
	char					resp = 'n';			
						// initial op = get random data
	CSSM_BOOL				weMalloc = CSSM_FALSE;
	unsigned char			seed[SEED_SIZE];
	CSSM_CRYPTO_DATA		cseed;
	int 					i;
	unsigned				reqLen = 16;
	CSSM_BOOL				explicitSeed;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
	    switch(argv[arg][0]) {
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
			default:
				usage(argv);
		}
	}
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	
	/* set up for explicit seed */
	cseed.Param.Length = SEED_SIZE;
	cseed.Param.Data = seed;
	for(i=0; i<SEED_SIZE; i++) {
		seed[i] = i;
	}
	cseed.Callback = NULL;
	cseed.CallerCtx = NULL;
	explicitSeed = CSSM_TRUE;
	
	while(1) {
		printf(" g   Get random data w/seed\n");
		printf(" n   Get random data w/o seed\n");
		printf(" c   Get random data using current context\n");
		printf(" m   we malloc (current: %s)\n",
			weMalloc ? "True" : "False");
		printf(" M   CSP mallocs\n");
		printf(" s   seed via explicit data (current: %d)\n", (int)explicitSeed);
		printf(" S   seed via callback\n");
		printf(" q   quit\n");
		fpurge(stdin);
		printf("\ncommand me: ");
	nextChar:
		resp = getchar();
		if(resp == 'q') {
			break;
		}
		switch(resp) {
			case 'g':
				if(ccHand != 0) {
					crtn = CSSM_DeleteContext(ccHand);
					if(crtn) {
						printError("CSSM_DeleteContext", crtn);
					}
					ccHand = 0;
				}
				doGenRand(cspHand, ccHand, &cseed, reqLen, weMalloc);
				break;
			case 'n':
				if(ccHand != 0) {
					crtn = CSSM_DeleteContext(ccHand);
					if(crtn) {
						printError("CSSM_DeleteContext", crtn);
					}
					ccHand = 0;
				}
				doGenRand(cspHand, ccHand, NULL, reqLen, weMalloc);
				break;
			case 'c':
				doGenRand(cspHand, ccHand, NULL, reqLen, weMalloc);
				break;
			case 'm':
				weMalloc = CSSM_TRUE;
				break;
			case 'M':
				weMalloc = CSSM_FALSE;
				break;
			case 'l':
				printf("New length: ");
				scanf("%d", &reqLen);
				break;
			case 's':
				/* explicit seed - presence of callback is the determinant */
				cseed.Callback = NULL;
				explicitSeed = CSSM_TRUE;
				break;
			case 'S':
				/* seed by calllback */
				cseed.Callback = seedCallback;
				explicitSeed = CSSM_FALSE;
				break;
			case '\n':
				goto nextChar;
			default:
				printf("Huh?\n");
		}
	}
	if((crtn = CSSM_ModuleDetach(cspHand))) {
		printError("CSSM_ModuleDetach", crtn);
		exit(1);
	}
	return 0;
}
