/* 
 * ccHmacClone - test CommonCrypto's clone context for HMAC.  
 *
 * Written 3/30/2006 by Doug Mitchell. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "common.h"
#include <string.h>
#include <CommonCrypto/CommonHMAC.h>

/*
 * Defaults.
 */
#define LOOPS_DEF		200

#define MIN_DATA_SIZE	8
#define MAX_DATA_SIZE	10000			/* bytes */
#define MIN_KEY_SIZE	1
#define MAX_KEY_SIZE	256				/* bytes */
#define LOOP_NOTIFY		20

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_MD5 = 1,
	ALG_SHA1,
	ALG_SHA224,
	ALG_SHA256,
	ALG_SHA384,
	ALG_SHA512,
} HmacAlg;
#define ALG_FIRST			ALG_MD5
#define ALG_LAST			ALG_SHA512

#define LOG_SIZE			0
#if		LOG_SIZE
#define logSize(s)	printf s
#else
#define logSize(s)
#endif

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (5=MD5; s=SHA1; 4=SHA224; 2=SHA256; 3=SHA384; 1=SHA512; default=all)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   k=keySizeInBytes\n");
	printf("   m=maxPtextSize (default=%d)\n", MAX_DATA_SIZE);
	printf("   n=minPtextSize (default=%d)\n", MIN_DATA_SIZE);
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   s (all ops single-shot, not staged)\n");
	printf("   S (all ops staged)\n");
	printf("   z (keys and plaintext all zeroes)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* 
 * Given an initialized CCHmacContext, feed it some data and get the result.
 */
static void hmacRun(
	CCHmacContext *ctx,
	bool randomUpdates,
	const unsigned char *ptext,
	size_t ptextLen,
	void *dataOut)
{
	while(ptextLen) {
		size_t thisMoveIn;			/* input to CCryptUpdate() */
		
		if(randomUpdates) {
			thisMoveIn = genRand(1, ptextLen);
		}
		else {
			thisMoveIn = ptextLen;
		}
		logSize(("###ptext segment (1) len %lu\n", (unsigned long)thisMoveIn)); 
		CCHmacUpdate(ctx, ptext, thisMoveIn);
		ptext	 += thisMoveIn;
		ptextLen -= thisMoveIn;
	}
	CCHmacFinal(ctx, dataOut);
}


#define MAX_HMAC_SIZE	CC_SHA512_DIGEST_LENGTH

static int doTest(const uint8_t *ptext,
	size_t ptextLen,
	CCHmacAlgorithm hmacAlg,			
	uint32 keySizeInBytes,
	bool stagedOrig,
	bool stagedClone,
	bool quiet,
	bool verbose)
{
	uint8_t			keyBytes[MAX_KEY_SIZE];
	uint8_t			hmacOrig[MAX_HMAC_SIZE];
	uint8_t			hmacClone[MAX_HMAC_SIZE];
	int				rtn = 0;
	CCHmacContext	ctxOrig;
	CCHmacContext	ctxClone;
	unsigned		die;		/* 0..3 indicates when to clone */
	unsigned		loopNum = 0;
	size_t			hmacLen;
	bool			didClone = false;
	
	switch(hmacAlg) {
		case kCCHmacAlgSHA1:
			hmacLen = CC_SHA1_DIGEST_LENGTH;
			break;
		case kCCHmacAlgMD5:
			hmacLen = CC_MD5_DIGEST_LENGTH;
			break;
		case kCCHmacAlgSHA224:
			hmacLen = CC_SHA224_DIGEST_LENGTH;
			break;
		case kCCHmacAlgSHA256:
			hmacLen = CC_SHA256_DIGEST_LENGTH;
			break;
		case kCCHmacAlgSHA384:
			hmacLen = CC_SHA384_DIGEST_LENGTH;
			break;
		case kCCHmacAlgSHA512:
			hmacLen = CC_SHA512_DIGEST_LENGTH;
			break;
		default:
			printf("***BRRRZAP!\n");
			exit(1);
	}
	
	/* random key */
	appGetRandomBytes(keyBytes, keySizeInBytes);
	
	/* cook up first context */
	CCHmacInit(&ctxOrig, hmacAlg, keyBytes, keySizeInBytes);
	
	/* roll the dice */
	die = genRand(0, 3);
	
	/* 
	 * In this loop we do updates to the ctxOrig up until we
	 * clone it, then we use hmacRun to finish both of them.
	 */
	while(ptextLen) {
		if((die == loopNum) || !stagedOrig) {
			/* make the clone now */
			if(verbose) {
				printf("   ...cloning at loop %u\n", loopNum);
			}
			ctxClone = ctxOrig;
			didClone = true;
			
			/* do all of the clone's updates and final here */
			hmacRun(&ctxClone, stagedClone, ptext, ptextLen, hmacClone);
			
			/* now do all remaining updates and final for original */
			hmacRun(&ctxOrig, stagedOrig, ptext, ptextLen, hmacOrig);
			
			/* we're all done, time to check the HMAC values */
			break;
		}	/* making clone */
		
		/* feed some data into cryptorOrig */
		size_t thisMove;
		if(stagedOrig) {
			thisMove = genRand(1, ptextLen);
		}
		else {
			thisMove = ptextLen;
		}
		logSize(("###ptext segment (2) len %lu\n", (unsigned long)thisMove)); 
		CCHmacUpdate(&ctxOrig, ptext, thisMove);
		ptext += thisMove;
		ptextLen -= thisMove;
		loopNum++;
	}
		
	/* 
	 * It's possible to get here without cloning or doing any finals,
	 * if we ran thru multiple updates and finished ptextLen for cryptorOrig
	 * before we hit the cloning spot.
	 */
	if(!didClone) {
		if(!quiet) {
			printf("...ctxOrig finished before we cloned; skipping test\n");
		}
		return 0;
	}
	if(memcmp(hmacOrig, hmacClone, hmacLen)) {
		printf("***data miscompare\n");
		rtn = testError(quiet);
	}
	return rtn;
}

bool isBitSet(unsigned bit, unsigned word) 
{
	if(bit > 31) {
		printf("We don't have that many bits\n");
		exit(1);
	}
	unsigned mask = 1 << bit;
	return (word & mask) ? true : false;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	uint8				*ptext;
	size_t				ptextLen;
	bool				stagedOrig;
	bool				stagedClone;
	const char			*algStr;
	CCHmacAlgorithm		hmacAlg;	
	int					i;
	int					currAlg;		// ALG_xxx
	uint32				keySizeInBytes;
	int					rtn = 0;
	
	/*
	 * User-spec'd params
	 */
	bool		keySizeSpec = false;		// false: use rand key size
	HmacAlg		minAlg = ALG_FIRST;
	HmacAlg		maxAlg = ALG_LAST;
	unsigned	loops = LOOPS_DEF;
	bool		verbose = false;
	size_t		minPtextSize = MIN_DATA_SIZE;
	size_t		maxPtextSize = MAX_DATA_SIZE;
	bool		quiet = false;
	unsigned	pauseInterval = 0;
	bool		stagedSpec = false;		// true means caller fixed stagedOrig and stagedClone
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case '5':
						minAlg = maxAlg = ALG_MD5;
						break;
					case 's':
						minAlg = maxAlg = ALG_SHA1;
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
					case '1':
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
				minPtextSize = atoi(&argp[2]);
				break;
		    case 'm':
				maxPtextSize = atoi(&argp[2]);
				break;
		    case 'k':
		    	keySizeInBytes = atoi(&argp[2]);
		    	keySizeSpec = true;
				break;
		    case 'v':
		    	verbose = true;
				break;
		    case 'q':
		    	quiet = true;
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
				break;
		    case 's':
		    	stagedOrig = stagedClone = false;
				stagedSpec = true;
				break;
		    case 'S':
		    	stagedOrig = stagedClone = true;
				stagedSpec = true;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext = (uint8 *)malloc(maxPtextSize);
	if(ptext == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	/* ptext length set in test loop */
	
	printf("Starting ccHmacClone; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	
	if(pauseInterval) {
		fpurge(stdin);
		printf("Top of test; hit CR to proceed: ");
		getchar();
	}

	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		/* when zero, set size randomly or per user setting */
		switch(currAlg) {
			case ALG_MD5:
				hmacAlg = kCCHmacAlgMD5;
				algStr = "HMACMD5";
				break;
			case ALG_SHA1:
				hmacAlg = kCCHmacAlgSHA1;
				algStr = "HMACSHA1";
				break;
			case ALG_SHA224:
				hmacAlg = kCCHmacAlgSHA224;
				algStr = "HMACSHA224";
				break;
			case ALG_SHA256:
				hmacAlg = kCCHmacAlgSHA256;
				algStr = "HMACSHA256";
				break;
			case ALG_SHA384:
				hmacAlg = kCCHmacAlgSHA384;
				algStr = "HMACSHA384";
				break;
			case ALG_SHA512:
				hmacAlg = kCCHmacAlgSHA512;
				algStr = "HMACSHA512";
				break;
			default:
				printf("***BRRZAP!\n");
				exit(1);
		}
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			ptextLen = genRand(minPtextSize, maxPtextSize);
			appGetRandomBytes(ptext, ptextLen);
			if(!keySizeSpec) {
				keySizeInBytes = genRand(MIN_KEY_SIZE, MAX_KEY_SIZE);
			}
			
			/* per-loop settings */
			if(!stagedSpec) {
				stagedOrig = isBitSet(1, loop);
				stagedClone = isBitSet(2, loop);
			}
			
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %d ptextLen %4lu keySize %3lu stagedOrig=%d "
						"stagedClone=%d\n", 
						loop, (unsigned long)ptextLen, (unsigned long)keySizeInBytes,
						(int)stagedOrig, (int)stagedClone);
				}
			}
			
			if(doTest(ptext, ptextLen,
					hmacAlg, keySizeInBytes,
					stagedOrig, stagedClone, quiet, verbose)) {
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
	if(pauseInterval) {
		fpurge(stdin);
		printf("ModuleDetach/Unload complete; hit CR to exit: ");
		getchar();
	}
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	free(ptext);
	return rtn;
}


