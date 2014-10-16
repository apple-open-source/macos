/*
 * Verify SHA2 against FIPS test vectors. 
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <CommonCrypto/CommonDigest.h>

static void usage(char **argv) 
{
	printf("Usage: %s [q(uiet)]\n", argv[0]);
	exit(1);
}

/*
 * These test vectors came from FIPS Processing Standards Publication 180-2, 
 * 2002 August 1. 
 * 
 * http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */
 
/* SHA256 vectors */
static const char *v1_256_text = "abc";
static int v1_256_textLen=3;
static unsigned const char v1_256_digest[] = {
	0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 
	0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
	0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
	0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
};

static const char *v2_256_text = "abcdbcdecdefdefgefghfghighijhi"
								 "jkijkljklmklmnlmnomnopnopq";
#define v2_256_textLen strlen(v2_256_text)
static unsigned const char v2_256_digest[] = {
	0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 
	0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
	0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
	0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
};

/* Vector 3: text = one million 'a' characters */
static unsigned const char v3_256_digest[] = {
	0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92,
	0x81, 0xa1, 0xc7, 0xe2, 0x84, 0xd7, 0x3e, 0x67,
	0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97, 0x20, 0x0e,
	0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0
};

/* SHA384 vectors */
static const char *v1_384_text = "abc";
static int v1_384_textLen=3;
static unsigned const char v1_384_digest[] = {
	0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b, 
	0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07, 
	0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63, 
	0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed, 
	0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23, 
	0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7
};

static const char *v2_384_text = "abcdefghbcdefghicdefghijdefghij"
								 "kefghijklfghijklmghijklmn"
								 "hijklmnoijklmnopjklmnopqklmnopqr"
								 "lmnopqrsmnopqrstnopqrstu";
#define v2_384_textLen strlen(v2_384_text)
static unsigned const char v2_384_digest[] = {
	0x09, 0x33, 0x0c, 0x33, 0xf7, 0x11, 0x47, 0xe8, 
	0x3d, 0x19, 0x2f, 0xc7, 0x82, 0xcd, 0x1b, 0x47, 
	0x53, 0x11, 0x1b, 0x17, 0x3b, 0x3b, 0x05, 0xd2, 
	0x2f, 0xa0, 0x80, 0x86, 0xe3, 0xb0, 0xf7, 0x12, 
	0xfc, 0xc7, 0xc7, 0x1a, 0x55, 0x7e, 0x2d, 0xb9, 
	0x66, 0xc3, 0xe9, 0xfa, 0x91, 0x74, 0x60, 0x39
};

/* Vector 3: text = one million 'a' characters */
static unsigned const char v3_384_digest[] = {
	0x9d, 0x0e, 0x18, 0x09, 0x71, 0x64, 0x74, 0xcb, 
	0x08, 0x6e, 0x83, 0x4e, 0x31, 0x0a, 0x4a, 0x1c, 
	0xed, 0x14, 0x9e, 0x9c, 0x00, 0xf2, 0x48, 0x52, 
	0x79, 0x72, 0xce, 0xc5, 0x70, 0x4c, 0x2a, 0x5b, 
	0x07, 0xb8, 0xb3, 0xdc, 0x38, 0xec, 0xc4, 0xeb, 
	0xae, 0x97, 0xdd, 0xd8, 0x7f, 0x3d, 0x89, 0x85
};

/* SHA512 vectors */
static const char *v1_512_text = "abc";
static int v1_512_textLen=3;
static unsigned const char v1_512_digest[] = {
	0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba,
	0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
	0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2,
	0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
	0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8,
	0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
	0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e,
	0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f
};

static const char *v2_512_text = "abcdefghbcdefghicdefghijdefgh"
								 "ijkefghijklfghijklmghijklmn"
								 "hijklmnoijklmnopjklmnopqklmn"
								 "opqrlmnopqrsmnopqrstnopqrstu";
#define v2_512_textLen strlen(v2_512_text)
static unsigned const char v2_512_digest[] = {
	0x8e, 0x95, 0x9b, 0x75, 0xda, 0xe3, 0x13, 0xda,
	0x8c, 0xf4, 0xf7, 0x28, 0x14, 0xfc, 0x14, 0x3f,
	0x8f, 0x77, 0x79, 0xc6, 0xeb, 0x9f, 0x7f, 0xa1,
	0x72, 0x99, 0xae, 0xad, 0xb6, 0x88, 0x90, 0x18,
	0x50, 0x1d, 0x28, 0x9e, 0x49, 0x00, 0xf7, 0xe4,
	0x33, 0x1b, 0x99, 0xde, 0xc4, 0xb5, 0x43, 0x3a,
	0xc7, 0xd3, 0x29, 0xee, 0xb6, 0xdd, 0x26, 0x54,
	0x5e, 0x96, 0xe5, 0x5b, 0x87, 0x4b, 0xe9, 0x09
};

/* Vector 3: one million 'a' characters */
static unsigned const char v3_512_digest[] = {
	0xe7, 0x18, 0x48, 0x3d, 0x0c, 0xe7, 0x69, 0x64, 
	0x4e, 0x2e, 0x42, 0xc7, 0xbc, 0x15, 0xb4, 0x63, 
	0x8e, 0x1f, 0x98, 0xb1, 0x3b, 0x20, 0x44, 0x28, 
	0x56, 0x32, 0xa8, 0x03, 0xaf, 0xa9, 0x73, 0xeb, 
	0xde, 0x0f, 0xf2, 0x44, 0x87, 0x7e, 0xa6, 0x0a, 
	0x4c, 0xb0, 0x43, 0x2c, 0xe5, 0x77, 0xc3, 0x1b, 
	0xeb, 0x00, 0x9c, 0x5c, 0x2c, 0x49, 0xaa, 0x2e, 
	0x4e, 0xad, 0xb2, 0x17, 0xad, 0x8c, 0xc0, 0x9b
};

/* 
 * SHA224 vectors, not part of the FIPS standard; these were obtained from RFC 3874.
 */
static const char *v1_224_text = "abc";
static int v1_224_textLen=3;
static unsigned const char v1_224_digest[] = {
	0x23, 0x09, 0x7d, 0x22, 0x34, 0x05, 0xd8, 0x22,
	0x86, 0x42, 0xa4, 0x77, 0xbd, 0xa2, 0x55, 0xb3,
	0x2a, 0xad, 0xbc, 0xe4, 0xbd, 0xa0, 0xb3, 0xf7,
	0xe3, 0x6c, 0x9d, 0xa7
};

static const char *v2_224_text = "abcdbcdecdefdefgefghfghighi"
								 "jhijkijkljklmklmnlmnomnopnopq";
static int v2_224_textLen=56;
static unsigned const char v2_224_digest[] = {
	0x75, 0x38, 0x8b, 0x16, 0x51, 0x27, 0x76, 0xcc,
	0x5d, 0xba, 0x5d, 0xa1, 0xfd, 0x89, 0x01, 0x50,
	0xb0, 0xc6, 0x45, 0x5c, 0xb4, 0xf5, 0x8b, 0x19,
	0x52, 0x52, 0x25, 0x25
};

/* Vector 3: one million 'a' characters */
static unsigned const char v3_224_digest[] = {
	0x20, 0x79, 0x46, 0x55, 0x98, 0x0c, 0x91, 0xd8,
	0xbb, 0xb4, 0xc1, 0xea, 0x97, 0x61, 0x8a, 0x4b,
	0xf0, 0x3f, 0x42, 0x58, 0x19, 0x48, 0xb2, 0xee,
	0x4e, 0xe7, 0xad, 0x67
};

static void dumpBuffer(
	const char *bufName,	// optional
	const unsigned char *buf,
	unsigned len)
{
	unsigned i;
	
	if(bufName) {
		printf("%s\n", bufName);
	}
	printf("   ");
	for(i=0; i<len; i++) {
		printf("%02X", buf[i]);
		if((i % 4) == 3) {
			printf(" ");
		}
		if((i % 32) == 31) {
			printf("\n   ");
		}
	}
	printf("\n");
}

static int doTest(
	const char *title,
	const unsigned char *digest,	/* 2 * digestLen bytes, second half should be zero */
	const unsigned char *expect,
	unsigned digestLen)
{
	if(memcmp(digest, expect, digestLen)) {
		printf("**Error on %s\n", title);
		dumpBuffer("Expected", expect, digestLen);
		dumpBuffer("Obtained", digest, digestLen);
		return 1;
	}
	const unsigned char *cp = digest + digestLen;
	for(unsigned dex=0; dex<digestLen; dex++) {
		if(*cp++ != 0) {
			printf("***%s: Buffer overwrite at byte %u\n", title, dex);
			dumpBuffer("This should be all zeroes:", digest+digestLen, digestLen);
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	bool quiet = false;
	
	for(int arg=1; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'q':
				quiet = true;
				break;
			default:
				usage(argv);
		}
	}
	
	if(!quiet) {
		printf("...testing SHA224\n");
	}
	
	CC_SHA256_CTX ctx224;
	/* double the digest and check for mods at end */
	unsigned char dig224[CC_SHA224_DIGEST_LENGTH * 2];
	
	memset(dig224, 0, CC_SHA224_DIGEST_LENGTH * 2);
	CC_SHA224_Init(&ctx224);
	CC_SHA224_Update(&ctx224, v1_224_text, v1_224_textLen);
	CC_SHA224_Final(dig224, &ctx224);
	if(doTest("SHA224 Vector 1", dig224, v1_224_digest, CC_SHA224_DIGEST_LENGTH)) {
		exit(1);
	}
	
	memset(dig224, 0, CC_SHA224_DIGEST_LENGTH);
	CC_SHA224_Init(&ctx224);
	CC_SHA224_Update(&ctx224, v2_224_text, v2_224_textLen);
	CC_SHA224_Final(dig224, &ctx224);
	if(doTest("SHA224 Vector 2", dig224, v2_224_digest, CC_SHA224_DIGEST_LENGTH)) {
		exit(1);
	}

	memset(dig224, 0, CC_SHA224_DIGEST_LENGTH);
	CC_SHA224_Init(&ctx224);
	/* one million 'a' chars; do 100,000 loops of ten of 'em */
	for(unsigned dex=0; dex<100000; dex++) {
		CC_SHA224_Update(&ctx224, "aaaaaaaaaa", 10);
	}
	CC_SHA224_Final(dig224, &ctx224);
	if(doTest("SHA224 Vector 3", dig224, v3_224_digest, CC_SHA224_DIGEST_LENGTH)) {
		exit(1);
	}

	if(!quiet) {
		printf("...testing SHA256\n");
	}
	CC_SHA256_CTX ctx256;
	
	/* double the digest and check for mods at end */
	unsigned char dig256[CC_SHA256_DIGEST_LENGTH * 2];
	
	memset(dig256, 0, CC_SHA256_DIGEST_LENGTH * 2);
	CC_SHA256_Init(&ctx256);
	CC_SHA256_Update(&ctx256, v1_256_text, v1_256_textLen);
	CC_SHA256_Final(dig256, &ctx256);
	if(doTest("SHA256 Vector 1", dig256, v1_256_digest, CC_SHA256_DIGEST_LENGTH)) {
		exit(1);
	}
	
	memset(dig256, 0, CC_SHA256_DIGEST_LENGTH);
	CC_SHA256_Init(&ctx256);
	CC_SHA256_Update(&ctx256, v2_256_text, v2_256_textLen);
	CC_SHA256_Final(dig256, &ctx256);
	if(doTest("SHA256 Vector 2", dig256, v2_256_digest, CC_SHA256_DIGEST_LENGTH)) {
		exit(1);
	}

	memset(dig256, 0, CC_SHA256_DIGEST_LENGTH);
	CC_SHA256_Init(&ctx256);
	/* one million 'a' chars; do 100,000 loops of ten of 'em */
	for(unsigned dex=0; dex<100000; dex++) {
		CC_SHA256_Update(&ctx256, "aaaaaaaaaa", 10);
	}
	CC_SHA256_Final(dig256, &ctx256);
	if(doTest("SHA256 Vector 3", dig256, v3_256_digest, CC_SHA256_DIGEST_LENGTH)) {
		exit(1);
	}

	if(!quiet) {
		printf("...testing SHA384\n");
	}

	CC_SHA512_CTX ctx384;
	unsigned char dig384[CC_SHA384_DIGEST_LENGTH * 2];
	
	memset(dig384, 0, CC_SHA384_DIGEST_LENGTH * 2);
	CC_SHA384_Init(&ctx384);
	CC_SHA384_Update(&ctx384, v1_384_text, v1_384_textLen);
	CC_SHA384_Final(dig384, &ctx384);
	if(doTest("SHA384 Vector 1", dig384, v1_384_digest, CC_SHA384_DIGEST_LENGTH)) {
		exit(1);
	}

	memset(dig384, 0, CC_SHA384_DIGEST_LENGTH);
	CC_SHA384_Init(&ctx384);
	CC_SHA384_Update(&ctx384, v2_384_text, v2_384_textLen);
	CC_SHA384_Final(dig384, &ctx384);
	if(doTest("SHA384 Vector 2", dig384, v2_384_digest, CC_SHA384_DIGEST_LENGTH)) {
		exit(1);
	}

	memset(dig384, 0, CC_SHA384_DIGEST_LENGTH);
	CC_SHA384_Init(&ctx384);
	/* one million 'a' chars; do 100,000 loops of ten of 'em */
	for(unsigned dex=0; dex<100000; dex++) {
		CC_SHA384_Update(&ctx384, "aaaaaaaaaa", 10);
	}
	CC_SHA384_Final(dig384, &ctx384);
	if(doTest("SHA384 Vector 3", dig384, v3_384_digest, CC_SHA384_DIGEST_LENGTH)) {
		exit(1);
	}

	if(!quiet) {
		printf("...testing SHA512\n");
	}

	CC_SHA512_CTX ctx512;
	unsigned char dig512[CC_SHA512_DIGEST_LENGTH * 2];

	memset(dig512, 0, CC_SHA512_DIGEST_LENGTH * 2);
	CC_SHA512_Init(&ctx512);
	CC_SHA512_Update(&ctx512, v1_512_text, v1_512_textLen);
	CC_SHA512_Final(dig512, &ctx512);
	if(doTest("SHA512 Vector 1", dig512, v1_512_digest, CC_SHA512_DIGEST_LENGTH)) {
		exit(1);
	}

	memset(dig512, 0, CC_SHA512_DIGEST_LENGTH);
	CC_SHA512_Init(&ctx512);
	CC_SHA512_Update(&ctx512, v2_512_text, v2_512_textLen);
	CC_SHA512_Final(dig512, &ctx512);
	if(doTest("SHA512 Vector 2", dig512, v2_512_digest, CC_SHA512_DIGEST_LENGTH)) {
		exit(1);
	}

	memset(dig512, 0, CC_SHA512_DIGEST_LENGTH);
	CC_SHA512_Init(&ctx512);
	/* one million 'a' chars; do 100,000 loops of ten of 'em */
	for(unsigned dex=0; dex<100000; dex++) {
		CC_SHA512_Update(&ctx512, "aaaaaaaaaa", 10);
	}
	CC_SHA512_Final(dig512, &ctx512);
	if(doTest("SHA512 Vector 3", dig512, v3_512_digest, CC_SHA512_DIGEST_LENGTH)) {
		exit(1);
	}

	if(!quiet) {
		printf("...SHA2 test vectors passed\n");
	}
	return 0;
}
