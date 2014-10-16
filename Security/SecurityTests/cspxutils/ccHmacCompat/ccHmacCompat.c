/* 
 * ccHmacCompat.c - test compatibilty of CommonCrypto's HMAC implementation with
 *					openssl.
 *
 * Written by Doug Mitchell. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "common.h"
#include <string.h>
#include <CommonCrypto/CommonHMAC.h>
#include <openssl/hmac.h>

/* SHA2-based HMAC testing disabled until openssl provides it */
#define HMAC_SHA2_ENABLE	0

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
#if 	HMAC_SHA2_ENABLE
#define ALG_LAST			ALG_SHA512
#else
#define ALG_LAST			ALG_SHA1
#endif	/* HMAC_SHA2_ENABLE */

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
	printf("   z (keys and plaintext all zeroes)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* 
 * Test harness for CCCryptor/HMAC with lots of options. 
 */
static void doHmacCC(
	CCHmacAlgorithm hmacAlg, bool randUpdates,		
	const void *keyBytes, size_t keyLen,
	const uint8_t *inText, size_t inTextLen,
	uint8_t *outText)		/* returned, caller mallocs */
{
	CCHmacContext ctx;
	size_t toMove;			/* total to go */
	const uint8 *inp;
	
	if(!randUpdates) {
		/* one shot */
		CCHmac(hmacAlg, keyBytes, keyLen, inText, inTextLen, outText);
		return;
	}
	
	/* random multi updates */
	CCHmacInit(&ctx, hmacAlg, keyBytes, keyLen);
	
	toMove = inTextLen;		/* total to go */
	inp = (const uint8 *)inText;
	
	while(toMove) {
		uint32 thisMoveIn;			/* input to CCryptUpdate() */
		
		thisMoveIn = genRand(1, toMove);
		logSize(("###ptext segment len %lu\n", (unsigned long)thisMoveIn)); 
		CCHmacUpdate(&ctx, inp, thisMoveIn);
		inp			+= thisMoveIn;
		toMove		-= thisMoveIn;
	}
	
	CCHmacFinal(&ctx, outText);
}

/* 
 * Produce HMAC with reference implementation (currently, openssl)
 */
static int doHmacRef(
	CCHmacAlgorithm hmacAlg,			
	const void *keyBytes, size_t keyLen,
	const uint8_t *inText, size_t inTextLen,
	uint8_t *outText, size_t *outTextLen)			/* caller mallocs */
{
	const EVP_MD *md;
	
	switch(hmacAlg) {
		case kCCHmacAlgMD5:
			md = EVP_md5();
			break;
		case kCCHmacAlgSHA1:
			md = EVP_sha1();
			break;
		#if		HMAC_SHA2_ENABLE
		case kCCHmacAlgSHA224:
			md = EVP_sha224();
			break;
		case kCCHmacAlgSHA256:
			md = EVP_sha256();
			break;
		case kCCHmacAlgSHA384:
			md = EVP_sha384();
			break;
		case kCCHmacAlgSHA512:
			md = EVP_sha512();
			break;
		#endif	/* HMAC_SHA2_ENABLE */
		default:
			printf("***Bad hmacAlg (%d)\n", (int)hmacAlg);
			return -1;
	}
	unsigned md_len = *outTextLen;
	HMAC(md, keyBytes, (int)keyLen, 
		(const unsigned char *)inText, (int)inTextLen, 
		(unsigned char *)outText, &md_len);
	*outTextLen = md_len;
	return 0;
}


#define LOG_FREQ		20
#define MAX_HMAC_SIZE	CC_SHA512_DIGEST_LENGTH		

static int doTest(const uint8_t *ptext,
	size_t ptextLen,
	CCHmacAlgorithm hmacAlg,			
	uint32 keySizeInBytes,
	bool staged,
	bool allZeroes,
	bool quiet)
{
	uint8_t			keyBytes[MAX_KEY_SIZE];
	uint8_t			hmacCC[MAX_HMAC_SIZE];
	size_t			hmacCCLen;
	uint8_t			hmacRef[MAX_HMAC_SIZE];
	size_t			hmacRefLen;
	int				rtn = 0;
	
	if(allZeroes) {
		memset(keyBytes, 0, keySizeInBytes);
	}
	else {
		/* random key */
		appGetRandomBytes(keyBytes, keySizeInBytes);
	}
	
	hmacCCLen = MAX_HMAC_SIZE;
	doHmacCC(hmacAlg, staged,
		keyBytes, keySizeInBytes, 
		ptext, ptextLen,
		hmacCC);

	hmacRefLen = MAX_HMAC_SIZE;
	rtn = doHmacRef(hmacAlg, 
		keyBytes, keySizeInBytes, 
		ptext, ptextLen,
		hmacRef, &hmacRefLen);
	if(rtn) {
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	
	if(memcmp(hmacRef, hmacCC, hmacRefLen)) {
		printf("***data miscompare\n");
		rtn = testError(quiet);
	}
abort:
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
	bool				staged;
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
	bool		stagedSpec = false;			// ditto for stagedEncr and stagedDecr
	bool		allZeroes = false;
	
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
		    	staged = false;
				stagedSpec = true;
				break;
			case 'z':
				allZeroes = true;
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
	if(allZeroes) {
		memset(ptext, 0, maxPtextSize);
	}
	
	printf("Starting ccHmacCompat; args: ");
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
			if(!allZeroes) {
				appGetRandomBytes(ptext, ptextLen);
			}
			if(!keySizeSpec) {
				keySizeInBytes = genRand(MIN_KEY_SIZE, MAX_KEY_SIZE);
			}
			
			/* per-loop settings */
			if(!stagedSpec) {
				staged = isBitSet(1, loop);
			}
			
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %d ptextLen %lu  keySize %lu  staged=%d\n",
						loop, (unsigned long)ptextLen, (unsigned long)keySizeInBytes,
						(int)staged);
				}
			}
			
			if(doTest(ptext, ptextLen,
					hmacAlg, keySizeInBytes,
					staged, allZeroes, quiet)) {
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


