/*
 * sshKey.cpp - Standalone SSH key parser and converter. Uses libcrypto for 
 *              representing and storing RSA and DSA keys and for 
 *              writing and reading BIGNUMS to/from memory.
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuEnc64.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/devrandom.h>
#include <ctype.h>

#define dprintf(s...)		printf(s)

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -i inFile\n");
	printf("  -o outFile\n");
	printf("  -v             -- private key input; default is public\n");
	printf("  -V             -- private key output; default is public\n");
	printf("  -d             -- DSA; default is RSA\n");
	printf("  -r             -- parse & print inFile\n");
	printf("  -f ssh1|ssh2   -- input format; default = ssh2\n");
	printf("  -F ssh1|ssh2   -- output format; default = ssh2\n");
	printf("  -p password\n");
	printf("  -P             -- no password; private keys in the clear\n");
	printf("  -c comment\n");
	exit(1);
}

static const char *authfile_id_string = "SSH PRIVATE KEY FILE FORMAT 1.1\n";

/* from openssh cipher.h */
#define SSH_CIPHER_NONE		0	/* no encryption */
#define SSH_CIPHER_IDEA		1	/* IDEA CFB */
#define SSH_CIPHER_DES		2	/* DES CBC */
#define SSH_CIPHER_3DES		3	/* 3DES CBC */
#define SSH_CIPHER_BROKEN_TSS	4	/* TRI's Simple Stream encryption CBC */
#define SSH_CIPHER_BROKEN_RC4	5	/* Alleged RC4 */
#define SSH_CIPHER_BLOWFISH	6
#define SSH_CIPHER_RESERVED	7

#define SSH2_RSA_HEADER		"ssh-rsa"
#define SSH2_DSA_HEADER		"ssh-dss"

#pragma mark --- commmon code --- 

static uint32_t readUint32(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len)					// IN/OUT 
{
	uint32_t r = 0;
	
	for(unsigned dex=0; dex<sizeof(uint32_t); dex++) {
		r <<= 8;
		r |= *cp++;
	}
	len -= 4;
	return r;
}

static uint16_t readUint16(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len)					// IN/OUT 
{
	uint16_t r = *cp++;
	r <<= 8;
	r |= *cp++;
	len -= 2;
	return r;
}

static void appendUint32(
	CFMutableDataRef cfOut,
	uint32_t ui)
{
	UInt8 buf[sizeof(uint32_t)];
	
	for(int dex=(sizeof(uint32_t) - 1); dex>=0; dex--) {
		buf[dex] = ui & 0xff;
		ui >>= 8;
	}
	CFDataAppendBytes(cfOut, buf, sizeof(uint32_t));
}

static void appendUint16(
	CFMutableDataRef cfOut,
	uint16_t ui)
{
	UInt8 buf[sizeof(uint16_t)];
	
	buf[1] = ui & 0xff;
	ui >>= 8;
	buf[0] = ui;
	CFDataAppendBytes(cfOut, buf, sizeof(uint16_t));
}

/* parse text as decimal, return BIGNUM */
static BIGNUM *parseDecimalBn(
	const unsigned char *cp,
	unsigned len)
{
	for(unsigned dex=0; dex<len; dex++) {
		char c = *cp;
		if((c < '0') || (c > '9')) {
			return NULL;
		}
	}
	char *str = (char *)malloc(len + 1);
	memmove(str, cp, len);
	str[len] = '\0';
	BIGNUM *bn = NULL;
	BN_dec2bn(&bn, str);
	free(str);
	return bn;
}

/* Read BIGNUM, OpenSSH-1 version */
static BIGNUM *readBigNum(
	const unsigned char *&cp,	// IN/OUT
	unsigned &remLen)			// IN/OUT
{
	if(remLen < sizeof(uint16_t)) {
		dprintf("readBigNum: short record(1)\n");
		return NULL;
	}
	uint16_t numBits = readUint16(cp, remLen);
	unsigned bytes = (numBits + 7) / 8;
	if(remLen < bytes) {
		dprintf("readBigNum: short record(2)\n");
		return NULL;
	}
	BIGNUM *bn = BN_bin2bn(cp, bytes, NULL);
	if(bn == NULL) {
		dprintf("readBigNum: BN_bin2bn error\n");
		return NULL;
	}
	cp += bytes;
	remLen -= bytes;
	return bn;
}
	
/* Write BIGNUM, OpenSSH-1 version */
static int appendBigNum(
	CFMutableDataRef cfOut, 
	const BIGNUM *bn)
{
	/* 16 bits of numbits */
	unsigned numBits = BN_num_bits(bn);
	appendUint16(cfOut, numBits);
	
	/* serialize the bytes */
	int numBytes = (numBits + 7) / 8;
	unsigned char outBytes[numBytes];	// gcc is so cool...
	int moved = BN_bn2bin(bn, outBytes);
	if(moved != numBytes) {
		dprintf("appendBigNum: BN_bn2bin() screwup\n");
		return -1;
	}
	CFDataAppendBytes(cfOut, (UInt8 *)outBytes, numBytes);
	return 0;
}

/* read BIGNUM, OpenSSH-2 mpint version */
static BIGNUM *readBigNum2(
	const unsigned char *&cp,	// IN/OUT
	unsigned &remLen)			// IN/OUT
{
	if(remLen < 4) {
		dprintf("readBigNum2: short record(1)\n");
		return NULL;
	}
	uint32_t bytes = readUint32(cp, remLen);
	if(remLen < bytes) {
		dprintf("readBigNum2: short record(2)\n");
		return NULL;
	}
	BIGNUM *bn = BN_bin2bn(cp, bytes, NULL);
	if(bn == NULL) {
		dprintf("readBigNum2: BN_bin2bn error\n");
		return NULL;
	}
	cp += bytes;
	remLen -= bytes;
	return bn;
}

/* write BIGNUM, OpenSSH v2 format (with a 4-byte byte count) */
static int appendBigNum2(
	CFMutableDataRef cfOut,
	const BIGNUM *bn)
{
	if(bn == NULL) {
		dprintf("appendBigNum2: NULL bn");
		return -1;
	}
	if (BN_is_zero(bn)) {
		appendUint32(cfOut, 0);
		return 0;
	}
	if(bn->neg) {
		dprintf("appendBigNum2: negative numbers not supported\n");
		return -1;
	}
	int numBytes = BN_num_bytes(bn);
	unsigned char buf[numBytes];
	int moved = BN_bn2bin(bn, buf);
	if(moved != numBytes) {
		dprintf("appendBigNum: BN_bn2bin() screwup\n");
		return -1;
	}
	bool appendZero = false;
	if(buf[0] & 0x80) {
		/* prepend leading zero to make it positive */
		appendZero = true;
		numBytes++;		// to encode the correct 4-byte length 
	}
	appendUint32(cfOut, (uint32_t)numBytes);
	if(appendZero) {
		UInt8 z = 0;
		CFDataAppendBytes(cfOut, &z, 1);
		numBytes--;		// to append the correct number of bytes
	}
	CFDataAppendBytes(cfOut, buf, numBytes);
	memset(buf, 0, numBytes);
	return 0;
}

/* Write BIGNUM, OpenSSH-1 decimal (public key) version */
static int appendBigNumDec(
	CFMutableDataRef cfOut, 
	const BIGNUM *bn)
{
	char *buf = BN_bn2dec(bn);
	if(buf == NULL) {
		dprintf("appendBigNumDec: BN_bn2dec() error");
		return -1;
	}
	CFDataAppendBytes(cfOut, (const UInt8 *)buf, strlen(buf));
	OPENSSL_free(buf);
	return 0;
}

/* write string, OpenSSH v2 format (with a 4-byte byte count) */
static void appendString(
	CFMutableDataRef cfOut,
	const char *str,
	unsigned strLen)
{
	appendUint32(cfOut, (uint32_t)strLen);
	CFDataAppendBytes(cfOut, (UInt8 *)str, strLen);
}

/* skip whitespace */
static void skipWhite(
	const unsigned char *&cp,
	unsigned &bytesLeft)
{
	while(bytesLeft != 0) {
		if(isspace((int)(*cp))) {
			cp++;
			bytesLeft--;
		}
		else {
			return;
		}
	}
}

/* find next whitespace or EOF - if EOF, rtn pointer points to one past EOF */
static const unsigned char *findNextWhite(
	const unsigned char *cp,
	unsigned &bytesLeft)
{
	while(bytesLeft != 0) {
		if(isspace((int)(*cp))) {
			return cp;
		}
		cp++;
		bytesLeft--;
	}
	return cp;
}


/* 
 * Calculate d mod{p-1,q-1}
 * Used when decoding OpenSSH-1 private RSA key.
 */
static int
rsa_generate_additional_parameters(RSA *rsa)
{
	BIGNUM *aux;
	BN_CTX *ctx;
	
	if((rsa->dmq1 = BN_new()) == NULL) {
		dprintf("rsa_generate_additional_parameters: BN_new failed");
		return -1;
	}
	if((rsa->dmp1 = BN_new()) == NULL) {
		dprintf("rsa_generate_additional_parameters: BN_new failed");
		return -1;
	}
	if ((aux = BN_new()) == NULL) {
		dprintf("rsa_generate_additional_parameters: BN_new failed");
		return -1;
	}
	if ((ctx = BN_CTX_new()) == NULL) {
		dprintf("rsa_generate_additional_parameters: BN_CTX_new failed");
		BN_clear_free(aux);
		return -1;
	} 

	BN_sub(aux, rsa->q, BN_value_one());
	BN_mod(rsa->dmq1, rsa->d, aux, ctx);

	BN_sub(aux, rsa->p, BN_value_one());
	BN_mod(rsa->dmp1, rsa->d, aux, ctx);

	BN_clear_free(aux);
	BN_CTX_free(ctx);
	return 0;
}

#pragma mark --- OpenSSH-1 crypto --- 

static int ssh1DES3Crypt(
	unsigned char cipher,
	bool doEncrypt,
	const unsigned char *inText,
	unsigned inTextLen,
	const char *password,		// C string
	unsigned char *outText,		// data RETURNED here, caller mallocs
	unsigned *outTextLen)		// RETURNED
{
	switch(cipher) {
		case SSH_CIPHER_3DES:
			break;
		case SSH_CIPHER_NONE:
			/* cleartext RSA private key, e.g. host key. */
			memmove(outText, inText, inTextLen);
			*outTextLen = inTextLen;
			return 0;
		default:
			/* who knows how we're going to figure these out */
			printf("***Unsupported cipher (%u)\n", cipher);
			return -1;
	}
	
	/* key starts with MD5(password) */
	unsigned char pwdDigest[CC_MD5_DIGEST_LENGTH];
	CC_MD5(password, strlen(password), pwdDigest);
	
	/* three keys from that, like so: */
	unsigned char k1[kCCKeySizeDES];
	unsigned char k2[kCCKeySizeDES];
	unsigned char k3[kCCKeySizeDES];
	memmove(k1, pwdDigest, kCCKeySizeDES);
	memmove(k2, pwdDigest + kCCKeySizeDES, kCCKeySizeDES);
	memmove(k3, pwdDigest, kCCKeySizeDES);
	
	CCOperation op1_3;
	CCOperation op2;
	if(doEncrypt) {
		op1_3 = kCCEncrypt;
		op2   = kCCDecrypt;
	}
	else {
		op1_3 = kCCDecrypt;
		op2   = kCCEncrypt;
	}
	
	/* the openssh v1 pseudo triple DES. Each DES pass has its own CBC. */
	size_t moved = 0;

	CCCryptorStatus cstat = CCCrypt(op1_3, kCCAlgorithmDES, 
		0,						// no padding
		k1, kCCKeySizeDES, 
		NULL,		// IV
		inText, inTextLen,
		outText, inTextLen, &moved);
	if(cstat) {
		dprintf("***ssh1DES3Crypt: CCCrypt()(1) returned %u\n", (unsigned)cstat);
		return -1;
	}
	cstat = CCCrypt(op2, kCCAlgorithmDES, 
		0,						// no padding - SSH does that itself 
		k2, kCCKeySizeDES, 
		NULL,		// IV
		outText, moved,
		outText, inTextLen, &moved);
	if(cstat) {
		dprintf("***ssh1DES3Crypt: CCCrypt()(2) returned %u\n", (unsigned)cstat);
		return -1;
	}
	cstat = CCCrypt(op1_3, kCCAlgorithmDES, 
		0,						// no padding - SSH does that itself 
		k3, kCCKeySizeDES, 
		NULL,		// IV
		outText, moved,
		outText, inTextLen, &moved);
	if(cstat) {
		dprintf("***ssh1DES3Crypt: CCCrypt()(3) returned %u\n", (unsigned)cstat);
		return -1;
	}

	*outTextLen = moved;	
	return 0;
}
	
#pragma mark --- OpenSSH-1 decode --- 

/* Decode OpenSSH-1 RSA private key */
static int decodeSSH1RSAPrivKey(
	const unsigned char *key,
	unsigned keyLen,
	char *password,
	RSA *rsa,						// returned
	char **comment)					// returned
{
	const unsigned char *cp = key;		// running pointer
	unsigned remLen = keyLen;
	unsigned len = strlen(authfile_id_string);
	
	/* length: ID string, NULL, Cipher, 4-byte spare */
	if(remLen < (len + 6)) {
		dprintf("decodeSSH1RSAPrivKey: short record(1)\n");
		return -1;
	}
	
	/* ID string plus a NULL */
	if(memcmp(authfile_id_string, cp, len)) {
		dprintf("decodeSSH1RSAPrivKey: bad header\n");
		return -1;
	}
	cp += (len + 1);
	remLen -= (len + 1);
	
	/* cipher */
	unsigned char cipherSpec = *cp;
	switch(cipherSpec) {
		case SSH_CIPHER_NONE:
			if(password != NULL) {
				dprintf("decodeSSH1RSAPrivKey: Attempt to decrypt plaintext key\n");
				return -1;
			}
			break;
		case SSH_CIPHER_3DES:
			if(password == NULL) {
				dprintf("decodeSSH1RSAPrivKey: Encrypted key with no decryptKey\n");
				return -1;
			}
			break;
		default:
			/* I hope we don't see any other values here */
			dprintf("decodeOpenSSHv1PrivKey: unknown cipherSpec (%u)\n", cipherSpec);
				return -1;
	}
	
	/* skip cipher, spares */
	cp += 5;
	remLen -= 5;
	
	/*
	 * Clear text public key:
	 * uint32 bits
	 * bignum n
	 * bignum e
	 */
	if(remLen < sizeof(uint32_t)) {
		dprintf("decodeSSH1RSAPrivKey: bad len(1)\n");
		return -1;
	}
	/* skip over keybits */
	readUint32(cp, remLen);
	rsa->n = readBigNum(cp, remLen);
	if(rsa->n == NULL) {
		dprintf("decodeSSH1RSAPrivKey: error decoding n\n");
		return -1;
	}
	rsa->e = readBigNum(cp, remLen);
	if(rsa->e == NULL) {
		dprintf("decodeSSH1RSAPrivKey: error decoding e\n");
		return -1;
	}
	
	/* comment string: 4-byte length and the string w/o NULL */
	if(remLen < sizeof(uint32_t)) {
		dprintf("decodeSSH1RSAPrivKey: bad len(2)\n");
		return -1;
	}
	uint32_t commentLen = readUint32(cp, remLen);
	if(commentLen > remLen) {
		dprintf("decodeSSH1RSAPrivKey: bad len(3)\n");
		return -1;
	}
	*comment = (char *)malloc(commentLen + 1);
	memmove(*comment, cp, commentLen);
	(*comment)[commentLen] = '\0';
	cp += commentLen;
	remLen -= commentLen;
	
	/* everything that remains is ciphertext */
	unsigned char ptext[remLen];
	unsigned ptextLen = 0;
	if(ssh1DES3Crypt(cipherSpec, false, cp, remLen, password, ptext, &ptextLen)) {
		dprintf("decodeSSH1RSAPrivKey: decrypt error\n");
		return -1;
	}
	/* subsequent errors to errOut: */
	
	int ourRtn = 0;
	
	/* plaintext contents:
	
	[0-1]		-- random bytes
	[2-3]		-- copy of [01] for passphrase validity checking
	buffer_put_bignum(d)
	buffer_put_bignum(iqmp)
	buffer_put_bignum(q)
	buffer_put_bignum(p)
	pad to block size
	*/
	cp = ptext;
	remLen = ptextLen;
	if(remLen < 4) {
		dprintf("decodeSSH1RSAPrivKey: bad len(4)\n");
		ourRtn = -1;
		goto errOut;
	}
	if((cp[0] != cp[2]) || (cp[1] != cp[3])) {
		/* decrypt fail */
		dprintf("decodeSSH1RSAPrivKey: check byte error\n");
		ourRtn = -1;
		goto errOut;
	}
	cp += 4;
	remLen -= 4;
	
	/* remainder comprises private portion of RSA key */
	rsa->d = readBigNum(cp, remLen);
	if(rsa->d == NULL) {
		dprintf("decodeSSH1RSAPrivKey: error decoding d\n");
		return -1;
	}
	rsa->iqmp = readBigNum(cp, remLen);
	if(rsa->iqmp == NULL) {
		dprintf("decodeSSH1RSAPrivKey: error decoding iqmp\n");
		return -1;
	}
	rsa->q = readBigNum(cp, remLen);
	if(rsa->q == NULL) {
		dprintf("decodeSSH1RSAPrivKey: error decoding q\n");
		return -1;
	}
	rsa->p = readBigNum(cp, remLen);
	if(rsa->p == NULL) {
		dprintf("decodeSSH1RSAPrivKey: error decoding p\n");
		return -1;
	}

	/* calculate d mod{p-1,q-1} */
	ourRtn = rsa_generate_additional_parameters(rsa);

errOut:
	memset(ptext, 0, ptextLen);
	return ourRtn; 
}

/* Decode OpenSSH-1 RSA public key */
static int decodeSSH1RSAPubKey(
	const unsigned char *key,
	unsigned keyLen,
	RSA *rsa,						// returned
	char **comment)					// returned
{
	const unsigned char *cp = key;		// running pointer
	unsigned remLen = keyLen;
	
	*comment = NULL;
	skipWhite(cp, remLen);
	
	/* 
	 * cp points to start of size_in_bits in ASCII decimal' we really don't care about 
	 * this field. Find next space.
	 */
	cp = findNextWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("decodeSSH1RSAPubKey: short key (1)\n");
		return -1;
	}
	skipWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("decodeSSH1RSAPubKey: short key (2)\n");
		return -1;
	}
	
	/*
	 * cp points to start of e
	 */
	const unsigned char *ep = findNextWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("decodeSSH1RSAPubKey: short key (3)\n");
		return -1;
	}
	unsigned len = ep - cp;
	rsa->e = parseDecimalBn(cp, len);
	if(rsa->e == NULL) {
		return -1;
	}
	cp += len;
	remLen -= len;
	
	skipWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("decodeSSH1RSAPubKey: short key (4)\n");
		return -1;
	}
	
	/* cp points to start of n */
	ep = findNextWhite(cp, remLen);
	len = ep - cp;
	rsa->n = parseDecimalBn(cp, len);
	if(rsa->n == NULL) {
		return -1;
	}
	cp += len;
	remLen -= len;
	skipWhite(cp, remLen);
	if(remLen == 0) {
		/* no comment; we're done */
		return 0;
	}

	ep = findNextWhite(cp, remLen);
	len = ep - cp;
	if(len == 0) {
		return 0;
	}
	*comment = (char *)malloc(len + 1);
	memmove(*comment, cp, len);
	if((*comment)[len - 1] == '\n') {
		/* normal case closes with a newline, not part of the comment */
		len--;
	}
	(*comment)[len] = '\0';
	return 0;

}

#pragma mark --- OpenSSH-1 encode --- 

/* Encode OpenSSH-1 RSA private key */
static int encodeSSH1RSAPrivKey(
	RSA *rsa, 
	const char *password, 
	const char *comment, 
	unsigned char **outKey,		// mallocd and RETURNED 
	unsigned *outKeyLen) 		// RETURNED
{
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	
	/* ID string including NULL */
	CFDataAppendBytes(cfOut, (const UInt8 *)authfile_id_string, strlen(authfile_id_string) + 1);
	
	/* one byte cipher */
	UInt8 cipherSpec = SSH_CIPHER_3DES;
	CFDataAppendBytes(cfOut, &cipherSpec, 1);
	
	/* spares */
	UInt8 spares[4] = {0};
	CFDataAppendBytes(cfOut, spares, 4);
	
	/*
	 * Clear text public key:
	 * uint32 bits
	 * bignum n
	 * bignum e
	 */
	uint32_t keybits = RSA_size(rsa) * 8;
	appendUint32(cfOut, keybits);
	appendBigNum(cfOut, rsa->n);
	appendBigNum(cfOut, rsa->e);

	/* comment string: 4-byte length and the string w/o NULL */
	if(comment) {
		uint32_t len = strlen(comment);
		appendUint32(cfOut, len);
		CFDataAppendBytes(cfOut, (const UInt8 *)comment, len);
	}
	
	/* 
	 * Remainder is encrypted, consisting of
	 *
	 * [0-1]		-- random bytes
	 * [2-3]		-- copy of [01] for passphrase validity checking
	 * buffer_put_bignum(d)
	 * buffer_put_bignum(iqmp)
	 * buffer_put_bignum(q)
	 * buffer_put_bignum(p)
	 * pad to block size
	 */
	CFMutableDataRef ptext = CFDataCreateMutable(NULL, 0);
	
	/* [0..3] check bytes */
	UInt8 checkBytes[4];
	DevRandomGenerator rng = DevRandomGenerator();
	rng.random(checkBytes, 2);
	checkBytes[2] = checkBytes[0];
	checkBytes[3] = checkBytes[1];
	CFDataAppendBytes(ptext, checkBytes, 4);
	
	/* d, iqmp, q, p */
	appendBigNum(ptext, rsa->d);
	appendBigNum(ptext, rsa->iqmp);
	appendBigNum(ptext, rsa->q);
	appendBigNum(ptext, rsa->p);
	
	/* encrypt it */
	unsigned ptextLen = CFDataGetLength(ptext);
	unsigned padding = 0;
	unsigned rem = ptextLen & 0x7;
	if(rem) {
		padding = 8 - rem;
	}
	UInt8 padByte = 0;
	for(unsigned dex=0; dex<padding; dex++) {
		CFDataAppendBytes(ptext, &padByte, 1);
	}
	ptextLen = CFDataGetLength(ptext);
	unsigned char ctext[ptextLen];
	unsigned ctextLen;
	int ourRtn = ssh1DES3Crypt(SSH_CIPHER_3DES, true, 
		(unsigned char *)CFDataGetBytePtr(ptext), ptextLen, 
		password,
		ctext, &ctextLen);
	if(ourRtn != 0) {
		goto errOut;
	}
	
	/* appended encrypted portion */
	CFDataAppendBytes(cfOut, ctext, ctextLen);
	*outKeyLen = (unsigned)CFDataGetLength(cfOut);
	*outKey = (unsigned char *)malloc(*outKeyLen);
	memmove(*outKey, CFDataGetBytePtr(cfOut), *outKeyLen);
errOut:
	CFRelease(cfOut);
	/* it would be proper to zero out ptext here, but we can't do that to a CFData */
	CFRelease(ptext);
	return ourRtn;
}

/* Encode OpenSSH-1 RSA public key */
static int encodeSSH1RSAPubKey(
	RSA *rsa, 
	const char *comment, 
	unsigned char **outKey,		// mallocd and RETURNED 
	unsigned *outKeyLen) 		// RETURNED
{
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	
	/*
	 * Format is
	 * num_bits in decimal
	 * <space>
	 * e, bignum in decimal
	 * <space>
	 * n, bignum in decimal
	 * <space>
	 * optional comment
	 * newline
	 */
	int ourRtn = 0;
	unsigned numBits = BN_num_bits(rsa->n);
	char bitString[20];
	UInt8 c = ' ';

	snprintf(bitString, sizeof(bitString), "%u ", numBits);
	CFDataAppendBytes(cfOut, (const UInt8 *)bitString, strlen(bitString));
	if(appendBigNumDec(cfOut, rsa->e)) {
		ourRtn = -1;
		goto errOut;
	}
	CFDataAppendBytes(cfOut, &c, 1);
	if(appendBigNumDec(cfOut, rsa->n)) {
		ourRtn = -1;
		goto errOut;
	}
	if(comment != NULL) {
		CFDataAppendBytes(cfOut, &c, 1);
		CFDataAppendBytes(cfOut, (UInt8 *)comment, strlen(comment));
	}
	c = '\n';
	CFDataAppendBytes(cfOut, &c, 1);
	*outKeyLen = CFDataGetLength(cfOut);
	*outKey = (unsigned char *)malloc(*outKeyLen);
	memmove(*outKey, CFDataGetBytePtr(cfOut), *outKeyLen);
errOut:
	CFRelease(cfOut);
	return ourRtn;
}

#pragma mark --- OpenSSH-2 public key decode --- 

/* 
 * Decode components from an SSHv2 public key.
 * Also verifies the leading header, e.g. "ssh-rsa".
 * The returned decodedBlob is algorithm-specific.
 */
static int parseSSH2PubKey(
	const unsigned char *key,
	unsigned keyLen,
	const char *header,				// SSH2_RSA_HEADER, SSH2_DSA_HEADER
	unsigned char **decodedBlob,	// mallocd and RETURNED
	unsigned *decodedBlobLen,		// RETURNED
	char **comment)					// optionally mallocd and RETURNED, NULL terminated
{
	unsigned len = strlen(header);
	const unsigned char *endOfKey = key + keyLen;
	*decodedBlob = NULL;
	*comment = NULL;
	
	/* ID string plus at least one space */
	if(keyLen < (len + 1)) {
		dprintf("parseSSH2PubKey: short record(1)\n");
		return -1;
	}
	
	if(memcmp(header, key, len)) {
		dprintf("parseSSH2PubKey: bad header (1)\n");
		return -1;
	}
	key += len;
	if(*key++ != ' ') {
		dprintf("parseSSH2PubKey: bad header (2)\n");
		return -1;
	}
	keyLen -= (len + 1);

	/* key points to first whitespace after header */
	skipWhite(key, keyLen);
	if(keyLen == 0) {
		dprintf("parseSSH2PubKey: short key\n");
		return -1;
	}
	
	/* key is start of base64 blob */
	const unsigned char *encodedBlob = key;
	const unsigned char *endBlob = findNextWhite(key, keyLen);
	unsigned encodedBlobLen = endBlob - encodedBlob;
	
	/* decode base 64 */
	*decodedBlob = cuDec64(encodedBlob, encodedBlobLen, decodedBlobLen);
	if(*decodedBlob == NULL) {
		dprintf("parseSSH2PubKey: base64 decode error\n");
		return -1;
	}
	
	/* skip over the encoded blob and possible whitespace after it */
	key = endBlob;
	keyLen = endOfKey - endBlob;
	skipWhite(key, keyLen);
	if(keyLen == 0) {
		/* nothing remains, no comment, no error */
		return 0;
	}
	
	/* optional comment */
	*comment = (char *)malloc(keyLen + 1);
	memmove(*comment, key, keyLen);
	if((*comment)[keyLen - 1] == '\n') {
		/* normal case closes with a newline, not part of the comment */
		keyLen--;
	}
	(*comment)[keyLen] = '\0';
	return 0;
}
	
static int decodeSSH2RSAPubKey(
	const unsigned char *key,
	unsigned keyLen,
	RSA *rsa,						// returned
	char **comment)					// returned
{
	/* 
	 * Verify header
	 * get base64-decoded blob plus optional comment 
	 */
	unsigned char *decodedBlob = NULL;
	unsigned decodedBlobLen = 0;
	if(parseSSH2PubKey(key, keyLen, SSH2_RSA_HEADER, &decodedBlob, &decodedBlobLen, comment)) {
		return -1;
	}
	/* subsequent errors to errOut: */
	
	/*
	 * The inner base64-decoded blob, consisting of
	 * ssh-rsa
	 * e
	 * n
	 */
	uint32_t decLen;
	unsigned len;
	int ourRtn = 0;
	
	key = decodedBlob;
	keyLen = decodedBlobLen;
	if(keyLen < 12) {
		/* three length fields at least */
		dprintf("decodeSSH2RSAPubKey: short record(2)\n");
		ourRtn = -1;
		goto errOut;
	}
	decLen = readUint32(key, keyLen);
	len = strlen(SSH2_RSA_HEADER);
	if(decLen != len) {
		dprintf("decodeSSH2RSAPubKey: bad header (2)\n");
		ourRtn = -1;
		goto errOut;
	}
	if(memcmp(SSH2_RSA_HEADER, key, len)) {
		dprintf("decodeSSH2RSAPubKey: bad header (1)\n");
		return -1;
	}
	key += len;
	keyLen -= len;
	
	rsa->e = readBigNum2(key, keyLen);
	if(rsa->e == NULL) {
		ourRtn = -1;
		goto errOut;
	}
	rsa->n = readBigNum2(key, keyLen);
	if(rsa->n == NULL) {
		ourRtn = -1;
		goto errOut;
	}

errOut:
	free(decodedBlob);
	return ourRtn;
}

static int decodeSSH2DSAPubKey(
	const unsigned char *key,
	unsigned keyLen,
	DSA *dsa,						// returned
	char **comment)					// returned
{
	/* 
	 * Verify header
	 * get base64-decoded blob plus optional comment 
	 */
	unsigned char *decodedBlob = NULL;
	unsigned decodedBlobLen = 0;
	if(parseSSH2PubKey(key, keyLen, SSH2_DSA_HEADER, &decodedBlob, &decodedBlobLen, comment)) {
		return -1;
	}
	/* subsequent errors to errOut: */

	/*
	 * The inner base64-decoded blob, consisting of
	 * ssh-dss
	 * p
	 * q
	 * g
	 * pub_key
	 */
	uint32_t decLen;
	int ourRtn = 0;
	unsigned len;
	
	key = decodedBlob;
	keyLen = decodedBlobLen;
	if(keyLen < 20) {
		/* five length fields at least */
		dprintf("decodeSSH2DSAPubKey: short record(2)\n");
		ourRtn = -1;
		goto errOut;
	}
	decLen = readUint32(key, keyLen);
	len = strlen(SSH2_DSA_HEADER);
	if(decLen != len) {
		dprintf("decodeSSH2DSAPubKey: bad header (2)\n");
		ourRtn = -1;
		goto errOut;
	}
	if(memcmp(SSH2_DSA_HEADER, key, len)) {
		dprintf("decodeSSH2DSAPubKey: bad header (1)\n");
		return -1;
	}
	key += len;
	keyLen -= len;
	
	dsa->p = readBigNum2(key, keyLen);
	if(dsa->p == NULL) {
		ourRtn = -1;
		goto errOut;
	}
	dsa->q = readBigNum2(key, keyLen);
	if(dsa->q == NULL) {
		ourRtn = -1;
		goto errOut;
	}
	dsa->g = readBigNum2(key, keyLen);
	if(dsa->g == NULL) {
		ourRtn = -1;
		goto errOut;
	}
	dsa->pub_key = readBigNum2(key, keyLen);
	if(dsa->pub_key == NULL) {
		ourRtn = -1;
		goto errOut;
	}

errOut:
	free(decodedBlob);
	return ourRtn;
}

#pragma mark --- OpenSSH-2 public key encode --- 

static int encodeSSH2RSAPubKey(
	RSA *rsa, 
	const char *comment, 
	unsigned char **outKey,		// mallocd and RETURNED 
	unsigned *outKeyLen) 		// RETURNED
{
	unsigned char *b64 = NULL;
	unsigned b64Len;
	UInt8 c;
	
	/* 
	 * First, the inner base64-encoded blob, consisting of
	 * ssh-rsa
	 * e
	 * n
	 */
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	int ourRtn = 0;
	appendString(cfOut, SSH2_RSA_HEADER, strlen(SSH2_RSA_HEADER));
	ourRtn = appendBigNum2(cfOut, rsa->e);
	if(ourRtn) {
		goto errOut;
	}
	ourRtn = appendBigNum2(cfOut, rsa->n);
	if(ourRtn) {
		goto errOut;
	}
	
	/* base64 encode that */
	b64 = cuEnc64((unsigned char *)CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut), &b64Len);
	
	/* cuEnc64 added newline and NULL, which we really don't want */
	b64Len -= 2;
	
	/* Now start over, dropping that base64 into a public blob. */
	CFDataSetLength(cfOut, 0);
	CFDataAppendBytes(cfOut, (UInt8 *)SSH2_RSA_HEADER, strlen(SSH2_RSA_HEADER));
	c = ' ';
	CFDataAppendBytes(cfOut, &c, 1);
	CFDataAppendBytes(cfOut, b64, b64Len);
	
	/* optional comment */
	if(comment) {
		CFDataAppendBytes(cfOut, &c, 1);
		CFDataAppendBytes(cfOut, (UInt8 *)comment, strlen(comment));
	}
	
	/* finish it with a newline */
	c = '\n';
	CFDataAppendBytes(cfOut, &c, 1);
	
	*outKeyLen = (unsigned)CFDataGetLength(cfOut);
	*outKey = (unsigned char *)malloc(*outKeyLen);
	memmove(*outKey, CFDataGetBytePtr(cfOut), *outKeyLen);
	
errOut:
	CFRelease(cfOut);
	if(b64) {
		free(b64);
	}
	return ourRtn;
}

static int encodeSSH2DSAPubKey(
	DSA *dsa, 
	const char *comment, 
	unsigned char **outKey,		// mallocd and RETURNED 
	unsigned *outKeyLen) 		// RETURNED
{
	unsigned char *b64 = NULL;
	unsigned b64Len;
	UInt8 c;
	
	/* 
	 * First, the inner base64-encoded blob, consisting of
	 * ssh-dss
	 * p
	 * q
	 * g
	 * pub_key
	 */
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	int ourRtn = 0;
	appendString(cfOut, SSH2_DSA_HEADER, strlen(SSH2_DSA_HEADER));
	ourRtn = appendBigNum2(cfOut, dsa->p);
	if(ourRtn) {
		goto errOut;
	}
	ourRtn = appendBigNum2(cfOut, dsa->q);
	if(ourRtn) {
		goto errOut;
	}
	ourRtn = appendBigNum2(cfOut, dsa->g);
	if(ourRtn) {
		goto errOut;
	}
	ourRtn = appendBigNum2(cfOut, dsa->pub_key);
	if(ourRtn) {
		goto errOut;
	}
	
	/* base64 encode that */
	b64 = cuEnc64((unsigned char *)CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut), &b64Len);
	
	/* cuEnc64 added newline and NULL, which we really don't want */
	b64Len -= 2;
	
	/* Now start over, dropping that base64 into a public blob. */
	CFDataSetLength(cfOut, 0);
	CFDataAppendBytes(cfOut, (UInt8 *)SSH2_DSA_HEADER, strlen(SSH2_DSA_HEADER));
	c = ' ';
	CFDataAppendBytes(cfOut, &c, 1);
	CFDataAppendBytes(cfOut, b64, b64Len);
	
	/* optional comment */
	if(comment) {
		CFDataAppendBytes(cfOut, &c, 1);
		CFDataAppendBytes(cfOut, (UInt8 *)comment, strlen(comment));
	}
	
	/* finish it with a newline */
	c = '\n';
	CFDataAppendBytes(cfOut, &c, 1);
	
	*outKeyLen = (unsigned)CFDataGetLength(cfOut);
	*outKey = (unsigned char *)malloc(*outKeyLen);
	memmove(*outKey, CFDataGetBytePtr(cfOut), *outKeyLen);
	
errOut:
	CFRelease(cfOut);
	if(b64) {
		free(b64);
	}
	return ourRtn;
}


#pragma mark --- print RSA/DSA keys --- 

static void printBNLong(
	BN_ULONG bnl)
{
	/* for now assume it's 32 bits */
	unsigned i = bnl >> 24;
	printf("%02X ", i);
	i = (bnl >> 16) & 0xff;
	printf("%02X ", i);
	i = (bnl >> 8) & 0xff;
	printf("%02X ", i);
	i = bnl & 0xff;
	printf("%02X ", i);
}

static void printBN(
	const char *label,
	BIGNUM *bn)
{
	printf("%s: %d bits: bn->top %d: ", label, BN_num_bits(bn), bn->top);
	for(int dex=bn->top-1; dex>=0; dex--) {
		printBNLong(bn->d[dex]);
	}
	printf("\n");
}
static void printRSA(
	RSA *rsa)
{
	if(rsa->n) {
		printBN("   n", rsa->n);
	}
	if(rsa->e) {
		printBN("   e", rsa->e);
	}
	if(rsa->d) {
		printBN("   d", rsa->d);
	}
	if(rsa->p) {
		printBN("   p", rsa->p);
	}
	if(rsa->q) {
		printBN("   q", rsa->q);
	}
	if(rsa->dmp1) {
		printBN("dmp1", rsa->dmp1);
	}
	if(rsa->dmq1) {
		printBN("dmq1", rsa->dmq1);
	}
	if(rsa->iqmp) {
		printBN("iqmp", rsa->iqmp);
	}
}

/* only public keys here */
static void printDSA(
	DSA *dsa)
{
	if(dsa->p) {
		printBN("   p", dsa->p);
	}
	if(dsa->q) {
		printBN("   q", dsa->q);
	}
	if(dsa->g) {
		printBN("   g", dsa->g);
	}
	if(dsa->pub_key) {
		printBN(" pub", dsa->pub_key);
	}
}

/* parse format string, returns nonzero on error */
static int parseFormat(
	const char *formatStr,
	bool *isSSH1)
{
	if(!strcmp(formatStr, "ssh1")) {
		*isSSH1 = true;
		return 0;
	}
	else if(!strcmp(formatStr, "ssh2")) {
		*isSSH1 = false;
		return 0;
	}
	else {
		return -1;
	}
}

#pragma mark --- main --- 

/* parse format string */
int main(int argc, char **argv)
{
	char *inFile = NULL;
	char *outFile = NULL;
	bool privKeyIn = false;
	bool privKeyOut = false;
	char *password = NULL;
	char *comment = NULL;
	bool doPrint = false;
	bool isDSA = false;
	bool inputSSH1 = false;
	bool outputSSH1 = false;
	bool clearPrivKeys = false;
	
	int ourRtn = 0;
	
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "i:o:vVdrf:F:p:Pc:h")) != -1) {
		switch (arg) {
			case 'i':
				inFile = optarg;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 'v':
				privKeyIn = true;
				break;
			case 'V':
				privKeyOut = true;
				break;
			case 'd':
				isDSA = true;
				break;
			case 'r':
				doPrint = true;
				break;
			case 'f':
				if(parseFormat(optarg, &inputSSH1)) {
					usage(argv);
				}
				break;
			case 'F':
				if(parseFormat(optarg, &outputSSH1)) {
					usage(argv);
				}
				break;
			case 'p':
				password = optarg;
				break;
			case 'P':
				clearPrivKeys = true;
				break;
			case 'c':
				comment = optarg;
				break;
			case 'h':
			default:
				usage(argv);
		}
	}
	
	if(inFile == NULL) {
		printf("***You must specify an input file.\n");
		usage(argv);
	}
	if((privKeyIn && !inputSSH1) || (privKeyOut && !outputSSH1)) {
		printf("***Private keys in SSH2 format are handled elsewhere - Wrapped OpenSSL.\n");
		exit(1);
	}
	if((privKeyIn || privKeyOut) && (password == NULL) & !clearPrivKeys) {
		printf("***Private key handling requires a password or the -P option.\n");
		usage(argv);
	}
	unsigned char *inKey = NULL;
	unsigned inKeyLen = 0;
	if(readFile(inFile, &inKey, &inKeyLen)) {
		printf("Error reading %s. Aborting.\n", inFile);
		exit(1);
	}
	
	RSA *rsa = NULL;
	DSA *dsa = NULL;
	
	/* parse incoming key */
	if(isDSA) {
		if(inputSSH1) {
			printf("***SSHv1 did not support DSA keys.\n");
			exit(1);
		}
		/* already verified that this is not SSH2 & priv (Wrapped OpenSSL) */
		dsa = DSA_new();
		if(decodeSSH2DSAPubKey(inKey, inKeyLen, dsa, &comment)) {
			printf("***Error decoding SSH2 DSA public key.\n");
			exit(1);
		}
	}
	else {
		rsa = RSA_new();
		if(privKeyIn) {
			/* already verified that this is SSH1 (SSH2 is Wrapped OpenSSL) */
			if(decodeSSH1RSAPrivKey(inKey, inKeyLen, password, rsa, &comment)) {
				printf("***Error decoding SSH1 RSA Private key.\n");
				exit(1);
			}
		}
		else {
			if(inputSSH1) {
				if(decodeSSH1RSAPubKey(inKey, inKeyLen, rsa, &comment)) {
					printf("***Error decoding SSH1 RSA Public key.\n");
					exit(1);
				}
			}
			else {
				if(decodeSSH2RSAPubKey(inKey, inKeyLen, rsa, &comment)) {
					printf("***Error decoding SSH2 RSA Public key.\n");
					exit(1);
				}
			}
		}
	}

	/* optionally display the key */
	if(doPrint) {
		if(isDSA) {
			printf("DSA key:\n");
			printDSA(dsa);
			printf("Comment: %s\n", comment);
		}
		else {
			printf("RSA key:\n");
			printRSA(rsa);
			printf("Comment: %s\n", comment);
		}
	}
	
	/* optionally convert to (optionally different) output format */
	
	if(outFile) {
		unsigned char *outKey = NULL;
		unsigned outKeyLen = 0;
		
		if(isDSA) {
			if(outputSSH1 || privKeyOut) {
				printf("***DSA: Only public SSHv2 keys allowed.\n");
				exit(1);
			}
			if(encodeSSH2DSAPubKey(dsa, comment, &outKey, &outKeyLen)) {
				printf("***Error encoding DSA public key.\n");
				exit(1);
			}
		}
		else {
			if(privKeyOut) {
				/* already verified that this is SSH1 (SSH2 is Wrapped OpenSSL) */
				if(encodeSSH1RSAPrivKey(rsa, password, comment, &outKey, &outKeyLen)) {
					printf("***Error encoding RSA private key.\n");
					exit(1);
				}
			}
			else {
				if(outputSSH1) {
					if(encodeSSH1RSAPubKey(rsa, comment, &outKey, &outKeyLen)) {
						printf("***Error encoding RSA public key.\n");
						exit(1);
					}
				}
				else {
					if(encodeSSH2RSAPubKey(rsa, comment, &outKey, &outKeyLen)) {
						printf("***Error encoding RSA public key.\n");
						exit(1);
					}
				}
			}	/* RSA public */
		}	/* RSA */
		
		if(writeFile(outFile, outKey, outKeyLen)) {
			printf("***Error writing to %s.\n", outFile);
			ourRtn = -1;
		}
		else {
			printf("...wrote %u bytes to %s.\n", outKeyLen, outFile);
		}
		free(outKey);
	}
	else if(!doPrint) {
		printf("...parsed a key but you didn't ask me to do anything with it.\n");
	}
	if(rsa) {
		RSA_free(rsa);
	}
	if(dsa) {
		DSA_free(dsa);
	}
	
	return 0;
}
