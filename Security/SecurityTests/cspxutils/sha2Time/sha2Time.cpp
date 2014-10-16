/*
 * Measure performance of SHA using raw CommonCrypto digests. 
 * Use hashTime to measure performance thru the CSP.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <CommonCrypto/CommonDigest.h>
#include "cputime.h"
#include "common.h"

static void usage(char **argv) 
{
	printf("Usage: %s bytecount [q(uiet)]\n", argv[0]);
	exit(1);
}

#define BUFSIZE		(8 * 1024)

int main(int argc, char **argv)
{
	bool quiet = false;
	unsigned byteCount;
	
	if(argc < 2) {
		usage(argv);
	}
	byteCount = atoi(argv[1]);
	for(int arg=2; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'q':
				quiet = true;
				break;
			default:
				usage(argv);
		}
	}
	
	unsigned char *text = (unsigned char *)malloc(BUFSIZE);
	appGetRandomBytes(text, BUFSIZE);

	unsigned	toMove = byteCount;
	unsigned	thisMove;
	CPUTime		startTime;
	double		timeSpentMs;
	
	if(!quiet) {
		printf("...testing SHA1\n");
	}
	
	CC_SHA1_CTX ctx1;
	unsigned char dig1[CC_SHA1_DIGEST_LENGTH];
	
	CC_SHA1_Init(&ctx1);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	do {
		if(toMove > BUFSIZE) {
			thisMove = BUFSIZE;
		}
		else {
			thisMove = toMove;
		}
		toMove -= thisMove;
		CC_SHA1_Update(&ctx1, text, thisMove);
	} while(toMove);
	CC_SHA1_Final(dig1, &ctx1);
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	printf("SHA1: Digest %u bytes : %.2f ms\n", byteCount, timeSpentMs);

	/* SHA256 */
	if(!quiet) {
		printf("...testing SHA256\n");
	}
	
	CC_SHA256_CTX ctx256;
	unsigned char dig256[CC_SHA256_DIGEST_LENGTH];
	toMove = byteCount;
	CC_SHA256_Init(&ctx256);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	do {
		if(toMove > BUFSIZE) {
			thisMove = BUFSIZE;
		}
		else {
			thisMove = toMove;
		}
		toMove -= thisMove;
		CC_SHA256_Update(&ctx256, text, thisMove);
	} while(toMove);
	CC_SHA256_Final(dig256, &ctx256);
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	printf("SHA256: Digest %u bytes : %.2f ms\n", byteCount, timeSpentMs);

	/* SHA256 */
	if(!quiet) {
		printf("...testing SHA512\n");
	}
	
	CC_SHA512_CTX ctx512;
	unsigned char dig512[CC_SHA512_DIGEST_LENGTH];
	toMove = byteCount;
	CC_SHA512_Init(&ctx512);
	
	/* start critical timing loop */
	startTime = CPUTimeRead();
	do {
		if(toMove > BUFSIZE) {
			thisMove = BUFSIZE;
		}
		else {
			thisMove = toMove;
		}
		toMove -= thisMove;
		CC_SHA512_Update(&ctx512, text, thisMove);
	} while(toMove);
	CC_SHA512_Final(dig512, &ctx512);
	timeSpentMs = CPUTimeDeltaMs(startTime, CPUTimeRead());
	printf("SHA512: Digest %u bytes : %.2f ms\n", byteCount, timeSpentMs);

	return 0;
}
