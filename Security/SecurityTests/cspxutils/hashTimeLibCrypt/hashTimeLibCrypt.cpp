/*
 * hashTimeLibCrypt.cpp - measure performance of libcrypt digest ops.
 * 
 * Thjis is obsolete; hashTime does this a lot better,a dn it also measures raw
 * CommonCrypto and CryptKit versions. 
 */
 
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cputime.h"
#include "cspwrap.h"
#include "common.h"
#include "pbkdDigest.h"

/* enumerate digest algorithms our way */
typedef int HT_Alg;
enum {
	HA_MD5 = 0,
	HA_SHA1
};

#define FIRST_ALG	HA_MD5
#define LAST_ALG	HA_SHA1

static void usage(char **argv)
{
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   t=testspec; default=all\n");
	printf("     test specs: c digest context setup/teardown\n");
	printf("                 b basic single block digest\n");
	printf("                 d digest lots of data\n");
	printf("   l=loops (only valid if testspec is given)\n");
	exit(1);
}

/* passed to each test */	
typedef struct {
	unsigned			loops;
	bool				isSha;
} TestParams;

/* just digest context setup/teardown */
/* returns nonzero on error */
static int hashContext(
	TestParams	*params)
{
	unsigned 		loop;
	CPUTime 		startTime;
	double			timeSpentMs;
	DigestCtx		ctx;
	int				rtn;
	
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		rtn = DigestCtxInit(&ctx, params->isSha);
		if(!rtn) {
			return -1;
		}
	}
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());

	printf("   context setup/delete : %u ops in %.2f ms; %f ms/op\n",
		params->loops, timeSpentMs, timeSpentMs / (double)params->loops);
	return 0;
}

/* Minimal init/digest/final */
#define BASIC_BLOCK_SIZE	64		// to digest in bytes
#define MAX_DIGEST_SIZE		20		// we provide, no malloc below CSSM

static int hashBasic(
	TestParams	*params)
{
	unsigned 		loop;
	CPUTime 		startTime;
	double			timeSpentMs;
	uint8			ptext[BASIC_BLOCK_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	DigestCtx		ctx;
	int 			rtn;
	
	/* random data, const thru the loops */
	appGetRandomBytes(ptext, BASIC_BLOCK_SIZE);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		rtn = DigestCtxInit(&ctx, params->isSha);
		if(!rtn) {
			return -1;
		}
		rtn = DigestCtxUpdate(&ctx, ptext, BASIC_BLOCK_SIZE);
		if(!rtn) {
			return -1;
		}
		rtn = DigestCtxFinal(&ctx, digest);
		if(!rtn) {
			return -1;
		}
	}
	DigestCtxFree(&ctx);
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	printf("   Digest one %u byte block : %u ops in %.2f ms; %f ms/op\n",
		BASIC_BLOCK_SIZE, params->loops, 
		timeSpentMs, timeSpentMs / (double)params->loops);
	return 0;
}

/* Lots of data */
#define PTEXT_SIZE			1000		// to digest in bytes
#define INNER_LOOPS			1000

static int hashDataRate(
	TestParams	*params)
{
	unsigned 		loop;
	unsigned		iloop;
	CPUTime 		startTime;
	double			timeSpent, timeSpentMs;
	uint8			ptext[PTEXT_SIZE];
	uint8			digest[MAX_DIGEST_SIZE];
	DigestCtx		ctx;
	int				rtn;
	
	/* random data, const thru the loops */
	appGetRandomBytes(ptext, PTEXT_SIZE);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	for(loop=0; loop<params->loops; loop++) {
		rtn = DigestCtxInit(&ctx, params->isSha);
		if(!rtn) {
			return -1;
		}
		for(iloop=0; iloop<INNER_LOOPS; iloop++) {
			rtn = DigestCtxUpdate(&ctx, ptext, PTEXT_SIZE);
			if(!rtn) {
				return -1;
			}
		}
		rtn = DigestCtxFinal(&ctx, digest);
		if(!rtn) {
			return -1;
		}
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
	return 0;
}


typedef int (*testRunFcn)(TestParams *testParams);

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

static void algToAlgId(
	HT_Alg			alg,
	bool			*isSha,
	const char		**algStr)
{
	switch(alg) {
		case HA_MD5:
			*isSha = false;
			*algStr = "MD5";
			break;
		case HA_SHA1:
			*isSha = true;
			*algStr = "SHA1";
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
	int				rtn;
	int 			arg;
	char			*argp;
	unsigned		cmdLoops = 0;		// can be specified in cmd line
										// if not, use TestDefs.loops
	char			testSpec = '\0';	// allows specification of one test
										// otherwise run all
	HT_Alg			alg;
	const char		*algStr;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 't':
				testSpec = argp[2];
				break;
			case 'l':
				cmdLoops = atoi(&argp[2]);
				break;
			default:
				usage(argv);
		}
	}

	for(unsigned testNum=0; testNum<NUM_TESTS; testNum++) {
		testDef = &testDefs[testNum];
		
		if(testSpec && (testDef->testSpec != testSpec)) {
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
		for(alg=FIRST_ALG; alg<=LAST_ALG; alg++) {
			algToAlgId(alg, &testParams.isSha, &algStr);
			printf("   === %s ===\n", algStr);
			rtn = testDef->run(&testParams);
			if(rtn) {
				printf("Test returned error\n");
				exit(1);
			}
		}
	}
	return 0;
}
