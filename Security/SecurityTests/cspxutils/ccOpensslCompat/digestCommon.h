/*
 * digestCommon.h - openssl-style digest ops that can be redirected to CommonDigest.h
 */
 
/*
 * Currently, SHA2 functions are not implemented by OpenSSL, even though they are
 * present in the header <openssl/fips_sha.h>. 
 *
 * <rdar://problem/4753005> SHA2 functions are declared, but not implemented
 *
 * For now we'll have to redirect openssl SHA2 calls manually just to get this
 * program to build and run correctly. (We could just disable all of the SHA2
 * code but we still get a benefit from compiling openssl-style code).
 */
#define OPENSSL_SHA2_IMPLEMENTED	0

#include "digestCommonExtern.h"

#ifdef	COMMON_DIGEST_FOR_OPENSSL

/* redirecting to CommonDigest */

#define doMD2		md2cc
#define doMD4		md4cc
#define doMD5		md5cc
#define doSHA1		sha1cc
#define doSHA224	sha224cc
#define doSHA256	sha256cc
#define doSHA384	sha384cc
#define doSHA512	sha512cc

#else	/* !COMMON_DIGEST_FOR_OPENSSL */

/* openssl */

#define doMD2		md2os
#define doMD4		md4os
#define doMD5		md5os
#define doSHA1		sha1os
#define doSHA224	sha224os
#define doSHA256	sha256os
#define doSHA384	sha384os
#define doSHA512	sha512os

#if		!OPENSSL_SHA2_IMPLEMENTED

/* Hack: redirect SHA2 calls after all */

#include <CommonCrypto/CommonDigest.h> 

#define SHA256_CTX					CC_SHA256_CTX
#define SHA224_Init(c)				CC_SHA224_Init(c)
#define SHA224_Update(c,d,l)		CC_SHA224_Update(c,d,l)
#define SHA224_Final(m, c)			CC_SHA224_Final(m,c)

#define SHA256_Init(c)				CC_SHA256_Init(c)
#define SHA256_Update(c,d,l)		CC_SHA256_Update(c,d,l)
#define SHA256_Final(m, c)			CC_SHA256_Final(m,c)

#define SHA512_CTX					CC_SHA512_CTX
#define SHA384_Init(c)				CC_SHA384_Init(c)
#define SHA384_Update(c,d,l)		CC_SHA384_Update(c,d,l)
#define SHA384_Final(m, c)			CC_SHA384_Final(m,c)

#define SHA512_Init(c)				CC_SHA512_Init(c)
#define SHA512_Update(c,d,l)		CC_SHA512_Update(c,d,l)
#define SHA512_Final(m, c)			CC_SHA512_Final(m,c)

#endif	/* OPENSSL_SHA2_IMPLEMENTED */


#endif	/* COMMON_DIGEST_FOR_OPENSSL */

/* all functions return nonzero on error */

int doMD2(const void *p, unsigned long len, unsigned char *md)
{
	/* OPenSSL MD2 is not orthogonal: the pointer is a const unsigned char * */
	MD2_CTX ctx;
	const unsigned char *cp = (const unsigned char *)p;
	
	if(!MD2_Init(&ctx)) {
		return -1;
	}
	if(!MD2_Update(&ctx, cp, len)) {
		return -1;
	}	
	if(!MD2_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}

int doMD4(const void *p, unsigned long len, unsigned char *md)
{
	MD4_CTX ctx;
	if(!MD4_Init(&ctx)) {
		return -1;
	}
	if(!MD4_Update(&ctx, p, len)) {
		return -1;
	}	
	if(!MD4_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}

int doMD5(const void *p, unsigned long len, unsigned char *md)
{
	MD5_CTX ctx;
	if(!MD5_Init(&ctx)) {
		return -1;
	}
	if(!MD5_Update(&ctx, p, len)) {
		return -1;
	}	
	if(!MD5_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}

int doSHA1(const void *p, unsigned long len, unsigned char *md)
{
	SHA_CTX ctx;
	if(!SHA1_Init(&ctx)) {
		return -1;
	}
	if(!SHA1_Update(&ctx, p, len)) {
		return -1;
	}	
	if(!SHA1_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}

int doSHA224(const void *p, unsigned long len, unsigned char *md)
{
	SHA256_CTX ctx;
	if(!SHA224_Init(&ctx)) {
		return -1;
	}
	if(!SHA224_Update(&ctx, p, len)) {
		return -1;
	}	
	if(!SHA224_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}

int doSHA256(const void *p, unsigned long len, unsigned char *md)
{
	SHA256_CTX ctx;
	if(!SHA256_Init(&ctx)) {
		return -1;
	}
	if(!SHA256_Update(&ctx, p, len)) {
		return -1;
	}	
	if(!SHA256_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}

int doSHA384(const void *p, unsigned long len, unsigned char *md)
{
	SHA512_CTX ctx;
	if(!SHA384_Init(&ctx)) {
		return -1;
	}
	if(!SHA384_Update(&ctx, p, len)) {
		return -1;
	}	
	if(!SHA384_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}

int doSHA512(const void *p, unsigned long len, unsigned char *md)
{
	SHA512_CTX ctx;
	if(!SHA512_Init(&ctx)) {
		return -1;
	}
	if(!SHA512_Update(&ctx, p, len)) {
		return -1;
	}	
	if(!SHA512_Final(md, &ctx)) {
		return -1;
	}
	return 0;
}
