/*
 * ccPerform.cpp - measure performance of CommonCrypto encryption
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonCryptor.h>
#include "common.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define LOOPS_DEF		1000
#define BUFSIZE_DEF		10240
#define MAX_KEY_SIZE	kCCKeySizeMaxRC4		/* bytes */


/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_AES_128,
	ALG_AES_192,
	ALG_AES_256,
	ALG_DES,
	ALG_3DES,
	ALG_CAST,
	ALG_RC4
} SymAlg;
#define ALG_FIRST			ALG_AES_128
#define ALG_LAST			ALG_RC4

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -a alg     -- alg : d=DES; 3=3DES; a=AES128; n=AES192; A=AES256;\n");
	printf("                      c=CAST; 4=RC4; default=all\n");
	printf("  -l loops   -- loops; default %u\n", LOOPS_DEF);
	printf("  -b bufsize -- buffer size; default %u\n", BUFSIZE_DEF);
	printf("  -e         -- ECB mode; default is CBC\n");
	
	exit(1);
}

static void printCCError(const char *str, OSStatus ortn)
{
	printf("***%s returned %ld\n", str, (long)ortn);
}

int main(int argc, char **argv)
{
	unsigned loops = LOOPS_DEF;
	unsigned bufSize = BUFSIZE_DEF;
	unsigned algFirst = ALG_FIRST;
	unsigned algLast = ALG_LAST;
	bool ecbMode = false;
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "a:l:b:eh")) != -1) {
		switch (arg) {
			case 'a':
				switch(optarg[0]) {
					case 'a':
						algFirst = algLast = ALG_AES_128;
						break;
					case 'n':
						algFirst = algLast = ALG_AES_192;
						break;
					case 'A':
						algFirst = algLast = ALG_AES_256;
						break;
					case 'd':
						algFirst = algLast = ALG_DES;
						break;
					case '3':
						algFirst = algLast = ALG_3DES;
						break;
					case 'c':
						algFirst = algLast = ALG_CAST;
					case '4':
						algFirst = algLast = ALG_RC4;
				}
				break;
			case 'l':
				loops = atoi(optarg);
				break;
			case 'b':
				bufSize = atoi(optarg);
				break;
			case 'e':
				ecbMode = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}

	/* 
	 * encrypt and decrypt on workBuf
	 * save original ptext in saveBuf, compare at end as sanity check 
	 *   for ECB only
	 */
	unsigned char *workBuf = (unsigned char *)malloc(bufSize);
	unsigned char *saveBuf = (unsigned char *)malloc(bufSize);
	if((workBuf == NULL) || (saveBuf == NULL)) {
		printf("***malloc failure\n");
		exit(1);
	}
	appGetRandomBytes(workBuf, bufSize);
	memmove(saveBuf, workBuf, bufSize);
	
	uint8_t keyBytes[MAX_KEY_SIZE];
	size_t keyLength;
	
	appGetRandomBytes(keyBytes, MAX_KEY_SIZE);
	
	CCCryptorRef cryptor;
	CCAlgorithm alg;
	CCOptions options = 0;
	OSStatus ortn;
	
	if(ecbMode) {
		options |= kCCOptionECBMode;
	}
	
	unsigned currAlg;
	for(currAlg=algFirst; currAlg<=algLast; currAlg++) {
		const char *algStr = NULL;
		
		switch(currAlg) {
			case ALG_DES:
				keyLength = kCCKeySizeDES;
				alg = kCCAlgorithmDES;
				algStr = "DES ";
				break;
			case ALG_3DES:
				keyLength = kCCKeySize3DES;
				alg = kCCAlgorithm3DES;
				algStr = "3DES";
				break;
			case ALG_AES_128:
				keyLength = kCCKeySizeAES128;
				alg = kCCAlgorithmAES128;
				algStr = "AES128";
				break;
			case ALG_AES_192:
				keyLength = kCCKeySizeAES192;
				alg = kCCAlgorithmAES128;
				algStr = "AES192";
				break;
			case ALG_AES_256:
				keyLength = kCCKeySizeAES256;
				alg = kCCAlgorithmAES128;
				algStr = "AES256";
				break;
			case ALG_CAST:
				keyLength = kCCKeySizeMaxCAST;
				alg = kCCAlgorithmCAST;
				algStr = "CAST";
				break;
			case ALG_RC4:
				keyLength = kCCKeySizeMaxRC4;
				alg = kCCAlgorithmRC4;
				algStr = "RC4";
				break;
		}
		
		printf("Algorithm: %s  keySize: %u  mode: %s  loops: %u  bufSize: %u\n",
			algStr, (unsigned)keyLength, ecbMode ? "ECB" : "CBC",
			(unsigned)loops, (unsigned)bufSize);
			
		CFAbsoluteTime start, end;
		unsigned loop;
		size_t thisMoved;
		
		/* encrypt: GO */
		start = CFAbsoluteTimeGetCurrent();
		
		ortn = CCCryptorCreate(kCCEncrypt, alg, options,
			keyBytes, keyLength, NULL, &cryptor);
		if(ortn) {
			printCCError("CCCryptorCreate", ortn);
			exit(1);
		}
		
		for(loop=0; loop<loops; loop++) {
			ortn = CCCryptorUpdate(cryptor, workBuf, bufSize,
				workBuf, bufSize, &thisMoved);
			if(ortn) {
				printCCError("CCCryptorUpdate", ortn);
				exit(1);
			}
		}
		/* no padding, CCCryptFinal not needed */
		end = CFAbsoluteTimeGetCurrent();
		
		printf("   encrypt %u * %u bytes took %gs: %g KBytes/s\n",
			(unsigned)loops, (unsigned)bufSize,
			end - start,
			(loops * bufSize) / (end - start) / 1024.0);
			
		/* dncrypt: GO */
		start = CFAbsoluteTimeGetCurrent();
		
		ortn = CCCryptorCreate(kCCDecrypt, alg, options,
			keyBytes, keyLength, NULL, &cryptor);
		if(ortn) {
			printCCError("CCCryptorCreate", ortn);
			exit(1);
		}
		
		for(loop=0; loop<loops; loop++) {
			ortn = CCCryptorUpdate(cryptor, workBuf, bufSize,
				workBuf, bufSize, &thisMoved);
			if(ortn) {
				printCCError("CCCryptorUpdate", ortn);
				exit(1);
			}
		}
		/* no padding, CCCryptFinal not needed */
		end = CFAbsoluteTimeGetCurrent();
		
		printf("   decrypt %u * %u bytes took %gs: %g KBytes/s\n",
			(unsigned)loops, (unsigned)bufSize,
			end - start,
			(loops * bufSize) / (end - start) / 1024.0);
			
		if(ecbMode) {
			if(memcmp(workBuf, saveBuf, bufSize)) {
				printf("***plaintext miscompare!\n");
			}
		}
	}
	return 0;
}
