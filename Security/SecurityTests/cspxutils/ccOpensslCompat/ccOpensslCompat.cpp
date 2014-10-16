/*
 * ccOpensslCompat.cpp - verify that the compatibility macros at the end of CommonDigest.h
 *                       result in openssl-compatible calls
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include "digestCommonExtern.h"
#include "common.h"

#define MAX_DIGEST_LEN		64

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("   q              -- quiet\n");
	/* etc. */
	exit(1);
}

int main(int argc, char **argv)
{
	int quiet = false;
	int arg;
	char *argp;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'q':
				quiet = true;
				break;
			default:
				usage(argv);
		}
	}
	
	testStartBanner("ccOpensslCompat", argc, argv);
	
	const char *str = "digest this";
	unsigned len = strlen(str);
	unsigned char digestOS[MAX_DIGEST_LEN];
	unsigned char digestCC[MAX_DIGEST_LEN];
	
	/* MD2 */
	if(!quiet) {
		printf("...testing MD2\n");
	}
	if(md2os(str, len, digestOS)) {
		printf("***Error on openssl MD2\n");
		exit(1);
	}
	if(md2cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto MD2\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 16)) {
		printf("***MD2 Digest miscompare\n");
		exit(1);
	}
	
	/* MD4 */
	if(!quiet) {
		printf("...testing MD4\n");
	}
	if(md4os(str, len, digestOS)) {
		printf("***Error on openssl MD4\n");
		exit(1);
	}
	if(md4cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto MD5\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 16)) {
		printf("***MD4 Digest miscompare\n");
		exit(1);
	}

	/* MD5 */
	if(!quiet) {
		printf("...testing MD5\n");
	}
	if(md5os(str, len, digestOS)) {
		printf("***Error on openssl MD5\n");
		exit(1);
	}
	if(md5cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto MD5\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 16)) {
		printf("***MD5 Digest miscompare\n");
		exit(1);
	}

	/* SHA1 */
	if(!quiet) {
		printf("...testing SHA1\n");
	}
	if(sha1os(str, len, digestOS)) {
		printf("***Error on openssl SHA1\n");
		exit(1);
	}
	if(sha1cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto SHA1\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 20)) {
		printf("***SHA1 Digest miscompare\n");
		exit(1);
	}

	/* SHA224 */
	if(!quiet) {
		printf("...testing SHA224\n");
	}
	if(sha224os(str, len, digestOS)) {
		printf("***Error on openssl SHA224\n");
		exit(1);
	}
	if(sha224cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto SHA224\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 28)) {
		printf("***SHA224 Digest miscompare\n");
		exit(1);
	}

	/* SHA256 */
	if(!quiet) {
		printf("...testing SHA256\n");
	}
	if(sha256os(str, len, digestOS)) {
		printf("***Error on openssl SHA256\n");
		exit(1);
	}
	if(sha256cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto SHA256\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 32)) {
		printf("***SHA256 Digest miscompare\n");
		exit(1);
	}

	/* SHA384 */
	if(!quiet) {
		printf("...testing SHA384\n");
	}
	if(sha384os(str, len, digestOS)) {
		printf("***Error on openssl SHA384\n");
		exit(1);
	}
	if(sha384cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto SHA384\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 48)) {
		printf("***SHA384 Digest miscompare\n");
		exit(1);
	}

	/* SHA512 */
	if(!quiet) {
		printf("...testing SHA512\n");
	}
	if(sha512os(str, len, digestOS)) {
		printf("***Error on openssl SHA512\n");
		exit(1);
	}
	if(sha512cc(str, len, digestCC)) {
		printf("***Error on CommonCrypto SHA512\n");
		exit(1);
	}
	if(memcmp(digestOS, digestCC, 64)) {
		printf("***SHA512 Digest miscompare\n");
		exit(1);
	}

	if(!quiet) {
		printf("...success\n");
	}
	return 0;
}
