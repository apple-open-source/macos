/*
 * hashTimeSA.cpp - measure performance of digest ops, standalone version (no
 *    dependency on Security.framewortk or on CommonCrypto portion of libSystem). 
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CommonCrypto/CommonDigest.h>		/* static lib used in Tiger */
#include "MD5.h"				/* CryptKit version used in Panther and prior */
#include "SHA1.h"				/* ditto */
#include <Security/cssmtype.h>	/* for ALGID values */
#include <Security/cssmapple.h>	/* more ALGID values */
#include <CoreFoundation/CFDate.h>

/* enumerate digest algorithms our way */
typedef int HT_Alg;
enum {
	HA_MD5 = 0,
	HA_SHA1,
	HA_SHA224,
	HA_SHA256,
	HA_SHA384,
	HA_SHA512
};

#define FIRST_ALG	HA_MD5
#define LAST_ALG	HA_SHA512

static void usage(char **argv)
{
    printf("Usage: %s c|k [option ...]\n", argv[0]);
	printf("   c=CommonCrypto; k=CryptKit\n");
    printf("Options:\n");
	printf("   a=alg; default=all\n");
	printf("           algs: m : MD5\n");
	printf("                 s : SHA1\n");
	printf("                 4 : SHA224\n");
	printf("                 2 : SHA256\n");
	printf("                 3 : SHA384\n");
	printf("                 5 : SHA512\n");
	printf("   l=loops (only valid if testspec is given)\n");
	printf("   v verify digest by printing it\n");
	exit(1);
}

static void dumpDigest(
	const unsigned char *digest,
	unsigned len)
{
	for(unsigned dex=0; dex<len; dex++) {
		printf("%02X", *digest++);
		if((dex % 4) == 3) {
			printf(" ");
		}
	}
	printf("\n");
}

/* sort-of random, but repeatable */
static void initPtext(
	unsigned char *ptext,
	unsigned len)
{
	srandom(1);
	for(unsigned dex=0; dex<len; dex++) {
		*ptext++ = random();
	}
}

/* passed to each test */	
typedef struct {
	unsigned			loops;
	CSSM_ALGORITHMS		algId;		// MD5, SHA1
	bool				dumpDigest;
} TestParams;

#define MAX_DIGEST_SIZE		64			// we provide, no malloc below CSSM

#define PTEXT_SIZE			1000		// to digest in bytes
#define INNER_LOOPS			500


/* SHA1 digest is not orthoganal, fix up here */
static void ckSha1Final(
	void *ctx,
	unsigned char *digest)
{
	sha1GetDigest((sha1Obj)ctx, digest);
}

typedef void (*ckInitFcn)(void *digestCtx);
typedef void (*ckUpdateFcn)(void *digestCtx, const void *data, unsigned len);
typedef void (*ckFinalFcn)(void *digestCtx, unsigned char *digest);

static CSSM_RETURN hashDataRateCryptKit(
	TestParams	*params)
{
	ckUpdateFcn		updatePtr = NULL;
	ckFinalFcn		finalPtr = NULL;
	ckInitFcn			initPtr = NULL;
	struct MD5Context	md5;
	sha1Obj				sha;
	void			*ctx;

	unsigned 		loop;
	unsigned		iloop;
	double			startTime, endTime;
	double			timeSpent, timeSpentMs;
	uint8			ptext[PTEXT_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	unsigned		digestLen = 16;
	
	/* we reuse this one inside the loop */
	switch(params->algId) {
		case CSSM_ALGID_SHA1:
			sha = sha1Alloc();
			ctx = sha;
			initPtr = (ckInitFcn)sha1Reinit;
			updatePtr = (ckUpdateFcn)sha1AddData;
			finalPtr = (ckFinalFcn)ckSha1Final;
			digestLen = 20;
			break;
		case CSSM_ALGID_MD5:
			ctx = &md5;
			initPtr = (ckInitFcn)MD5Init;
			updatePtr = (ckUpdateFcn)MD5Update;
			finalPtr = (ckFinalFcn)MD5Final;
			break;
		default:
			printf("***Sorry, CryptKit can only do SHA1 and MD5.\n");
			return 1;
	}
		
	/* random data, const thru the loops */
	initPtext(ptext, PTEXT_SIZE);
	
	/* start critical timing loop */
	startTime = CFAbsoluteTimeGetCurrent();
	for(loop=0; loop<params->loops; loop++) {
		initPtr(ctx);
		for(iloop=0; iloop<INNER_LOOPS; iloop++) {
			updatePtr(ctx, ptext, PTEXT_SIZE);
		}
		finalPtr(ctx, digest);
	}
	endTime = CFAbsoluteTimeGetCurrent();
	timeSpent = endTime - startTime;
	timeSpentMs = timeSpent * 1000.0;
	
	float bytesPerLoop = INNER_LOOPS * PTEXT_SIZE;
	float totalBytes   = params->loops * bytesPerLoop;
	
	/* careful, KByte = 1024, ms = 1/1000 */
	printf("   Digest %.0f bytes : %u ops in %.2f ms; %f ms/op, %.0f KBytes/s\n",
		bytesPerLoop, params->loops, 
		timeSpentMs, timeSpentMs / (double)params->loops,
		((float)totalBytes / 1024.0) / timeSpent);
	if(params->dumpDigest) {
		dumpDigest(digest, digestLen);
	}
	return CSSM_OK;
}

typedef union {
	CC_MD5_CTX		md5;
	CC_SHA1_CTX		sha;
	CC_SHA256_CTX	sha256;
	CC_SHA512_CTX	sha512;
} CC_CTX;

typedef void (*ccInitFcn)(void *digestCtx);
typedef void (*ccUpdateFcn)(void *digestCtx, const void *data, CC_LONG len);
typedef void (*ccFinalFcn)(unsigned char *digest, void *digestCtx);

static CSSM_RETURN hashDataRateCommonCrypto(
	TestParams	*params)
{
	CC_CTX			ctx;
	ccUpdateFcn		updatePtr = NULL;
	ccFinalFcn		finalPtr = NULL;
	ccInitFcn		initPtr = NULL;
	unsigned 		loop;
	unsigned		iloop;
	double			startTime, endTime;
	double			timeSpent, timeSpentMs;
	uint8			ptext[PTEXT_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	unsigned		digestLen = 16;
	
	/* we reuse this one inside the loop */
	switch(params->algId) {
		case CSSM_ALGID_SHA1:
			initPtr = (ccInitFcn)CC_SHA1_Init;
			updatePtr = (ccUpdateFcn)CC_SHA1_Update;
			finalPtr = (ccFinalFcn)CC_SHA1_Final;
			digestLen = 20;
			break;
		case CSSM_ALGID_SHA224:
			initPtr = (ccInitFcn)CC_SHA224_Init;
			updatePtr = (ccUpdateFcn)CC_SHA224_Update;
			finalPtr = (ccFinalFcn)CC_SHA224_Final;
			digestLen = 28;
			break;
		case CSSM_ALGID_SHA256:
			initPtr = (ccInitFcn)CC_SHA256_Init;
			updatePtr = (ccUpdateFcn)CC_SHA256_Update;
			finalPtr = (ccFinalFcn)CC_SHA256_Final;
			digestLen = 32;
			break;
		case CSSM_ALGID_SHA384:
			initPtr = (ccInitFcn)CC_SHA384_Init;
			updatePtr = (ccUpdateFcn)CC_SHA384_Update;
			finalPtr = (ccFinalFcn)CC_SHA384_Final;
			digestLen = 48;
			break;
		case CSSM_ALGID_SHA512:
			initPtr = (ccInitFcn)CC_SHA512_Init;
			updatePtr = (ccUpdateFcn)CC_SHA512_Update;
			finalPtr = (ccFinalFcn)CC_SHA512_Final;
			digestLen = 64;
			break;
		case CSSM_ALGID_MD5:
			initPtr = (ccInitFcn)CC_MD5_Init;
			updatePtr = (ccUpdateFcn)CC_MD5_Update;
			finalPtr = (ccFinalFcn)CC_MD5_Final;
			digestLen = 16;
			break;
		default:
			printf("***BRRRZAP!\n");
			return 1;
	}
		
	/* random data, const thru the loops */
	initPtext(ptext, PTEXT_SIZE);
	
	/* start critical timing loop */
	startTime = CFAbsoluteTimeGetCurrent();
	for(loop=0; loop<params->loops; loop++) {
		initPtr(&ctx);
		for(iloop=0; iloop<INNER_LOOPS; iloop++) {
			updatePtr(&ctx, ptext, PTEXT_SIZE);
		}
		finalPtr(digest, &ctx);
	}
	endTime = CFAbsoluteTimeGetCurrent();
	timeSpent = endTime - startTime;
	timeSpentMs = timeSpent * 1000.0;
	
	float bytesPerLoop = INNER_LOOPS * PTEXT_SIZE;
	float totalBytes   = params->loops * bytesPerLoop;
	
	/* careful, KByte = 1024, ms = 1/1000 */
	printf("   Digest %.0f bytes : %u ops in %.2f ms; %f ms/op, %.0f KBytes/s\n",
		bytesPerLoop, params->loops, 
		timeSpentMs, timeSpentMs / (double)params->loops,
		((float)totalBytes / 1024.0) / timeSpent);
	if(params->dumpDigest) {
		dumpDigest(digest, digestLen);
	}
	return CSSM_OK;
}

typedef CSSM_RETURN (*testRunFcn)(TestParams *testParams);

/*
 * Static declaration of a test
 */
typedef struct {
	const char 			*testName;
	unsigned			loops;
	testRunFcn			run;
	char				testSpec;		// for t=xxx cmd line opt
} TestDefs;

static TestDefs testDefsCryptKit = 
{ 	"Large data digest, CryptKit",
	1000,
	hashDataRateCryptKit,
	'd',
};

static TestDefs testDefsCommonCrypto = 
{ 	"Large data digest, CommonCrypto",
	1000,
	hashDataRateCommonCrypto,
	'd',
};

static void algToAlgId(
	HT_Alg			alg,
	CSSM_ALGORITHMS	*algId,
	const char		**algStr)
{
	switch(alg) {
		case HA_MD5:
			*algId = CSSM_ALGID_MD5;
			*algStr = "MD5";
			break;
		case HA_SHA1:
			*algId = CSSM_ALGID_SHA1;
			*algStr = "SHA1";
			break;
		case HA_SHA224:
			*algId = CSSM_ALGID_SHA224;
			*algStr = "SHA224";
			break;
		case HA_SHA256:
			*algId = CSSM_ALGID_SHA256;
			*algStr = "SHA256";
			break;
		case HA_SHA384:
			*algId = CSSM_ALGID_SHA384;
			*algStr = "SHA384";
			break;
		case HA_SHA512:
			*algId = CSSM_ALGID_SHA512;
			*algStr = "SHA512";
			break;
		default:
			printf("***algToAlgId screwup\n");
			exit(1);
	}
}

int main(int argc, char **argv)
{
	TestParams 		testParams;
	TestDefs		*testDefs = NULL;
	CSSM_RETURN		crtn;
	int 			arg;
	char			*argp;
	unsigned		cmdLoops = 0;		// can be specified in cmd line
										// if not, use TestDefs.loops
	HT_Alg			alg;
	const char		*algStr;
	int				firstAlg = FIRST_ALG;
	int				lastAlg = LAST_ALG;
	
	memset(&testParams, 0, sizeof(testParams));
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'c':
			testDefs = &testDefsCommonCrypto;
			break;
		case 'k':
			testDefs = &testDefsCryptKit; 
			break;
		default:
			usage(argv);
	}
	
	for(arg=2; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'l':
				cmdLoops = atoi(&argp[2]);
				break;
			case 'a':
				if(argp[1] == '\0') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'm':
						firstAlg = lastAlg = HA_MD5;
						break;
					case 's':
						firstAlg = lastAlg = HA_SHA1;
						break;
					case '4':
						firstAlg = lastAlg = HA_SHA224;
						break;
					case '2':
						firstAlg = lastAlg = HA_SHA256;
						break;
					case '3':
						firstAlg = lastAlg = HA_SHA384;
						break;
					case '5':
						firstAlg = lastAlg = HA_SHA512;
						break;
					default:
						usage(argv);
				}
				break;
			case 'v':
				testParams.dumpDigest = true; 
				break;
			default:
				usage(argv);
		}
	}

	printf("%s:\n", testDefs->testName);
	if(cmdLoops) {
		/* user specified */
		testParams.loops = cmdLoops;
	}	
	else {
		/* default */
		testParams.loops = testDefs->loops;
	}
	if((lastAlg > HA_SHA1) && (testDefs == &testDefsCryptKit)) {
		/* CryptKit can only do MD5 and SHA1 */
		lastAlg = HA_SHA1;
	}
	for(alg=firstAlg; alg<=lastAlg; alg++) {
		algToAlgId(alg, &testParams.algId, &algStr);
		printf("   === %s ===\n", algStr);
		crtn = testDefs->run(&testParams);
		if(crtn) {
			printf("***Error detected in test, somehow....aborting.\n");
			exit(1);
		}
	}
	return 0;
}
