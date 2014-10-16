/* 
 * aesVect.c - generate NIST-compatible test vectors for various AES implementations. 
 *
 * Written by Doug Mitchell. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "enDecrypt.h"
#include "rijndael-alg-ref.h"
#include <strings.h>

static void usage(char **argv)
{
	printf("usage: %s r|t|c vectorStyle [options]\n", argv[0]);
	printf("r=reference  t=testImple  c=CSP\n");
	printf("vectorStyle:\n");
	printf("   k   Variable key KAT\n");
	printf("   p   Variable plaintext KAT\n");
	printf("options:\n");
	printf("   k=keySizeInBits (default = 128)\n");
	printf("   i=max value of i (default = max per context)\n");
	printf("   h(elp)\n");
	exit(1);
}

static void setBit(
	unsigned char *val,
	unsigned valBytes,
	unsigned i)
{
	unsigned whichByte;
	unsigned whichBit;
	
	i--;
	whichByte = i / 8;
	whichBit = 7 - (i % 8);
	memset(val, 0, valBytes);
	val[whichByte] = 1 << whichBit;
}

static void printBigNum(
	const char *name,
	unsigned char *val,
	unsigned valBytes)	
{
	unsigned i;
	printf("%s=", name);
	for(i=0; i<valBytes; i++) {
		unsigned iNum = val[i] & 0xff;
		printf("%02X", iNum);
	}
	printf("\n");
}

static void varKeyKAT(
	encrDecrFcn		encrDecr,			// encryptDecryptRef, encryptDecryptTest
	unsigned		keySizeInBits,
	unsigned		maxI)
{
	unsigned i;
	unsigned char key[MAX_AES_KEY_BYTES];
	unsigned char ptext[MIN_AES_BLOCK_BYTES];
	unsigned char ctext[MIN_AES_BLOCK_BYTES];
	unsigned keyBytes = keySizeInBits / 8;
	
	memset(ptext, 0, MIN_AES_BLOCK_BYTES);
	if(maxI == 0) {
		maxI = keySizeInBits;
	}
	
	printf("KEYSIZE=%d\n\n", keySizeInBits);
	printf("PT=00000000000000000000000000000000\n\n");
	
	for(i=1; i<=maxI; i++) {
		setBit(key, keyBytes, i);
		encrDecr(CSSM_TRUE,
			keySizeInBits,
			MIN_AES_BLOCK_BITS,
			key,
			ptext,
			MIN_AES_BLOCK_BYTES,
			ctext);
		printf("I=%d\n", i);
		printBigNum("KEY", key, keyBytes);
		printBigNum("CT", ctext, MIN_AES_BLOCK_BYTES);
		printf("\n");
	}
}

static void varPtextKAT(
	encrDecrFcn		encrDecr,			// encryptDecryptRef, encryptDecryptTest
	unsigned		keySizeInBits,
	unsigned		maxI)
{
	unsigned i;
	unsigned char key[MAX_AES_KEY_BYTES];
	unsigned char ptext[MIN_AES_BLOCK_BYTES];
	unsigned char ctext[MIN_AES_BLOCK_BYTES];
	unsigned keyBytes = keySizeInBits / 8;
	
	memset(key, 0, MAX_AES_KEY_BYTES);
	if(maxI == 0) {
		maxI = MIN_AES_BLOCK_BITS;
	}
	
	printf("KEYSIZE=%d\n\n", keySizeInBits);
	printBigNum("KEY", key, keyBytes);
	printf("\n");
	
	for(i=1; i<=maxI; i++) {
		setBit(ptext, MIN_AES_BLOCK_BYTES, i);
		encrDecr(CSSM_TRUE,
			keySizeInBits,
			MIN_AES_BLOCK_BITS,
			key,
			ptext,
			MIN_AES_BLOCK_BYTES,
			ctext);
		printf("I=%d\n", i);
		printBigNum("PT", ptext, MIN_AES_BLOCK_BYTES);
		printBigNum("CT", ctext, MIN_AES_BLOCK_BYTES);
		printf("\n");
	}
}

int main(int argc, char **argv)
{
	int			arg;
	char		*argp;
	
	unsigned	keySizeInBits = MIN_AES_KEY_BITS;
	unsigned	maxI = 0;
	encrDecrFcn	encrDecr;
	
	if(argc < 3) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'r':
			encrDecr = encryptDecryptRef;
			break;
		case 't':
			encrDecr = encryptDecryptTest;
			break;
		case 'c':
			encrDecr = encryptDecryptCsp;
			break;
		default:
			usage(argv);
	}
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'k':
				keySizeInBits = atoi(&argp[2]);
				break;
		    case 'i':
				maxI = atoi(&argp[2]);
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	switch(argv[2][0]) {
		case 'k': 
			varKeyKAT(encrDecr, keySizeInBits, maxI);
			break;
		case 'p':
			varPtextKAT(encrDecr, keySizeInBits, maxI);
			exit(1);
		default:
			usage(argv);
			
	}
	return 0;
}
