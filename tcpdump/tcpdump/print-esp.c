/*	$NetBSD: print-ah.c,v 1.4 1996/05/20 00:41:16 fvdl Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-esp.c,v 1.58 2007-12-07 00:03:07 mcr Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <tcpdump-stdinc.h>

#include <stdlib.h>

#ifdef __APPLE__
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>

struct CCCryptoCipherData
{
	CCAlgorithm	algorithm;
	CCMode		mode;
	CCPadding	padding;
	size_t		keySizeInBytes;
};
typedef struct CCCryptoCipherData CCCryptoCipherData;

/*!
	@function	CCGetCryptoCipherDataFromName
	@abstract	This function will use the name specified by the name parameter to fill out the outCipherData parameter
	@param		name The name of the cipher to use.  This supports the OpenSSL naming convention
	@param		outCipherData An out parameter that will be filled in with the cipher data for the named cipher
	@result		returns 1 if successful 0 otherwise
*/
int CCGetCryptoCipherDataFromName(const char* name, CCCryptoCipherData* outCipherData);

/*!
	@function	CCCryptorCreateFromCipherData
	@abstract	This function will create a CCCryptorRef given a valid CCCryptoCipherData , operation and an iv
	@param		cipherData This is a pointer to a CCCryptoCipherData containing the data describing the cipher
	@param		op This is the operation encrypt or decrypt that the new CCCryptorRef will perform
	@param		key The key to use. Must not be NULL
	@param		iv An optional iv parameter to be set for the cipher
	@param		cryptorRef This is an out parameter for the CCCryptorRef
	@result		return the status of the call
*/
CCCryptorStatus CCCryptorCreateFromCipherData(CCCryptoCipherData* cipherData, 
                    CCOperation op, const void* key, const void* iv, CCCryptorRef *cryptorRef);

#else /* __APPLE__ */

#ifdef HAVE_LIBCRYPTO
#ifdef HAVE_OPENSSL_EVP_H
#include <openssl/evp.h>
#endif
#endif
#endif /* __APPLE__ */

#include <stdio.h>

#include "ip.h"
#include "esp.h"
#ifdef INET6
#include "ip6.h"
#endif

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#ifdef __APPLE__

// OpenSSL uses a default key size on ciphers that have vaiable key sizes
#define gDefaultOpenSSLKeySize 16

struct CipherTable
{
	const char*		cipherName;
	CCCryptoCipherData	cipherData;
};
typedef struct CipherTable CipherTable;


// The default for OpenSSL is that padding will be done.  That is the default
// here as well
static CipherTable const gCiphers[] =
{ 
	// Lower case strings
	
	{"aes-128-ecb", {kCCAlgorithmAES128, kCCModeECB, ccPKCS7Padding, kCCKeySizeAES128}},
	{"aes-128-cbc", {kCCAlgorithmAES128, kCCModeCBC, ccPKCS7Padding, kCCKeySizeAES128}},
	{"aes-128-ofb", {kCCAlgorithmAES128, kCCModeOFB, ccPKCS7Padding, kCCKeySizeAES128}},
	{"aes-128-cfb", {kCCAlgorithmAES128, kCCModeCFB, ccPKCS7Padding, kCCKeySizeAES128}},
	{"aes-128-cfb8", {kCCAlgorithmAES128, kCCModeCFB8, ccPKCS7Padding, kCCKeySizeAES128}},
	
	{"aes-192-ecb", {kCCAlgorithmAES128, kCCModeECB, ccPKCS7Padding, kCCKeySizeAES192}},
	{"aes-192-cbc", {kCCAlgorithmAES128, kCCModeCBC, ccPKCS7Padding, kCCKeySizeAES192}},
	{"aes-192-ofb", {kCCAlgorithmAES128, kCCModeOFB, ccPKCS7Padding, kCCKeySizeAES192}},
	{"aes-192-cfb", {kCCAlgorithmAES128, kCCModeCFB, ccPKCS7Padding, kCCKeySizeAES192}},
	{"aes-192-cfb8", {kCCAlgorithmAES128, kCCModeCFB8, ccPKCS7Padding, kCCKeySizeAES192}},
	
	{"aes-256-ecb", {kCCAlgorithmAES128, kCCModeECB, ccPKCS7Padding, kCCKeySizeAES256}},
	{"aes-256-cbc", {kCCAlgorithmAES128, kCCModeCBC, ccPKCS7Padding, kCCKeySizeAES256}},
	{"aes-256-ofb", {kCCAlgorithmAES128, kCCModeOFB, ccPKCS7Padding, kCCKeySizeAES256}},
	{"aes-256-cfb", {kCCAlgorithmAES128, kCCModeCFB, ccPKCS7Padding, kCCKeySizeAES256}},
	{"aes-256-cfb8", {kCCAlgorithmAES128, kCCModeCFB8, ccPKCS7Padding, kCCKeySizeAES256}},
	
	{"des-ecb", {kCCAlgorithmDES, kCCModeECB, ccPKCS7Padding, kCCKeySizeDES}},
	{"des-cbc", {kCCAlgorithmDES, kCCModeCBC, ccPKCS7Padding, kCCKeySizeDES}},
	{"des-ofb", {kCCAlgorithmDES, kCCModeOFB, ccPKCS7Padding, kCCKeySizeDES}},
	{"des-cfb", {kCCAlgorithmDES, kCCModeCFB, ccPKCS7Padding, kCCKeySizeDES}},
	{"des-cfb8", {kCCAlgorithmDES, kCCModeCFB8, ccPKCS7Padding, kCCKeySizeDES}},
	
	{"des-ede3", {kCCAlgorithm3DES, kCCModeECB, ccPKCS7Padding, kCCKeySize3DES}},
	{"3des", {kCCAlgorithm3DES, kCCModeCBC, ccPKCS7Padding, kCCKeySize3DES}},
	{"des-ede3-cbc", {kCCAlgorithm3DES, kCCModeCBC, ccPKCS7Padding, kCCKeySize3DES}},
	{"des-ede-ofb", {kCCAlgorithm3DES, kCCModeOFB, ccPKCS7Padding, kCCKeySize3DES}},
	{"des-ede3-cfb", {kCCAlgorithm3DES, kCCModeCFB, ccPKCS7Padding, kCCKeySize3DES}},
	{"des-ede3-cfb8", {kCCAlgorithm3DES, kCCModeCFB8, ccPKCS7Padding, kCCKeySize3DES}},	
	
	{"rc4", {kCCAlgorithmRC4, kCCModeECB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	
	{"rc2-ecb", {kCCAlgorithmRC2, kCCModeECB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"rc2-cbc", {kCCAlgorithmRC2, kCCModeCBC, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"rc2-ofb", {kCCAlgorithmRC2, kCCModeOFB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"rc2-cfb", {kCCAlgorithmRC2, kCCModeCFB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	
	{"bf-ecb", {kCCAlgorithmBlowfish, kCCModeECB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"bf-cbc", {kCCAlgorithmBlowfish, kCCModeCBC, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"bf-ofb", {kCCAlgorithmBlowfish, kCCModeOFB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"bf-cfb", {kCCAlgorithmBlowfish, kCCModeCFB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	
	{"cast5-ecb", {kCCAlgorithmCAST, kCCModeECB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"cast5-cbc", {kCCAlgorithmCAST, kCCModeCBC, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"cast5-ofb", {kCCAlgorithmCAST, kCCModeOFB, ccPKCS7Padding, gDefaultOpenSSLKeySize}},
	{"cast5-cfb", {kCCAlgorithmCAST, kCCModeCFB, ccPKCS7Padding, gDefaultOpenSSLKeySize}}
};

/* ==========================================================================
	Function:	CCGetCryptoCipherDataFromName
	Description:	Provide a way to get the information needed for creating 
			a CommonCrypto CCCryptoRef from an OpenSSL style cipher 
			name
   ========================================================================== */		
int CCGetCryptoCipherDataFromName(const char* name, CCCryptoCipherData* outCipherData)
{
	int result = 0;					// guilty until proven
	int numCiphers = 0;				// Number of cipher records to check
	int iCnt = 0;					// for loop counter
	const CipherTable* tablePtr = gCiphers;	// pointer into the static table
	const char* tableCipherName = NULL;
	
	// Parameter checking
	if (NULL == name || NULL == outCipherData) {
		return result;
	}
    
	outCipherData->algorithm = (CCAlgorithm)-1; // guilt until proven
		
	numCiphers = sizeof(gCiphers) / sizeof(CipherTable);
	for (iCnt = 0; iCnt < numCiphers; iCnt++, tablePtr++) {
		tableCipherName = tablePtr->cipherName;
		if (!strcmp(name, tableCipherName))
		{
			// Found one
			*outCipherData = tablePtr->cipherData;
			result = 1;
			break;
		}
	}
	
	return result;	
}

/* ==========================================================================
	Function:	CCCryptorCreateFromCipherData
	Description:	Given a CCCryptoCipherData record, create a CCCryptorRef
   ========================================================================== */
CCCryptorStatus CCCryptorCreateFromCipherData(CCCryptoCipherData* cipherData, 
                    CCOperation op, const void* key, const void* iv, 
					CCCryptorRef *cryptorRef)
{	
	// Parameter checking
	if (NULL == cipherData || NULL == cryptorRef || NULL == key || 
		cipherData->algorithm == (CCAlgorithm)-1) {
		return kCCParamError;
	}
	
	// Create the CryptoRef	
	return CCCryptorCreateWithMode(op, cipherData->mode, cipherData->algorithm,
		cipherData->padding, iv, key, cipherData->keySizeInBytes,
		NULL, 0, 0, 0, cryptorRef);
}

/* ==========================================================================
	Function:	IVLengthFromCipherData
	Description:	Given a CCCryptoCipherData record, return the correct 
			IV length in bytes
   ========================================================================== */
int IVLengthFromCipherData(CCCryptoCipherData* cipherData)
{
    int result = -1;    // guilt until proven
    if (NULL == cipherData || cipherData->algorithm == (CCAlgorithm)-1) {
        return -1;
    }
    
    switch(cipherData->algorithm)
    {
		case kCCAlgorithmAES128:
			result = kCCBlockSizeAES128;
			break;

		case kCCAlgorithmDES:
			result = kCCBlockSizeDES;
			break;
			
		case kCCAlgorithm3DES:
			result = kCCBlockSize3DES;
			break;
		
		case kCCAlgorithmCAST:
			result = kCCBlockSizeCAST;
			break;
			
		case kCCAlgorithmRC2:
			result = kCCBlockSizeRC2;
			break;

		case kCCAlgorithmBlowfish:
			result = kCCBlockSizeBlowfish;
			break;
		
   }
    
    return result;
}

/* ==========================================================================
	Function:		BlockSizeFromCipherData
	Description:	Given a CCCryptoCipherData record, return the correct 
                    block size in bytes
   ========================================================================== */
int BlockSizeFromCipherData(CCCryptoCipherData* cipherData)
{
	return IVLengthFromCipherData(cipherData);
}

#endif /* __APPLE__ */

#ifndef HAVE_SOCKADDR_STORAGE
#ifdef INET6
struct sockaddr_storage {
	union {
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} un;
};
#else
#define sockaddr_storage sockaddr
#endif
#endif /* HAVE_SOCKADDR_STORAGE */

#ifdef HAVE_LIBCRYPTO
struct sa_list {
	struct sa_list	*next;
	struct sockaddr_storage daddr;
	u_int32_t	spi;          /* if == 0, then IKEv2 */
	int             initiator;
	u_char          spii[8];      /* for IKEv2 */
	u_char          spir[8];
#ifdef __APPLE__
	CCCryptoCipherData    cipherData;
#else /* __APPLE__ */
	const EVP_CIPHER *evp;
#endif /* __APPLE__ */
	int		ivlen;
	int		authlen;
	u_char          authsecret[256];
	int             authsecret_len;
	u_char		secret[256];  /* is that big enough for all secrets? */
	int		secretlen;
};

/*
 * this will adjust ndo_packetp and ndo_snapend to new buffer!
 */
int esp_print_decrypt_buffer_by_ikev2(netdissect_options *ndo,
				      int initiator,
				      u_char spii[8], u_char spir[8],
				      u_char *buf, u_char *end)
{
	struct sa_list *sa;
	u_char *iv;
	int len;
#ifdef __APPLE__
	CCCryptorRef    ctx;
	size_t          dataMoved = 0;
#else /* __APPLE__ */
	EVP_CIPHER_CTX ctx;
#endif /* __APPLE__ */

	/* initiator arg is any non-zero value */
	if(initiator) initiator=1;
				       
	/* see if we can find the SA, and if so, decode it */
	for (sa = ndo->ndo_sa_list_head; sa != NULL; sa = sa->next) {
		if (sa->spi == 0
		    && initiator == sa->initiator
		    && memcmp(spii, sa->spii, 8) == 0
		    && memcmp(spir, sa->spir, 8) == 0)
			break;
	}

	if(sa == NULL) return 0;
#ifdef __APPLE__
	if(sa->cipherData.algorithm == (CCAlgorithm)-1) return 0;
#else /* __APPLE__ */
	if(sa->evp == NULL) return 0;
#endif /* __APPLE__ */

	/*
	 * remove authenticator, and see if we still have something to
	 * work with
	 */
	end = end - sa->authlen;
	iv  = buf;
	buf = buf + sa->ivlen;
	len = end-buf;

	if(end <= buf) return 0;

#ifdef __APPLE__
	ctx = NULL;
	if (kCCSuccess != CCCryptorCreateFromCipherData(&sa->cipherData, kCCDecrypt, sa->secret, iv, &ctx))
		(*ndo->ndo_warning)(ndo, "espkey init failed");
	
	(void)CCCryptorUpdate(ctx, buf, len, buf, len, &dataMoved);
	CCCryptorRelease(ctx);
	ctx = NULL;
#else /* __APPLE__ */
	memset(&ctx, 0, sizeof(ctx));
	if (EVP_CipherInit(&ctx, sa->evp, sa->secret, NULL, 0) < 0)
		(*ndo->ndo_warning)(ndo, "espkey init failed");
	EVP_CipherInit(&ctx, NULL, NULL, iv, 0);
	EVP_Cipher(&ctx, buf, buf, len);
	EVP_CIPHER_CTX_cleanup(&ctx);
#endif /* __APPLE__ */

	ndo->ndo_packetp = buf;
	ndo->ndo_snapend = end;

	return 1;
	
}

static void esp_print_addsa(netdissect_options *ndo,
			    struct sa_list *sa, int sa_def)
{
	/* copy the "sa" */

	struct sa_list *nsa;

	nsa = (struct sa_list *)malloc(sizeof(struct sa_list));
	if (nsa == NULL)
		(*ndo->ndo_error)(ndo, "ran out of memory to allocate sa structure");

	*nsa = *sa;

	if (sa_def)
		ndo->ndo_sa_default = nsa;

	nsa->next = ndo->ndo_sa_list_head;
	ndo->ndo_sa_list_head = nsa;
}


static u_int hexdigit(netdissect_options *ndo, char hex)
{
	if (hex >= '0' && hex <= '9')
		return (hex - '0');
	else if (hex >= 'A' && hex <= 'F')
		return (hex - 'A' + 10);
	else if (hex >= 'a' && hex <= 'f')
		return (hex - 'a' + 10);
	else {
		(*ndo->ndo_error)(ndo, "invalid hex digit %c in espsecret\n", hex);
		return 0;
	}
}

static u_int hex2byte(netdissect_options *ndo, char *hexstring)
{
	u_int byte;

	byte = (hexdigit(ndo, hexstring[0]) << 4) + hexdigit(ndo, hexstring[1]);
	return byte;
}

/*
 * returns size of binary, 0 on failure.
 */
static
int espprint_decode_hex(netdissect_options *ndo,
			u_char *binbuf, unsigned int binbuf_len,
			char *hex)
{
	unsigned int len;
	int i;

	len = strlen(hex) / 2;
		
	if (len > binbuf_len) {
		(*ndo->ndo_warning)(ndo, "secret is too big: %d\n", len);
		return 0;
	}
		
	i = 0;
	while (hex[0] != '\0' && hex[1]!='\0') {
		binbuf[i] = hex2byte(ndo, hex);
		hex += 2;
		i++;
	}

	return i;
}

/*
 * decode the form:    SPINUM@IP <tab> ALGONAME:0xsecret
 */

static int
espprint_decode_encalgo(netdissect_options *ndo,
			char *decode, struct sa_list *sa)
{
	int len;
	size_t i;
#ifdef __APPLE__
	CCCryptoCipherData cipherData;
#else /* __APPLE__ */
	const EVP_CIPHER *evp;
#endif /* __APPLE__	*/	
	int authlen = 0;
	char *colon, *p;
	
	colon = strchr(decode, ':');
	if (colon == NULL) {
		(*ndo->ndo_warning)(ndo, "failed to decode espsecret: %s\n", decode);
		return 0;
	}
	*colon = '\0';
	
	len = colon - decode;
	if (strlen(decode) > strlen("-hmac96") &&
	    !strcmp(decode + strlen(decode) - strlen("-hmac96"),
		    "-hmac96")) {
		p = strstr(decode, "-hmac96");
		*p = '\0';
		authlen = 12;
	}
	if (strlen(decode) > strlen("-cbc") &&
	    !strcmp(decode + strlen(decode) - strlen("-cbc"), "-cbc")) {
		p = strstr(decode, "-cbc");
		*p = '\0';
	}

#ifdef __APPLE__
	if (!CCGetCryptoCipherDataFromName(decode, &cipherData)) {
		(*ndo->ndo_warning)(ndo, "failed to find cipher algo %s\n", decode);
		sa->authlen = 0;
		sa->ivlen = 0;
		return 0;
	}
#else /* __APPLE__ */
	evp = EVP_get_cipherbyname(decode);

	if (!evp) {
		(*ndo->ndo_warning)(ndo, "failed to find cipher algo %s\n", decode);
		sa->evp = NULL;
		sa->authlen = 0;
		sa->ivlen = 0;
		return 0;
	}
#endif /* __APPLE__	*/
	
#ifdef __APPLE__
	sa->cipherData = cipherData;
	sa->authlen = authlen;
	sa->ivlen =IVLengthFromCipherData(&cipherData);
#else /* __APPLE__ */
	sa->evp = evp;
	sa->authlen = authlen;
	sa->ivlen = EVP_CIPHER_iv_length(evp);
#endif /* __APPLE__	*/

	colon++;
	if (colon[0] == '0' && colon[1] == 'x') {
		/* decode some hex! */

		colon += 2;
		sa->secretlen = espprint_decode_hex(ndo, sa->secret, sizeof(sa->secret), colon);
		if(sa->secretlen == 0) return 0;
	} else {
		i = strlen(colon);
		
		if (i < sizeof(sa->secret)) {
			memcpy(sa->secret, colon, i);
			sa->secretlen = i;
		} else {
			memcpy(sa->secret, colon, sizeof(sa->secret));
			sa->secretlen = sizeof(sa->secret);
		}
	}

	return 1;
}

/*
 * for the moment, ignore the auth algorith, just hard code the authenticator
 * length. Need to research how openssl looks up HMAC stuff.
 */
static int
espprint_decode_authalgo(netdissect_options *ndo,
			 char *decode, struct sa_list *sa)
{
	char *colon;

	colon = strchr(decode, ':');
	if (colon == NULL) {
		(*ndo->ndo_warning)(ndo, "failed to decode espsecret: %s\n", decode);
		return 0;
	}
	*colon = '\0';
	
	if(strcasecmp(colon,"sha1") == 0 ||
	   strcasecmp(colon,"md5") == 0) {
		sa->authlen = 12;
	}
	return 1;
}

static void esp_print_decode_ikeline(netdissect_options *ndo, char *line,
				     const char *file, int lineno)
{
	/* it's an IKEv2 secret, store it instead */
	struct sa_list sa1;

	char *init;
	char *icookie, *rcookie;
	int   ilen, rlen;
	char *authkey;
	char *enckey;
	
	init = strsep(&line, " \t");
	icookie = strsep(&line, " \t");
	rcookie = strsep(&line, " \t");
	authkey = strsep(&line, " \t");
	enckey  = strsep(&line, " \t");
	
	/* if any fields are missing */
	if(!init || !icookie || !rcookie || !authkey || !enckey) {
		(*ndo->ndo_warning)(ndo, "print_esp: failed to find all fields for ikev2 at %s:%u",
				    file, lineno);
		
		return;
	}
	
	ilen = strlen(icookie);
	rlen = strlen(rcookie);

	if((init[0]!='I' && init[0]!='R')
	   || icookie[0]!='0' || icookie[1]!='x'
	   || rcookie[0]!='0' || rcookie[1]!='x'
	   || ilen!=18
	   || rlen!=18) {
		(*ndo->ndo_warning)(ndo, "print_esp: line %s:%u improperly formatted.",
				    file, lineno);

		(*ndo->ndo_warning)(ndo, "init=%s icookie=%s(%u) rcookie=%s(%u)",
				    init, icookie, ilen, rcookie, rlen);
		
		return;
	}

	sa1.spi = 0;
	sa1.initiator = (init[0] == 'I');
	if(espprint_decode_hex(ndo, sa1.spii, sizeof(sa1.spii), icookie+2)!=8)
		return;

	if(espprint_decode_hex(ndo, sa1.spir, sizeof(sa1.spir), rcookie+2)!=8)
		return;

	if(!espprint_decode_encalgo(ndo, enckey, &sa1)) return;

	if(!espprint_decode_authalgo(ndo, authkey, &sa1)) return;
	
	esp_print_addsa(ndo, &sa1, FALSE);
}

/*
 *
 * special form: file /name
 * causes us to go read from this file instead.
 *
 */
static void esp_print_decode_onesecret(netdissect_options *ndo, char *line,
				       const char *file, int lineno)
{
	struct sa_list sa1;
	int sa_def;

	char *spikey;
	char *decode;

	spikey = strsep(&line, " \t");
	sa_def = 0;
	memset(&sa1, 0, sizeof(struct sa_list));

	/* if there is only one token, then it is an algo:key token */
	if (line == NULL) {
		decode = spikey;
		spikey = NULL;
		/* memset(&sa1.daddr, 0, sizeof(sa1.daddr)); */
		/* sa1.spi = 0; */
		sa_def    = 1;
	} else
		decode = line;

	if (spikey && strcasecmp(spikey, "file") == 0) {
		/* open file and read it */
		FILE *secretfile;
		char  fileline[1024];
		int   lineno=0;
		char  *nl;
		char *filename = line;

		secretfile = fopen(filename, FOPEN_READ_TXT);
		if (secretfile == NULL) {
			perror(filename);
			exit(3);
		}

		while (fgets(fileline, sizeof(fileline)-1, secretfile) != NULL) {
			lineno++;
			/* remove newline from the line */
			nl = strchr(fileline, '\n');
			if (nl)
				*nl = '\0';
			if (fileline[0] == '#') continue;
			if (fileline[0] == '\0') continue;

			esp_print_decode_onesecret(ndo, fileline, filename, lineno);
		}
		fclose(secretfile);

		return;
	}

	if (spikey && strcasecmp(spikey, "ikev2") == 0) {
		esp_print_decode_ikeline(ndo, line, file, lineno);
		return;
	} 

	if (spikey) {
		
		char *spistr, *foo;
		u_int32_t spino;
		struct sockaddr_in *sin;
#ifdef INET6
		struct sockaddr_in6 *sin6;
#endif
		
		spistr = strsep(&spikey, "@");
		
		spino = strtoul(spistr, &foo, 0);
		if (spistr == foo || !spikey) {
			(*ndo->ndo_warning)(ndo, "print_esp: failed to decode spi# %s\n", foo);
			return;
		}
		
		sa1.spi = spino;
		
		sin = (struct sockaddr_in *)&sa1.daddr;
#ifdef INET6
		sin6 = (struct sockaddr_in6 *)&sa1.daddr;
		if (inet_pton(AF_INET6, spikey, &sin6->sin6_addr) == 1) {
#ifdef HAVE_SOCKADDR_SA_LEN
			sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
			sin6->sin6_family = AF_INET6;
		} else
#endif
			if (inet_pton(AF_INET, spikey, &sin->sin_addr) == 1) {
#ifdef HAVE_SOCKADDR_SA_LEN
				sin->sin_len = sizeof(struct sockaddr_in);
#endif
				sin->sin_family = AF_INET;
			} else {
				(*ndo->ndo_warning)(ndo, "print_esp: can not decode IP# %s\n", spikey);
				return;
			}
	}

	if (decode) {
		/* skip any blank spaces */
		while (isspace((unsigned char)*decode))
			decode++;
		
		if(!espprint_decode_encalgo(ndo, decode, &sa1)) {
			return;
		}
	}

	esp_print_addsa(ndo, &sa1, sa_def);
}

static void esp_init(netdissect_options *ndo _U_)
{
#ifndef __APPLE__
	OpenSSL_add_all_algorithms();
	EVP_add_cipher_alias(SN_des_ede3_cbc, "3des");
#endif /* __APPLE__ */
}

void esp_print_decodesecret(netdissect_options *ndo)
{
	char *line;
	char *p;
	static int initialized = 0;

	if (!initialized) {
		esp_init(ndo);
		initialized = 1;
	}

	p = ndo->ndo_espsecret;

	while (p && p[0] != '\0') {
		/* pick out the first line or first thing until a comma */
		if ((line = strsep(&p, "\n,")) == NULL) {
			line = p;
			p = NULL;
		}

		esp_print_decode_onesecret(ndo, line, "cmdline", 0);
	}

	ndo->ndo_espsecret = NULL;
}

#endif

int
esp_print(netdissect_options *ndo,
	  const u_char *bp, const int length, const u_char *bp2
#ifndef HAVE_LIBCRYPTO
	_U_
#endif
	,
	int *nhdr
#ifndef HAVE_LIBCRYPTO
	_U_
#endif
	,
	int *padlen
#ifndef HAVE_LIBCRYPTO
	_U_
#endif
	)
{
	register const struct newesp *esp;
	register const u_char *ep;
#ifdef HAVE_LIBCRYPTO
	struct ip *ip;
	struct sa_list *sa = NULL;
	int espsecret_keylen;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
	int advance;
	int len;
	u_char *secret;
	int ivlen = 0;
	u_char *ivoff;
	u_char *p;
#ifdef __APPLE__
	CCCryptorRef    ctx;
	size_t          dataMoved = 0;
#else /* __APPLE__ */
	EVP_CIPHER_CTX ctx;
	int blocksz;
#endif /* __APPLE__ */
#endif

	esp = (struct newesp *)bp;

#ifdef HAVE_LIBCRYPTO
	secret = NULL;
	advance = 0;
#endif

#if 0
	/* keep secret out of a register */
	p = (u_char *)&secret;
#endif

	/* 'ep' points to the end of available data. */
	ep = ndo->ndo_snapend;

	if ((u_char *)(esp + 1) >= ep) {
		fputs("[|ESP]", stdout);
		goto fail;
	}
	(*ndo->ndo_printf)(ndo, "ESP(spi=0x%08x", EXTRACT_32BITS(&esp->esp_spi));
	(*ndo->ndo_printf)(ndo, ",seq=0x%x)", EXTRACT_32BITS(&esp->esp_seq));
        (*ndo->ndo_printf)(ndo, ", length %u", length);

#ifndef HAVE_LIBCRYPTO
	goto fail;
#else
	/* initiailize SAs */
	if (ndo->ndo_sa_list_head == NULL) {
		if (!ndo->ndo_espsecret)
			goto fail;

		esp_print_decodesecret(ndo);
	}

	if (ndo->ndo_sa_list_head == NULL)
		goto fail;

	ip = (struct ip *)bp2;
	switch (IP_V(ip)) {
#ifdef INET6
	case 6:
		ip6 = (struct ip6_hdr *)bp2;
		/* we do not attempt to decrypt jumbograms */
		if (!EXTRACT_16BITS(&ip6->ip6_plen))
			goto fail;
		/* if we can't get nexthdr, we do not need to decrypt it */
		len = sizeof(struct ip6_hdr) + EXTRACT_16BITS(&ip6->ip6_plen);

		/* see if we can find the SA, and if so, decode it */
		for (sa = ndo->ndo_sa_list_head; sa != NULL; sa = sa->next) {
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&sa->daddr;
			if (sa->spi == EXTRACT_32BITS(&esp->esp_spi) &&
			    sin6->sin6_family == AF_INET6 &&
			    memcmp(&sin6->sin6_addr, &ip6->ip6_dst,
				   sizeof(struct in6_addr)) == 0) {
				break;
			}
		}
		break;
#endif /*INET6*/
	case 4:
		/* nexthdr & padding are in the last fragment */
		if (EXTRACT_16BITS(&ip->ip_off) & IP_MF)
			goto fail;
		len = EXTRACT_16BITS(&ip->ip_len);

		/* see if we can find the SA, and if so, decode it */
		for (sa = ndo->ndo_sa_list_head; sa != NULL; sa = sa->next) {
			struct sockaddr_in *sin = (struct sockaddr_in *)&sa->daddr;
			if (sa->spi == EXTRACT_32BITS(&esp->esp_spi) &&
			    sin->sin_family == AF_INET &&
			    sin->sin_addr.s_addr == ip->ip_dst.s_addr) {
				break;
			}
		}
		break;
	default:
		goto fail;
	}

	/* if we didn't find the specific one, then look for
	 * an unspecified one.
	 */
	if (sa == NULL)
		sa = ndo->ndo_sa_default;
	
	/* if not found fail */
	if (sa == NULL)
		goto fail;

	/* if we can't get nexthdr, we do not need to decrypt it */
	if (ep - bp2 < len)
		goto fail;
	if (ep - bp2 > len) {
		/* FCS included at end of frame (NetBSD 1.6 or later) */
		ep = bp2 + len;
	}

	ivoff = (u_char *)(esp + 1) + 0;
	ivlen = sa->ivlen;
	secret = sa->secret;
	espsecret_keylen = sa->secretlen;
	ep = ep - sa->authlen;

#ifdef __APPLE__
	if (sa->cipherData.algorithm != (CCAlgorithm)-1) {
		ctx = NULL;
		p = ivoff;
		if (kCCSuccess != CCCryptorCreateFromCipherData(&sa->cipherData, kCCDecrypt, secret, p, &ctx))
			(*ndo->ndo_warning)(ndo, "espkey init failed");
		len = ep - (p + ivlen);
		CCCryptorUpdate(ctx, p + ivlen, len, p + ivlen, len, &dataMoved);
		CCCryptorRelease(ctx);
		ctx = NULL;
		advance = ivoff - (u_char *)esp + ivlen;
	}
	else
		advance = sizeof(struct newesp);
#else /* __APPLE__ */
	if (sa->evp) {
		memset(&ctx, 0, sizeof(ctx));
		if (EVP_CipherInit(&ctx, sa->evp, secret, NULL, 0) < 0)
			(*ndo->ndo_warning)(ndo, "espkey init failed");

		blocksz = EVP_CIPHER_CTX_block_size(&ctx);

		p = ivoff;
		EVP_CipherInit(&ctx, NULL, NULL, p, 0);
		EVP_Cipher(&ctx, p + ivlen, p + ivlen, ep - (p + ivlen));
		EVP_CIPHER_CTX_cleanup(&ctx);
		advance = ivoff - (u_char *)esp + ivlen;
	} else
		advance = sizeof(struct newesp);
#endif /* __APPLE__ */

	/* sanity check for pad length */
	if (ep - bp < *(ep - 2))
		goto fail;

	if (padlen)
		*padlen = *(ep - 2) + 2;

	if (nhdr)
		*nhdr = *(ep - 1);

	(ndo->ndo_printf)(ndo, ": ");
	return advance;
#endif

fail:
	return -1;
}

/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */
