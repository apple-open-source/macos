/* SRP SASL plugin
 * Ken Murchison
 * Tim Martin  3/17/00
 * $Id: srp.c,v 1.2 2002/05/22 17:57:03 snsimon Exp $
 */
/* 
 * Copyright (c) 2001 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

/* for big number support */
#include <openssl/bn.h>

/* for digest and cipher support */
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <sasl.h>
#if OPENSSL_VERSION_NUMBER < 0x00907000L
#define MD5_H  /* suppress internal MD5 */
#endif
#include <saslplug.h>

#include "plugin_common.h"
#include "sasldb.h"

#ifdef WIN32
/* This must be after sasl.h, saslutil.h */
#include "saslSRP.h"
#endif

#ifdef macintosh
#include <sasl_srp_plugin_decl.h>
#endif 

/*****************************  Common Section  *****************************/

static const char plugin_id[] = "$Id: srp.c,v 1.2 2002/05/22 17:57:03 snsimon Exp $";

/* Size of diffie-hellman secrets a and b */
#define BITSFORab 64
/* How many bytes big should the salt be? */
#define SRP_SALT_SIZE 16
/* Size limit of SRP buffer */
#define MAXBUFFERSIZE 2147483643

#define DEFAULT_MDA		"SHA-1"

#define OPTION_MDA		"mda="
#define OPTION_REPLAY_DETECTION	"replay detection"
#define OPTION_INTEGRITY	"integrity="
#define OPTION_CONFIDENTIALITY	"confidentiality="
#define OPTION_MANDATORY	"mandatory="
#define OPTION_MAXBUFFERSIZE	"maxbuffersize="

/* Table of recommended Modulus (base 16) and Generator pairs */
struct Ng {
    char *N;
    unsigned long g;
} Ng_tab[] = {
    /* [264 bits] */
    { "115B8B692E0E045692CF280B436735C77A5A9E8A9E7ED56C965F87DB5B2A2ECE3",
      2
    },
    /* [384 bits] */
    { "8025363296FB943FCE54BE717E0E2958A02A9672EF561953B2BAA3BAACC3ED5754EB764C7AB7184578C57D5949CCB41B",
      2
    },
    /* [512 bits] */
    { "D4C7F8A2B32C11B8FBA9581EC4BA4F1B04215642EF7355E37C0FC0443EF756EA2C6B8EEB755A1C723027663CAA265EF785B8FF6A9B35227A52D86633DBDFCA43",
      2
    },
    /* [640 bits] */
    { "C94D67EB5B1A2346E8AB422FC6A0EDAEDA8C7F894C9EEEC42F9ED250FD7F0046E5AF2CF73D6B2FA26BB08033DA4DE322E144E7A8E9B12A0E4637F6371F34A2071C4B3836CBEEAB15034460FAA7ADF483",
      2
    },
    /* [768 bits] */
    { "B344C7C4F8C495031BB4E04FF8F84EE95008163940B9558276744D91F7CC9F402653BE7147F00F576B93754BCDDF71B636F2099E6FFF90E79575F3D0DE694AFF737D9BE9713CEF8D837ADA6380B1093E94B6A529A8C6C2BE33E0867C60C3262B",
      2
    },
    /* [1024 bits] */
    { "EEAF0AB9ADB38DD69C33F80AFA8FC5E86072618775FF3C0B9EA2314C9C256576D674DF7496EA81D3383B4813D692C6E0E0D5D8E250B98BE48E495C1D6089DAD15DC7D7B46154D6B6CE8EF4AD69B15D4982559B297BCF1885C529F566660E57EC68EDBC3C05726CC02FD4CBF4976EAA9AFD5138FE8376435B9FC61D2FC0EB06E3",
      2
    },
    /* [1280 bits] */
    { "D77946826E811914B39401D56A0A7843A8E7575D738C672A090AB1187D690DC43872FC06A7B6A43F3B95BEAEC7DF04B9D242EBDC481111283216CE816E004B786C5FCE856780D41837D95AD787A50BBE90BD3A9C98AC0F5FC0DE744B1CDE1891690894BC1F65E00DE15B4B2AA6D87100C9ECC2527E45EB849DEB14BB2049B163EA04187FD27C1BD9C7958CD40CE7067A9C024F9B7C5A0B4F5003686161F0605B",
      2
    },
    /* [1536 bits] */
    { "9DEF3CAFB939277AB1F12A8617A47BBBDBA51DF499AC4C80BEEEA9614B19CC4D5F4F5F556E27CBDE51C6A94BE4607A291558903BA0D0F84380B655BB9A22E8DCDF028A7CEC67F0D08134B1C8B97989149B609E0BE3BAB63D47548381DBC5B1FC764E3F4B53DD9DA1158BFD3E2B9C8CF56EDF019539349627DB2FD53D24B7C48665772E437D6C7F8CE442734AF7CCB7AE837C264AE3A9BEB87F8A2FE9B8B5292E5A021FFF5E91479E8CE7A28C2442C6F315180F93499A234DCF76E3FED135F9BB",
      2
    },
    /* [2048 bits] */
    { "AC6BDB41324A9A9BF166DE5E1389582FAF72B6651987EE07FC3192943DB56050A37329CBB4A099ED8193E0757767A13DD52312AB4B03310DCD7F48A9DA04FD50E8083969EDB767B0CF6095179A163AB3661A05FBD5FAAAE82918A9962F0B93B855F97993EC975EEAA80D740ADBF4FF747359D041D5C33EA71D281E446B14773BCA97B43A23FB801676BD207A436C6481F1D2B9078717461A5B9D32E688F87748544523B524B0D57D5EA77A2775D2ECFA032CFBDBF52FB3786160279004E57AE6AF874E7303CE53299CCC041C7BC308D82A5698F3A8D0C38271AE35F8E9DBFBB694B5C803D89F7AE435DE236D525F54759B65E372FCD68EF20FA7111F9E4AFF73",
      2
    }
};

#define NUM_Ng (sizeof(Ng_tab) / sizeof(struct Ng))


typedef struct layer_option_s {
    const char *name;		/* name used in option strings */
    unsigned enabled;		/* enabled?  determined at run-time */
    unsigned bit;		/* unique bit in bitmask */
    sasl_ssf_t ssf;		/* ssf of layer */
    const char *evp_name;	/* name used for lookup in EVP table */
} layer_option_t;

static layer_option_t digest_options[] = {
    {"SHA-1",		0, (1<<0), 1,	"sha1"},
    {"RIPEMD-160",	0, (1<<1), 1,	"rmd160"},
    {"MD5",		0, (1<<2), 1,	"md5"},
    {NULL,		0, (0<<0), 0,	NULL}
};
static layer_option_t *default_digest = &digest_options[0];
static layer_option_t *server_mda = NULL;

static layer_option_t cipher_options[] = {
    {"DES",		0, (1<<0), 56,	"des-ofb"},
    {"3DES",		0, (1<<1), 112,	"des-ede-ofb"},
    {"AES",		0, (1<<2), 128,	"aes-128-ofb"},
    {"Blowfish",	0, (1<<3), 128,	"bf-ofb"},
    {"CAST-128",	0, (1<<4), 128,	"cast5-ofb"},
    {"IDEA",		0, (1<<5), 128,	"idea-ofb"},
    {NULL,		0, (0<<0), 0,	NULL}
};
/* XXX Hack until OpenSSL 0.9.7 */
#if OPENSSL_VERSION_NUMBER < 0x00907000L
static layer_option_t *default_cipher = &cipher_options[0];
#else
static layer_option_t *default_cipher = &cipher_options[2];
#endif


enum {
    BIT_REPLAY_DETECTION=	(1<<0),
    BIT_INTEGRITY=		(1<<1),
    BIT_CONFIDENTIALITY=	(1<<2)
};

typedef struct srp_options_s {
    unsigned mda;		/* bitmask of MDAs */
    unsigned replay_detection;	/* replay detection on/off flag */
    unsigned integrity;		/* bitmask of integrity layers */
    unsigned confidentiality;	/* bitmask of confidentiality layers */
    unsigned mandatory;		/* bitmask of mandatory layers */
    unsigned long maxbufsize;	/* max # bytes processed by security layer */
} srp_options_t;

/* The main SRP context */
typedef struct context {
    int state;
    
    BIGNUM N;
    BIGNUM g;
    
    BIGNUM v;			/* verifier */
    
    BIGNUM B;
    
    BIGNUM a;
    BIGNUM A;
    
    char *K;
    int Klen;
    
    char *M1;
    int M1len;
    
    char *authid;		/* authentication id (server) */
    char *userid;		/* authorization id (server) */
    sasl_secret_t *password;	/* user secret (client) */
    unsigned int free_password; /* set if we need to free password */
    
    char *client_options;
    char *server_options;
    
    srp_options_t client_opts;
    
    char *salt;
    int saltlen;
    
    const EVP_MD *md;		/* underlying MDA */
    
    /* copy of utils from the params structures */
    const sasl_utils_t *utils;
    
    /* per-step mem management */
    char *out_buf;
    unsigned out_buf_len;
    
    /* Layer foo */
    unsigned enabled;		/* bitmask of enabled layers */
    const EVP_MD *hmac_md;	/* HMAC for integrity */
    const EVP_CIPHER *cipher;	/* cipher for confidentiality */
    
    /* replay detection sequence numbers */
    int seqnum_out;
    int seqnum_in;
    
    /* for encoding/decoding mem management */
    buffer_info_t  *enc_in_buf;
    char           *encode_buf, *decode_buf, *decode_once_buf;
    unsigned       encode_buf_len, decode_buf_len, decode_once_buf_len;
    char           *encode_tmp_buf, *decode_tmp_buf;
    unsigned       encode_tmp_buf_len, decode_tmp_buf_len;
    
    /* layers buffering */
    char           *buffer;
    int            bufsize;
    char           sizebuf[4];
    int            cursize;
    int            size;
    int            needsize;
    
} context_t;

static int srp_encode(void *context,
		      const struct iovec *invec,
		      unsigned numiov,
		      const char **output,
		      unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int hashlen = 0;
    char hash[EVP_MAX_MD_SIZE+1]; /* 1 for os() count */
    int tmpnum;
    struct buffer_info *inblob, bufinfo;
    char *input;
    unsigned inputlen;
    int ret;
    
    if (!context || !invec || !numiov || !output || !outputlen) {
	PARAMERROR( text->utils );
	return SASL_BADPARAM;
    }
    
    if (numiov > 1) {
	ret = _plug_iovec_to_buf(text->utils, invec, numiov,
				 &text->enc_in_buf);
	if (ret != SASL_OK) return ret;
	inblob = text->enc_in_buf;
    } else {
	/* avoid the data copy */
	bufinfo.data = invec[0].iov_base;
	bufinfo.curlen = invec[0].iov_len;
	inblob = &bufinfo;
    }
    
    input = inblob->data;
    inputlen = inblob->curlen;
    
    if (text->enabled & BIT_CONFIDENTIALITY) {
	EVP_CIPHER_CTX ctx;
	unsigned char IV[EVP_MAX_IV_LENGTH];
	unsigned char block1[EVP_MAX_IV_LENGTH];
	unsigned k = 8; /* EVP_CIPHER_CTX_block_size() isn't working */
	unsigned enclen = 0;
	unsigned tmplen;
	
	ret = _plug_buf_alloc(text->utils, &(text->encode_tmp_buf),
			      &(text->encode_tmp_buf_len),
			      inputlen + 2 * k);
	if (ret != SASL_OK) return ret;
	
	EVP_CIPHER_CTX_init(&ctx);
	
	memset(IV, 0, sizeof(IV));
	EVP_EncryptInit(&ctx, text->cipher, text->K, IV);
	
	/* construct the first block so that octets #k-1 and #k
	 * are exact copies of octets #1 and #2
	 */
	text->utils->rand(text->utils->rpool, block1, k - 2);
	memcpy(block1 + k-2, block1, 2);
	
	EVP_EncryptUpdate(&ctx, text->encode_tmp_buf, &tmplen, block1, k);
	enclen += tmplen;
	
	EVP_EncryptUpdate(&ctx, text->encode_tmp_buf + enclen, &tmplen,
			  input, inputlen);
	enclen += tmplen;
	
	EVP_EncryptFinal(&ctx, text->encode_tmp_buf + enclen, &tmplen);
	enclen += tmplen;
	
	EVP_CIPHER_CTX_cleanup(&ctx);
	
	input = text->encode_tmp_buf;
	inputlen = enclen;
    }
    
    if (text->enabled & BIT_INTEGRITY) {
	HMAC_CTX hmac_ctx;
	
	HMAC_Init(&hmac_ctx, text->K, text->Klen, text->hmac_md);
	
	HMAC_Update(&hmac_ctx, input, inputlen);
	
	if (text->enabled & BIT_REPLAY_DETECTION) {
	    tmpnum = htonl(text->seqnum_out);
	    HMAC_Update(&hmac_ctx, (char *) &tmpnum, 4);
	    
	    text->seqnum_out++;
	}
	
	HMAC_Final(&hmac_ctx, hash+1, &hashlen);
	hash[0] = hashlen++ & 0xFF; /* set os() count */
    }
    
    /* 4 for length + input size + hashlen for integrity (could be zero) */
    *outputlen = 4 + inputlen + hashlen;
    
    ret = _plug_buf_alloc(text->utils, &(text->encode_buf),
			  &(text->encode_buf_len),
			  *outputlen);
    if (ret != SASL_OK) return ret;
    
    tmpnum = inputlen+hashlen;
    tmpnum = htonl(tmpnum);
    memcpy(text->encode_buf, &tmpnum, 4);
    memcpy(text->encode_buf+4, input, inputlen);
    memcpy(text->encode_buf+4+inputlen, hash, hashlen);
    
    *output = text->encode_buf;
    
    return SASL_OK;
}

/* decode a single SRP packet */
static int srp_decode_once(void *context,
			   const char **input,
			   unsigned *inputlen,
			   char **output,
			   unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int tocopy;
    unsigned diff;
    int ret;
    
    if (text->needsize > 0) { /* 4 bytes for how long message is */
	/* if less than 4 bytes just copy those we have into text->size */
	if (*inputlen < 4) 
	    tocopy = *inputlen;
	else
	    tocopy = 4;
	
	if (tocopy > text->needsize)
	    tocopy = text->needsize;
	
	memcpy(text->sizebuf + 4 - text->needsize, *input, tocopy);
	text->needsize-=tocopy;
	
	*input += tocopy;
	*inputlen -= tocopy;
	    
	if (text->needsize == 0) { /* got all of size */
	    memcpy(&(text->size), text->sizebuf, 4);
	    text->cursize = 0;
	    text->size = ntohl(text->size);
	    
	    if ((text->size > 0xFFFF) || (text->size < 0)) {
		return SASL_FAIL; /* too big probably error */
	    }
	    
	    if (!text->buffer)
		text->buffer=text->utils->malloc(text->size+5);
	    else
		text->buffer=text->utils->realloc(text->buffer,text->size+5);
	    if (text->buffer == NULL) return SASL_NOMEM;
	}

	*outputlen = 0;
	*output = NULL;

	if (*inputlen == 0) /* have to wait until next time for data */
	    return SASL_OK;
	
	if (text->size==0)  /* should never happen */
	    return SASL_FAIL;
    }
    
    diff = text->size - text->cursize; /* bytes need for full message */
    
    if (!text->buffer)
	return SASL_FAIL;
    
    if (*inputlen < diff) { /* not enough for a decode */
	memcpy(text->buffer + text->cursize, *input, *inputlen);
	text->cursize += *inputlen;
	*inputlen = 0;
	*outputlen = 0;
	*output = NULL;
	return SASL_OK;
    } else {
	memcpy(text->buffer+text->cursize, *input, diff);
	*input += diff;      
	*inputlen -= diff;
    }
    
    {
	char *buf = text->buffer;
	int buflen = text->size;
	int hashlen = 0;
	
	if (text->enabled & BIT_INTEGRITY) {
	    HMAC_CTX hmac_ctx;
	    char hash[EVP_MAX_MD_SIZE+1]; /* 1 for os() count */
	    int tmpnum;
	    int i;
	    
	    HMAC_Init(&hmac_ctx, text->K, text->Klen, text->hmac_md);
	    
	    hashlen = EVP_MD_size(text->hmac_md) + 1; /* 1 for os() count */
	    
	    if (buflen < hashlen) {
		text->utils->seterror(text->utils->conn, 0,
				      "SRP input is smaller"
				      "than hash length: %d vs %d\n",
				      buflen, hashlen);
		return SASL_BADPROT;
	    }
	    
	    /* create my version of the hash */
	    HMAC_Update(&hmac_ctx, buf, buflen - hashlen);
	    
	    if (text->enabled & BIT_REPLAY_DETECTION) {
		tmpnum = htonl(text->seqnum_in);
		HMAC_Update(&hmac_ctx, (char *) &tmpnum, 4);
		
		text->seqnum_in ++;
	    }
	    
	    HMAC_Final(&hmac_ctx, hash+1, NULL);
	    hash[0] = (hashlen-1) & 0xFF; /* set os() count */
	    
	    /* compare to hash given */
	    for (i = 0; i < hashlen; i++) {
		if (hash[i] != buf[buflen - hashlen + i]) {
		    SETERROR(text->utils, "Hash is incorrect\n");
		    return SASL_BADMAC;
		}
	    }
	}
	
	if (text->enabled & BIT_CONFIDENTIALITY) {
	    EVP_CIPHER_CTX ctx;
	    unsigned char IV[EVP_MAX_IV_LENGTH];
	    unsigned char block1[EVP_MAX_IV_LENGTH];
	    unsigned k = 8; /* EVP_CIPHER_CTX_block_size() isn't working */
	    
	    unsigned declen = 0;
	    unsigned tmplen;
	    
	    ret = _plug_buf_alloc(text->utils, &text->decode_tmp_buf,
				  &text->decode_tmp_buf_len,
				  buflen - hashlen);
	    if (ret != SASL_OK) return ret;
	    
	    EVP_CIPHER_CTX_init(&ctx);
	    
	    memset(IV, 0, sizeof(IV));
	    EVP_DecryptInit(&ctx, text->cipher, text->K, IV);
	    
	    /* check the first block and see if octets #k-1 and #k
	     * are exact copies of octects #1 and #2
	     */
	    EVP_DecryptUpdate(&ctx, block1, &tmplen, buf, k);
	    
	    if ((block1[0] != block1[k-2]) || (block1[1] != block1[k-1])) {
		return SASL_BADAUTH;
	    }
	    
	    EVP_DecryptUpdate(&ctx, text->decode_tmp_buf, &tmplen,
			      buf + k, buflen - k - hashlen);
	    declen += tmplen;
	    
	    EVP_DecryptFinal(&ctx, text->decode_tmp_buf + declen, &tmplen);
	    declen += tmplen;
	    
	    EVP_CIPHER_CTX_cleanup(&ctx);
	    
	    buf = text->decode_tmp_buf;
	    *outputlen = declen;
	} else {
	    *outputlen = buflen - hashlen;
	}
	
	ret = _plug_buf_alloc(text->utils, &(text->decode_once_buf),
			      &(text->decode_once_buf_len),
			      *outputlen);
	if (ret != SASL_OK) return ret;
	
	memcpy(text->decode_once_buf, buf, *outputlen);
	
	*output = text->decode_once_buf;
    }
    
    text->size = -1;
    text->needsize = 4;
    
    return SASL_OK;
}

/* decode and concatenate multiple SRP packets */
static int srp_decode(void *context,
		      const char *input, unsigned inputlen,
		      const char **output, unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int ret;
    
    ret = _plug_decode(text->utils, context, input, inputlen,
		       &text->decode_buf, &text->decode_buf_len, outputlen,
		       srp_decode_once);
    
    *output = text->decode_buf;
    
    return ret;
}

#define MAX_BUFFER_LEN 2147483643
#define MAX_UTF8_LEN 65535
#define MAX_OS_LEN 255

/*
 * Make a SRP buffer
 *
 * in1 must exist but the rest may be NULL
 *
 */
static int MakeBuffer(context_t *text,
		      char *in1, int in1len,
		      char *in2, int in2len,
		      char *in3, int in3len,
		      char *in4, int in4len,
		      const char **out,
		      unsigned *outlen)
{
    int result;
    int len;
    int inbyteorder;
    char *out2;
    
    if (!in1) {
	text->utils->log(NULL, SASL_LOG_ERR,
			 "At least one buffer must be active\n");
	return SASL_FAIL;
    }
    
    len = in1len + in2len + in3len + in4len;
    
    if (len > MAX_BUFFER_LEN) {
	text->utils->log(NULL, SASL_LOG_ERR,
			 "String too long to create SRP buffer string\n");
	return SASL_FAIL;
    }
    
    result = _plug_buf_alloc(text->utils, &text->out_buf, &text->out_buf_len,
			     len + 4);
    if (result != SASL_OK) return result;
    
    out2 = text->out_buf;
    
    /* put length in */
    inbyteorder = htonl(len);
    memcpy(out2, &inbyteorder, 4);
    
    /* copy in data */
    memcpy((out2)+4, in1, in1len);
    
    if (in2len)
	memcpy((out2)+4+in1len, in2, in2len);
    
    if (in3len)
	memcpy((out2)+4+in1len+in2len, in3, in3len);
    
    if (in4len)
	memcpy((out2)+4+in1len+in2len+in3len, in4, in4len);
    
    *outlen = len + 4;
    
    *out = out2;
    
    return SASL_OK;
}

/* Un'buffer' a string
 *
 * 'out' becomes a pointer into 'in' not an allocation
 */
static int UnBuffer(const sasl_utils_t *utils, char *in, int inlen,
		    char **out, int *outlen)
{
    int lenbyteorder;
    int len;
    
    if ((!in) || (inlen < 4)) {
	utils->seterror(utils->conn, 0,
			"Buffer is not big enough to be SRP buffer: %d\n", inlen);
	return SASL_BADPROT;
    }
    
    /* get the length */
    memcpy(&lenbyteorder, in, 4);
    len = ntohl(lenbyteorder);
    
    /* make sure it's right */
    if (len + 4 != inlen) {
	SETERROR(utils, "SRP Buffer isn't of the right length\n");
	return SASL_BADPROT;
    }
    
    *out = in+4;
    *outlen = len;
    
    return SASL_OK;
}

static int MakeUTF8(const sasl_utils_t *utils,
		    char *in,
		    char **out,
		    int *outlen)
{
    int llen;
    short len;
    short inbyteorder;
    
    if (!in) {
	utils->log(NULL, SASL_LOG_ERR, "Can't create utf8 string from null");
	return SASL_FAIL;
    }
    
    /* xxx actual utf8 conversion */
    
    llen = strlen(in);
    
    if (llen > MAX_UTF8_LEN) {
	utils->log(NULL, SASL_LOG_ERR,
		   "String too long to create utf8 string\n");
	return SASL_FAIL;
    }
    len = (short)llen;
    
    *out = utils->malloc(len+2);
    if (!*out) return SASL_NOMEM;
    
    /* put in len */
    inbyteorder = htons(len);
    memcpy(*out, &inbyteorder, 2);
    
    /* put in data */
    memcpy((*out)+2, in, len);
    
    *outlen = len+2;
    
    return SASL_OK;
}

static int GetUTF8(const sasl_utils_t *utils, char *data, int datalen,
		   char **outstr, char **left, int *leftlen)
{
    short lenbyteorder;
    int len;
    
    if ((!data) || (datalen < 2)) {
	SETERROR(utils, "Buffer is not big enough to be SRP UTF8\n");
	return SASL_BADPROT;
    }
    
    /* get the length */
    memcpy(&lenbyteorder, data, 2);
    len = ntohs(lenbyteorder);
    
    /* make sure it's right */
    if (len + 2 > datalen) {
	SETERROR(utils, "Not enough data for this SRP UTF8\n");
	return SASL_BADPROT;
    }
    
    *outstr = (char *)utils->malloc(len+1);
    if (!*outstr) return SASL_NOMEM;
    
    memcpy(*outstr, data+2, len);
    (*outstr)[len] = '\0';
    
    *left = data+len+2;
    *leftlen = datalen - (len+2);
    
    return SASL_OK;
}

static int MakeOS(const sasl_utils_t *utils,
		  char *in, 
		  int inlen,
		  char **out,
		  int *outlen)
{
    if (!in) {
	utils->log(NULL, SASL_LOG_ERR, "Can't create SRP os string from null");
	return SASL_FAIL;
    }
    
    if (inlen > MAX_OS_LEN) {
	utils->log(NULL, SASL_LOG_ERR,
		   "String too long to create SRP os string\n");
	return SASL_FAIL;
    }
    
    *out = utils->malloc(inlen+1);
    if (!*out) return SASL_NOMEM;
    
    /* put in len */
    (*out)[0] = inlen & 0xFF;
    
    /* put in data */
    memcpy((*out)+1, in, inlen);
    
    *outlen = inlen+1;
    
    return SASL_OK;
}

static int GetOS(const sasl_utils_t *utils, char *data, int datalen,
		 char **outstr, int *outlen, char **left, int *leftlen)
{
    int len;
    
    if ((!data) || (datalen < 1)) {
	SETERROR(utils, "Buffer is not big enough to be SRP os\n");
	return SASL_BADPROT;
    }
    
    /* get the length */
    len = (unsigned char)data[0];
    
    /* make sure it's right */
    if (len + 1 > datalen) {
	SETERROR(utils, "Not enough data for this SRP os\n");
	return SASL_FAIL;
    }
    
    *outstr = (char *)utils->malloc(len+1);
    if (!*outstr) return SASL_NOMEM;
    
    memcpy(*outstr, data+1, len);
    (*outstr)[len] = '\0';
    
    *outlen = len;
    
    *left = data+len+1;
    *leftlen = datalen - (len+1);
    
    return SASL_OK;
}

/*
 * Convert a big integer to it's byte representation
 */
static int BigIntToBytes(BIGNUM *num, char *out, int maxoutlen, int *outlen)
{
    int len;
    
    len = BN_num_bytes(num);
    
    if (len > maxoutlen) return SASL_FAIL;
    
    *outlen = BN_bn2bin(num, out);
    
    return SASL_OK;    
}

static int BigIntCmpWord(BIGNUM *a, BN_ULONG w)
{
    BIGNUM *b = BN_new();
    int r;
    
    BN_set_word(b, w);
    r = BN_cmp(a, b);
    BN_free(b);
    return r;
}

static int MakeMPI(const sasl_utils_t *utils,
		   BIGNUM *num,
		   char **out,
		   int *outlen)
{
    int shortlen;
    int len;
    short inbyteorder;
    int alloclen;
    int r;
    
    alloclen = BN_num_bytes(num);
    
    *out = utils->malloc(alloclen+2);
    if (!*out) return SASL_NOMEM;
    
    r = BigIntToBytes(num, (*out)+2, alloclen, &len);
    if (r) {
	utils->free(*out);
	return r;
    }
    
    *outlen = 2+len;
    
    /* put in len */
    shortlen = len;
    inbyteorder = htons(shortlen);
    memcpy(*out, &inbyteorder, 2);
    
    return SASL_OK;
}

static int GetMPI(const sasl_utils_t *utils,
		  unsigned char *data, int datalen, BIGNUM *outnum,
		  char **left, int *leftlen)
{
    
    
    short lenbyteorder;
    int len;
    
    if ((!data) || (datalen < 2)) {
	utils->seterror(utils->conn, 0,
			"Buffer is not big enough to be SRP MPI: %d\n",
			datalen);
	return SASL_BADPROT;
    }
    
    /* get the length */
    memcpy(&lenbyteorder, data, 2);
    len = ntohs(lenbyteorder);
    
    /* make sure it's right */
    if (len + 2 > datalen) {
	utils->seterror(utils->conn, 0,
			"Not enough data for this SRP MPI: we have %d; "
			"it says it's %d\n", datalen, len+2);
	return SASL_BADPROT;
    }
    
    BN_init(outnum);
    BN_bin2bn(data+2, len, outnum);
    
    *left = data+len+2;
    *leftlen = datalen - (len+2);
    
    return SASL_OK;
}

static void GetRandBigInt(BIGNUM *out)
{
    BN_init(out);
    
    /* xxx likely should use sasl random funcs */
    BN_rand(out, BITSFORab, 0, 0);
}

/*
 * Call the hash function on some data
 */
static void HashData(context_t *text, char *in, int inlen,
		     unsigned char outhash[], int *outlen)
{
    EVP_MD_CTX mdctx;
    
    EVP_DigestInit(&mdctx, text->md);
    EVP_DigestUpdate(&mdctx, in, inlen);
    EVP_DigestFinal(&mdctx, outhash, outlen);
}

/*
 * Call the hash function on the data of a BigInt
 */
static int HashBigInt(context_t *text, BIGNUM *in,
		      unsigned char outhash[], int *outlen)
{
    int r;
    char buf[4096];
    int buflen;
    EVP_MD_CTX mdctx;
    
    r = BigIntToBytes(in, buf, sizeof(buf)-1, &buflen);
    if (r) return r;
    
    EVP_DigestInit(&mdctx, text->md);
    EVP_DigestUpdate(&mdctx, buf, buflen);
    EVP_DigestFinal(&mdctx, outhash, outlen);
    
    return 0;
}

static int HashInterleaveBigInt(context_t *text, BIGNUM *num,
				char **out, int *outlen)
{
    int r;
    char buf[4096];
    int buflen;
    
    int klen;
    int limit;
    int i;
    int offset;
    int j;
    EVP_MD_CTX mdEven;
    EVP_MD_CTX mdOdd;
    unsigned char Evenb[EVP_MAX_MD_SIZE];
    unsigned char Oddb[EVP_MAX_MD_SIZE];
    int hashlen;
    
    /* make bigint into bytes */
    r = BigIntToBytes(num, buf, sizeof(buf)-1, &buflen);
    if (r) return r;
    
    limit = buflen;
    
    /* skip by leading zero's */
    for (offset = 0; offset < limit && buf[offset] == 0x00; offset++) {
	/* nada */
    }
    
    klen = (limit - offset) / 2;
    
    EVP_DigestInit(&mdEven, text->md);
    EVP_DigestInit(&mdOdd, text->md);
    
    j = limit - 1;
    for (i = 0; i < klen; i++) {
	EVP_DigestUpdate(&mdEven, buf + j, 1);
	j--;
	EVP_DigestUpdate(&mdOdd, buf + j, 1);
	j--;
    }
    
    EVP_DigestFinal(&mdEven, Evenb, NULL);
    EVP_DigestFinal(&mdOdd, Oddb, &hashlen);
    
    *outlen = 2 * hashlen;
    *out = text->utils->malloc(*outlen);
    if (!*out) return SASL_NOMEM;
    
    for (i = 0, j = 0; i < hashlen; i++)
	{
	    (*out)[j++] = Evenb[i];
	    (*out)[j++] = Oddb[i];
	}
    
    return SASL_OK;
}

/*
 * Calculate 'x' which is needed to calculate 'K'
 *
 */
static int CalculateX(context_t *text,
		      const char *salt, 
		      int saltlen, 
		      const char *user, 
		      const char *pass, 
		      int passlen, 
		      BIGNUM *x)
{
    EVP_MD_CTX mdctx;
    char hash[EVP_MAX_MD_SIZE];
    int hashlen;
    
    /* x = H(salt | H(user | ':' | pass))
     *
     */      
    
    EVP_DigestInit(&mdctx, text->md);
    
    EVP_DigestUpdate(&mdctx, (char*) user, strlen(user));
    EVP_DigestUpdate(&mdctx, ":", 1);
    EVP_DigestUpdate(&mdctx, (char*) pass, passlen);
    
    EVP_DigestFinal(&mdctx, hash, &hashlen);
    
    
    EVP_DigestInit(&mdctx, text->md);
    
    EVP_DigestUpdate(&mdctx, (char*) salt, saltlen);
    EVP_DigestUpdate(&mdctx, hash, hashlen);
    
    EVP_DigestFinal(&mdctx, hash, &hashlen);
    
    BN_init(x);
    BN_bin2bn(hash, hashlen, x);
    
    return SASL_OK;
}

/*
 *  H(
 *            bytes(H( bytes(N) )) ^ bytes( H( bytes(g) )))
 *          | bytes(H( bytes(U) ))
 *          | bytes(s)
 *          | bytes(H( bytes(L) ))
 *          | bytes(A)
 *          | bytes(B)
 *          | bytes(K)
 *      )
 *
 * H() is the result of digesting the designated input/data with the
 * underlying Message Digest Algorithm function (see Section 1).
 *
 * ^ is the bitwise XOR operator.
 */
static int CalculateM1(context_t *text,
		       BIGNUM *N,
		       BIGNUM *g,
		       char *U,			/* username */
		       char *salt, int saltlen,	/* salt */
		       char *L,			/* server's options */
		       BIGNUM *A,		/* client's public key */
		       BIGNUM *B,		/* server's public key */
		       char *K, int Klen,
		       char **out, int *outlen)
{
    int i;
    int r;
    unsigned char p1a[EVP_MAX_MD_SIZE];
    unsigned char p1b[EVP_MAX_MD_SIZE];
    unsigned char p1[EVP_MAX_MD_SIZE];
    int p1len;
    char p2[EVP_MAX_MD_SIZE];
    int p2len;
    char *p3;
    int p3len;
    char p4[1024];
    int p4len;
    char p5[1024];
    int p5len;
    char *p6;
    int p6len;
    char p7[EVP_MAX_MD_SIZE];
    int p7len;
    char *tot;
    int totlen = 0;
    char *totp;
    
    /* p1) bytes(H( bytes(N) )) ^ bytes( H( bytes(g) )) */
    r = HashBigInt(text, N, p1a, NULL);
    if (r) return r;
    r = HashBigInt(text, g, p1b, &p1len);
    if (r) return r;
    
    for (i = 0; i < p1len; i++) {
	p1[i] = (p1a[i] ^ p1b[i]);
    }
    
    /* p2) bytes(H( bytes(U) )) */
    HashData(text, U, strlen(U), p2, &p2len);
    
    /* p3) bytes(s) */
    p3 = salt;
    p3len = saltlen;
    
    /* p4) bytes(A) */
    r = BigIntToBytes(A, p4, sizeof(p4), &p4len);
    if (r) return r;
    
    /* p5) bytes(B) */
    r = BigIntToBytes(B, p5, sizeof(p5), &p5len);
    if (r) return r;
    
    /* p6) bytes(K) */
    p6 = K;
    p6len = Klen;
    
    /* p7) bytes(H( bytes(L) )) */
    HashData(text, L, strlen(L), p7, &p7len);
    
    /* merge p1-p7 together */
    totlen = p1len + p2len + p3len + p4len + p5len + p6len + p7len;
    tot = text->utils->malloc(totlen);
    if (!tot) return SASL_NOMEM;
    
    totp = tot;
    
    memcpy(totp, p1, p1len); totp+=p1len;
    memcpy(totp, p2, p2len); totp+=p2len;
    memcpy(totp, p3, p3len); totp+=p3len;
    memcpy(totp, p4, p4len); totp+=p4len;
    memcpy(totp, p5, p5len); totp+=p5len;
    memcpy(totp, p6, p6len); totp+=p6len;
    memcpy(totp, p7, p7len); totp+=p7len;
    
    /* do the hash over the whole thing */
    *out = text->utils->malloc(EVP_MAX_MD_SIZE);
    if (!*out) {
	text->utils->free(tot);
	return SASL_NOMEM;
    }
    
    HashData(text, tot, totlen, *out, outlen);
    text->utils->free(tot);
    
    return SASL_OK;
}

/*
 *          H(
 *                  bytes(A)
 *                | bytes(H( bytes(U) ))
 *                | bytes(H( bytes(I) ))
 *                | bytes(H( bytes(o) ))
 *                | bytes(M1)
 *                | bytes(K)
 *            )
 *
 *
 * where: 
 *
 * H() is the result of digesting the designated input/data with the
 * underlying Message Digest Algorithm function (see Section 1)
 *
 */
static int CalculateM2(context_t *text,
		       BIGNUM *A,
		       char *U,
		       char *I,
		       char *o,
		       char *M1, int M1len,
		       char *K, int Klen,
		       char **out, int *outlen)
{
    int r;
    unsigned char p1[1024];
    int p1len;
    char *p2;
    int p2len;
    char *p3;
    int p3len;
    char p4[EVP_MAX_MD_SIZE];
    int p4len;
    char p5[EVP_MAX_MD_SIZE];
    int p5len;
    char p6[EVP_MAX_MD_SIZE];
    int p6len;
    char *tot;
    int totlen = 0;
    char *totp;
    
    /* p1) bytes(A) */
    r = BigIntToBytes(A, p1, sizeof(p1), &p1len);
    if (r) return r;    
    
    /* p2) bytes(M1) */
    p2 = M1;
    p2len = M1len;
    
    /* p3) bytes(K) */
    p3 = K;
    p3len = Klen;
    
    /* p4) bytes(H( bytes(U) )) */
    HashData(text, U, strlen(U), p4, &p4len);
    
    /* p5) bytes(H( bytes(I) )) */
    HashData(text, I, strlen(I), p5, &p5len);
    
    /* p6) bytes(H( bytes(o) )) */
    HashData(text, o, strlen(o), p6, &p6len);
    
    /* merge p1-p6 together */
    totlen = p1len + p2len + p3len + p4len + p5len + p6len;
    tot = text->utils->malloc(totlen);
    if (!tot) return SASL_NOMEM;
    
    totp = tot;
    
    memcpy(totp, p1, p1len); totp+=p1len;
    memcpy(totp, p2, p2len); totp+=p2len;
    memcpy(totp, p3, p3len); totp+=p3len;
    memcpy(totp, p4, p4len); totp+=p4len;
    memcpy(totp, p5, p5len); totp+=p5len;
    memcpy(totp, p6, p6len); totp+=p6len;
    
    /* do the hash over the whole thing */
    *out = text->utils->malloc(EVP_MAX_MD_SIZE);
    if (!*out) {
	return SASL_NOMEM;
	text->utils->free(tot);
    }
    
    HashData(text, tot, totlen, *out, outlen);
    text->utils->free(tot);
    
    return SASL_OK;
}

/* Parse an option out of an option string
 * Place found option in 'option'
 * 'nextptr' points to rest of string or NULL if at end
 */
static int ParseOption(const sasl_utils_t *utils,
		       char *in, char **option, char **nextptr)
{
    char *comma;
    int len;
    int i;
    
    if (strlen(in) == 0) {
	*option = NULL;
	return SASL_OK;
    }
    
    comma = strchr(in,',');    
    if (comma == NULL) comma = in + strlen(in);
    
    len = comma - in;
    
    *option = utils->malloc(len + 1);
    if (!*option) return SASL_NOMEM;
    
    /* lowercase string */
    for (i = 0; i < len; i++) {
	(*option)[i] = tolower((int)in[i]);
    }
    (*option)[len] = '\0';
    
    if (*comma) {
	*nextptr = comma+1;
    } else {
	*nextptr = NULL;
    }
    
    return SASL_OK;
}

static int FindBit(char *name, layer_option_t *opts)
{
    while (opts->name) {
	if (!strcasecmp(name, opts->name)) {
	    return opts->bit;
	}
	
	opts++;
    }
    
    return 0;
}

static layer_option_t *FindOptionFromBit(unsigned bit, layer_option_t *opts)
{
    while (opts->name) {
	if (opts->bit == bit) {
	    return opts;
	}
	
	opts++;
    }
    
    return NULL;
}

static int ParseOptionString(const sasl_utils_t *utils,
			     char *str, srp_options_t *opts, int isserver)
{
    if (!strncasecmp(str, OPTION_MDA, strlen(OPTION_MDA))) {
	
	int bit = FindBit(str+strlen(OPTION_MDA), digest_options);
	
	if (isserver && (!bit || opts->mda)) {
	    opts->mda = -1;
	    if (!bit)
		utils->seterror(utils->conn, 0,
				"SRP MDA %s not supported\n",
				str+strlen(OPTION_MDA));
	    else
		SETERROR(utils, "Multiple SRP MDAs given\n");
	    return SASL_BADPROT;
	}
	
	opts->mda = opts->mda | bit;
	
    } else if (!strcasecmp(str, OPTION_REPLAY_DETECTION)) {
	if (opts->replay_detection) {
	    SETERROR(utils, "SRP Replay Detection option appears twice\n");
	    return SASL_BADPROT;
	}
	opts->replay_detection = 1;
	
    } else if (!strncasecmp(str, OPTION_INTEGRITY, strlen(OPTION_INTEGRITY)) &&
	       !strncasecmp(str+strlen(OPTION_INTEGRITY), "HMAC-", 5)) {
	
	int bit = FindBit(str+strlen(OPTION_INTEGRITY)+5, digest_options);
	
	if (isserver && (!bit || opts->integrity)) {
	    opts->integrity = -1;
	    if (!bit)
		utils->seterror(utils->conn, 0,
				"SRP Integrity option %s not supported\n",
				str+strlen(OPTION_INTEGRITY));
	    else
		SETERROR(utils, "Multiple SRP Integrity options given\n");
	    return SASL_BADPROT;
	}
	
	opts->integrity = opts->integrity | bit;
	
    } else if (!strncasecmp(str, OPTION_CONFIDENTIALITY,
			    strlen(OPTION_CONFIDENTIALITY))) {
	
	int bit = FindBit(str+strlen(OPTION_CONFIDENTIALITY),
			  cipher_options);
	
	if (isserver && (!bit || opts->confidentiality)) {
	    opts->confidentiality = -1;
	    if (!bit)
		utils->seterror(utils->conn, 0,
				"SRP Confidentiality option %s not supported\n",
				str+strlen(OPTION_CONFIDENTIALITY));
	    else
		SETERROR(utils,
			 "Multiple SRP Confidentiality options given\n");
	    return SASL_FAIL;
	}
	
	opts->confidentiality = opts->confidentiality | bit;
	
    } else if (!isserver && !strncasecmp(str, OPTION_MANDATORY,
					 strlen(OPTION_MANDATORY))) {
	
	char *layer = str+strlen(OPTION_MANDATORY);
	
	if (!strcasecmp(layer, OPTION_REPLAY_DETECTION))
	    opts->mandatory |= BIT_REPLAY_DETECTION;
	else if (!strncasecmp(layer, OPTION_INTEGRITY,
			      strlen(OPTION_INTEGRITY)-1))
	    opts->mandatory |= BIT_INTEGRITY;
	else if (!strncasecmp(layer, OPTION_CONFIDENTIALITY,
			      strlen(OPTION_CONFIDENTIALITY)-1))
	    opts->mandatory |= BIT_CONFIDENTIALITY;
	else {
	    utils->seterror(utils->conn, 0,
			    "Mandatory SRP option %s not supported\n", layer);
	    return SASL_BADPROT;
	}
	
    } else if (!strncasecmp(str, OPTION_MAXBUFFERSIZE,
			    strlen(OPTION_MAXBUFFERSIZE))) {
	
	opts->maxbufsize = strtoul(str+strlen(OPTION_MAXBUFFERSIZE), NULL, 10);
	
	if (opts->maxbufsize > MAXBUFFERSIZE) {
	    utils->seterror(utils->conn, 0,
			    "SRP Maxbuffersize %lu too big (> %lu)\n",
			    opts->maxbufsize, MAXBUFFERSIZE);
	    return SASL_BADPROT;
	}
	
    } else {
	/* Ignore unknown options */
    }
    
    return SASL_OK;
}

static int ParseOptions(const sasl_utils_t *utils,
			char *in, srp_options_t *out, int isserver)
{
    int r;
    
    memset(out, 0, sizeof(srp_options_t));
    out->maxbufsize = MAXBUFFERSIZE;
    
    while (in) {
	char *opt;
	
	r = ParseOption(utils, in, &opt, &in);
	if (r) return r;
	
	if (opt == NULL) return SASL_OK;
	
	utils->log(NULL, SASL_LOG_DEBUG, "Got option: [%s]\n",opt);
	
	r = ParseOptionString(utils, opt, out, isserver);
	utils->free(opt);
	
	if (r) return r;
    }
    
    return SASL_OK;
}

static layer_option_t *FindBest(int available, sasl_ssf_t min_ssf,
				sasl_ssf_t max_ssf, layer_option_t *opts)
{
    layer_option_t *best = NULL;
    
    if (!available) return NULL;
    
    while (opts->name) {
	if (opts->enabled && (available & opts->bit) &&
	    (opts->ssf >= min_ssf) && (opts->ssf <= max_ssf) &&
	    (!best || (opts->ssf > best->ssf))) {
	    best = opts;
	}
	
	opts++;
    }
    
    return best;
}

static int OptionsToString(const sasl_utils_t *utils,
			   srp_options_t *opts, char **out)
{
    char *ret = NULL;
    int alloced = 0;
    int first = 1;
    layer_option_t *optlist;
    
    ret = utils->malloc(1);
    if (!ret) return SASL_NOMEM;
    alloced = 1;
    ret[0] = '\0';
    
    optlist = digest_options;
    while(optlist->name) {
	if (opts->mda & optlist->bit) {
	    alloced += strlen(OPTION_MDA)+strlen(optlist->name)+1;
	    ret = utils->realloc(ret, alloced);
	    if (!ret) return SASL_NOMEM;
	    
	    if (!first) strcat(ret, ",");
	    strcat(ret, OPTION_MDA);
	    strcat(ret, optlist->name);
	    first = 0;
	}
	
	optlist++;
    }
    
    if (opts->replay_detection) {
	alloced += strlen(OPTION_REPLAY_DETECTION)+1;
	ret = utils->realloc(ret, alloced);
	if (!ret) return SASL_NOMEM;
	
	if (!first) strcat(ret, ",");
	strcat(ret, OPTION_REPLAY_DETECTION);
	first = 0;
    }
    
    optlist = digest_options;
    while(optlist->name) {
	if (opts->integrity & optlist->bit) {
	    alloced += strlen(OPTION_INTEGRITY)+5+strlen(optlist->name)+1;
	    ret = utils->realloc(ret, alloced);
	    if (!ret) return SASL_NOMEM;
	    
	    if (!first) strcat(ret, ",");
	    strcat(ret, OPTION_INTEGRITY);
	    strcat(ret, "HMAC-");
	    strcat(ret, optlist->name);
	    first = 0;
	}
	
	optlist++;
    }
    
    optlist = cipher_options;
    while(optlist->name) {
	if (opts->confidentiality & optlist->bit) {
	    alloced += strlen(OPTION_CONFIDENTIALITY)+strlen(optlist->name)+1;
	    ret = utils->realloc(ret, alloced);
	    if (!ret) return SASL_NOMEM;
	    
	    if (!first) strcat(ret, ",");
	    strcat(ret, OPTION_CONFIDENTIALITY);
	    strcat(ret, optlist->name);
	    first = 0;
	}
	
	optlist++;
    }
    
    if ((opts->integrity || opts->confidentiality) &&
	opts->maxbufsize < MAXBUFFERSIZE) {
	alloced += strlen(OPTION_MAXBUFFERSIZE)+10+1;
	ret = utils->realloc(ret, alloced);
	if (!ret) return SASL_NOMEM;
	
	if (!first) strcat(ret, ",");
	strcat(ret, OPTION_MAXBUFFERSIZE);
	sprintf(ret+strlen(ret), "%lu", opts->maxbufsize);
	first = 0;
    }
    
    if (opts->mandatory & BIT_REPLAY_DETECTION) {
	alloced += strlen(OPTION_MANDATORY)+strlen(OPTION_REPLAY_DETECTION)+1;
	ret = utils->realloc(ret, alloced);
	if (!ret) return SASL_NOMEM;
	
	if (!first) strcat(ret, ",");
	strcat(ret, OPTION_MANDATORY);
	strcat(ret, OPTION_REPLAY_DETECTION);
	first = 0;
    }
    
    if (opts->mandatory & BIT_INTEGRITY) {
	alloced += strlen(OPTION_MANDATORY)+strlen(OPTION_INTEGRITY)-1+1;
	ret = utils->realloc(ret, alloced);
	if (!ret) return SASL_NOMEM;
	
	if (!first) strcat(ret, ",");
	strcat(ret, OPTION_MANDATORY);
	strncat(ret, OPTION_INTEGRITY, strlen(OPTION_INTEGRITY)-1);
	/* terminate string */
	ret[alloced-1] = '\0';
	first = 0;
    }
    
    if (opts->mandatory & BIT_CONFIDENTIALITY) {
	alloced += strlen(OPTION_MANDATORY)+strlen(OPTION_CONFIDENTIALITY)-1+1;
	ret = utils->realloc(ret, alloced);
	if (!ret) return SASL_NOMEM;
	
	if (!first) strcat(ret, ",");
	strcat(ret, OPTION_MANDATORY);
	strncat(ret, OPTION_CONFIDENTIALITY, strlen(OPTION_CONFIDENTIALITY)-1);
	/* terminate string */
	ret[alloced-1] = '\0';
	first = 0;
    }
    
    *out = ret;
    return SASL_OK;
}

/* Set the options (called by client and server)
 *
 * Set up variables/hashes/that sorta thing so layers
 * will operate properly
 */
static int SetOptions(srp_options_t *opts,
		      context_t *text,
		      const sasl_utils_t *utils,
		      sasl_out_params_t *oparams)
{
    layer_option_t *opt;
    
    opt = FindOptionFromBit(opts->mda, digest_options);
    if (!opt) {
	utils->log(NULL, SASL_LOG_ERR,
		   "Unable to find SRP MDA option now\n");
	return SASL_FAIL;
    }
    
    text->md = EVP_get_digestbyname(opt->evp_name);
    
    text->size = -1;
    text->needsize = 4;
    
    if ((opts->integrity == 0) && (opts->confidentiality == 0)) {
	oparams->encode = NULL;
	oparams->decode = NULL;
	oparams->mech_ssf = 0;
	utils->log(NULL, SASL_LOG_DEBUG, "Using no layer\n");
	return SASL_OK;
    }
    
    oparams->encode = &srp_encode;
    oparams->decode = &srp_decode;
    oparams->maxoutbuf = opts->maxbufsize - 4; /* account for eos() count */
    
    if (opts->replay_detection) {
	text->enabled |= BIT_REPLAY_DETECTION;
	
	/* If no integrity layer specified, use default */
	if (!opts->integrity)
	    opts->integrity = default_digest->bit;
    }
    
    if (opts->integrity) {
	
	text->enabled |= BIT_INTEGRITY;
	
	opt = FindOptionFromBit(opts->integrity, digest_options);
	if (!opt) {
	    utils->log(NULL, SASL_LOG_ERR,
		       "Unable to find SRP integrity layer option now\n");
	    return SASL_FAIL;
	}
	
	oparams->mech_ssf = opt->ssf;
	text->hmac_md = EVP_get_digestbyname(opt->evp_name);
	
	/* account for os() */
	oparams->maxoutbuf -= (EVP_MD_size(text->hmac_md) + 1);
    }
    
    if (opts->confidentiality) {
	
	text->enabled |= BIT_CONFIDENTIALITY;
	
	opt = FindOptionFromBit(opts->confidentiality, cipher_options);
	if (!opt) {
	    utils->log(NULL, SASL_LOG_ERR,
		       "Unable to find SRP confidentiality layer option now\n");
	    return SASL_FAIL;
	}
	
	oparams->mech_ssf = opt->ssf;
	text->cipher = EVP_get_cipherbyname(opt->evp_name);
    }
    
    return SASL_OK;
}


/*
 * Dispose of a SRP context (could be server or client)
 */ 
static void srp_common_mech_dispose(void *conn_context,
				    const sasl_utils_t *utils)
{
    context_t *text = (context_t *) conn_context;
    
    if (!text) return;
    
    BN_clear_free(&text->N);
    BN_clear_free(&text->g);
    BN_clear_free(&text->v);
    BN_clear_free(&text->B);
    BN_clear_free(&text->a);
    BN_clear_free(&text->A);
    
    if (text->K)		utils->free(text->K);
    if (text->M1)		utils->free(text->M1);
    
    if (text->authid)		utils->free(text->authid);
    if (text->userid)		utils->free(text->userid);
    if (text->free_password)	_plug_free_secret(utils, &(text->password));
    if (text->salt)		utils->free(text->salt);
    
    if (text->client_options)	utils->free(text->client_options);
    if (text->server_options)	utils->free(text->server_options);
    
    if (text->buffer)		utils->free(text->buffer);
    if (text->encode_buf)	utils->free(text->encode_buf);
    if (text->encode_tmp_buf)	utils->free(text->encode_tmp_buf);
    if (text->decode_buf)	utils->free(text->decode_buf);
    if (text->decode_once_buf)	utils->free(text->decode_once_buf);
    if (text->decode_tmp_buf)	utils->free(text->decode_tmp_buf);
    if (text->out_buf)		utils->free(text->out_buf);
    
    if (text->enc_in_buf) {
	if (text->enc_in_buf->data) utils->free(text->enc_in_buf->data);
	utils->free(text->enc_in_buf);
    }
    
    utils->free(text);
}

static void
srp_common_mech_free(void *global_context __attribute__((unused)),
		     const sasl_utils_t *utils __attribute__((unused)))
{
    EVP_cleanup();
}


/*****************************  Server Section  *****************************/

/* A large safe prime (N = 2q+1, where q is prime)
 *
 * Use N with the most bits from our table.
 *
 * All arithmetic is done modulo N
 */
static int generate_N_and_g(BIGNUM *N, BIGNUM *g)
{
    int result;
    
    BN_init(N);
    result = BN_hex2bn(&N, Ng_tab[NUM_Ng-1].N);
    if (!result) return SASL_FAIL;
    
    BN_init(g);
    BN_set_word(g, Ng_tab[NUM_Ng-1].g);
    
    return SASL_OK;
}

static int CalculateV(context_t *text,
		      BIGNUM *N, BIGNUM *g,
		      const char *user,
		      const char *pass, unsigned passlen,
		      BIGNUM *v, char **salt, int *saltlen)
{
    BIGNUM x;
    BN_CTX *ctx = BN_CTX_new();
    int r;    
    
    /* generate <salt> */    
    *saltlen = SRP_SALT_SIZE;
    *salt = (char *)text->utils->malloc(*saltlen);
    if (!*salt) return SASL_NOMEM;
    text->utils->rand(text->utils->rpool, *salt, *saltlen);
    
    r = CalculateX(text, *salt, *saltlen, user, pass, passlen, &x);
    if (r) {
	text->utils->seterror(text->utils->conn, 0, 
			      "Error calculating 'x'");
	return r;
    }
    
    /* v = g^x % N */
    BN_init(v);
    BN_mod_exp(v, g, &x, N, ctx);
    
    BN_CTX_free(ctx);
    BN_clear_free(&x);
    
    return r;   
}

static int ServerCalculateK(context_t *text, BIGNUM *v,
			    BIGNUM *N, BIGNUM *g, BIGNUM *B, BIGNUM *A,
			    char **key, int *keylen)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    BIGNUM b;
    BIGNUM u;
    BIGNUM base;
    BIGNUM S;
    BN_CTX *ctx = BN_CTX_new();
    int r;
    
    do {
	/* Generate b */
	GetRandBigInt(&b);
	
	/* Per [SRP]: make sure b > log[g](N) -- g is always 2 */
        BN_add_word(&b, BN_num_bits(N));
	
	/* B = (v + g^b) % N */
	BN_init(B);
	BN_mod_exp(B, g, &b, N, ctx);
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
	BN_mod_add(B, B, v, N, ctx);
#else
	BN_add(B, B, v);
	BN_mod(B, B, N, ctx);
#endif
	
	/* u is first 32 bits of B hashed; MSB first */
	r = HashBigInt(text, B, hash, NULL);
	if (r) return r;
	
	BN_init(&u);
	BN_bin2bn(hash, 4, &u);
	
    } while (BN_is_zero(&u)); /* Per Tom Wu: make sure u != 0 */
    
    /* calculate K
     *
     * Host:  S = (Av^u) ^ b % N             (computes session key)
     * Host:  K = Hi(S)
     */
    BN_init(&base);
    BN_mod_exp(&base, v, &u, N, ctx);
    BN_mod_mul(&base, &base, A, N, ctx);
    
    BN_init(&S);
    BN_mod_exp(&S, &base, &b, N, ctx);
    
    /* per Tom Wu: make sure Av^u != 1 (mod N) */
    if (BN_is_one(&base)) {
	SETERROR(text->utils, "Unsafe SRP value for 'Av^u'\n");
	r = SASL_BADPROT;
	goto err;
    }
    
    /* per Tom Wu: make sure Av^u != -1 (mod N) */
    BN_add_word(&base, 1);
    if (BN_cmp(&S, N) == 0) {
	SETERROR(text->utils, "Unsafe SRP value for 'Av^u'\n");
	r = SASL_BADPROT;
	goto err;
    }
    
    /* K = Hi(S) */
    r = HashInterleaveBigInt(text, &S, key, keylen);
    if (r) goto err;
    
    r = SASL_OK;
    
  err:
    BN_CTX_free(ctx);
    BN_clear_free(&b);
    BN_clear_free(&u);
    BN_clear_free(&base);
    BN_clear_free(&S);
    
    return r;
}

static int ParseUserSecret(const sasl_utils_t *utils,
			   char *secret, size_t seclen,
			   char **mda, BIGNUM *v, char **salt, int *saltlen)
{
    int r;
    char *data;
    int datalen;
    
    /* The secret data is stored as suggested in RFC 2945:
     *
     *  mda  - utf8
     *  v    - mpi
     *  salt - os 
     */
    r = UnBuffer(utils, secret, seclen, &data, &datalen);
    if (r) {
	utils->seterror(utils->conn, 0, 
			"Error UnBuffering secret data");
	return r;
    }
    
    r = GetUTF8(utils, data, datalen, mda, &data, &datalen);
    if (r) {
	utils->seterror(utils->conn, 0, 
			"Error parsing out 'mda'");
	return r;
    }
    
    r = GetMPI(utils, data, datalen, v, &data, &datalen);
    if (r) {
	utils->seterror(utils->conn, 0, 
			"Error parsing out 'v'");
	return r;
    }
    
    r = GetOS(utils, data, datalen, salt, saltlen, &data, &datalen);
    if (r) {
	utils->seterror(utils->conn, 0, 
			"Error parsing out salt");
	return r;
    }
    
    if (datalen != 0) {
	utils->seterror(utils->conn, 0, 
			"Extra data in request step 2");
	r = SASL_FAIL;
    }
    
    return r;
}

static int CreateServerOptions(sasl_server_params_t *sparams, char **out)
{
    srp_options_t opts;
    sasl_ssf_t limitssf, requiressf;
    layer_option_t *optlist;
    
    /* zero out options */
    memset(&opts,0,sizeof(srp_options_t));
    
    /* Add mda */
    opts.mda = server_mda->bit;
    
    if (sparams->props.max_ssf < sparams->external_ssf) {
	limitssf = 0;
    } else {
	limitssf = sparams->props.max_ssf - sparams->external_ssf;
    }
    if (sparams->props.min_ssf < sparams->external_ssf) {
	requiressf = 0;
    } else {
	requiressf = sparams->props.min_ssf - sparams->external_ssf;
    }
    
    /*
     * Add integrity options
     * Can't advertise integrity w/o support for default HMAC
     */
    if (default_digest->enabled) {
	optlist = digest_options;
	while(optlist->name) {
	    if (optlist->enabled &&
		/*(requiressf <= 1) &&*/ (limitssf >= 1)) {
		opts.integrity |= optlist->bit;
	    }
	    optlist++;
	}
    }
    
    /* if we set any integrity options we can advertise replay detection */
    if (opts.integrity) {
	opts.replay_detection = 1;
    }
    
    /*
     * Add confidentiality options
     * Can't advertise confidentiality w/o support for default cipher
     */
    if (default_cipher->enabled) {
	optlist = cipher_options;
	while(optlist->name) {
	    if (optlist->enabled &&
		(requiressf <= optlist->ssf) &&
		(limitssf >= optlist->ssf)) {
		opts.confidentiality |= optlist->bit;
	    }
	    optlist++;
	}
    }
    
    /* Add mandatory options */
    if (requiressf >= 1)
	opts.mandatory = BIT_REPLAY_DETECTION | BIT_INTEGRITY;
    if (requiressf > 1)
	opts.mandatory |= BIT_CONFIDENTIALITY;
    
    /* Add maxbuffersize */
    opts.maxbufsize = MAXBUFFERSIZE;
    if (sparams->props.maxbufsize &&
	sparams->props.maxbufsize < opts.maxbufsize)
	opts.maxbufsize = sparams->props.maxbufsize;
    
    return OptionsToString(sparams->utils, &opts, out);
}

static int
srp_server_mech_new(void *glob_context __attribute__((unused)),
		    sasl_server_params_t *params,
		    const char *challenge __attribute__((unused)),
		    unsigned challen __attribute__((unused)),
		    void **conn_context)
{
    context_t *text;
    
    /* holds state are in */
    text = params->utils->malloc(sizeof(context_t));
    if (text == NULL) {
	MEMERROR(params->utils);
	return SASL_NOMEM;
    }
    
    memset(text, 0, sizeof(context_t));
    
    text->state = 1;
    text->utils = params->utils;
    text->md = EVP_get_digestbyname(server_mda->evp_name);
    
    *conn_context = text;
    
    return SASL_OK;
}

static int srp_server_mech_step1(context_t *text,
				 sasl_server_params_t *params,
				 const char *clientin,
				 unsigned clientinlen,
				 const char **serverout,
				 unsigned *serveroutlen,
				 sasl_out_params_t *oparams)
{
    char *data;
    int datalen;
    int result;    
    char *mpiN = NULL;
    int mpiNlen;
    char *mpig = NULL;
    int mpiglen;
    char *osS = NULL;
    int osSlen;
    char *utf8L = NULL;
    int utf8Llen;
    char *realm = NULL;
    char *user = NULL;
    const char *password_request[] = { SASL_AUX_PASSWORD,
				       "*cmusaslsecretSRP",
				       NULL };
    struct propval auxprop_values[3];
    
    /* Expect:
     *
     * U - authentication identity
     * I - authorization identity
     *
     * { utf8(U) utf8(I) }
     *
     */
    result = UnBuffer(params->utils, (char *) clientin, clientinlen,
		      &data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error 'unbuffer'ing input for step 1");
	return result;
    }
    
    result = GetUTF8(params->utils, data, datalen, &text->authid,
		     &data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error getting UTF8 string from input");
	return result;
    }
    
    result = GetUTF8(params->utils, data, datalen, &text->userid,
		     &data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error parsing out userid");
	return result;
    }
    
    if (datalen != 0) {
	params->utils->seterror(params->utils->conn, 0, 
				"Extra data to SRP step 1");
	return SASL_FAIL;
    }
    
    /* Get the realm */
    result = _plug_parseuser(params->utils, &user, &realm, params->user_realm,
			     params->serverFQDN, text->authid);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error getting realm");
	goto cleanup;
    }
    
    /* Generate N and g */
    result = generate_N_and_g(&text->N, &text->g);
    if (result) {
	params->utils->seterror(text->utils->conn, 0, 
				"Error calculating N and g");
	return result;
    }
    
    /* Get user secret */
    result = params->utils->prop_request(params->propctx, password_request);
    if (result != SASL_OK) goto cleanup;
    
    /* this will trigger the getting of the aux properties */
    result = params->canon_user(params->utils->conn,
				user, 0, SASL_CU_AUTHID, oparams);
    if (result != SASL_OK) goto cleanup;
    
    result = params->canon_user(params->utils->conn,
				text->userid, 0, SASL_CU_AUTHZID, oparams);
    if (result != SASL_OK) goto cleanup;
    
    result = params->utils->prop_getnames(params->propctx, password_request,
					  auxprop_values);
    if (result < 0 ||
	((!auxprop_values[0].name || !auxprop_values[0].values) &&
	 (!auxprop_values[1].name || !auxprop_values[1].values))) {
	/* We didn't find this username */
	params->utils->seterror(params->utils->conn,0,
				"no secret in database");
	result = SASL_NOUSER;
	goto cleanup;
    }
    
    if (auxprop_values[1].name && auxprop_values[1].values) {
	char *mda = NULL;
	
	/* We have a precomputed verifier */
	result = ParseUserSecret(params->utils,
				 (char*) auxprop_values[1].values[0],
				 auxprop_values[1].valsize,
				 &mda, &text->v, &text->salt, &text->saltlen);
	
	if (result) {
	    /* ParseUserSecret sets error, if any */
	    if (mda) params->utils->free(mda);
	    goto cleanup;
	}
	
	/* find mda */
	server_mda = digest_options;
	while (server_mda->name) {
	    if (!strcasecmp(server_mda->name, mda))
		break;
	    
	    server_mda++;
	}
	
	if (!server_mda->name) {
	    params->utils->seterror(params->utils->conn, 0,
				    "unknown SRP mda '%s'", mda);
	    params->utils->free(mda);
	    result = SASL_FAIL;
	    goto cleanup;
	}
	params->utils->free(mda);
	
    } else if (auxprop_values[0].name && auxprop_values[0].values) {
	/* We only have the password -- calculate the verifier */
	int len = strlen(auxprop_values[0].values[0]);

	if (len == 0) {
	    params->utils->seterror(params->utils->conn,0,
				    "empty secret");
	    result = SASL_FAIL;
	    goto cleanup;
	}
	
	result = CalculateV(text, &text->N, &text->g, user,
		       auxprop_values[0].values[0], len,
		       &text->v, &text->salt, &text->saltlen);
	if (result) {
	    params->utils->seterror(params->utils->conn, 0, 
				    "Error calculating v");
	    goto cleanup;
	}
    } else {
	params->utils->seterror(params->utils->conn, 0,
				"Have neither type of secret");
	result = SASL_FAIL;
	goto cleanup;
    }    
    
    params->utils->prop_clear(params->propctx, 1);
    
    result = CreateServerOptions(params, &text->server_options);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error creating server options");
	goto cleanup;
    }
    
    /* Send out:
     *
     * N - safe prime modulus
     * g - generator
     * s - salt
     * L - server options (available layers etc)
     *
     * { mpi(N) mpi(g) os(s) utf8(L) }
     *
     */
    
    result = MakeMPI(params->utils, &text->N, &mpiN, &mpiNlen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error creating 'mpi' string for N");
	goto cleanup;
    }
    
    result = MakeMPI(params->utils, &text->g, &mpig, &mpiglen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error creating 'mpi' string for g");
	goto cleanup;
    }
    
    result = MakeOS(params->utils, text->salt, text->saltlen, &osS, &osSlen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error turning salt into 'os' string");
	goto cleanup;
    }
    
    result = MakeUTF8(params->utils, text->server_options, &utf8L, &utf8Llen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error creating 'UTF8' string for L (server options)");
	goto cleanup;
    }
    
    result = MakeBuffer(text, mpiN, mpiNlen, mpig, mpiglen, osS, osSlen,
		   utf8L, utf8Llen, serverout, serveroutlen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error creating SRP buffer from data in step 1");
	goto cleanup;
    }
    
    text->state = 2;
    result = SASL_CONTINUE;
    
  cleanup:
    if (user) params->utils->free(user);
    if (realm) params->utils->free(realm);
    if (mpiN) params->utils->free(mpiN);
    if (mpig) params->utils->free(mpig);
    if (osS) params->utils->free(osS);
    if (utf8L) params->utils->free(utf8L);
    
    return result;
}

static int srp_server_mech_step2(context_t *text,
			sasl_server_params_t *params,
			const char *clientin,
			unsigned clientinlen,
			const char **serverout,
			unsigned *serveroutlen,
			sasl_out_params_t *oparams)
{
    char *data;
    int datalen;
    int result;    
    char *mpiB = NULL;
    int mpiBlen;
    srp_options_t client_opts;
    
    /* Expect:
     *
     * A - client's public key
     * o - client option list
     *
     * { mpi(A) utf8(o) }
     *
     */
    result = UnBuffer(params->utils, (char *) clientin, clientinlen,
		      &data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error UnBuffering input in step 2");
	return result;
    }
    
    result = GetMPI(params->utils, data, datalen, &text->A, &data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error parsing out 'A'");
	return result;
    }
    
    /* Per [SRP]: reject A <= 0 */
    if (BigIntCmpWord(&text->A, 0) <= 0) {
	SETERROR(params->utils, "Illegal value for 'A'\n");
	return SASL_BADPROT;
    }
    
    result = GetUTF8(params->utils, data, datalen, &text->client_options,
		&data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error parsing out SRP client options 'o'");
	return result;
    }
    params->utils->log(NULL, SASL_LOG_DEBUG, "o: '%s'", text->client_options);
    
    if (datalen != 0) {
	params->utils->seterror(params->utils->conn, 0, 
				"Extra data in request step 2");
	return SASL_FAIL;
    }
    
    /* parse client options */
    result = ParseOptions(params->utils, text->client_options, &client_opts, 1);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error parsing user's options");
	
	if (client_opts.confidentiality) {
	    /* Mark that we attempted confidentiality layer negotiation */
	    oparams->mech_ssf = 2;
	}
	else if (client_opts.integrity || client_opts.replay_detection) {
	    /* Mark that we attempted integrity layer negotiation */
	    oparams->mech_ssf = 1;
	}
	return result;
    }
    
    result = SetOptions(&client_opts, text, params->utils, oparams);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error setting options");
	return result;   
    }
    
    /* Calculate K (and B) */
    result = ServerCalculateK(text, &text->v,
			      &text->N, &text->g, &text->B, &text->A,
			      &text->K, &text->Klen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error calculating K");
	return result;
    }
    
    /* Send out:
     *
     * B - server's public key
     *
     * { mpi(B) }
     */
    
    result = MakeMPI(params->utils, &text->B, &mpiB, &mpiBlen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error turning 'B' into 'mpi' string");
	goto cleanup;
    }
    
    result = MakeBuffer(text, mpiB, mpiBlen, NULL, 0, NULL, 0, NULL, 0,
			serverout, serveroutlen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error putting all the data together in step 2");
	goto cleanup;
    }
    
    text->state = 3;

    result = SASL_CONTINUE;
    
  cleanup:
    if (mpiB) params->utils->free(mpiB);
    
    return result;
}

static int srp_server_mech_step3(context_t *text,
				 sasl_server_params_t *params,
				 const char *clientin,
				 unsigned clientinlen,
				 const char **serverout,
				 unsigned *serveroutlen,
				 sasl_out_params_t *oparams)
{
    char *data;
    int datalen;
    int result;
    char *M1 = NULL;
    int M1len;
    char *myM1 = NULL;
    int myM1len;
    char *M2 = NULL;
    int M2len;
    int i;
    char *osM2 = NULL;
    int osM2len;
    
    /* Expect:
     *
     * M1 = client evidence
     *
     * { os(M1) }
     *
     */
    result = UnBuffer(params->utils, (char *) clientin, clientinlen,
		      &data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error parsing input buffer in step 3");
	goto cleanup;
    }
    
    result = GetOS(params->utils, data, datalen, &M1,&M1len, &data, &datalen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error getting 'os' M1 (client evidenice)");
	goto cleanup;
    }
    
    if (datalen != 0) {
	result = SASL_FAIL;
	params->utils->seterror(params->utils->conn, 0, 
				"Extra data in input SRP step 3");
	goto cleanup;
    }
    
    /* See if M1 is correct */
    result = CalculateM1(text, &text->N, &text->g, text->authid,
			 text->salt, text->saltlen,
			 text->server_options, &text->A, &text->B,
			 text->K, text->Klen, &myM1, &myM1len);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error calculating M1");
	goto cleanup;
    }
    
    if (myM1len != M1len) {
	params->utils->seterror(params->utils->conn, 0, 
				"SRP M1 lengths do not match");
	result = SASL_BADAUTH;
	goto cleanup;
    }
    
    for (i = 0; i < myM1len; i++) {
	if (myM1[i] != M1[i]) {
	    params->utils->seterror(params->utils->conn, 0, 
				    "client evidence does not match what we "
				    "calculated. Probably a password error");
	    result = SASL_BADAUTH;
	    goto cleanup;
	}
    }
    
    /* if we have a confidentiality layer we're done - send nothing */
    if (text->enabled & BIT_CONFIDENTIALITY) {
	
	/* set oparams */
	oparams->doneflag = 1;
	oparams->param_version = 0;
	
	result = SASL_OK;
	goto cleanup;
    }
    
    /* calculate M2 to send */
    result = CalculateM2(text, &text->A, text->authid, text->userid,
			 text->client_options, myM1, myM1len,
			 text->K, text->Klen, &M2, &M2len);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error calculating M2 (server evidence)");
	goto cleanup;
    }
    
    /* Send out:
     *
     * M2 = server evidence
     *
     * { os(M2) }
     */
    
    result = MakeOS(params->utils, M2, M2len, &osM2, &osM2len);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error making 'os' string from M2 (server evidence)");
	goto cleanup;
    }
    
    result = MakeBuffer(text, osM2, osM2len, NULL, 0, NULL, 0, NULL, 0,
			serverout, serveroutlen);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error making output buffer in SRP step 3");
	goto cleanup;
    }
    
    /* set oparams */
    oparams->doneflag = 1;
    oparams->param_version = 0;
    
    result = SASL_OK;
    
  cleanup:
    if (osM2) params->utils->free(osM2);
    if (M2) params->utils->free(M2);
    if (myM1) params->utils->free(myM1);
    if (M1) params->utils->free(M1);
    
    return result;    
}

static int srp_server_mech_step(void *conn_context,
				sasl_server_params_t *sparams,
				const char *clientin,
				unsigned clientinlen,
				const char **serverout,
				unsigned *serveroutlen,
				sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) conn_context;
    
    if (!sparams
	|| !serverout
	|| !serveroutlen
	|| !oparams)
	return SASL_BADPARAM;
    
    sparams->utils->log(NULL, SASL_LOG_DEBUG,
			"SRP server step %d\n", text->state);
    
    *serverout = NULL;
    *serveroutlen = 0;
	
    switch (text->state) {

    case 1:
	return srp_server_mech_step1(text, sparams, clientin, clientinlen,
				     serverout, serveroutlen, oparams);

    case 2:
	return srp_server_mech_step2(text, sparams, clientin, clientinlen,
				     serverout, serveroutlen, oparams);

    case 3:
	return srp_server_mech_step3(text, sparams, clientin, clientinlen,
				     serverout, serveroutlen, oparams);

    default:
	sparams->utils->seterror(sparams->utils->conn, 0,
				 "Invalid SRP server step %d", text->state);
	return SASL_FAIL;
    }
    
    return SASL_FAIL; /* should never get here */
}

#ifdef DO_SRP_SETPASS
static int srp_setpass(void *glob_context __attribute__((unused)),
		       sasl_server_params_t *sparams,
		       const char *userstr,
		       const char *pass,
		       unsigned passlen __attribute__((unused)),
		       const char *oldpass __attribute__((unused)),
		       unsigned oldpasslen __attribute__((unused)),
		       unsigned flags)
{
    int r;
    char *user = NULL;
    char *realm = NULL;
    sasl_secret_t *sec = NULL;
    
    /* Do we have database support? */
    /* Note that we can use a NULL sasl_conn_t because our
     * sasl_utils_t is "blessed" with the global callbacks */
    if(_sasl_check_db(sparams->utils, NULL) != SASL_OK) {
	SETERROR(sparams->utils, "No database support");
	return SASL_FAIL;
    }
    
    r = _plug_parseuser(sparams->utils, &user, &realm, sparams->user_realm,
			sparams->serverFQDN, userstr);
    if (r) {
	sparams->utils->seterror(sparams->utils->conn, 0, 
				 "Error parsing user");
	return r;
    }
    
    if ((flags & SASL_SET_DISABLE) || pass == NULL) {
	sec = NULL;
    } else {
	context_t *text;
	BIGNUM N;
	BIGNUM g;
	BIGNUM v;
	char *salt;
	int saltlen;
	char *utf8mda = NULL;
	int utf8mdalen;
	char *mpiv = NULL;
	int mpivlen;    
	char *osSalt = NULL;
	int osSaltlen;
	const char *buffer = NULL;
	int bufferlen;
	
	text = sparams->utils->malloc(sizeof(context_t));
	if (text==NULL) {
	    MEMERROR(sparams->utils);
	    return SASL_NOMEM;
	}
	
	memset(text, 0, sizeof(context_t));
	
	text->utils = sparams->utils;
	text->md = EVP_get_digestbyname(server_mda->evp_name);
	
	r = generate_N_and_g(&N, &g);
	if (r) {
	    sparams->utils->seterror(sparams->utils->conn, 0, 
				     "Error calculating N and g");
	    goto end;
	}
	
	r = CalculateV(text, &N, &g, user, pass, passlen, &v, &salt, &saltlen);
	if (r) {
	    sparams->utils->seterror(sparams->utils->conn, 0, 
				     "Error calculating v");
	    goto end;
	}
	
	/* The secret data is stored as suggested in RFC 2945:
	 *
	 *  mda  - utf8
	 *  v    - mpi
	 *  salt - os 
	 */
	
	r = MakeUTF8(sparams->utils, (char*) server_mda->name,
		     &utf8mda, &utf8mdalen);
	if (r) {
	    sparams->utils->seterror(sparams->utils->conn, 0, 
				     "Error turning 'mda' into 'utf8' string");
	    goto end;
	}
	
	r = MakeMPI(sparams->utils, &v, &mpiv, &mpivlen);
	if (r) {
	    sparams->utils->seterror(sparams->utils->conn, 0, 
				     "Error turning 'v' into 'mpi' string");
	    goto end;
	}
	
	r = MakeOS(sparams->utils, salt, saltlen, &osSalt, &osSaltlen);
	if (r) {
	    sparams->utils->seterror(sparams->utils->conn, 0, 
				     "Error turning salt into 'os' string");
	    goto end;
	}
	
	r = MakeBuffer(text, utf8mda, utf8mdalen, mpiv, mpivlen,
		       osSalt, osSaltlen, NULL, 0, &buffer, &bufferlen);
	
	if (r) {
	    sparams->utils->seterror(sparams->utils->conn, 0, 
				     "Error making buffer for secret");
	    goto end;
	}
	
	/* Put 'buffer' into sasl_secret_t */
	sec = sparams->utils->malloc(sizeof(sasl_secret_t)+bufferlen+1);
	if (!sec) {
	    r = SASL_NOMEM;
	    goto end;
	}
	memcpy(sec->data, buffer, bufferlen);
	sec->len = bufferlen;    
	
	/* Clean everything up */
      end:
	if (mpiv)   sparams->utils->free(mpiv);
	if (osSalt) sparams->utils->free(osSalt);
	if (buffer) sparams->utils->free((void *) buffer);
	BN_clear_free(&N);
	BN_clear_free(&g);
	BN_clear_free(&v);
	sparams->utils->free(text);
	
	if (r) return r;
    }
    
    /* do the store */
    r = (*_sasldb_putdata)(sparams->utils, sparams->utils->conn,
			   user, realm, "cmusaslsecretSRP",
			   (sec ? sec->data : NULL), (sec ? sec->len : 0));
    
    if (r) {
	sparams->utils->seterror(sparams->utils->conn, 0, 
				 "Error putting secret");
	goto cleanup;
    }
    
    sparams->utils->log(NULL, SASL_LOG_DEBUG, "Setpass for SRP successful\n");
    
  cleanup:
    
    if (user) 	sparams->utils->free(user);
    if (realm) 	sparams->utils->free(realm);
    if (sec)    sparams->utils->free(sec);
    
    return r;
}
#endif /* DO_SRP_SETPASS */

static int srp_mech_avail(void *glob_context __attribute__((unused)),
			  sasl_server_params_t *sparams,
			  void **conn_context __attribute__((unused))) 
{
    /* Do we have access to the selected MDA? */
    if (!server_mda || !server_mda->enabled) {
	SETERROR(sparams->utils,
		 "SRP unavailable due to selected MDA unavailable");
	return SASL_NOMECH;
    }
    
    return SASL_OK;
}

static sasl_server_plug_t srp_server_plugins[] = 
{
    {
	"SRP",				/* mech_name */
	0,				/* max_ssf */
	SASL_SEC_NOPLAINTEXT
	| SASL_SEC_NOANONYMOUS
	| SASL_SEC_NOACTIVE
	| SASL_SEC_NODICTIONARY
	| SASL_SEC_FORWARD_SECRECY
	| SASL_SEC_MUTUAL_AUTH,		/* security_flags */
	SASL_FEAT_WANT_CLIENT_FIRST,	/* features */
	NULL,				/* glob_context */
	&srp_server_mech_new,		/* mech_new */
	&srp_server_mech_step,		/* mech_step */
	&srp_common_mech_dispose,	/* mech_dispose */
	&srp_common_mech_free,		/* mech_free */
#if DO_SRP_SETPASS
	&srp_setpass,			/* setpass */
#else
	NULL,
#endif
	NULL,				/* user_query */
	NULL,				/* idle */
	&srp_mech_avail,		/* mech avail */
	NULL				/* spare */
    }
};

int srp_server_plug_init(const sasl_utils_t *utils,
			 int maxversion,
			 int *out_version,
			 const sasl_server_plug_t **pluglist,
			 int *plugcount,
			 const char *plugname __attribute__((unused)))
{
    const char *mda;
    unsigned int len;
    layer_option_t *opts;
    
    if (maxversion < SASL_SERVER_PLUG_VERSION) {
	SETERROR(utils, "SRP version mismatch");
	return SASL_BADVERS;
    }
    
    utils->getopt(utils->getopt_context, "SRP", "srp_mda", &mda, &len);
    if (!mda) mda = DEFAULT_MDA;
    
    /* Add all digests and ciphers */
    OpenSSL_add_all_algorithms();
    
    /* See which digests we have available and set max_ssf accordingly */
    opts = digest_options;
    while (opts->name) {
	if (EVP_get_digestbyname(opts->evp_name)) {
	    opts->enabled = 1;
	    
	    srp_server_plugins[0].max_ssf = opts->ssf;
	}
	
	/* Locate the server MDA */
	if (!strcasecmp(opts->name, mda) || !strcasecmp(opts->evp_name, mda)) {
	    server_mda = opts;
	}
	
	opts++;
    }
    
    /* See which ciphers we have available and set max_ssf accordingly */
    opts = cipher_options;
    while (opts->name) {
	if (EVP_get_cipherbyname(opts->evp_name)) {
	    opts->enabled = 1;
	    
	    if (opts->ssf > srp_server_plugins[0].max_ssf) {
		srp_server_plugins[0].max_ssf = opts->ssf;
	    }
	}
	
	opts++;
    }
    
    *out_version = SASL_SERVER_PLUG_VERSION;
    *pluglist = srp_server_plugins;
    *plugcount = 1;
    
    return SASL_OK;
}

/*****************************  Client Section  *****************************/

/* Check to see if N,g is in the recommended list */
static int check_N_and_g(const sasl_utils_t *utils, BIGNUM *N, BIGNUM *g)
{
    char *N_prime;
    unsigned long g_prime;
    unsigned i;
    int r = SASL_FAIL;
    
    N_prime = BN_bn2hex(N);
    g_prime = BN_get_word(g);
    
    for (i = 0; i < NUM_Ng; i++) {
	if (!strcasecmp(N_prime, Ng_tab[i].N) && (g_prime == Ng_tab[i].g)) {
	    r = SASL_OK;
	    break;
	}
    }
    
    if (N_prime) utils->free(N_prime);
    
    return r;
}

/* Calculate shared context key K
 *
 * User:  x = H(s, password)
 * User:  S = (B - g^x) ^ (a + ux) % N
 *                  
 * User:  K = Hi(S)
 *
 */
static int CalculateK_client(context_t *text,
			     char *salt,
			     int saltlen,
			     char *user,
			     char *pass,
			     int passlen,
			     char **key,
			     int *keylen)
{
    int r;
    unsigned char hash[EVP_MAX_MD_SIZE];
    BIGNUM x;
    BIGNUM u;
    BIGNUM aux;
    BIGNUM gx;
    BIGNUM base;
    BIGNUM S;
    BN_CTX *ctx = BN_CTX_new();
    
    /* u is first 32 bits of B hashed; MSB first */
    r = HashBigInt(text, &text->B, hash, NULL);
    if (r) goto err;
    BN_init(&u);
    BN_bin2bn(hash, 4, &u);
    
    /* per Tom Wu: make sure u != 0 */
    if (BN_is_zero(&u)) {
	SETERROR(text->utils, "SRP: Illegal value for 'u'\n");
	r = SASL_BADPROT;
	goto err;
    }
    
    r = CalculateX(text, salt, saltlen, user, pass, passlen, &x);
    if (r) return r;
    
    /* a + ux */
    BN_init(&aux);
    BN_mul(&aux, &u, &x, ctx);
    BN_add(&aux, &aux, &text->a);
    
    /* gx = g^x % N */
    BN_init(&gx);
    BN_mod_exp(&gx, &text->g, &x, &text->N, ctx);
    
    /* base = (B - g^x) % N */
    BN_init(&base);
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
    BN_mod_sub(&base, &text->B, &gx, &text->N, ctx);
#else
    BN_sub(&base, &text->B, &gx);
    BN_mod(&base, &base, &text->N, ctx);
    if (BigIntCmpWord(&base, 0) < 0) {
	BN_add(&base, &base, &text->N);
    }
#endif
    
    /* S = base^aux % N */
    BN_init(&S);
    BN_mod_exp(&S, &base, &aux, &text->N, ctx);
    
    /* K = Hi(S) */
    r = HashInterleaveBigInt(text, &S, key, keylen);
    if (r) goto err;
    
    r = SASL_OK;
    
  err:
    BN_CTX_free(ctx);
    BN_clear_free(&x);
    BN_clear_free(&u);
    BN_clear_free(&aux);
    BN_clear_free(&gx);
    BN_clear_free(&base);
    BN_clear_free(&S);
    
    return r;
}

static int CreateClientOpts(sasl_client_params_t *params, 
			    srp_options_t *available, 
			    srp_options_t *out)
{
    layer_option_t *opt;
    sasl_ssf_t external;
    sasl_ssf_t limit;
    sasl_ssf_t musthave;
    
    /* zero out output */
    memset(out, 0, sizeof(srp_options_t));
    
    params->utils->log(NULL, SASL_LOG_DEBUG,
		       "Available MDA = %d\n",available->mda);
    
    /* mda */
    opt = FindBest(available->mda, 0, 256, digest_options);
    
    if (opt) {
	out->mda = opt->bit;
    }
    else {
	SETERROR(params->utils, "Can't find an acceptable SRP MDA\n");
	return SASL_BADAUTH;
    }
    
    /* get requested ssf */
    external = params->external_ssf;
    
    /* what do we _need_?  how much is too much? */
    if (params->props.max_ssf > external) {
	limit = params->props.max_ssf - external;
    } else {
	limit = 0;
    }
    if (params->props.min_ssf > external) {
	musthave = params->props.min_ssf - external;
    } else {
	musthave = 0;
    }
    
    /* we now go searching for an option that gives us at least "musthave"
       and at most "limit" bits of ssf. */
    params->utils->log(NULL, SASL_LOG_DEBUG,
		       "Available confidentiality = %d\n",
		       available->confidentiality);
    
    /* confidentiality */
    if (limit > 1) {
	
	opt = FindBest(available->confidentiality, musthave, limit,
		       cipher_options);
	
	if (opt) {
	    out->confidentiality = opt->bit;
	    /* we've already satisfied the SSF with the confidentiality
	     * layer, but we'll also use an integrity layer if we can
	     */
	    musthave = 0;
	}
	else if (musthave > 1) {
	    SETERROR(params->utils,
		     "Can't find an acceptable SRP confidentiality layer\n");
	    return SASL_TOOWEAK;
	}
    }
    
    params->utils->log(NULL, SASL_LOG_DEBUG,
		       "Available integrity = %d\n",available->integrity);
    
    /* integrity */
    if ((limit >= 1) && (musthave <= 1)) {
	
	opt = FindBest(available->integrity, musthave, limit,
		       digest_options);
	
	if (opt) {
	    out->integrity = opt->bit;
	    
	    /* if we set an integrity option we can set replay detection */
	    out->replay_detection = available->replay_detection;
	}
	else if (musthave > 0) {
	    SETERROR(params->utils,
		     "Can't find an acceptable SRP integrity layer\n");
	    return SASL_TOOWEAK;
	}
    }
    
    /* Check to see if we've satisfied all of the servers mandatory layers */
    params->utils->log(NULL, SASL_LOG_DEBUG,
		       "Mandatory layers = %d\n",available->mandatory);
    
    if ((!out->replay_detection &&
	 (available->mandatory & BIT_REPLAY_DETECTION)) ||
	(!out->integrity &&
	 (available->mandatory & BIT_INTEGRITY)) ||
	(!out->confidentiality &&
	 (available->mandatory & BIT_CONFIDENTIALITY))) {
	SETERROR(params->utils, "Mandatory SRP layer not supported\n");
	return SASL_BADAUTH;
    }
    
    /* Add maxbuffersize */
    out->maxbufsize = MAXBUFFERSIZE;
    if (params->props.maxbufsize && params->props.maxbufsize < out->maxbufsize)
	out->maxbufsize = params->props.maxbufsize;
    
    return SASL_OK;
}

static int srp_client_mech_new(void *glob_context __attribute__((unused)),
			       sasl_client_params_t *params,
			       void **conn_context)
{
    context_t *text;
    
    /* holds state are in */
    text = params->utils->malloc(sizeof(context_t));
    if (text == NULL) {
	MEMERROR( params->utils );
	return SASL_NOMEM;
    }
    
    memset(text, 0, sizeof(context_t));
    
    text->state = 1;
    text->utils = params->utils;

    *conn_context = text;
    
    return SASL_OK;
}

static int
srp_client_mech_step1(context_t *text,
		      sasl_client_params_t *params,
		      const char *serverin __attribute__((unused)),
		      unsigned serverinlen,
		      sasl_interact_t **prompt_need,
		      const char **clientout,
		      unsigned *clientoutlen,
		      sasl_out_params_t *oparams)
{
    const char *authid, *userid;
    int auth_result = SASL_OK;
    int pass_result = SASL_OK;
    int user_result = SASL_OK;
    int result;
    char *utf8U = NULL, *utf8I = NULL;
    int utf8Ulen, utf8Ilen;
    
    /* Expect: 
     *   absolutely nothing
     * 
     */
    if (serverinlen > 0) {
	SETERROR(params->utils, "Invalid input to first step of SRP\n");
	return SASL_BADPROT;
    }
    
    /* try to get the authid */
    if (oparams->authid==NULL) {
	auth_result = _plug_get_authid(params->utils, &authid, prompt_need);
	
	if ((auth_result != SASL_OK) && (auth_result != SASL_INTERACT))
	    return auth_result;
    }
    
    /* try to get the userid */
    if (oparams->user == NULL) {
	user_result = _plug_get_userid(params->utils, &userid, prompt_need);
	
	if ((user_result != SASL_OK) && (user_result != SASL_INTERACT))
	    return user_result;
    }
    
    /* try to get the password */
    if (text->password == NULL) {
	pass_result=_plug_get_password(params->utils, &text->password,
				       &text->free_password, prompt_need);
	    
	if ((pass_result != SASL_OK) && (pass_result != SASL_INTERACT))
	    return pass_result;
    }
    
    /* free prompts we got */
    if (prompt_need && *prompt_need) {
	params->utils->free(*prompt_need);
	*prompt_need = NULL;
    }
    
    /* if there are prompts not filled in */
    if ((auth_result == SASL_INTERACT) || (user_result == SASL_INTERACT) ||
	(pass_result == SASL_INTERACT)) {
	/* make the prompt list */
	result =
	    _plug_make_prompts(params->utils, prompt_need,
			       user_result == SASL_INTERACT ?
			       "Please enter your authorization name" : NULL,
			       NULL,
			       auth_result == SASL_INTERACT ?
			       "Please enter your authentication name" : NULL,
			       NULL,
			       pass_result == SASL_INTERACT ?
			       "Please enter your password" : NULL, NULL,
			       NULL, NULL, NULL,
			       NULL, NULL, NULL);
	if (result != SASL_OK) return result;
	    
	return SASL_INTERACT;
    }
    
    result = params->canon_user(params->utils->conn, authid, 0,
				SASL_CU_AUTHID, oparams);
    if (result != SASL_OK) return result;
    result = params->canon_user(params->utils->conn, userid, 0,
				SASL_CU_AUTHZID, oparams);
    if (result != SASL_OK) return result;
    
    /* Send out:
     *
     * U - authentication identity 
     * I - authorization identity
     *
     * { utf8(U) utf8(I) }
     */
    
    result = MakeUTF8(params->utils, (char *) oparams->authid,
		      &utf8U, &utf8Ulen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error making UTF8 string from authid ('U')\n");
	goto cleanup;
    }
    
    result = MakeUTF8(params->utils, (char *) oparams->user, &utf8I, &utf8Ilen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error making UTF8 string from userid ('I')\n");
	goto cleanup;
    }
    
    result = MakeBuffer(text, utf8U, utf8Ulen, utf8I, utf8Ilen, NULL, 0,
			NULL, 0, clientout, clientoutlen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR, "Error making output buffer\n");
	goto cleanup;
    }
    
    text->state = 2;

    result = SASL_CONTINUE;
    
  cleanup:
    if (utf8U) params->utils->free(utf8U);
    if (utf8I) params->utils->free(utf8I);
    
    return result;
}

static int
srp_client_mech_step2(context_t *text,
		      sasl_client_params_t *params,
		      const char *serverin,
		      unsigned serverinlen,
		      sasl_interact_t **prompt_need __attribute__((unused)),
		      const char **clientout,
		      unsigned *clientoutlen,
		      sasl_out_params_t *oparams)
{
    char *data;
    int datalen;
    int result;    
    char *mpiA = NULL, *utf8o = NULL;
    int mpiAlen, utf8olen;
    srp_options_t server_opts;
    
    /* expect:
     *  { mpi(N) mpi(g) os(s) utf8(L) }
     *
     */
    
    result = UnBuffer(params->utils, (char *) serverin, serverinlen,
		      &data, &datalen);
    if (result) return result;
    
    result = GetMPI(params->utils, (unsigned char *)data, datalen, &text->N,
		    &data, &datalen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error getting MPI string for 'N'\n");
	goto cleanup;
    }
    
    result = GetMPI(params->utils, (unsigned char *) data, datalen, &text->g,
		    &data, &datalen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error getting MPI string for 'g'\n");
	goto cleanup;
    }
    
    /* Check N and g to see if they are one of the recommended pairs */
    result = check_N_and_g(params->utils, &text->N, &text->g);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Values of 'N' and 'g' are not recommended\n");
	goto cleanup;
    }
    
    result = GetOS(params->utils, (unsigned char *) data, datalen,
		   &text->salt, &text->saltlen, &data, &datalen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error getting OS string for 's'\n");
	goto cleanup;
    }
    
    result = GetUTF8(params->utils, data, datalen, &text->server_options,
		     &data, &datalen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error getting UTF8 string for 'L'");
	goto cleanup;
    }
    params->utils->log(NULL, SASL_LOG_DEBUG, "L: '%s'", text->server_options);
    
    if (datalen != 0) {
	params->utils->log(NULL, SASL_LOG_ERR, "Extra data parsing buffer\n");
	goto cleanup;
    }
    
    /* parse server options */
    memset(&server_opts, 0, sizeof(srp_options_t));
    result = ParseOptions(params->utils, text->server_options, &server_opts, 0);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error parsing SRP server options\n");
	goto cleanup;
    }
    
    /* create an 'a' */
    GetRandBigInt(&text->a);
    
    /* Per [SRP]: make sure a > log[g](N) -- g is always 2 */
    BN_add_word(&text->a, BN_num_bits(&text->N));
    
    /* calculate 'A' 
     *
     * A = g^a % N 
     */
    {
	BN_CTX *ctx = BN_CTX_new();
	BN_init(&text->A);
	BN_mod_exp(&text->A, &text->g, &text->a, &text->N, ctx);
	BN_CTX_free(ctx);
    }
    
    /* make o */
    result = CreateClientOpts(params, &server_opts, &text->client_opts);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error creating client options\n");
	goto cleanup;
    }
    
    result = OptionsToString(params->utils, &text->client_opts,
			     &text->client_options);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error converting client options to an option string\n");
	goto cleanup;
    }
    
    result = SetOptions(&text->client_opts, text, params->utils, oparams);
    if (result) {
	params->utils->seterror(params->utils->conn, 0, 
				"Error setting options");
	goto cleanup;
    }
    
    /* Send out:
     *
     * A - client's public key
     * o - client option list
     *
     * { mpi(A) utf8(o) }
     */
    
    result = MakeMPI(params->utils, &text->A, &mpiA, &mpiAlen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error making MPI string from A\n");
	goto cleanup;
    }
    
    result = MakeUTF8(params->utils, text->client_options, &utf8o, &utf8olen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error making UTF8 string from client options ('o')\n");
	goto cleanup;
    }
    
    result = MakeBuffer(text, mpiA, mpiAlen, utf8o, utf8olen,
		   NULL, 0, NULL, 0, clientout, clientoutlen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR, "Error making output buffer\n");
	goto cleanup;
    }
    
    text->state = 3;

    result = SASL_CONTINUE;
    
  cleanup:
    if (mpiA) params->utils->free(mpiA);
    if (utf8o) params->utils->free(utf8o);
    
    return result;
}

static int
srp_client_mech_step3(context_t *text,
		      sasl_client_params_t *params,
		      const char *serverin,
		      unsigned serverinlen,
		      sasl_interact_t **prompt_need __attribute__((unused)),
		      const char **clientout,
		      unsigned *clientoutlen,
		      sasl_out_params_t *oparams)
{
    char *data;
    int datalen;
    int result;    
    char *osM1 = NULL;
    int osM1len;
    
    /* Expect:
     *  { mpi(B) }
     *
     */
    result = UnBuffer(params->utils, (char *) serverin, serverinlen,
		 &data, &datalen);
    if (result) return result;
    
    result = GetMPI(params->utils, (unsigned char *) data, datalen, &text->B,
	       &data, &datalen);
    if (result) return result;
    
    /* Per [SRP]: reject B <= 0, B >= N */
    if (BigIntCmpWord(&text->B, 0) <= 0 || BN_cmp(&text->B, &text->N) >= 0) {
	SETERROR(params->utils, "Illegal value for 'B'\n");
	return SASL_BADPROT;
    }
    
    if (datalen != 0) {
	SETERROR(params->utils, "Extra data parsing buffer\n");
	return SASL_BADPROT;
    }
    
    /* Calculate shared context key K
     *
     */
    result = CalculateK_client(text, text->salt, text->saltlen,
			       (char *) oparams->authid, 
			       text->password->data, text->password->len,
			       &text->K, &text->Klen);
    if (result) return result;
    
    /* Now calculate M1 (client evidence)
     *
     */
    result = CalculateM1(text, &text->N, &text->g, (char *) oparams->authid,
			 text->salt, text->saltlen,
			 text->server_options, &text->A, &text->B,
			 text->K, text->Klen,
			 &text->M1, &text->M1len);
    if (result) return result;
    
    /* Send:
     *
     * { os(M1) }
     */
    
    result = MakeOS(params->utils, text->M1, text->M1len, &osM1, &osM1len);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error creating OS string for M1\n");
	goto cleanup;
    }
    
    result = MakeBuffer(text, osM1, osM1len, NULL, 0, NULL, 0, NULL, 0,
		   clientout, clientoutlen);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error creating buffer in step 3\n");
	goto cleanup;
    }
    
    /* if we have a confidentiality layer we're done */
    if (text->enabled & BIT_CONFIDENTIALITY) {
	/* set oparams */
	oparams->doneflag = 1;
	oparams->param_version = 0;

	result = SASL_OK;
	goto cleanup;
    }
    
    text->state = 4;

    result = SASL_CONTINUE;

  cleanup:
    if (osM1) params->utils->free(osM1);
    
    return result;
}

static int
srp_client_mech_step4(context_t *text,
		      sasl_client_params_t *params,
		      const char *serverin,
		      unsigned serverinlen,
		      sasl_interact_t **prompt_need __attribute__((unused)),
		      const char **clientout __attribute__((unused)),
		      unsigned *clientoutlen __attribute__((unused)),
		      sasl_out_params_t *oparams)
{
    char *data;
    int datalen;
    int result;    
    char *serverM2 = NULL;
    int serverM2len;
    int i;
    char *myM2 = NULL;
    int myM2len;
    
    /* Input:
     *
     * M2 - server evidence
     *
     *   { os(M2) }
     */
    result = UnBuffer(params->utils, (char *) serverin, serverinlen,
		      &data, &datalen);
    if (result) return result;
    
    result = GetOS(params->utils, (unsigned char *)data, datalen,
		   &serverM2, &serverM2len, &data, &datalen);
    if (result) return result;
    
    if (datalen != 0) {
	SETERROR(params->utils, "Extra data parsing buffer\n");
	result = SASL_BADPROT;
	goto cleanup;
    }
    
    /* calculate our own M2 */
    result = CalculateM2(text, &text->A, (char *) oparams->authid,
			 (char *) oparams->user,
			 text->client_options, text->M1, text->M1len,
			 text->K, text->Klen, &myM2, &myM2len);
    if (result) {
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Error calculating our own M2 (server evidence)\n");
	goto cleanup;
    }
    
    /* compare to see if is server spoof */
    if (myM2len != serverM2len) {
	SETERROR(params->utils, "SRP Server M2 length wrong\n");
	result = SASL_BADSERV;
	goto cleanup;
    }
    
    
    for (i = 0; i < myM2len; i++) {
	if (serverM2[i] != myM2[i]) {
	    SETERROR(params->utils,
		     "SRP Server spoof detected. M2 incorrect\n");
	    result = SASL_BADSERV;
	    goto cleanup;
	}
    }
    
    /*
     * Send out: nothing
     */
    
    /* set oparams */
    oparams->doneflag = 1;
    oparams->param_version = 0;

    result = SASL_OK;
    
  cleanup:
    if (serverM2) params->utils->free(serverM2);
    if (myM2) params->utils->free(myM2);
    
    return result;
}

static int srp_client_mech_step(void *conn_context,
				sasl_client_params_t *params,
				const char *serverin,
				unsigned serverinlen,
				sasl_interact_t **prompt_need,
				const char **clientout,
				unsigned *clientoutlen,
				sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) conn_context;
    
    params->utils->log(NULL, SASL_LOG_DEBUG,
		       "SRP client step %d\n", text->state);
    
    *clientout = NULL;
    *clientoutlen = 0;
    
    switch (text->state) {

    case 1:
	return srp_client_mech_step1(text, params, serverin, serverinlen, 
				     prompt_need, clientout, clientoutlen,
				     oparams);

    case 2:
	return srp_client_mech_step2(text, params, serverin, serverinlen, 
				     prompt_need, clientout, clientoutlen,
				     oparams);

    case 3:
	return srp_client_mech_step3(text, params, serverin, serverinlen, 
				     prompt_need, clientout, clientoutlen,
				     oparams);

    case 4:
	return srp_client_mech_step4(text, params, serverin, serverinlen, 
				     prompt_need, clientout, clientoutlen,
				     oparams);

    default:
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Invalid SRP client step %d\n", text->state);
	return SASL_FAIL;
    }
    
    return SASL_FAIL; /* should never get here */
}


static sasl_client_plug_t srp_client_plugins[] = 
{
    {
	"SRP",				/* mech_name */
	0,				/* max_ssf */
	SASL_SEC_NOPLAINTEXT
	| SASL_SEC_NOANONYMOUS
	| SASL_SEC_NOACTIVE
	| SASL_SEC_NODICTIONARY
	| SASL_SEC_FORWARD_SECRECY
	| SASL_SEC_MUTUAL_AUTH,		/* security_flags */
	SASL_FEAT_WANT_CLIENT_FIRST,	/* features */
	NULL,				/* required_prompts */
	NULL,				/* glob_context */
	&srp_client_mech_new,		/* mech_new */
	&srp_client_mech_step,		/* mech_step */
	&srp_common_mech_dispose,	/* mech_dispose */
	&srp_common_mech_free,		/* mech_free */
	NULL,				/* idle */
	NULL,				/* spare */
	NULL				/* spare */
    }
};

int srp_client_plug_init(const sasl_utils_t *utils __attribute__((unused)),
			 int maxversion,
			 int *out_version,
			 const sasl_client_plug_t **pluglist,
			 int *plugcount,
			 const char *plugname __attribute__((unused)))
{
    layer_option_t *opts;
    
    if (maxversion < SASL_CLIENT_PLUG_VERSION) {
	SETERROR(utils, "SRP version mismatch");
	return SASL_BADVERS;
    }
    
    /* Add all digests and ciphers */
    OpenSSL_add_all_algorithms();
    
    /* See which digests we have available and set max_ssf accordingly */
    opts = digest_options;
    while (opts->name) {
	if (EVP_get_digestbyname(opts->evp_name)) {
	    opts->enabled = 1;
	    
	    srp_client_plugins[0].max_ssf = opts->ssf;
	}
	
	opts++;
    }
    
    /* See which ciphers we have available and set max_ssf accordingly */
    opts = cipher_options;
    while (opts->name) {
	if (EVP_get_cipherbyname(opts->evp_name)) {
	    opts->enabled = 1;
	    
	    if (opts->ssf > srp_client_plugins[0].max_ssf) {
		srp_client_plugins[0].max_ssf = opts->ssf;
	    }
	}
	
	opts++;
    }
    
    *out_version = SASL_CLIENT_PLUG_VERSION;
    *pluglist = srp_client_plugins;
    *plugcount=1;
    
    return SASL_OK;
}
