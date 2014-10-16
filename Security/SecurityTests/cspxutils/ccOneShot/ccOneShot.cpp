/* 
 * ccOneShot.c - Ensure that one-shot CommonDigest routines behave correctly.
 *
 * Written 3/31/06 by Doug Mitchell. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "common.h"
#include <string.h>
#include <CommonCrypto/CommonDigest.h>
#include <openssl/hmac.h>

/*
 * Defaults.
 */
#define LOOPS_DEF		200
#define MIN_DATA_SIZE	1
#define MAX_DATA_SIZE	10000			/* bytes */
#define LOOP_NOTIFY		20

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_MD2 = 1,
	ALG_MD4,
	ALG_MD5,
	ALG_SHA1,
	ALG_SHA224,
	ALG_SHA256,
	ALG_SHA384,
	ALG_SHA512
} HashAlg;
#define ALG_FIRST			ALG_MD2
#define ALG_LAST			ALG_SHA512

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (default %d)\n", LOOPS_DEF);
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* the context pointers are void * here for polymorphism later on */
typedef int (*initFcn)(void *ctx);
typedef int (*updateFcn)(void *ctx, const void *data, CC_LONG len);
typedef int (*finalFcn)(unsigned char *md, void *ctx);
typedef unsigned char (*oneShotFcn)(const void *data, CC_LONG len, unsigned char *md);

typedef struct {
	HashAlg				alg;
	const char			*algName;
	size_t				digestSize;
	initFcn				init;
	updateFcn			update;
	finalFcn			final;
	oneShotFcn			oneShot;
} CommonDigestInfo;

/* casts are necessary to cover the void* context args */
static const CommonDigestInfo digests[] = 
{
	{	ALG_MD2, "MD2", CC_MD2_DIGEST_LENGTH,
		(initFcn)CC_MD2_Init, (updateFcn)CC_MD2_Update,
		(finalFcn)CC_MD2_Final, (oneShotFcn)CC_MD2
	},
	{	ALG_MD4, "MD4", CC_MD4_DIGEST_LENGTH,
		(initFcn)CC_MD4_Init, (updateFcn)CC_MD4_Update,
		(finalFcn)CC_MD4_Final, (oneShotFcn)CC_MD4
	},
	{	ALG_MD5, "MD5", CC_MD5_DIGEST_LENGTH,
		(initFcn)CC_MD5_Init, (updateFcn)CC_MD5_Update,
		(finalFcn)CC_MD5_Final, (oneShotFcn)CC_MD5
	},
	{	ALG_SHA1, "SHA1", CC_SHA1_DIGEST_LENGTH,
		(initFcn)CC_SHA1_Init, (updateFcn)CC_SHA1_Update,
		(finalFcn)CC_SHA1_Final, (oneShotFcn)CC_SHA1
	},
	{	ALG_SHA224, "SHA224", CC_SHA224_DIGEST_LENGTH,
		(initFcn)CC_SHA224_Init, (updateFcn)CC_SHA224_Update,
		(finalFcn)CC_SHA224_Final, (oneShotFcn)CC_SHA224
	},
	{	ALG_SHA256, "SHA256", CC_SHA256_DIGEST_LENGTH,
		(initFcn)CC_SHA256_Init, (updateFcn)CC_SHA256_Update,
		(finalFcn)CC_SHA256_Final, (oneShotFcn)CC_SHA256
	},
	{	ALG_SHA384, "SHA384", CC_SHA384_DIGEST_LENGTH,
		(initFcn)CC_SHA384_Init, (updateFcn)CC_SHA384_Update,
		(finalFcn)CC_SHA384_Final, (oneShotFcn)CC_SHA384
	},
	{	ALG_SHA512, "SHA512", CC_SHA512_DIGEST_LENGTH,
		(initFcn)CC_SHA512_Init, (updateFcn)CC_SHA512_Update,
		(finalFcn)CC_SHA512_Final, (oneShotFcn)CC_SHA512
	},
};
#define NUM_DIGESTS		(sizeof(digests) / sizeof(digests[0]))

static const CommonDigestInfo *findDigestInfo(unsigned alg)
{
	unsigned dex;
	for(dex=0; dex<NUM_DIGESTS; dex++) {
		if((unsigned)(digests[dex].alg) == alg) {
			return &digests[dex];
		}
	}
	return NULL;
}


/* 
 * These consts let us allocate context and digest buffers for 
 * any arbitrary algorithm.
 */
#define MAX_DIGEST_SIZE		64
#define MAX_CONTEXT_SIZE	sizeof(CC_SHA512_CTX)

/* staged digest with random updates */
static void doStaged(
	const CommonDigestInfo *digestInfo,
	const unsigned char *ptext,
	unsigned ptextLen,
	unsigned char *md)
{
	char ctx[MAX_CONTEXT_SIZE];
	unsigned thisMove;
	
	digestInfo->init(ctx);
	while(ptextLen) {
		thisMove = genRand(1, ptextLen);
		digestInfo->update(ctx, ptext, thisMove);
		ptext += thisMove;
		ptextLen -= thisMove;
	}
	digestInfo->final(md, ctx);
}

static int doTest(
	const CommonDigestInfo *digestInfo,
	const unsigned char *ptext,
	unsigned ptextLen,
	bool quiet)
{
	unsigned char mdStaged[MAX_DIGEST_SIZE];
	unsigned char mdOneShot[MAX_DIGEST_SIZE];
	
	digestInfo->oneShot(ptext, ptextLen, mdOneShot);
	doStaged(digestInfo, ptext, ptextLen, mdStaged);
	if(memcmp(mdStaged, mdOneShot, digestInfo->digestSize)) {
		printf("***Digest miscompare for %s\n", digestInfo->algName);
		if(testError(quiet)) {
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int						arg;
	char					*argp;
	unsigned				loop;
	uint8					*ptext;
	size_t					ptextLen;
	unsigned				currAlg;
	const CommonDigestInfo	*digestInfo;
	int						rtn = 0;
	int						i;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	bool		quiet = false;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'q':
		    	quiet = true;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext = (uint8 *)malloc(MAX_DATA_SIZE);
	if(ptext == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	/* ptext length set in test loop */
	
	printf("Starting ccOneShot; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	
	for(currAlg=ALG_FIRST; currAlg<=ALG_LAST; currAlg++) {
		digestInfo = findDigestInfo(currAlg);
		if(!quiet) {
			printf("Testing alg %s\n", digestInfo->algName);
		}
		for(loop=1; ; loop++) {
			ptextLen = genRand(MIN_DATA_SIZE, MAX_DATA_SIZE);
			appGetRandomBytes(ptext, ptextLen);
			if(!quiet) {
			   	if((loop % LOOP_NOTIFY) == 0) {
					printf("..loop %d ptextLen %lu\n",
						loop, (unsigned long)ptextLen);
				}
			}
			
			if(doTest(digestInfo, ptext, ptextLen, quiet)) {
				rtn = 1;
				break;
			}
			if(loops && (loop == loops)) {
				break;
			}
		}	/* main loop */
		if(rtn) {
			break;
		}
		
	}	/* for algs */
	
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	free(ptext);
	return rtn;
}


