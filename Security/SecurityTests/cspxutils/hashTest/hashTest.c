/* Copyright (c) 1998,2003-2006,2008 Apple Inc.
 *
 * hashTest.c - test CDSA digest functions.
 *
 * Revision History
 * ----------------
 *   4 May 2000  Doug Mitchell
 *		Ported to X/CDSA2. 
 *  12 May 1998	Doug Mitchell at Apple
 *		Created.
 */
/*
 * text size =       {random, from 100 bytes to 100k, in
 *                   geometrical steps, i.e. the number of
 *                   bytes would be 10^r, where r is random out of
 *                   {2,3,4,5,6}, plus a random integer in {0,..99}};
 *
 * for loop_count
 *     text contents = {random data, random size as specified above};
 *     generate digest in one shot;
 *	   generate digest with multiple random-sized updates;
 *     verify digests compare;
 *     for various bytes of text {
 *        corrupt text byte;
 *		  generate digest in one shot;
 *		  veridy digest is different;
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
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"

/*
 * Defaults.
 */
#define LOOPS_DEF		50
#define MIN_EXP			2		/* for data size 10**exp */
#define DEFAULT_MAX_EXP	3
#define MAX_EXP			5
#define INCR_DEFAULT	0		/* munge every incr bytes - zero means
								 * "adjust per ptext size" */
typedef enum {
	ALG_MD2 = 1,
	ALG_MD5,
	ALG_SHA1,
	ALG_SHA224,
	ALG_SHA256,
	ALG_SHA384,
	ALG_SHA512
};

#define ALG_FIRST		ALG_MD2
#define ALG_LAST		ALG_SHA512
#define MAX_DATA_SIZE	(100000 + 100)	/* bytes */
#define LOOP_NOTIFY		20

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (s=SHA1; m=MD5; M=MD2; 4=SHA224; 2=SHA256; 3=SHA384; 5=SHA512; "
					"default=all\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=%d, max=%d)\n", DEFAULT_MAX_EXP, MAX_EXP);
	printf("   i=increment (default=%d)\n", INCR_DEFAULT);
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   z (zero data)\n");
	printf("   I (incrementing data)\n");
	printf("   g (good digest only)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

#define LOG_FREQ	20

static int doTest(CSSM_CSP_HANDLE cspHand,
	uint32 alg,
	CSSM_DATA_PTR ptext,
	CSSM_BOOL verbose,
	CSSM_BOOL quiet,
	CSSM_BOOL mallocDigest,
	unsigned incr,
	CSSM_BOOL goodOnly)
{
	CSSM_DATA 		refDigest = {0, NULL};
	CSSM_DATA		testDigest = {0, NULL};
	unsigned		length;
	unsigned		byte;
	unsigned char	*data;
	unsigned char	origData;
	unsigned char	bits;
	int				rtn = 0;
	CSSM_RETURN		crtn;
	unsigned		loop = 0;
	
	/*
	 *     generate digest in one shot;
	 *	   generate digest with multiple random-sized updates;
	 *     verify digests compare;
	 *     for various bytes of ptext {
	 *        corrupt ptext byte;
	 *		  generate digest in one shot;
	 *		  verify digest is different;
	 *        restore corrupted byte;
	 *     }
	 */
	crtn = cspDigest(cspHand,
		alg,
		mallocDigest,
		ptext,
		&refDigest);
	if(crtn) {
		rtn = testError(quiet);
		goto abort;
	}
	crtn = cspStagedDigest(cspHand,
		alg,
		mallocDigest,
		CSSM_TRUE,			// multi updates
		ptext,
		&testDigest);
	if(crtn) {
		rtn = testError(quiet);
		goto abort;
	}
	if(refDigest.Length != testDigest.Length) {
		printf("Digest length mismatch (1)\n");
		rtn = testError(quiet);
		goto abort;
	}
	if(memcmp(refDigest.Data, testDigest.Data, refDigest.Length)) {
		printf("Digest miscompare (1)\n");
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	if(goodOnly) {
		rtn = 0;
		goto abort;
	}
	appFreeCssmData(&testDigest, CSSM_FALSE);
	testDigest.Length = 0;
	data = (unsigned char *)ptext->Data;
	length = ptext->Length;
	for(byte=0; byte<length; byte += incr) {
		if(verbose && ((loop++ % LOG_FREQ) == 0)) {
			printf("....byte %d\n", byte);
		}
		origData = data[byte];
		/*
		 * Generate random non-zero byte
		 */
		do {
			bits = genRand(1, 0xff) & 0xff;
		} while(bits == 0);
		data[byte] ^= bits;
		crtn = cspDigest(cspHand,
			alg,
			mallocDigest,
			ptext,
			&testDigest);
		if(crtn) {
			rtn = testError(quiet);
			break;
		}
		if(!memcmp(refDigest.Data, testDigest.Data, refDigest.Length)) {
			printf("Unexpected digest compare\n");
			rtn = testError(quiet);
			break;
		}
		appFreeCssmData(&testDigest, CSSM_FALSE);
		testDigest.Length = 0;
		data[byte] = origData;
	}
abort:
	/* free digests */
	if(refDigest.Length) {
		appFreeCssmData(&refDigest, CSSM_FALSE);
	}
	if(testDigest.Length) {
		appFreeCssmData(&testDigest, CSSM_FALSE);
	}
	return rtn;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned 			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_BOOL			mallocDigest;
	const char			*algStr;
	uint32				alg;		// ALG_MD5, etc.
	uint32				cssmAlg;	// CSSM_ALGID_MD5, etc.
	unsigned			actualIncr;
	int					i;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	unsigned	minExp = MIN_EXP;
	unsigned	maxExp = DEFAULT_MAX_EXP;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	incr = INCR_DEFAULT;
	unsigned	minAlg = ALG_FIRST;
	unsigned	maxAlg = ALG_LAST;
	unsigned	pauseInterval = 0;
	dataType 	dt;
	CSSM_BOOL	goodOnly = CSSM_FALSE;
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
			case 'z':
				dt = DT_Zero;
				break;
			case 'I':
				dt = DT_Increment;
				break;
			case 'g':
				goodOnly = CSSM_TRUE;
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
	
	printf("Starting hashTest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
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
			ptext.Length = genData(ptext.Data, minExp, maxExp, dt);
			if(incr == 0) {
				/* adjust increment as appropriate */
				actualIncr = (ptext.Length / 50) + 1;
			}
			else {
				actualIncr = incr;
			}
			/* mix up mallocing */
			mallocDigest = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %d text size %lu mallocDigest %d\n",
						loop, (unsigned long)ptext.Length, (int)mallocDigest);
				}
			}
			if(doTest(cspHand,
					cssmAlg,
					&ptext,
					verbose,
					quiet,
					mallocDigest,
					actualIncr,
					goodOnly)) {
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
	if(!quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return 0;
}
