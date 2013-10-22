/*	$NetBSD: crypto_openssl.c,v 1.11.6.1 2006/12/18 10:18:10 vanhu Exp $	*/

/* Id: crypto_openssl.c,v 1.47 2006/05/06 20:42:09 manubsd Exp */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#define COMMON_DIGEST_FOR_OPENSSL 1

#include <sys/types.h>
#include <sys/param.h>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#ifdef HAVE_OPENSSL
/* get openssl/ssleay version number */
#include <openssl/opensslv.h>

#if !defined(OPENSSL_VERSION_NUMBER) || (OPENSSL_VERSION_NUMBER < 0x0090602fL)
#error OpenSSL version 0.9.6 or later required.
#endif
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/des.h>
#include <openssl/crypto.h>
#ifdef HAVE_OPENSSL_ENGINE_H
#include <openssl/engine.h>
#endif
#include <openssl/err.h>
#else /* HAVE_OPENSSL */
#include <Security/SecDH.h>
#include <Security/SecRandom.h>
#endif /* HAVE_OPENSSL */

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonCryptor.h>

#ifdef HAVE_OPENSSL
/* 0.9.7 stuff? */
#if OPENSSL_VERSION_NUMBER < 0x0090700fL
typedef STACK_OF(GENERAL_NAME) GENERAL_NAMES;
#else
#define USE_NEW_DES_API
#endif

#define OpenSSL_BUG()	do { plog(ASL_LEVEL_ERR, "OpenSSL function failed\n"); } while(0)
#endif

#include "crypto_openssl.h"
#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"
#include "gcmalloc.h"


/*
 * I hate to cast every parameter to des_xx into void *, but it is
 * necessary for SSLeay/OpenSSL portability.  It sucks.
 */

#ifdef HAVE_OPENSSL
static X509 *mem2x509(vchar_t *);
#endif
static caddr_t eay_hmac_init (vchar_t *, CCHmacAlgorithm);


#ifdef HAVE_OPENSSL

/*
 * The following are derived from code in crypto/x509/x509_cmp.c
 * in OpenSSL0.9.7c:
 * X509_NAME_wildcmp() adds wildcard matching to the original
 * X509_NAME_cmp(), nocase_cmp() and nocase_spacenorm_cmp() are as is.
 */
#include <ctype.h>
/* Case insensitive string comparision */
static int nocase_cmp(const ASN1_STRING *a, const ASN1_STRING *b)
{
	int i;

	if (a->length != b->length)
		return (a->length - b->length);

	for (i=0; i<a->length; i++)
	{
		int ca, cb;

		ca = tolower(a->data[i]);
		cb = tolower(b->data[i]);

		if (ca != cb)
			return(ca-cb);
	}
	return 0;
}

/* Case insensitive string comparision with space normalization 
 * Space normalization - ignore leading, trailing spaces, 
 *       multiple spaces between characters are replaced by single space  
 */
static int nocase_spacenorm_cmp(const ASN1_STRING *a, const ASN1_STRING *b)
{
	unsigned char *pa = NULL, *pb = NULL;
	int la, lb;
	
	la = a->length;
	lb = b->length;
	pa = a->data;
	pb = b->data;

	/* skip leading spaces */
	while (la > 0 && isspace(*pa))
	{
		la--;
		pa++;
	}
	while (lb > 0 && isspace(*pb))
	{
		lb--;
		pb++;
	}

	/* skip trailing spaces */
	while (la > 0 && isspace(pa[la-1]))
		la--;
	while (lb > 0 && isspace(pb[lb-1]))
		lb--;

	/* compare strings with space normalization */
	while (la > 0 && lb > 0)
	{
		int ca, cb;

		/* compare character */
		ca = tolower(*pa);
		cb = tolower(*pb);
		if (ca != cb)
			return (ca - cb);

		pa++; pb++;
		la--; lb--;

		if (la <= 0 || lb <= 0)
			break;

		/* is white space next character ? */
		if (isspace(*pa) && isspace(*pb))
		{
			/* skip remaining white spaces */
			while (la > 0 && isspace(*pa))
			{
				la--;
				pa++;
			}
			while (lb > 0 && isspace(*pb))
			{
				lb--;
				pb++;
			}
		}
	}
	if (la > 0 || lb > 0)
		return la - lb;

	return 0;
}

static int X509_NAME_wildcmp(const X509_NAME *a, const X509_NAME *b)
{
    int i,j;
    X509_NAME_ENTRY *na,*nb;

    if (sk_X509_NAME_ENTRY_num(a->entries)
	!= sk_X509_NAME_ENTRY_num(b->entries))
	    return sk_X509_NAME_ENTRY_num(a->entries)
	      -sk_X509_NAME_ENTRY_num(b->entries);
    for (i=sk_X509_NAME_ENTRY_num(a->entries)-1; i>=0; i--)
    {
	    na=sk_X509_NAME_ENTRY_value(a->entries,i);
	    nb=sk_X509_NAME_ENTRY_value(b->entries,i);
	    j=OBJ_cmp(na->object,nb->object);
	    if (j) return(j);
	    if ((na->value->length == 1 && na->value->data[0] == '*')
	     || (nb->value->length == 1 && nb->value->data[0] == '*'))
		    continue;
	    j=na->value->type-nb->value->type;
	    if (j) return(j);
	    if (na->value->type == V_ASN1_PRINTABLESTRING)
		    j=nocase_spacenorm_cmp(na->value, nb->value);
	    else if (na->value->type == V_ASN1_IA5STRING
		    && OBJ_obj2nid(na->object) == NID_pkcs9_emailAddress)
		    j=nocase_cmp(na->value, nb->value);
	    else
		    {
		    j=na->value->length-nb->value->length;
		    if (j) return(j);
		    j=memcmp(na->value->data,nb->value->data,
			    na->value->length);
		    }
	    if (j) return(j);
	    j=na->set-nb->set;
	    if (j) return(j);
    }

    return(0);
}

/*
 * compare two subjectNames.
 * OUT:        0: equal
 *	positive:
 *	      -1: other error.
 */
int
eay_cmp_asn1dn(n1, n2)
	vchar_t *n1, *n2;
{
	X509_NAME *a = NULL, *b = NULL;
	caddr_t p;
	int i = -1;

	p = n1->v;
	if (!d2i_X509_NAME(&a, (void *)&p, n1->l))
		goto end;
	p = n2->v;
	if (!d2i_X509_NAME(&b, (void *)&p, n2->l))
		goto end;

	i = X509_NAME_wildcmp(a, b);

    end:
	if (a)
		X509_NAME_free(a);
	if (b)
		X509_NAME_free(b);
	return i;
}

/*
 * Get the common name from a cert
 */
#define EAY_MAX_CN_LEN 256
vchar_t *
eay_get_x509_common_name(cert)
	vchar_t *cert;
{
	X509 *x509 = NULL;	
	X509_NAME *name;
	vchar_t *commonName = NULL;
	
	commonName = vmalloc(EAY_MAX_CN_LEN);
	if (commonName == NULL) {
		plog(ASL_LEVEL_ERR, "no memory\n");
		return NULL;
	}
	
	x509 = mem2x509(cert);
	if (x509 == NULL) {
		vfree(commonName);
		return NULL;
	}

	name = X509_get_subject_name(x509);
	X509_NAME_get_text_by_NID(name, NID_commonName, commonName->v, EAY_MAX_CN_LEN);
	
	commonName->l = strlen(commonName->v);
	
	if (x509)
		X509_free(x509);
	return commonName;
}

/*
 * get the subjectAltName from X509 certificate.
 * the name must be terminated by '\0'.
 */
int
eay_get_x509subjectaltname(cert, altname, type, pos, len)
	vchar_t *cert;
	char **altname;
	int *type;
	int pos;
	int *len;
{
	X509 *x509 = NULL;
	int i;
	GENERAL_NAMES 	*gens = NULL;
	GENERAL_NAME 	*gen;
	int error = -1;

	*altname = NULL;
	*type = GENT_OTHERNAME;

	x509 = mem2x509(cert);
	if (x509 == NULL)
		goto end;

	gens = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
	if (gens == NULL)
		goto end;

	for (i = 0; i < sk_GENERAL_NAME_num(gens); i++) {
		if (i + 1 != pos)
			continue;
		break;
	}
	
	/* there is no data at "pos" */
	if (i == sk_GENERAL_NAME_num(gens))
		goto end;
		
	gen = sk_GENERAL_NAME_value(gens, i);

	/* make sure the data is terminated by '\0'. */
	if (gen->d.ia5->data[gen->d.ia5->length] != '\0') {
		plog(ASL_LEVEL_ERR, 
			"data is not terminated by 0.");
		hexdump(gen->d.ia5->data, gen->d.ia5->length + 1);
		goto end;
	}

	/* read DNSName / Email */
	if (gen->type == GEN_DNS	||
		gen->type == GEN_EMAIL  ||
		gen->type == GEN_URI) {
	
		*len = gen->d.ia5->length + 1;
		*altname = racoon_malloc(*len);
		if (!*altname)
			goto end;
			
		strlcpy(*altname, (const char *)gen->d.ia5->data, *len);
		*type = gen->type;
		
		error = 0;
	} else if (gen->type == GEN_IPADD) {
		
		*len = gen->d.ia5->length + 1;
		*altname = racoon_malloc(*len);
		if (!*altname)
			goto end;
			
		memcpy(*altname, (const char *)gen->d.ia5->data, *len);
		*type = gen->type;
		
		error = 0;
	}

   end:
	if (error) {
		if (*altname) {
			racoon_free(*altname);
			*altname = NULL;
		}
#ifndef EAYDEBUG
		plog(ASL_LEVEL_ERR, "%s\n", eay_strerror());
#else
		printf("%s\n", eay_strerror());
#endif
	}
	if (gens)
	        /* free the whole stack. */
		sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
	if (x509)
		X509_free(x509);

	return error;
}


/* get X509 structure from buffer. */
static X509 *
mem2x509(cert)
	vchar_t *cert;
{
	X509 *x509;

#ifndef EAYDEBUG
    {
	u_char *bp;

	bp = (unsigned char *) cert->v;

	x509 = d2i_X509(NULL, (void *)&bp, cert->l);
    }
#else
    {
	BIO *bio;
	int len;

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		return NULL;
	len = BIO_write(bio, cert->v, cert->l);
	if (len == -1)
		return NULL;
	x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
    }
#endif
	return x509;
}

/*
 * get error string
 * MUST load ERR_load_crypto_strings() first.
 */
char *
eay_strerror()
{
	static char ebuf[512];
	int len = 0, n;
	unsigned long l;
	char buf[200];
	const char *file, *data;
	int line, flags;
	unsigned long es;

	es = CRYPTO_thread_id();

	while ((l = ERR_get_error_line_data(&file, &line, &data, &flags)) != 0){
		n = snprintf(ebuf + len, sizeof(ebuf) - len,
				"%lu:%s:%s:%d:%s ",
				es, ERR_error_string(l, buf), file, line,
				(flags & ERR_TXT_STRING) ? data : "");
		if (n < 0 || n >= sizeof(ebuf) - len)
			break;
		len += n;
		if (sizeof(ebuf) < len)
			break;
	}

	return ebuf;
}

#endif /* HAVE_OPENSSL */

vchar_t *
eay_CCCrypt(CCOperation  oper,
			CCAlgorithm  algo,
			CCOptions    opts,
			vchar_t     *data,
			vchar_t     *key,
			vchar_t     *iv)
{
    vchar_t         *res;
    size_t           res_len = 0;
    CCCryptorStatus  status;

    /* allocate buffer for result */
    if ((res = vmalloc(data->l)) == NULL)
        return NULL;

    status = CCCrypt(oper,
                     algo,
                     opts,
                     key->v, key->l,
                     iv->v,
                     data->v, data->l,
                     res->v, res->l, &res_len);
    if (status == kCCSuccess) {
        if (res->l != res_len) {
            plog(ASL_LEVEL_ERR, 
                 "crypt %d %d length mismatch. expected: %zd. got: %zd.\n",
                 oper, algo, res->l, res_len);
        }
        return res;
    } else {
        plog(ASL_LEVEL_ERR, 
             "crypt %d %d error. status %d.\n",
             oper, algo, (int)status);
    }
    vfree(res);
    return NULL;
}

/*
 * DES-CBC
 */
vchar_t *
eay_des_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
    return(eay_CCCrypt(kCCEncrypt, kCCAlgorithmDES, 0 /* CBC */, data, key, iv));
}

vchar_t *
eay_des_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
    return(eay_CCCrypt(kCCDecrypt, kCCAlgorithmDES, 0 /* CBC */, data, key, iv));
}

int
eay_des_weakkey(key)
	vchar_t *key;
{
#ifdef HAVE_OPENSSL
#ifdef USE_NEW_DES_API
	return DES_is_weak_key((void *)key->v);
#else
	return des_is_weak_key((void *)key->v);
#endif
#else
	return 0;
#endif
}

int
eay_des_keylen(len)
	int len;
{
    /* CommonCrypto return lengths in bytes, ipsec-tools
     * uses lengths in bits, therefore conversion is required.
     */
    if (len != 0 && len != (kCCKeySizeDES << 3))
        return -1;

    return kCCKeySizeDES << 3;      
}

/*
 * 3DES-CBC
 */
vchar_t *
eay_3des_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
    return(eay_CCCrypt(kCCEncrypt, kCCAlgorithm3DES, 0 /* CBC */, data, key, iv));
}

vchar_t *
eay_3des_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
    return(eay_CCCrypt(kCCDecrypt, kCCAlgorithm3DES, 0 /* CBC */, data, key, iv));
}

int
eay_3des_weakkey(key)
	vchar_t *key;
{
	return 0;
}

int
eay_3des_keylen(len)
	int len;
{
    /* CommonCrypto return lengths in bytes, ipsec-tools
     * uses lengths in bits, therefore conversion is required.
     */
    if (len != 0 && len != (kCCKeySize3DES << 3))
        return -1;

    return kCCKeySize3DES << 3;
}

/*
 * AES(RIJNDAEL)-CBC
 */
vchar_t *
eay_aes_encrypt(data, key, iv)
vchar_t *data, *key, *iv;
{
    return(eay_CCCrypt(kCCEncrypt, kCCAlgorithmAES128 /* adapts to AES-192, or AES-256 depending on the key size*/, 0 /* CBC */, data, key, iv));
}

vchar_t *
eay_aes_decrypt(data, key, iv)
vchar_t *data, *key, *iv;
{
    return(eay_CCCrypt(kCCDecrypt, kCCAlgorithmAES128 /* adapts to AES-192, or AES-256 depending on the key size*/, 0 /* CBC */, data, key, iv));
}

int
eay_aes_keylen(len)
int len;
{
    /* CommonCrypto return lengths in bytes, ipsec-tools
     * uses lengths in bits, therefore conversion is required.
     */
    if (len != 0) {
        if (len != (kCCKeySizeAES128 << 3) &&
            len != (kCCKeySizeAES192 << 3) &&
            len != (kCCKeySizeAES256 << 3))
            return -1;
    } else {
        return kCCKeySizeAES128 << 3;
    }
    return len;
}


int
eay_aes_weakkey(key)
	vchar_t *key;
{
	return 0;
}

/* for ipsec part */
int
eay_null_hashlen()
{
	return 0;
}

int
eay_null_keylen(len)
	int len;
{
	return 0;
}

/*
 * HMAC functions
 */
static caddr_t
eay_hmac_init(key, algorithm)
	vchar_t *key;
	CCHmacAlgorithm algorithm;
{
	CCHmacContext *c = racoon_malloc(sizeof(*c));

	CCHmacInit(c, algorithm, key->v, key->l);

	return (caddr_t)c;
}

#ifdef WITH_SHA2
/*
 * HMAC SHA2-512
 */
vchar_t *
eay_hmacsha2_512_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha2_512_init(key);
	eay_hmacsha2_512_update(ctx, data);
	res = eay_hmacsha2_512_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha2_512_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, kCCHmacAlgSHA512);
}

void
eay_hmacsha2_512_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	CCHmacUpdate(ALIGNED_CAST(CCHmacContext *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha2_512_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(CC_SHA512_DIGEST_LENGTH)) == 0)
		return NULL;

	CCHmacFinal(ALIGNED_CAST(CCHmacContext *)c, res->v);
	res->l = CC_SHA512_DIGEST_LENGTH;
		
	(void)racoon_free(c);
	return(res);
}

/*
 * HMAC SHA2-384
 */
vchar_t *
eay_hmacsha2_384_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha2_384_init(key);
	eay_hmacsha2_384_update(ctx, data);
	res = eay_hmacsha2_384_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha2_384_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, kCCHmacAlgSHA384);
}

void
eay_hmacsha2_384_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	CCHmacUpdate(ALIGNED_CAST(CCHmacContext *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha2_384_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(CC_SHA384_DIGEST_LENGTH)) == 0)
		return NULL;

	CCHmacFinal(ALIGNED_CAST(CCHmacContext *)c, res->v);
	res->l = CC_SHA384_DIGEST_LENGTH;

	(void)racoon_free(c);
	return(res);
}

/*
 * HMAC SHA2-256
 */
vchar_t *
eay_hmacsha2_256_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha2_256_init(key);
	eay_hmacsha2_256_update(ctx, data);
	res = eay_hmacsha2_256_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha2_256_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, kCCHmacAlgSHA256);
}

void
eay_hmacsha2_256_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	CCHmacUpdate(ALIGNED_CAST(CCHmacContext *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha2_256_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(CC_SHA256_DIGEST_LENGTH)) == 0)
		return NULL;

	CCHmacFinal(ALIGNED_CAST(CCHmacContext *)c, res->v);
	res->l = CC_SHA256_DIGEST_LENGTH;

	(void)racoon_free(c);
	return(res);
}
#endif	/* WITH_SHA2 */

/*
 * HMAC SHA1
 */
vchar_t *
eay_hmacsha1_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha1_init(key);
	eay_hmacsha1_update(ctx, data);
	res = eay_hmacsha1_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha1_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, kCCHmacAlgSHA1);
}

void
eay_hmacsha1_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	CCHmacUpdate(ALIGNED_CAST(CCHmacContext *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha1_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(CC_SHA1_DIGEST_LENGTH)) == 0)
		return NULL;

	CCHmacFinal(ALIGNED_CAST(CCHmacContext *)c, res->v);
	res->l = CC_SHA1_DIGEST_LENGTH;

	(void)racoon_free(c);
	return(res);
}

/*
 * HMAC MD5
 */
vchar_t *
eay_hmacmd5_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacmd5_init(key);
	eay_hmacmd5_update(ctx, data);
	res = eay_hmacmd5_final(ctx);

	return(res);
}

caddr_t
eay_hmacmd5_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, kCCHmacAlgMD5);
}

void
eay_hmacmd5_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	CCHmacUpdate(ALIGNED_CAST(CCHmacContext *)c, data->v, data->l);
}

vchar_t *
eay_hmacmd5_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(CC_MD5_DIGEST_LENGTH)) == 0)
		return NULL;

	CCHmacFinal(ALIGNED_CAST(CCHmacContext *)c, res->v);
	res->l = CC_MD5_DIGEST_LENGTH;
	(void)racoon_free(c);

	return(res);
}



#ifdef WITH_SHA2
/*
 * SHA2-512 functions
 */
caddr_t
eay_sha2_512_init()
{
	SHA512_CTX *c = racoon_malloc(sizeof(*c));

	SHA512_Init(c);

	return((caddr_t)c);
}

void
eay_sha2_512_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA512_Update(ALIGNED_CAST(SHA512_CTX *)c, (unsigned char *) data->v, data->l);

	return;
}

vchar_t *
eay_sha2_512_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA512_DIGEST_LENGTH)) == 0)
		return(0);

	SHA512_Final((unsigned char *) res->v, ALIGNED_CAST(SHA512_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha2_512_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha2_512_init();
	eay_sha2_512_update(ctx, data);
	res = eay_sha2_512_final(ctx);

	return(res);
}

int
eay_sha2_512_hashlen()
{
	return SHA512_DIGEST_LENGTH << 3;
}
#endif

#ifdef WITH_SHA2
/*
 * SHA2-384 functions
 */
 
typedef SHA512_CTX SHA384_CTX;

caddr_t
eay_sha2_384_init()
{
	SHA384_CTX *c = racoon_malloc(sizeof(*c));

	SHA384_Init(c);

	return((caddr_t)c);
}

void
eay_sha2_384_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA384_Update(ALIGNED_CAST(SHA384_CTX *)c, (unsigned char *) data->v, data->l);

	return;
}

vchar_t *
eay_sha2_384_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA384_DIGEST_LENGTH)) == 0)
		return(0);

	SHA384_Final((unsigned char *) res->v, ALIGNED_CAST(SHA384_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha2_384_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha2_384_init();
	eay_sha2_384_update(ctx, data);
	res = eay_sha2_384_final(ctx);

	return(res);
}

int
eay_sha2_384_hashlen()
{
	return SHA384_DIGEST_LENGTH << 3;
}
#endif

#ifdef WITH_SHA2
/*
 * SHA2-256 functions
 */
caddr_t
eay_sha2_256_init()
{
	SHA256_CTX *c = racoon_malloc(sizeof(*c));

	SHA256_Init(c);

	return((caddr_t)c);
}

void
eay_sha2_256_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA256_Update(ALIGNED_CAST(SHA256_CTX *)c, (unsigned char *) data->v, data->l);

	return;
}

vchar_t *
eay_sha2_256_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA256_DIGEST_LENGTH)) == 0)
		return(0);

	SHA256_Final((unsigned char *) res->v, ALIGNED_CAST(SHA256_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha2_256_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha2_256_init();
	eay_sha2_256_update(ctx, data);
	res = eay_sha2_256_final(ctx);

	return(res);
}

int
eay_sha2_256_hashlen()
{
	return SHA256_DIGEST_LENGTH << 3;
}
#endif

/*
 * SHA functions
 */
caddr_t
eay_sha1_init()
{
	SHA_CTX *c = racoon_malloc(sizeof(*c));

	SHA1_Init(c);

	return((caddr_t)c);
}

void
eay_sha1_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA1_Update(ALIGNED_CAST(SHA_CTX *)c, data->v, data->l); 

	return;
}

vchar_t *
eay_sha1_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA_DIGEST_LENGTH)) == 0)
		return(0);

	SHA1_Final((unsigned char *) res->v, ALIGNED_CAST(SHA_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha1_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha1_init();
	eay_sha1_update(ctx, data);
	res = eay_sha1_final(ctx);

	return(res);
}

int
eay_sha1_hashlen()
{
	return SHA_DIGEST_LENGTH << 3;
}

/*
 * MD5 functions
 */
caddr_t
eay_md5_init()
{
	MD5_CTX *c = racoon_malloc(sizeof(*c));

	MD5_Init(c);

	return((caddr_t)c);
}

void
eay_md5_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	MD5_Update(ALIGNED_CAST(MD5_CTX *)c, data->v, data->l);

	return;
}

vchar_t *
eay_md5_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(MD5_DIGEST_LENGTH)) == 0)
		return(0);

	MD5_Final((unsigned char *) res->v, ALIGNED_CAST(MD5_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_md5_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_md5_init();
	eay_md5_update(ctx, data);
	res = eay_md5_final(ctx);

	return(res);
}

int
eay_md5_hashlen()
{
	return MD5_DIGEST_LENGTH << 3;
}


#ifdef HAVE_OPENSSL
/*
 * eay_set_random
 *   size: number of bytes.
 */
vchar_t *
eay_set_random(size)
	u_int32_t size;
{
	BIGNUM *r = NULL;
	vchar_t *res = 0;

	if ((r = BN_new()) == NULL)
		goto end;
	BN_rand(r, size * 8, 0, 0);
	eay_bn2v(&res, r);

end:
	if (r)
		BN_free(r);
	return(res);
}
#else
vchar_t *
eay_set_random(u_int32_t size)
{
	vchar_t *res = vmalloc(size);
	
	if (res == NULL)
		return NULL;
		
	if (SecRandomCopyBytes(kSecRandomDefault, size, (uint8_t*)res->v)) {
		vfree(res);
		return NULL;
	}
	
	return res;
}
#endif

#ifdef HAVE_OPENSSL
/* DH */
int
eay_dh_generate(prime, g, publen, pub, priv)
	vchar_t *prime, **pub, **priv;
	u_int publen;
	u_int32_t g;
{
	BIGNUM *p = NULL;
	DH *dh = NULL;
	int error = -1;

	/* initialize */
	/* pre-process to generate number */
	if (eay_v2bn(&p, prime) < 0)
		goto end;

	if ((dh = DH_new()) == NULL)
		goto end;
	dh->p = p;
	p = NULL;	/* p is now part of dh structure */
	dh->g = NULL;
	if ((dh->g = BN_new()) == NULL)
		goto end;
	if (!BN_set_word(dh->g, g))
		goto end;

	if (publen != 0)
		dh->length = publen;

	/* generate public and private number */
	if (!DH_generate_key(dh))
		goto end;

	/* copy results to buffers */
	if (eay_bn2v(pub, dh->pub_key) < 0)
		goto end;
	if (eay_bn2v(priv, dh->priv_key) < 0) {
		vfree(*pub);
		goto end;
	}

	error = 0;

end:
	if (dh != NULL)
		DH_free(dh);
	if (p != 0)
		BN_free(p);
	return(error);
}

int
eay_dh_compute(prime, g, pub, priv, pub2, key)
	vchar_t *prime, *pub, *priv, *pub2, **key;
	u_int32_t g;
{
	BIGNUM *dh_pub = NULL;
	DH *dh = NULL;
	int l;
	unsigned char *v = NULL;
	int error = -1;

	/* make public number to compute */
	if (eay_v2bn(&dh_pub, pub2) < 0)
		goto end;

	/* make DH structure */
	if ((dh = DH_new()) == NULL)
		goto end;
	if (eay_v2bn(&dh->p, prime) < 0)
		goto end;
	if (eay_v2bn(&dh->pub_key, pub) < 0)
		goto end;
	if (eay_v2bn(&dh->priv_key, priv) < 0)
		goto end;
	dh->length = pub2->l * 8;

	dh->g = NULL;
	if ((dh->g = BN_new()) == NULL)
		goto end;
	if (!BN_set_word(dh->g, g))
		goto end;

	if ((v = racoon_calloc(prime->l, sizeof(u_char))) == NULL)
		goto end;
	if ((l = DH_compute_key(v, dh_pub, dh)) == -1)
		goto end;
	memcpy((*key)->v + (prime->l - l), v, l);

	error = 0;

end:
	if (dh_pub != NULL)
		BN_free(dh_pub);
	if (dh != NULL)
		DH_free(dh);
	if (v != NULL)
		racoon_free(v);
	return(error);
}

/*
 * convert vchar_t <-> BIGNUM.
 *
 * vchar_t: unit is u_char, network endian, most significant byte first.
 * BIGNUM: unit is BN_ULONG, each of BN_ULONG is in host endian,
 *	least significant BN_ULONG must come first.
 *
 * hex value of "0x3ffe050104" is represented as follows:
 *	vchar_t: 3f fe 05 01 04
 *	BIGNUM (BN_ULONG = u_int8_t): 04 01 05 fe 3f
 *	BIGNUM (BN_ULONG = u_int16_t): 0x0104 0xfe05 0x003f
 *	BIGNUM (BN_ULONG = u_int32_t_t): 0xfe050104 0x0000003f
 */
int
eay_v2bn(bn, var)
	BIGNUM **bn;
	vchar_t *var;
{
	if ((*bn = BN_bin2bn((unsigned char *) var->v, var->l, NULL)) == NULL)
		return -1;

	return 0;
}

int
eay_bn2v(var, bn)
	vchar_t **var;
	BIGNUM *bn;
{
	*var = vmalloc(bn->top * BN_BYTES);
	if (*var == NULL)
		return(-1);

	(*var)->l = BN_bn2bin(bn, (unsigned char *) (*var)->v);

	return 0;
}

void
eay_init()
{
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
#ifdef HAVE_OPENSSL_ENGINE_H
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
#endif
}
#endif /* HAVE_OPENSSL */

u_int32_t
eay_random()
{
	u_int32_t result;
	vchar_t *vrand;

	vrand = eay_set_random(sizeof(result));
	memcpy(&result, vrand->v, sizeof(result));
	vfree(vrand);

	return result;
}

#ifdef HAVE_OPENSSL
const char *
eay_version()
{
	return SSLeay_version(SSLEAY_VERSION);
}
#endif
