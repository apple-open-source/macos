/*
 * DER decode test
 */
#include "testParams.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <Security/SecAsn1Coder.h>
#include <Security/SecAsn1Templates.h>
#include <stddef.h>
#include <unistd.h>

#define DO_BAD_DECODE		0
#define DO_PAUSE			0		/* for malloc debug */

/* good DER, bad DER */
static CSSM_DATA goodDer;
static CSSM_DATA badDer;

typedef struct {
	CSSM_DATA int1;
	CSSM_DATA int2;
} twoInts;

static const SecAsn1Template twoIntsTemp[] = {
	{ SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(twoInts) },
    { SEC_ASN1_INTEGER, offsetof(twoInts, int1) },
    { SEC_ASN1_INTEGER, offsetof(twoInts, int2) },
	{0}
};

static uint8 int1[] = {1,2,3};
static uint8 int2[] = {3,4,5,6,7};

#define DECODES_PER_LOOP	1

int derDecodeInit(
	TestParams *testParams)
{
	/*
	 * DER encode a sequence of two integers
	 */
	twoInts ti;
	ti.int1.Data = int1;
	ti.int1.Length = sizeof(int1);
	ti.int2.Data = int2;
	ti.int2.Length = sizeof(int2);
	
	/* encode --> tempDer */
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);
	
	CSSM_DATA tmpDer = {0, NULL};
	if(SecAsn1EncodeItem(coder, &ti, twoIntsTemp, &tmpDer)) {
		printf("***derDecodeInit: Error on encodeItem()\n");
		return -1;
	}
	
	/* copy to goodDer and badDer */
	appCopyCssmData(&tmpDer, &goodDer);
	appCopyCssmData(&tmpDer, &badDer);
	
	/* increment the length of the outer sequence to force error */
	badDer.Data[1]++;
	SecAsn1CoderRelease(coder);
	return 0;
}

int derDecodeTest(TestParams *testParams)
{
	/* which flavor - good or bad? */
	const CSSM_DATA *derSrc;
	bool expectErr = false;
	
	if((testParams->threadNum & 1) || !DO_BAD_DECODE) {
		derSrc = &goodDer;
	}
	else {
		derSrc = &badDer;
		expectErr = true;
	}
	for(unsigned loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("derDecode thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		for(unsigned dex=0; dex<DECODES_PER_LOOP; dex++) {
			SecAsn1CoderRef coder;
			SecAsn1CoderCreate(&coder);
			twoInts ti;
			OSStatus perr1, perr2;
			memset(&ti, 0, sizeof(ti));
			perr1 = SecAsn1DecodeData(coder, derSrc, twoIntsTemp, &ti);
			usleep(100);
			perr2 = SecAsn1DecodeData(coder, derSrc, twoIntsTemp, &ti);
			if(perr1 != perr2) {
				printf("***derDecodeTest: different errors (%d, %d)\n",
					(int)perr1, (int)perr2);
				return 1;
			}
			if(expectErr) {
				if(!perr1) {
					printf("derDecodeTest: expect failure, got success\n");
					return 1;
				}
			}
			else {
				if(perr1) {
					printf("derDecodeTest: expect success, got %d\n", (int)perr1);
					return 1;
				}
			}
			SecAsn1CoderRelease(coder);
		}
		#if DO_PAUSE
		fflush(stdin);
		printf("End of loop, CR to continue: ");
		getchar();
		#endif
	}
	return 0;
}

