/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		cryptType.h

	Contains:	Crypto structures and routines

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef _CRYPTTYPE_H_
#define _CRYPTTYPE_H_ 1

#include <Security/CipherSuite.h>
#include "sslPriv.h"
#include "sslContext.h"
#include "tls_hmac.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{   SSL2_RC4_128_WITH_MD5 =                 0x010080,
    SSL2_RC4_128_EXPORT_40_WITH_MD5 =       0x020080,
    SSL2_RC2_128_CBC_WITH_MD5 =             0x030080,
    SSL2_RC2_128_CBC_EXPORT40_WITH_MD5 =    0x040080,
    SSL2_IDEA_128_CBC_WITH_MD5 =            0x050080,
    SSL2_DES_64_CBC_WITH_MD5 =              0x060040,
    SSL2_DES_192_EDE3_CBC_WITH_MD5 =        0x0700C0
} SSL2CipherKind;

typedef struct
{   SSL2CipherKind  	cipherKind;
    SSLCipherSuite     	cipherSuite;
} SSLCipherMapping;

typedef OSStatus (*HashInit)(SSLBuffer *digestCtx, SSLContext *sslCtx);
typedef OSStatus (*HashUpdate)(SSLBuffer *digestCtx, const SSLBuffer *data);
/* HashFinal also does HashClose */
typedef OSStatus (*HashFinal)(SSLBuffer *digestCtx, SSLBuffer *digest);	
typedef OSStatus (*HashClose)(SSLBuffer *digestCtx, SSLContext *sslCtx);
typedef OSStatus (*HashClone)(const SSLBuffer *src, SSLBuffer *dest);
typedef struct
{   UInt32      contextSize;
    UInt32      digestSize;
    UInt32      macPadSize;
    HashInit    init;
    HashUpdate  update;
    HashFinal   final;
	HashClose	close;
    HashClone   clone;
} HashReference;

/*
 * TLS addenda: 
 *	-- new struct HashHmacReference
 *	-- structs which used to use HashReference now use HashHmacReference
 *	-- new union HashHmacContext, used in CipherContext.
 */
typedef struct {
	const HashReference	*hash;
	const HMACReference	*hmac;
} HashHmacReference;

typedef union {
	SSLBuffer			hashCtx;
	HMACContextRef		hmacCtx;
} HashHmacContext;

/* these are declared in tls_hmac.c */
extern const HashHmacReference HashHmacNull;
extern const HashHmacReference HashHmacMD5;
extern const HashHmacReference HashHmacSHA1;

/*
 * Hack to avoid circular dependency with tls_ssl.h.
 */
struct _SslTlsCallouts;

/*
 * All symmetric ciphers go thru CDSA, via these callouts.  
 */
struct CipherContext;
typedef struct CipherContext CipherContext;

typedef OSStatus (*SSLKeyFunc)(
	UInt8 *key, 
	UInt8 *iv, 
	CipherContext *cipherCtx, 
	SSLContext *ctx);
typedef OSStatus (*SSLCryptFunc)(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx, 
	SSLContext *ctx);
typedef OSStatus (*SSLFinishFunc)(
	CipherContext *cipherCtx, 
	SSLContext *ctx);

typedef enum
{   NotExportable = 0,
    Exportable = 1
} Exportability;

/*
 * Statically defined description of a symmetric sipher. 
 */
typedef struct {
    UInt8           	keySize;            /* Sizes are in bytes */
    UInt8           	secretKeySize;
    UInt8           	ivSize;
    UInt8          	 	blockSize;
    CSSM_ALGORITHMS		keyAlg;				/* CSSM_ALGID_DES, etc. */
    CSSM_ALGORITHMS		encrAlg;			/* ditto */
    CSSM_ENCRYPT_MODE	encrMode;			/* CSSM_ALGMODE_CBCPadIV8, etc. */
	CSSM_PADDING		encrPad;
    SSLKeyFunc      	initialize;
    SSLCryptFunc    	encrypt;
    SSLCryptFunc    	decrypt;
    SSLFinishFunc   	finish;
} SSLSymmetricCipher;

#define MAX_MAC_PADDING 	48	/* MD5 MAC padding size = 48 bytes */
#define MASTER_SECRET_LEN 	48	/* master secret = 3 x MD5 hashes concatenated */

/* SSL V2 - mac secret is the size of symmetric key, not digest */
#define MAX_SYMKEY_SIZE		24

typedef enum
{   SSL_NULL_auth,
    SSL_RSA,
    SSL_RSA_EXPORT,
    SSL_DH_DSS,
    SSL_DH_DSS_EXPORT,
    SSL_DH_RSA,
    SSL_DH_RSA_EXPORT,
    SSL_DHE_DSS,
    SSL_DHE_DSS_EXPORT,
    SSL_DHE_RSA,
    SSL_DHE_RSA_EXPORT,
    SSL_DH_anon,
    SSL_DH_anon_EXPORT,
    SSL_Fortezza,
	
	/* ECDSA addenda, RFC 4492 */
	SSL_ECDH_ECDSA,
	SSL_ECDHE_ECDSA,
	SSL_ECDH_RSA,
	SSL_ECDHE_RSA,
	SSL_ECDH_anon
} KeyExchangeMethod;

typedef struct {
    SSLCipherSuite      		cipherSpec;
    Exportability       		isExportable;
    KeyExchangeMethod   		keyExchangeMethod;
    const HashHmacReference     *macAlgorithm;
    const SSLSymmetricCipher  	*cipher;
} SSLCipherSpec;

extern const SSLCipherMapping SSL2CipherMap[];
extern const unsigned SSL2CipherMapCount;

/* Default size of server-generated Diffie-Hellman parameters and keys */
#ifdef	NDEBUG
#define SSL_DH_DEFAULT_PRIME_SIZE	1024			/* in bits */
#else
#define SSL_DH_DEFAULT_PRIME_SIZE	512				/* in bits */
#endif

#ifdef __cplusplus
}
#endif

#endif /* _CRYPTTYPE_H_ */
