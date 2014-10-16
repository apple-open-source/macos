/*
 * hashTime.cpp - measure performance of digest ops
 */
 
#include <Security/Security.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cputime.h"
#include "cspwrap.h"
#include "common.h"
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <CommonCrypto/CommonDigest.h>
#include "MD5.h"			/* CryptKit version used Panther and prior */
#include "SHA1.h"			/* ditto */

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
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   t=testspec; default=all\n");
	printf("     test specs: c : digest context setup/teardown\n");
	printf("                 b : basic single block digest\n");
	printf("                 d : digest lots of data\n");
	printf("   a=alg; default=all\n");
	printf("           algs: m : MD5\n");
	printf("                 s : SHA1\n");
	printf("                 4 : SHA224\n");
	printf("                 2 : SHA256\n");
	printf("                 3 : SHA384\n");
	printf("                 5 : SHA512\n");
	printf("   l=loops (only valid if testspec is given)\n");
	printf("   o (use openssl implementations, MD5 and SHA1 only)\n");
	printf("   c (use CommonCrypto implementation)\n");
	printf("   k (use CryptKit implementations, MD5 and SHA1 only\n");
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
	CSSM_CSP_HANDLE		cspHand;
	CSSM_ALGORITHMS		algId;		// MD5, SHA1
	bool				dumpDigest;
} TestParams;

/* just CDSA context setup/teardown - no CSP activity */
static CSSM_RETURN hashContext(
	TestParams	*params)
{
	CSSM_CC_HANDLE	ccHand;
	CSSM_RETURN 	crtn;
	unsigned 		loop;
	CPUTime 		startTime;
	double			timeSpentMs;
	
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		crtn = CSSM_CSP_CreateDigestContext(params->cspHand,
			params->algId, &ccHand);
		if(crtn) {
			return crtn;
		}
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			return crtn;
		}
	}
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	printf("   context setup/delete : %u ops in %.2f ms; %f ms/op\n",
		params->loops, timeSpentMs, timeSpentMs / (double)params->loops);
	return CSSM_OK;
}

/* Minimal CSP init/digest/final */
#define BASIC_BLOCK_SIZE	64		// to digest in bytes
#define MAX_DIGEST_SIZE		64		// we provide, no malloc below CSSM

static CSSM_RETURN hashBasic(
	TestParams	*params)
{
	CSSM_CC_HANDLE	ccHand;
	CSSM_RETURN 	crtn;
	unsigned 		loop;
	CPUTime 		startTime;
	double			timeSpentMs;
	uint8			ptext[BASIC_BLOCK_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	CSSM_DATA		ptextData = {BASIC_BLOCK_SIZE, ptext};
	CSSM_DATA		digestData = {MAX_DIGEST_SIZE, digest};
	
	/* we reuse this one inside the loop */
	crtn = CSSM_CSP_CreateDigestContext(params->cspHand,
			params->algId, &ccHand);
	if(crtn) {
		return crtn;
	}
	
	/* random data, const thru the loops */
	appGetRandomBytes(ptext, BASIC_BLOCK_SIZE);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		crtn = CSSM_DigestDataInit(ccHand);
		if(crtn) {
			return crtn;
		}
		crtn = CSSM_DigestDataUpdate(ccHand, &ptextData, 1);
		if(crtn) {
			return crtn;
		}
		crtn = CSSM_DigestDataFinal(ccHand, &digestData);
		if(crtn) {
			return crtn;
		}
	}
	CSSM_DeleteContext(ccHand);
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	printf("   Digest one %u byte block : %u ops in %.2f ms; %f ms/op\n",
		BASIC_BLOCK_SIZE, params->loops, 
		timeSpentMs, timeSpentMs / (double)params->loops);
	return CSSM_OK;
}

/* Lots of data */
#define PTEXT_SIZE			1000		// to digest in bytes
#define INNER_LOOPS			1000

static CSSM_RETURN hashDataRate(
	TestParams	*params)
{
	CSSM_CC_HANDLE	ccHand;
	CSSM_RETURN 	crtn;
	unsigned 		loop;
	unsigned		iloop;
	CPUTime 		startTime;
	double			timeSpent, timeSpentMs;
	uint8			ptext[PTEXT_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	CSSM_DATA		ptextData = {PTEXT_SIZE, ptext};
	CSSM_DATA		digestData = {MAX_DIGEST_SIZE, digest};
	
	/* we reuse this one inside the loop */
	crtn = CSSM_CSP_CreateDigestContext(params->cspHand,
			params->algId, &ccHand);
	if(crtn) {
		return crtn;
	}
	
	/* random data, const thru the loops */
	initPtext(ptext, PTEXT_SIZE);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		crtn = CSSM_DigestDataInit(ccHand);
		if(crtn) {
			return crtn;
		}
		for(iloop=0; iloop<INNER_LOOPS; iloop++) {
			crtn = CSSM_DigestDataUpdate(ccHand, &ptextData, 1);
			if(crtn) {
				return crtn;
			}
		}
		crtn = CSSM_DigestDataFinal(ccHand, &digestData);
		if(crtn) {
			return crtn;
		}
	}
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	timeSpent = timeSpentMs / 1000.0;
	
	CSSM_DeleteContext(ccHand);
	float bytesPerLoop = INNER_LOOPS * PTEXT_SIZE;
	float totalBytes   = params->loops * bytesPerLoop;
	
	/* careful, KByte = 1024, ms = 1/1000 */
	printf("   Digest %.0f bytes : %u ops in %.2f ms; %f ms/op, %.0f KBytes/s\n",
		bytesPerLoop, params->loops, 
		timeSpentMs, timeSpentMs / (double)params->loops,
		((float)totalBytes / 1024.0) / timeSpent);
	if(params->dumpDigest) {
		dumpDigest(digest, digestData.Length);
	}
	return CSSM_OK;
}

/* Lots of data, openssl version */

typedef union {
	MD5_CTX		md5;
	SHA_CTX		sha;
} OS_CTX;

typedef void (*initFcn)(void *digestCtx);
typedef void (*updateFcn)(void *digestCtx, const void *data, unsigned long len);
typedef void (*finalFcn)(unsigned char *digest, void *digestCtx);

static CSSM_RETURN hashDataRateOpenssl(
	TestParams	*params)
{
	OS_CTX			ctx;
	initFcn			initPtr = NULL;
	updateFcn		updatePtr = NULL;
	finalFcn		finalPtr = NULL;
	unsigned 		loop;
	unsigned		iloop;
	CPUTime 		startTime;
	double			timeSpent, timeSpentMs;
	uint8			ptext[PTEXT_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	unsigned		digestLen = 16;
	
	/* we reuse this one inside the loop */
	switch(params->algId) {
		case CSSM_ALGID_SHA1:
			initPtr = (initFcn)SHA1_Init;
			updatePtr = (updateFcn)SHA1_Update;
			finalPtr = (finalFcn)SHA1_Final;
			digestLen = 20;
			break;
		case CSSM_ALGID_MD5:
			initPtr = (initFcn)MD5_Init;
			updatePtr = (updateFcn)MD5_Update;
			finalPtr = (finalFcn)MD5_Final;
			break;
		default:
			printf("***Sorry, Openssl can only do SHA1 and MD5.\n");
			return 1;
	}
		
	/* random data, const thru the loops */
	initPtext(ptext, PTEXT_SIZE);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		initPtr(&ctx);
		for(iloop=0; iloop<INNER_LOOPS; iloop++) {
			updatePtr(&ctx, ptext, PTEXT_SIZE);
		}
		finalPtr(digest, &ctx);
	}
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	timeSpent = timeSpentMs / 1000.0;
	
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

/* Lots of data, CommonCrypto version (not thru CSP) */

typedef union {
	CC_MD5_CTX		md5;
	CC_SHA1_CTX		sha;
	CC_SHA256_CTX	sha256;
	CC_SHA512_CTX	sha512;
} CC_CTX;

typedef void (*ccUpdateFcn)(void *digestCtx, const void *data, CC_LONG len);
typedef void (*ccFinalFcn)(unsigned char *digest, void *digestCtx);

static CSSM_RETURN hashDataRateCommonCrypto(
	TestParams	*params)
{
	CC_CTX			ctx;
	ccUpdateFcn		updatePtr = NULL;
	ccFinalFcn		finalPtr = NULL;
	initFcn			initPtr = NULL;
	unsigned 		loop;
	unsigned		iloop;
	CPUTime 		startTime;
	double			timeSpent, timeSpentMs;
	uint8			ptext[PTEXT_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	unsigned		digestLen = 16;
	
	/* we reuse this one inside the loop */
	switch(params->algId) {
		case CSSM_ALGID_SHA1:
			initPtr = (initFcn)CC_SHA1_Init;
			updatePtr = (ccUpdateFcn)CC_SHA1_Update;
			finalPtr = (ccFinalFcn)CC_SHA1_Final;
			digestLen = CC_SHA1_DIGEST_LENGTH;
			break;
		case CSSM_ALGID_SHA224:
			initPtr = (initFcn)CC_SHA224_Init;
			updatePtr = (ccUpdateFcn)CC_SHA224_Update;
			finalPtr = (ccFinalFcn)CC_SHA224_Final;
			digestLen = CC_SHA224_DIGEST_LENGTH;
			break;
		case CSSM_ALGID_SHA256:
			initPtr = (initFcn)CC_SHA256_Init;
			updatePtr = (ccUpdateFcn)CC_SHA256_Update;
			finalPtr = (ccFinalFcn)CC_SHA256_Final;
			digestLen = CC_SHA256_DIGEST_LENGTH;
			break;
		case CSSM_ALGID_SHA384:
			initPtr = (initFcn)CC_SHA384_Init;
			updatePtr = (ccUpdateFcn)CC_SHA384_Update;
			finalPtr = (ccFinalFcn)CC_SHA384_Final;
			digestLen = CC_SHA384_DIGEST_LENGTH;
			break;
		case CSSM_ALGID_SHA512:
			initPtr = (initFcn)CC_SHA512_Init;
			updatePtr = (ccUpdateFcn)CC_SHA512_Update;
			finalPtr = (ccFinalFcn)CC_SHA512_Final;
			digestLen = CC_SHA512_DIGEST_LENGTH;
			break;
		case CSSM_ALGID_MD5:
			initPtr = (initFcn)CC_MD5_Init;
			updatePtr = (ccUpdateFcn)CC_MD5_Update;
			finalPtr = (ccFinalFcn)CC_MD5_Final;
			digestLen = CC_MD5_DIGEST_LENGTH;
			break;
		default:
			printf("***BRRRZAP!\n");
			return 1;
	}
		
	/* random data, const thru the loops */
	initPtext(ptext, PTEXT_SIZE);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		initPtr(&ctx);
		for(iloop=0; iloop<INNER_LOOPS; iloop++) {
			updatePtr(&ctx, ptext, PTEXT_SIZE);
		}
		finalPtr(digest, &ctx);
	}
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	timeSpent = timeSpentMs / 1000.0;
	
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

/* Lots of data, CryptKit version */

/* cryptkit final routines are not orthoganal, fix up here */
static void ckSha1Final(
	unsigned char *digest, 
	void *ctx)
{
	sha1GetDigest((sha1Obj)ctx, digest);
}

static void ckMD5Final(
	unsigned char *digest, 
	void *ctx)
{
	MD5Final((struct MD5Context *)ctx, digest);
}

typedef void (*ckUpdateFcn)(void *digestCtx, const void *data, unsigned len);
typedef void (*ckFinalFcn)(unsigned char *digest, void *digestCtx);

static CSSM_RETURN hashDataRateCryptKit(
	TestParams	*params)
{
	ckUpdateFcn		updatePtr = NULL;
	ckFinalFcn		finalPtr = NULL;
	initFcn			initPtr = NULL;
	struct MD5Context	md5;
	sha1Obj				sha;
	void			*ctx;

	unsigned 		loop;
	unsigned		iloop;
	CPUTime 		startTime;
	double			timeSpent, timeSpentMs;
	uint8			ptext[PTEXT_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	unsigned		digestLen = 16;
	
	/* we reuse this one inside the loop */
	switch(params->algId) {
		case CSSM_ALGID_SHA1:
			sha = sha1Alloc();
			ctx = sha;
			initPtr = (initFcn)sha1Reinit;
			updatePtr = (ckUpdateFcn)sha1AddData;
			finalPtr = (ckFinalFcn)ckSha1Final;
			digestLen = 20;
			break;
		case CSSM_ALGID_MD5:
			ctx = &md5;
			initPtr = (initFcn)MD5Init;
			updatePtr = (ckUpdateFcn)MD5Update;
			finalPtr = (ckFinalFcn)ckMD5Final;
			break;
		default:
			printf("***Sorry, CryptKit can only do SHA1 and MD5.\n");
			return 1;
	}
		
	/* random data, const thru the loops */
	initPtext(ptext, PTEXT_SIZE);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		initPtr(ctx);
		for(iloop=0; iloop<INNER_LOOPS; iloop++) {
			updatePtr(ctx, ptext, PTEXT_SIZE);
		}
		finalPtr(digest, ctx);
	}
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	timeSpent = timeSpentMs / 1000.0;
	
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

static TestDefs testDefs[] = 
{
	{ 	"Digest context setup/teardown",
		100000,
		hashContext,
		'c',
	},
	{ 	"Basic single block digest",
		100000,
		hashBasic,
		'b',
	},
	{ 	"Large data digest",
		1000,
		hashDataRate,
		'd',
	},
};

static TestDefs testDefsOpenSSL[] = 
{
	{ 	"Digest context setup/teardown",
		100000,
		NULL,			// not implemented
		'c',
	},
	{ 	"Basic single block digest",
		100000,
		NULL,			// not implemented
		'b',
	},
	{ 	"Large data digest, OpenSSL",
		1000,
		hashDataRateOpenssl,
		'd',
	},
};

static TestDefs testDefsCommonCrypto[] = 
{
	{ 	"Digest context setup/teardown",
		100000,
		NULL,			// not implemented
		'c',
	},
	{ 	"Basic single block digest",
		100000,
		NULL,			// not implemented
		'b',
	},
	{ 	"Large data digest, CommonCrypto",
		1000,
		hashDataRateCommonCrypto,
		'd',
	},
};

static TestDefs testDefsCryptKit[] = 
{
	{ 	"Digest context setup/teardown",
		100000,
		NULL,			// not implemented
		'c',
	},
	{ 	"Basic single block digest",
		100000,
		NULL,			// not implemented
		'b',
	},
	{ 	"Large data digest, CryptKit",
		1000,
		hashDataRateCryptKit,
		'd',
	},
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

#define NUM_TESTS	(sizeof(testDefs) / sizeof(testDefs[0]))

int main(int argc, char **argv)
{
	TestParams 		testParams;
	TestDefs		*testDef;
	TestDefs		*ourTestDefs = testDefs;
	CSSM_RETURN		crtn;
	int 			arg;
	char			*argp;
	unsigned		cmdLoops = 0;		// can be specified in cmd line
										// if not, use TestDefs.loops
	char			testSpec = '\0';	// allows specification of one test
										// otherwise run all
	HT_Alg			alg;
	const char		*algStr;
	int				firstAlg = FIRST_ALG;
	int				lastAlg = LAST_ALG;
	
	memset(&testParams, 0, sizeof(testParams));
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 't':
				testSpec = argp[2];
				break;
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
			case 'o':
				ourTestDefs = testDefsOpenSSL;
				break;
			case 'c':
				ourTestDefs = testDefsCommonCrypto;
				break;
			case 'k':
				ourTestDefs = testDefsCryptKit;
				break;
			case 'v':
				testParams.dumpDigest = true; 
				break;
			default:
				usage(argv);
		}
	}

	testParams.cspHand = cspStartup();
	if(testParams.cspHand == 0) {
		printf("***Error attaching to CSP. Aborting.\n");
		exit(1);
	}
	
	for(unsigned testNum=0; testNum<NUM_TESTS; testNum++) {
		testDef = &ourTestDefs[testNum];
		
		if(testSpec && (testDef->testSpec != testSpec)) {
			continue;
		}
		if(testDef->run == NULL) {
			continue;
		}
		printf("%s:\n", testDef->testName);
		if(cmdLoops) {
			/* user specified */
			testParams.loops = cmdLoops;
		}	
		else {
			/* default */
			testParams.loops = testDef->loops;
		}
		for(alg=firstAlg; alg<=lastAlg; alg++) {
			algToAlgId(alg, &testParams.algId, &algStr);
			printf("   === %s ===\n", algStr);
			crtn = testDef->run(&testParams);
			if(crtn) {
				exit(1);
			}
		}
	}
	return 0;
}
