/* DIGEST-MD5 SASL plugin
 * Rob Siemborski
 * Tim Martin
 * Alexey Melnikov 
 * $Id: digestmd5.c,v 1.5 2003/02/25 00:00:30 snsimon Exp $
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
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#ifndef macintosh
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <ctype.h>

/* DES support */
#ifdef WITH_DES
# ifdef WITH_SSL_DES
#  include <openssl/des.h>
# else /* system DES library */
#  include <des.h>
# endif
#endif /* WITH_DES */

#ifdef WIN32
# include <winsock.h>
#else /* Unix */
# include <netinet/in.h>
#endif /* WIN32 */

#include <sasl.h>
#include <saslplug.h>

#include "plugin_common.h"

#ifdef WIN32
/* This must be after sasl.h, saslutil.h */
#include "saslDIGESTMD5.h"
#else /* Unix */
extern int      strcasecmp(const char *s1, const char *s2);
#endif /* end WIN32 */

#ifdef macintosh
#include <sasl_md5_plugin_decl.h>
#endif

/* external definitions */

#ifdef sun
/* gotta define gethostname ourselves on suns */
extern int      gethostname(char *, int);
#endif

#define bool int

#ifndef TRUE
#define TRUE  (1)
#define FALSE (0)
#endif

/*****************************  Common Section  *****************************/

static const char plugin_id[] = "$Id: digestmd5.c,v 1.5 2003/02/25 00:00:30 snsimon Exp $";

/* Definitions */
#define NONCE_SIZE (32)		/* arbitrary */

/* Layer Flags */
#define DIGEST_NOLAYER    (1)
#define DIGEST_INTEGRITY  (2)
#define DIGEST_PRIVACY    (4)

/* defines */
#define HASHLEN 16
typedef unsigned char HASH[HASHLEN + 1];
#define HASHHEXLEN 32
typedef unsigned char HASHHEX[HASHHEXLEN + 1];

#define MAC_SIZE 10
#define MAC_OFFS 2

const char *SEALING_CLIENT_SERVER="Digest H(A1) to client-to-server sealing key magic constant";
const char *SEALING_SERVER_CLIENT="Digest H(A1) to server-to-client sealing key magic constant";

const char *SIGNING_CLIENT_SERVER="Digest session key to client-to-server signing key magic constant";
const char *SIGNING_SERVER_CLIENT="Digest session key to server-to-client signing key magic constant";

#define HT	(9)
#define CR	(13)
#define LF	(10)
#define SP	(32)
#define DEL	(127)

/* function definitions for cipher encode/decode */
typedef int cipher_function_t(void *,
			      const char *,
			      unsigned,
			      unsigned char[],
			      char *,
			      unsigned *);

typedef int cipher_init_t(void *, char [16], char [16]);
typedef void cipher_free_t(void *);

enum Context_type { SERVER = 0, CLIENT = 1 };

#ifdef WITH_RC4
typedef struct rc4_context_s rc4_context_t;
#endif

/* context that stores info used for fast reauth */
typedef struct global_context {
    /* common stuff */
    char *authid;
    char *realm;
    unsigned char *nonce;
    unsigned int nonce_count;
    unsigned char *cnonce;

    /* client stuff */
    char *serverFQDN;
    char *qop;
    struct digest_cipher *bestcipher;
    unsigned int server_maxbuf;
} global_context_t;

/* context that stores info */
typedef struct context {
    int state;			/* state in the authentication we are in */
    enum Context_type i_am;	/* are we the client or server? */
    
    global_context_t *global;

    char *response_value;
    
    unsigned int seqnum;
    unsigned int rec_seqnum;	/* for checking integrity */
    
    HASH Ki_send;
    HASH Ki_receive;
    
    HASH HA1;		/* Kcc or Kcs */
    
    /* copy of utils from the params structures */
    const sasl_utils_t *utils;
    
    /* For general use */
    char *out_buf;
    unsigned out_buf_len;
    
    /* for encoding/decoding */
    buffer_info_t *enc_in_buf;
    char *encode_buf, *decode_buf, *decode_once_buf;
    unsigned encode_buf_len, decode_buf_len, decode_once_buf_len;
    char *decode_tmp_buf;
    unsigned decode_tmp_buf_len;
    char *MAC_buf;
    unsigned MAC_buf_len;
    
    char *buffer;
    char sizebuf[4];
    int cursize;
    int size;
    int needsize;
    
    /* Server MaxBuf for Client or Client MaxBuf For Server */
    unsigned int maxbuf;
    
    /* if privacy mode is used use these functions for encode and decode */
    cipher_function_t *cipher_enc;
    cipher_function_t *cipher_dec;
    cipher_init_t *cipher_init;
    cipher_free_t *cipher_free;
    
#ifdef WITH_DES
    des_key_schedule keysched_enc;   /* key schedule for des initialization */
    des_cblock ivec_enc;	     /* initial vector for encoding */
    des_key_schedule keysched_dec;   /* key schedule for des initialization */
    des_cblock ivec_dec;	     /* init vec for decoding */
    
    des_key_schedule keysched_enc2;  /* key schedule for 3des initialization */
    des_key_schedule keysched_dec2;  /* key schedule for 3des initialization */
#endif
    
#ifdef WITH_RC4
    rc4_context_t *rc4_enc_context;
    rc4_context_t *rc4_dec_context;
#endif /* WITH_RC4 */
} context_t;

struct digest_cipher {
    char *name;
    sasl_ssf_t ssf;
    int n; /* bits to make privacy key */
    int flag; /* a bitmask to make things easier for us */
    
    cipher_function_t *cipher_enc;
    cipher_function_t *cipher_dec;
    cipher_init_t *cipher_init;
    cipher_free_t *cipher_free;
};

static const unsigned char *COLON = ":";

static void CvtHex(HASH Bin, HASHHEX Hex)
{
    unsigned short  i;
    unsigned char   j;
    
    for (i = 0; i < HASHLEN; i++) {
	j = (Bin[i] >> 4) & 0xf;
	if (j <= 9)
	    Hex[i * 2] = (j + '0');
	else
	    Hex[i * 2] = (j + 'a' - 10);
	j = Bin[i] & 0xf;
	if (j <= 9)
	    Hex[i * 2 + 1] = (j + '0');
	else
	    Hex[i * 2 + 1] = (j + 'a' - 10);
    }
    Hex[HASHHEXLEN] = '\0';
}

/*
 * calculate request-digest/response-digest as per HTTP Digest spec
 */
void
DigestCalcResponse(const sasl_utils_t * utils,
		   HASHHEX HA1,	/* H(A1) */
		   unsigned char *pszNonce,	/* nonce from server */
		   unsigned int pszNonceCount,	/* 8 hex digits */
		   unsigned char *pszCNonce,	/* client nonce */
		   unsigned char *pszQop,	/* qop-value: "", "auth",
						 * "auth-int" */
		   unsigned char *pszDigestUri,	/* requested URL */
		   unsigned char *pszMethod,
		   HASHHEX HEntity,	/* H(entity body) if qop="auth-int" */
		   HASHHEX Response	/* request-digest or response-digest */
    )
{
    MD5_CTX         Md5Ctx;
    HASH            HA2;
    HASH            RespHash;
    HASHHEX         HA2Hex;
    char ncvalue[10];
    
    /* calculate H(A2) */
    utils->MD5Init(&Md5Ctx);
    
    if (pszMethod != NULL) {
	utils->MD5Update(&Md5Ctx, pszMethod, strlen((char *) pszMethod));
    }
    utils->MD5Update(&Md5Ctx, (unsigned char *) COLON, 1);
    
    /* utils->MD5Update(&Md5Ctx, (unsigned char *) "AUTHENTICATE:", 13); */
    utils->MD5Update(&Md5Ctx, pszDigestUri, strlen((char *) pszDigestUri));
    if (strcasecmp((char *) pszQop, "auth") != 0) {
	/* append ":00000000000000000000000000000000" */
	utils->MD5Update(&Md5Ctx, COLON, 1);
	utils->MD5Update(&Md5Ctx, HEntity, HASHHEXLEN);
    }
    utils->MD5Final(HA2, &Md5Ctx);
    CvtHex(HA2, HA2Hex);
    
    /* calculate response */
    utils->MD5Init(&Md5Ctx);
    utils->MD5Update(&Md5Ctx, HA1, HASHHEXLEN);
    utils->MD5Update(&Md5Ctx, COLON, 1);
    utils->MD5Update(&Md5Ctx, pszNonce, strlen((char *) pszNonce));
    utils->MD5Update(&Md5Ctx, COLON, 1);
    if (*pszQop) {
	sprintf(ncvalue, "%08x", pszNonceCount);
	utils->MD5Update(&Md5Ctx, ncvalue, strlen(ncvalue));
	utils->MD5Update(&Md5Ctx, COLON, 1);
	utils->MD5Update(&Md5Ctx, pszCNonce, strlen((char *) pszCNonce));
	utils->MD5Update(&Md5Ctx, COLON, 1);
	utils->MD5Update(&Md5Ctx, pszQop, strlen((char *) pszQop));
	utils->MD5Update(&Md5Ctx, COLON, 1);
    }
    utils->MD5Update(&Md5Ctx, HA2Hex, HASHHEXLEN);
    utils->MD5Final(RespHash, &Md5Ctx);
    CvtHex(RespHash, Response);
}

static bool UTF8_In_8859_1(const unsigned char *base, int len)
{
    const unsigned char *scan, *end;
    
    end = base + len;
    for (scan = base; scan < end; ++scan) {
	if (*scan > 0xC3)
	    break;			/* abort if outside 8859-1 */
	if (*scan >= 0xC0 && *scan <= 0xC3) {
	    if (++scan == end || *scan < 0x80 || *scan > 0xBF)
		break;
	}
    }
    
    /* if scan >= end, then this is a 8859-1 string. */
    return (scan >= end);
}

/*
 * if the string is entirely in the 8859-1 subset of UTF-8, then translate to
 * 8859-1 prior to MD5
 */
void MD5_UTF8_8859_1(const sasl_utils_t * utils,
		     MD5_CTX * ctx,
		     bool In_ISO_8859_1,
		     const unsigned char *base,
		     int len)
{
    const unsigned char *scan, *end;
    unsigned char   cbuf;
    
    end = base + len;
    
    /* if we found a character outside 8859-1, don't alter string */
    if (!In_ISO_8859_1) {
	utils->MD5Update(ctx, base, len);
	return;
    }
    /* convert to 8859-1 prior to applying hash */
    do {
	for (scan = base; scan < end && *scan < 0xC0; ++scan);
	if (scan != base)
	    utils->MD5Update(ctx, base, scan - base);
	if (scan + 1 >= end)
	    break;
	cbuf = ((scan[0] & 0x3) << 6) | (scan[1] & 0x3f);
	utils->MD5Update(ctx, &cbuf, 1);
	base = scan + 2;
    }
    while (base < end);
}

static void DigestCalcSecret(const sasl_utils_t * utils,
			     unsigned char *pszUserName,
			     unsigned char *pszRealm,
			     unsigned char *Password,
			     int PasswordLen,
			     HASH HA1)
{
    bool            In_8859_1;
    
    MD5_CTX         Md5Ctx;
    
    /* Chris Newman clarified that the following text in DIGEST-MD5 spec
       is bogus: "if name and password are both in ISO 8859-1 charset"
       We shoud use code example instead */
    
    utils->MD5Init(&Md5Ctx);
    
    /* We have to convert UTF-8 to ISO-8859-1 if possible */
    In_8859_1 = UTF8_In_8859_1(pszUserName, strlen((char *) pszUserName));
    MD5_UTF8_8859_1(utils, &Md5Ctx, In_8859_1,
		    pszUserName, strlen((char *) pszUserName));
    
    utils->MD5Update(&Md5Ctx, COLON, 1);
    
    if (pszRealm != NULL && pszRealm[0] != '\0') {
	/* a NULL realm is equivalent to the empty string */
	utils->MD5Update(&Md5Ctx, pszRealm, strlen((char *) pszRealm));
    }      
    
    utils->MD5Update(&Md5Ctx, COLON, 1);
    
    /* We have to convert UTF-8 to ISO-8859-1 if possible */
    In_8859_1 = UTF8_In_8859_1(Password, PasswordLen);
    MD5_UTF8_8859_1(utils, &Md5Ctx, In_8859_1,
		    Password, PasswordLen);
    
    utils->MD5Final(HA1, &Md5Ctx);
}

static unsigned char *create_nonce(const sasl_utils_t * utils)
{
    unsigned char  *base64buf;
    int             base64len;
    
    char           *ret = (char *) utils->malloc(NONCE_SIZE);
    if (ret == NULL)
	return NULL;
    
    utils->rand(utils->rpool, (char *) ret, NONCE_SIZE);
    
    /* base 64 encode it so it has valid chars */
    base64len = (NONCE_SIZE * 4 / 3) + (NONCE_SIZE % 3 ? 4 : 0);
    
    base64buf = (unsigned char *) utils->malloc(base64len + 1);
    if (base64buf == NULL) {
	utils->seterror(utils->conn, 0, "Unable to allocate final buffer");
	utils->free(ret);
	return NULL;
    }
    
    /*
     * Returns SASL_OK on success, SASL_BUFOVER if result won't fit
     */
    if (utils->encode64(ret, NONCE_SIZE,
			(char *) base64buf, base64len, NULL) != SASL_OK) {
	utils->free(ret);
	utils->free(base64buf);
	return NULL;
    }
    utils->free(ret);
    
    return base64buf;
}

static int add_to_challenge(const sasl_utils_t *utils,
			    char **str, unsigned *buflen, unsigned *curlen,
			    char *name,
			    unsigned char *value,
			    bool need_quotes)
{
    int             namesize = strlen(name);
    int             valuesize = strlen((char *) value);
    int             ret;
    
    ret = _plug_buf_alloc(utils, str, buflen,
			  *curlen + 1 + namesize + 2 + valuesize + 2);
    if(ret != SASL_OK) return ret;
    
    *curlen = *curlen + 1 + namesize + 2 + valuesize + 2;
    
    strcat(*str, ",");
    strcat(*str, name);
    
    if (need_quotes) {
	strcat(*str, "=\"");
	strcat(*str, (char *) value);	/* XXX. What about quoting??? */
	strcat(*str, "\"");
    } else {
	strcat(*str, "=");
	strcat(*str, (char *) value);
    }
    
    return SASL_OK;
}

static char *skip_lws (char *s)
{
    if(!s) return NULL;
    
    /* skipping spaces: */
    while (s[0] == ' ' || s[0] == HT || s[0] == CR || s[0] == LF) {
	if (s[0]=='\0') break;
	s++;
    }  
    
    return s;
}

static char *skip_token (char *s, int caseinsensitive)
{
    if(!s) return NULL;
    
    while (s[0]>SP) {
	if (s[0]==DEL || s[0]=='(' || s[0]==')' || s[0]=='<' || s[0]=='>' ||
	    s[0]=='@' || s[0]==',' || s[0]==';' || s[0]==':' || s[0]=='\\' ||
	    s[0]=='\'' || s[0]=='/' || s[0]=='[' || s[0]==']' || s[0]== '?' ||
	    s[0]=='=' || s[0]== '{' || s[0]== '}') {
	    if (caseinsensitive == 1) {
		if (!isupper((unsigned char) s[0]))
		    break;
	    } else {
		break;
	    }
	}
	s++;
    }  
    return s;
}

/* NULL - error (unbalanced quotes), 
   otherwise pointer to the first character after value */
static char *unquote (char *qstr)
{
    char *endvalue;
    int   escaped = 0;
    char *outptr;
    
    if(!qstr) return NULL;
    
    if (qstr[0] == '"') {
	qstr++;
	outptr = qstr;
	
	for (endvalue = qstr; endvalue[0] != '\0'; endvalue++, outptr++) {
	    if (escaped) {
		outptr[0] = endvalue[0];
		escaped = 0;
	    }
	    else if (endvalue[0] == '\\') {
		escaped = 1;
		outptr--; /* Will be incremented at the end of the loop */
	    }
	    else if (endvalue[0] == '"') {
		break;
	    }      
	    else {
		outptr[0] = endvalue[0];      
	    }
	}
	
	if (endvalue[0] != '"') {
	    return NULL;
	}
	
	while (outptr <= endvalue) {
	    outptr[0] = '\0';
	    outptr++;
	}
	endvalue++;
    }
    else { /* not qouted value (token) */
	endvalue = skip_token(qstr,0);
    };
    
    return endvalue;  
} 

static void get_pair(char **in, char **name, char **value)
{
    char  *endpair;
    /* int    inQuotes; */
    char  *curp = *in;
    *name = NULL;
    *value = NULL;
    
    if (curp == NULL) return;
    if (curp[0] == '\0') return;
    
    /* skipping spaces: */
    curp = skip_lws(curp);
    
    *name = curp;
    
    curp = skip_token(curp,1);
    
    /* strip wierd chars */
    if (curp[0] != '=' && curp[0] != '\0') {
	*curp++ = '\0';
    };
    
    curp = skip_lws(curp);
    
    if (curp[0] != '=') { /* No '=' sign */ 
	*name = NULL;
	return;
    }
    
    curp[0] = '\0';
    curp++;
    
    curp = skip_lws(curp);  
    
    *value = (curp[0] == '"') ? curp+1 : curp;
    
    endpair = unquote (curp);
    if (endpair == NULL) { /* Unbalanced quotes */ 
	*name = NULL;
	return;
    }
    if (endpair[0] != ',') {
	if (endpair[0]!='\0') {
	    *endpair++ = '\0'; 
	}
    }
    
    endpair = skip_lws(endpair);
    
    /* syntax check: MUST be '\0' or ',' */  
    if (endpair[0] == ',') {
	endpair[0] = '\0';
	endpair++; /* skipping <,> */
    } else if (endpair[0] != '\0') { 
	*name = NULL;
	return;
    }
    
    *in = endpair;
}

#ifdef WITH_DES
/******************************
 *
 * 3DES functions
 *
 *****************************/


static int dec_3des(void *v,
		    const char *input,
		    unsigned inputlen,
		    unsigned char digest[16],
		    char *output,
		    unsigned *outputlen)
{
    context_t *text = (context_t *) v;
    int padding, p;
    
    des_ede2_cbc_encrypt((void *) input,
			 (void *) output,
			 inputlen,
			 text->keysched_dec,
			 text->keysched_dec2,
			 &text->ivec_dec,
			 DES_DECRYPT);
    
    /* now chop off the padding */
    padding = output[inputlen - 11];
    if (padding < 1 || padding > 8) {
	/* invalid padding length */
	return SASL_FAIL;
    }
    /* verify all padding is correct */
    for (p = 1; p <= padding; p++) {
	if (output[inputlen - 10 - p] != padding) {
	    return SASL_FAIL;
	}
    }
    
    /* chop off the padding */
    *outputlen = inputlen - padding - 10;
    
    /* copy in the HMAC to digest */
    memcpy(digest, output + inputlen - 10, 10);
    
    return SASL_OK;
}

static int enc_3des(void *v,
		    const char *input,
		    unsigned inputlen,
		    unsigned char digest[16],
		    char *output,
		    unsigned *outputlen)
{
    context_t *text = (context_t *) v;
    int len;
    int paddinglen;
    
    /* determine padding length */
    paddinglen = 8 - ((inputlen + 10) % 8);
    
    /* now construct the full stuff to be ciphered */
    memcpy(output, input, inputlen);                /* text */
    memset(output+inputlen, paddinglen, paddinglen);/* pad  */
    memcpy(output+inputlen+paddinglen, digest, 10); /* hmac */
    
    len=inputlen+paddinglen+10;
    
    des_ede2_cbc_encrypt((void *) output,
			 (void *) output,
			 len,
			 text->keysched_enc,
			 text->keysched_enc2,
			 &text->ivec_enc,
			 DES_ENCRYPT);
    
    *outputlen=len;
    
    return SASL_OK;
}

static int init_3des(void *v, 
		     char enckey[16],
		     char deckey[16])
    
    
    
{
    context_t *text = (context_t *) v;
    
    if(des_key_sched((des_cblock *) enckey, text->keysched_enc) < 0)
	return SASL_FAIL;
    if(des_key_sched((des_cblock *) deckey, text->keysched_dec) < 0)
	return SASL_FAIL;
    
    if(des_key_sched((des_cblock *) (enckey+7), text->keysched_enc2) < 0)
	return SASL_FAIL;
    if(des_key_sched((des_cblock *) (deckey+7), text->keysched_dec2) < 0)
	return SASL_FAIL;
    
    memcpy(text->ivec_enc, ((char *) enckey) + 8, 8);
    memcpy(text->ivec_dec, ((char *) deckey) + 8, 8);
    
    return SASL_OK;
}


/******************************
 *
 * DES functions
 *
 *****************************/

static int dec_des(void *v, 
		   const char *input,
		   unsigned inputlen,
		   unsigned char digest[16],
		   char *output,
		   unsigned *outputlen)
{
    context_t *text = (context_t *) v;
    int p,padding = 0;
    

    des_cbc_encrypt((void *) input,
		    (void *) output,
		    inputlen,
		    text->keysched_dec,
		    &text->ivec_dec,
		    DES_DECRYPT);

    /* Update the ivec (des_cbc_encrypt implementations tend to be broken in
       this way) */
    memcpy(text->ivec_dec, input + (inputlen - 8), 8);
    
    /* now chop off the padding */
    padding = output[inputlen - 11];
    if (padding < 1 || padding > 8) {
	/* invalid padding length */
	return SASL_FAIL;
    }
    /* verify all padding is correct */
    for (p = 1; p <= padding; p++) {
	if (output[inputlen - 10 - p] != padding) {
	    return SASL_FAIL;
	}
    }
    
    /* chop off the padding */
    *outputlen = inputlen - padding - 10;
    
    /* copy in the HMAC to digest */
    memcpy(digest, output + inputlen - 10, 10);
    
    return SASL_OK;
}

static int enc_des(void *v, 
		   const char *input,
		   unsigned inputlen,
		   unsigned char digest[16],
		   char *output,
		   unsigned *outputlen)
{
  context_t *text = (context_t *) v;
  int len;
  int paddinglen;
  
  /* determine padding length */
  paddinglen= 8 - ((inputlen+10)%8);

  /* now construct the full stuff to be ciphered */
  memcpy(output, input, inputlen);                /* text */
  memset(output+inputlen, paddinglen, paddinglen);/* pad  */
  memcpy(output+inputlen+paddinglen, digest, 10); /* hmac */

  len=inputlen+paddinglen+10;

  des_cbc_encrypt((void *) output,
		  (void *) output,
		  len,
		  text->keysched_enc,
		  &text->ivec_enc,
		  DES_ENCRYPT);

  /* Update the ivec (des_cbc_encrypt implementations tend to be broken in
     this way) */
  memcpy(text->ivec_enc, output + (len - 8), 8);

  *outputlen=len;

  return SASL_OK;
}

static int init_des(void *v,
		    char enckey[16],
		    char deckey[16])
{
    context_t *text = (context_t *) v;
    
    des_key_sched((des_cblock *) enckey, text->keysched_enc);
    memcpy(text->ivec_enc, ((char *) enckey) + 8, 8);
    
    des_key_sched((des_cblock *) deckey, text->keysched_dec);
    memcpy(text->ivec_dec, ((char *) deckey) + 8, 8);
    
    memcpy(text->ivec_enc, ((char *) enckey) + 8, 8);
    memcpy(text->ivec_dec, ((char *) deckey) + 8, 8);
    
    return SASL_OK;
}

#endif /* WITH_DES */

#ifdef WITH_RC4
/* quick generic implementation of RC4 */
struct rc4_context_s {
    unsigned char sbox[256];
    int i, j;
};

static void rc4_init(rc4_context_t *text,
		     const unsigned char *key,
		     unsigned keylen)
{
    int i, j;
    
    /* fill in linearly s0=0 s1=1... */
    for (i=0;i<256;i++)
	text->sbox[i]=i;
    
    j=0;
    for (i = 0; i < 256; i++) {
	unsigned char tmp;
	/* j = (j + Si + Ki) mod 256 */
	j = (j + text->sbox[i] + key[i % keylen]) % 256;
	
	/* swap Si and Sj */
	tmp = text->sbox[i];
	text->sbox[i] = text->sbox[j];
	text->sbox[j] = tmp;
    }
    
    /* counters initialized to 0 */
    text->i = 0;
    text->j = 0;
}

static void rc4_encrypt(rc4_context_t *text,
			const char *input,
			char *output,
			unsigned len)
{
    int tmp;
    int i = text->i;
    int j = text->j;
    int t;
    int K;
    const char *input_end = input + len;
    
    while (input < input_end) {
	i = (i + 1) % 256;
	
	j = (j + text->sbox[i]) % 256;
	
	/* swap Si and Sj */
	tmp = text->sbox[i];
	text->sbox[i] = text->sbox[j];
	text->sbox[j] = tmp;
	
	t = (text->sbox[i] + text->sbox[j]) % 256;
	
	K = text->sbox[t];
	
	/* byte K is Xor'ed with plaintext */
	*output++ = *input++ ^ K;
    }
    
    text->i = i;
    text->j = j;
}

static void rc4_decrypt(rc4_context_t *text,
			const char *input,
			char *output,
			unsigned len)
{
    int tmp;
    int i = text->i;
    int j = text->j;
    int t;
    int K;
    const char *input_end = input + len;
    
    while (input < input_end) {
	i = (i + 1) % 256;
	
	j = (j + text->sbox[i]) % 256;
	
	/* swap Si and Sj */
	tmp = text->sbox[i];
	text->sbox[i] = text->sbox[j];
	text->sbox[j] = tmp;
	
	t = (text->sbox[i] + text->sbox[j]) % 256;
	
	K = text->sbox[t];
	
	/* byte K is Xor'ed with plaintext */
	*output++ = *input++ ^ K;
    }
    
    text->i = i;
    text->j = j;
}

static void free_rc4(void *v) 
{
    context_t *text = (context_t *) v;
    
    /* allocate rc4 context structures */
    if(text->rc4_enc_context) text->utils->free(text->rc4_enc_context);
    if(text->rc4_dec_context) text->utils->free(text->rc4_dec_context);
}

static int init_rc4(void *v, 
		    char enckey[16],
		    char deckey[16])
{
    context_t *text = (context_t *) v;
    
    /* allocate rc4 context structures */
    text->rc4_enc_context=
	(rc4_context_t *) text->utils->malloc(sizeof(rc4_context_t));
    if (text->rc4_enc_context==NULL) return SASL_NOMEM;
    
    text->rc4_dec_context=
	(rc4_context_t *) text->utils->malloc(sizeof(rc4_context_t));
    if (text->rc4_dec_context==NULL) return SASL_NOMEM;
    
    /* initialize them */
    rc4_init(text->rc4_enc_context,(const unsigned char *) enckey, 16);
    rc4_init(text->rc4_dec_context,(const unsigned char *) deckey, 16);
    
    return SASL_OK;
}

static int dec_rc4(void *v,
		   const char *input,
		   unsigned inputlen,
		   unsigned char digest[16],
		   char *output,
		   unsigned *outputlen)
{
    context_t *text = (context_t *) v;
    
    /* decrypt the text part */
    rc4_decrypt(text->rc4_dec_context, input, output, inputlen-10);
    
    /* decrypt the HMAC part */
    rc4_decrypt(text->rc4_dec_context, 
		input+(inputlen-10), (char *) digest, 10);
    
    /* no padding so we just subtract the HMAC to get the text length */
    *outputlen = inputlen - 10;
    
    return SASL_OK;
}

static int enc_rc4(void *v,
		   const char *input,
		   unsigned inputlen,
		   unsigned char digest[16],
		   char *output,
		   unsigned *outputlen)
{
    context_t *text = (context_t *) v;
    
    /* pad is zero */
    *outputlen = inputlen+10;
    
    /* encrypt the text part */
    rc4_encrypt(text->rc4_enc_context, (const char *) input, output, inputlen);
    
    /* encrypt the HMAC part */
    rc4_encrypt(text->rc4_enc_context, (const char *) digest, 
		(output)+inputlen, 10);
    
    return SASL_OK;
}

#endif /* WITH_RC4 */

struct digest_cipher available_ciphers[] =
{
#ifdef WITH_RC4
    { "rc4-40", 40, 5, 0x01, &enc_rc4, &dec_rc4, &init_rc4, &free_rc4 },
    { "rc4-56", 56, 7, 0x02, &enc_rc4, &dec_rc4, &init_rc4, &free_rc4 },
    { "rc4", 128, 16, 0x04, &enc_rc4, &dec_rc4, &init_rc4, &free_rc4 },
#endif
#ifdef WITH_DES
    { "des", 55, 16, 0x08, &enc_des, &dec_des, &init_des, NULL },
    { "3des", 112, 16, 0x10, &enc_3des, &dec_3des, &init_3des, NULL },
#endif
    { NULL, 0, 0, 0, NULL, NULL, NULL, NULL }
};

static int create_layer_keys(context_t *text,
			     const sasl_utils_t *utils,
			     HASH key, int keylen,
			     char enckey[16], char deckey[16])
{
    MD5_CTX Md5Ctx;
    
    utils->MD5Init(&Md5Ctx);
    utils->MD5Update(&Md5Ctx, key, keylen);
    if (text->i_am == SERVER) {
	utils->MD5Update(&Md5Ctx, (const unsigned char *) SEALING_SERVER_CLIENT, 
			 strlen(SEALING_SERVER_CLIENT));
    } else {
	utils->MD5Update(&Md5Ctx, (const unsigned char *) SEALING_CLIENT_SERVER,
			 strlen(SEALING_CLIENT_SERVER));
    }
    utils->MD5Final((unsigned char *) enckey, &Md5Ctx);
    
    utils->MD5Init(&Md5Ctx);
    utils->MD5Update(&Md5Ctx, key, keylen);
    if (text->i_am != SERVER) {
	utils->MD5Update(&Md5Ctx, (const unsigned char *)SEALING_SERVER_CLIENT, 
			 strlen(SEALING_SERVER_CLIENT));
    } else {
	utils->MD5Update(&Md5Ctx, (const unsigned char *)SEALING_CLIENT_SERVER,
			 strlen(SEALING_CLIENT_SERVER));
    }
    utils->MD5Final((unsigned char *) deckey, &Md5Ctx);
    
    /* create integrity keys */
    /* sending */
    utils->MD5Init(&Md5Ctx);
    utils->MD5Update(&Md5Ctx, text->HA1, HASHLEN);
    if (text->i_am == SERVER) {
	utils->MD5Update(&Md5Ctx, (const unsigned char *)SIGNING_SERVER_CLIENT, 
			 strlen(SIGNING_SERVER_CLIENT));
    } else {
	utils->MD5Update(&Md5Ctx, (const unsigned char *)SIGNING_CLIENT_SERVER,
			 strlen(SIGNING_CLIENT_SERVER));
    }
    utils->MD5Final(text->Ki_send, &Md5Ctx);
    
    /* receiving */
    utils->MD5Init(&Md5Ctx);
    utils->MD5Update(&Md5Ctx, text->HA1, HASHLEN);
    if (text->i_am != SERVER) {
	utils->MD5Update(&Md5Ctx, (const unsigned char *)SIGNING_SERVER_CLIENT, 
			 strlen(SIGNING_SERVER_CLIENT));
    } else {
	utils->MD5Update(&Md5Ctx, (const unsigned char *)SIGNING_CLIENT_SERVER,
			 strlen(SIGNING_CLIENT_SERVER));
    }
    utils->MD5Final(text->Ki_receive, &Md5Ctx);
    
    return SASL_OK;
}

static const unsigned short version = 1;

/* len, CIPHER(Kc, {msg, pag, HMAC(ki, {SeqNum, msg})[0..9]}), x0001, SeqNum */

static int
digestmd5_privacy_encode(void *context,
			 const struct iovec *invec,
			 unsigned numiov,
			 const char **output,
			 unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int tmp;
    unsigned int tmpnum;
    unsigned short int tmpshort;
    int ret;
    char *out;
    unsigned char digest[16];
    struct buffer_info *inblob, bufinfo;
    
    if(!context || !invec || !numiov || !output || !outputlen) {
	PARAMERROR(text->utils);
	return SASL_BADPARAM;
    }
    
    if (numiov > 1) {
	ret = _plug_iovec_to_buf(text->utils, invec, numiov, &text->enc_in_buf);
	if (ret != SASL_OK) return ret;
	inblob = text->enc_in_buf;
    } else {
	/* avoid the data copy */
	bufinfo.data = invec[0].iov_base;
	bufinfo.curlen = invec[0].iov_len;
	inblob = &bufinfo;
    }
    
    /* make sure the output buffer is big enough for this blob */
    ret = _plug_buf_alloc(text->utils, &(text->encode_buf),
			  &(text->encode_buf_len),
			  (4 +                        /* for length */
			   inblob->curlen + /* for content */
			   10 +                       /* for MAC */
			   8 +                        /* maximum pad */
			   6 +                        /* for padding */
			   1));                       /* trailing null */
    if(ret != SASL_OK) return ret;
    
    /* skip by the length for now */
    out = (text->encode_buf)+4;
    
    /* construct (seqnum, msg) */
    /* We can just use the output buffer because it's big enough */
    tmpnum = htonl(text->seqnum);
    memcpy(text->encode_buf, &tmpnum, 4);
    memcpy(text->encode_buf + 4, inblob->data, inblob->curlen);
    
    /* HMAC(ki, (seqnum, msg) ) */
    text->utils->hmac_md5((const unsigned char *) text->encode_buf,
			  inblob->curlen + 4, 
			  text->Ki_send, HASHLEN, digest);
    
    /* calculate the encrpyted part */
    text->cipher_enc(text, inblob->data, inblob->curlen,
		     digest, out, outputlen);
    out+=(*outputlen);
    
    /* copy in version */
    tmpshort = htons(version);
    memcpy(out, &tmpshort, 2);	/* 2 bytes = version */
    
    out+=2;
    (*outputlen)+=2; /* for version */
    
    /* put in seqnum */
    tmpnum = htonl(text->seqnum);
    memcpy(out, &tmpnum, 4);	/* 4 bytes = seq # */  
    
    (*outputlen)+=4; /* for seqnum */
    
    /* put the 1st 4 bytes in */
    tmp=htonl(*outputlen);  
    memcpy(text->encode_buf, &tmp, 4);
    
    (*outputlen)+=4;
    
    *output = text->encode_buf;
    text->seqnum++;
    
    return SASL_OK;
}

static int
digestmd5_privacy_decode_once(void *context,
			      const char **input,
			      unsigned *inputlen,
			      char **output,
			      unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int tocopy;
    unsigned diff;
    int result;
    unsigned char digest[16];
    int tmpnum;
    int lup;
    
    if (text->needsize>0) /* 4 bytes for how long message is */
	{
	    /* if less than 4 bytes just copy those we have into text->size */
	    if (*inputlen<4) 
		tocopy=*inputlen;
	    else
		tocopy=4;
	    
	    if (tocopy>text->needsize)
		tocopy=text->needsize;
	    
	    memcpy(text->sizebuf+4-text->needsize, *input, tocopy);
	    text->needsize-=tocopy;
	    
	    *input+=tocopy;
	    *inputlen-=tocopy;
	    
	    if (text->needsize==0) /* got all of size */
		{
		    memcpy(&(text->size), text->sizebuf, 4);
		    text->cursize=0;
		    text->size=ntohl(text->size);
		    
		    if ((text->size>0xFFFF) || (text->size < 0)) {
			return SASL_FAIL; /* too big probably error */
		    }
		    
		    if(!text->buffer)
			text->buffer=text->utils->malloc(text->size+5);
		    else
			text->buffer=text->utils->realloc(text->buffer,text->size+5);	    
		    if (text->buffer == NULL) return SASL_NOMEM;
		}
	    *outputlen=0;
	    *output=NULL;
	    if (*inputlen==0) /* have to wait until next time for data */
		return SASL_OK;
	    
	    if (text->size==0)  /* should never happen */
		return SASL_FAIL;
	}
    
    diff=text->size - text->cursize; /* bytes need for full message */
    
    if (! text->buffer)
	return SASL_FAIL;
    
    if (*inputlen < diff) /* not enough for a decode */
	{
	    memcpy(text->buffer+text->cursize, *input, *inputlen);
	    text->cursize+=*inputlen;
	    *inputlen=0;
	    *outputlen=0;
	    *output=NULL;
	    return SASL_OK;
	} else {
	    memcpy(text->buffer+text->cursize, *input, diff);
	    *input+=diff;      
	    *inputlen-=diff;
	}
    
    {
	unsigned short ver;
	unsigned int seqnum;
	unsigned char checkdigest[16];
	
	result = _plug_buf_alloc(text->utils, &text->decode_once_buf,
				 &text->decode_once_buf_len,
				 text->size-6);
	if (result != SASL_OK)
	    return result;
	
	*output = text->decode_once_buf;
	*outputlen = *inputlen;
	
	result=text->cipher_dec(text,text->buffer,text->size-6,digest,
				*output, outputlen);
	
	if (result!=SASL_OK)
	    return result;
	
	{
	    int i;
	    for(i=10; i; i--) {
		memcpy(&ver, text->buffer+text->size-i,2);
		ver=ntohs(ver);
	    }
	}
	
	/* check the version number */
	memcpy(&ver, text->buffer+text->size-6, 2);
	ver=ntohs(ver);
	if (ver != version)
	    {
		text->utils->seterror(text->utils->conn, 0, "Wrong Version");
		return SASL_FAIL;
	    }
	
	/* check the CMAC */
	
	/* construct (seqnum, msg) */
	result = _plug_buf_alloc(text->utils, &text->decode_tmp_buf,
				 &text->decode_tmp_buf_len, *outputlen + 4);
	if(result != SASL_OK) return result;
	
	tmpnum = htonl(text->rec_seqnum);
	memcpy(text->decode_tmp_buf, &tmpnum, 4);
	memcpy(text->decode_tmp_buf + 4, *output, *outputlen);
	
	/* HMAC(ki, (seqnum, msg) ) */
	text->utils->hmac_md5((const unsigned char *) text->decode_tmp_buf,
			      (*outputlen) + 4, 
			      text->Ki_receive, HASHLEN, checkdigest);
	
	/* now check it */
	for (lup=0;lup<10;lup++)
	    if (checkdigest[lup]!=digest[lup])
		{
		    text->utils->seterror(text->utils->conn, 0,
					  "CMAC doesn't match at byte %d!", lup);
		    return SASL_FAIL;
		} 
	
	/* check the sequence number */
	memcpy(&seqnum, text->buffer+text->size-4,4);
	seqnum=ntohl(seqnum);
	
	if (seqnum!=text->rec_seqnum)
	    {
		text->utils->seterror(text->utils->conn, 0,
				      "Incorrect Sequence Number");
		return SASL_FAIL;
	    }
	
	text->rec_seqnum++; /* now increment it */
    }
    
    text->size=-1;
    text->needsize=4;
    
    return SASL_OK;
}

static int digestmd5_privacy_decode(void *context,
				    const char *input, unsigned inputlen,
				    const char **output, unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int ret;
    
    ret = _plug_decode(text->utils, context, input, inputlen,
		       &text->decode_buf, &text->decode_buf_len, outputlen,
		       digestmd5_privacy_decode_once);
    
    *output = text->decode_buf;
    
    return ret;
}

static int
digestmd5_integrity_encode(void *context,
			   const struct iovec *invec,
			   unsigned numiov,
			   const char **output,
			   unsigned *outputlen)
{
    context_t      *text = (context_t *) context;
    unsigned char   MAC[16];
    unsigned int    tmpnum;
    unsigned short int tmpshort;
    struct buffer_info *inblob, bufinfo;
    int ret;
    
    if(!context || !invec || !numiov || !output || !outputlen) {
	PARAMERROR( text->utils );
	return SASL_BADPARAM;
    }
    
    if (numiov > 1) {
	ret = _plug_iovec_to_buf(text->utils, invec, numiov, &text->enc_in_buf);
	if (ret != SASL_OK) return ret;
	inblob = text->enc_in_buf;
    } else {
	/* avoid the data copy */
	bufinfo.data = invec[0].iov_base;
	bufinfo.curlen = invec[0].iov_len;
	inblob = &bufinfo;
    }
    
    /* construct output */
    *outputlen = 4 + inblob->curlen + 16;
    
    ret = _plug_buf_alloc(text->utils, &(text->encode_buf),
			  &(text->encode_buf_len), *outputlen);
    if(ret != SASL_OK) return ret;
    
    /* construct (seqnum, msg) */
    /* we can just use the output buffer */
    tmpnum = htonl(text->seqnum);
    memcpy(text->encode_buf, &tmpnum, 4);
    memcpy(text->encode_buf + 4, inblob->data, inblob->curlen);
    
    /* HMAC(ki, (seqnum, msg) ) */
    text->utils->hmac_md5(text->encode_buf, inblob->curlen + 4, 
			  text->Ki_send, HASHLEN, MAC);
    
    /* create MAC */
    tmpshort = htons(version);
    memcpy(MAC + 10, &tmpshort, MAC_OFFS);	/* 2 bytes = version */
    
    tmpnum = htonl(text->seqnum);
    memcpy(MAC + 12, &tmpnum, 4);	/* 4 bytes = sequence number */
    
    /* copy into output */
    tmpnum = htonl((*outputlen) - 4);
    
    /* length of message in network byte order */
    memcpy(text->encode_buf, &tmpnum, 4);
    /* the message text */
    memcpy(text->encode_buf + 4, inblob->data, inblob->curlen);
    /* the MAC */
    memcpy(text->encode_buf + 4 + inblob->curlen, MAC, 16);
    
    text->seqnum++;		/* add one to sequence number */
    
    *output = text->encode_buf;
    
    return SASL_OK;
}

static int
create_MAC(context_t * text,
	   char *input,
	   int inputlen,
	   int seqnum,
	   unsigned char MAC[16])
{
    unsigned int    tmpnum;
    unsigned short int tmpshort;  
    int ret;
    
    if (inputlen < 0)
	return SASL_FAIL;
    
    ret = _plug_buf_alloc(text->utils, &(text->MAC_buf),
			  &(text->MAC_buf_len), inputlen + 4);
    if(ret != SASL_OK) return ret;
    
    /* construct (seqnum, msg) */
    tmpnum = htonl(seqnum);
    memcpy(text->MAC_buf, &tmpnum, 4);
    memcpy(text->MAC_buf + 4, input, inputlen);
    
    /* HMAC(ki, (seqnum, msg) ) */
    text->utils->hmac_md5(text->MAC_buf, inputlen + 4, 
			  text->Ki_receive, HASHLEN,
			  MAC);
    
    /* create MAC */
    tmpshort = htons(version);
    memcpy(MAC + 10, &tmpshort, 2);	/* 2 bytes = version */
    
    tmpnum = htonl(seqnum);
    memcpy(MAC + 12, &tmpnum, 4);	/* 4 bytes = sequence number */
    
    return SASL_OK;
}

static int
check_integrity(context_t * text,
		char *buf, int bufsize,
		char **output, unsigned *outputlen)
{
    unsigned char MAC[16];
    int result;
    
    result = create_MAC(text, buf, bufsize - 16, text->rec_seqnum, MAC);
    if (result != SASL_OK)
	return result;
    
    /* make sure the MAC is right */
    if (strncmp((char *) MAC, buf + bufsize - 16, 16) != 0)
	{
	    text->utils->seterror(text->utils->conn, 0, "MAC doesn't match");
	    return SASL_FAIL;
	}
    
    text->rec_seqnum++;
    
    /* ok make output message */
    result = _plug_buf_alloc(text->utils, &text->decode_once_buf,
			     &text->decode_once_buf_len,
			     bufsize - 15);
    if (result != SASL_OK)
	return result;
    
    *output = text->decode_once_buf;
    memcpy(*output, buf, bufsize - 16);
    *outputlen = bufsize - 16;
    (*output)[*outputlen] = 0;
    
    return SASL_OK;
}

static int
digestmd5_integrity_decode_once(void *context,
				const char **input,
				unsigned *inputlen,
				char **output,
				unsigned *outputlen)
{
    context_t      *text = (context_t *) context;
    int             tocopy;
    unsigned        diff;
    int             result;
    
    if (text->needsize > 0) {	/* 4 bytes for how long message is */
	/*
	 * if less than 4 bytes just copy those we have into text->size
	 */
	if (*inputlen < 4)
	    tocopy = *inputlen;
	else
	    tocopy = 4;
	
	if (tocopy > text->needsize)
	    tocopy = text->needsize;
	
	memcpy(text->sizebuf + 4 - text->needsize, *input, tocopy);
	text->needsize -= tocopy;
	
	*input += tocopy;
	*inputlen -= tocopy;
	
	if (text->needsize == 0) {	/* got all of size */
	    memcpy(&(text->size), text->sizebuf, 4);
	    text->cursize = 0;
	    text->size = ntohl(text->size);
	    
	    if ((text->size > 0xFFFF) || (text->size < 0))
		return SASL_FAIL;	/* too big probably error */
	    
	    
	    if(!text->buffer)
		text->buffer=text->utils->malloc(text->size+5);
	    else
		text->buffer=text->utils->realloc(text->buffer,text->size+5);
	    if (text->buffer == NULL) return SASL_NOMEM;
	}
	*outputlen = 0;
	*output = NULL;
	if (*inputlen == 0)		/* have to wait until next time for data */
	    return SASL_OK;
	
	if (text->size == 0)	/* should never happen */
	    return SASL_FAIL;
    }
    diff = text->size - text->cursize;	/* bytes need for full message */
    
    if(! text->buffer)
	return SASL_FAIL;
    
    if (*inputlen < diff) {	/* not enough for a decode */
	memcpy(text->buffer + text->cursize, *input, *inputlen);
	text->cursize += *inputlen;
	*inputlen = 0;
	*outputlen = 0;
	*output = NULL;
	return SASL_OK;
    } else {
	memcpy(text->buffer + text->cursize, *input, diff);
	*input += diff;
	*inputlen -= diff;
    }
    
    result = check_integrity(text, text->buffer, text->size,
			     output, outputlen);
    if (result != SASL_OK)
	return result;
    
    text->size = -1;
    text->needsize = 4;
    
    return SASL_OK;
}

static int digestmd5_integrity_decode(void *context,
				      const char *input, unsigned inputlen,
				      const char **output, unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int ret;
    
    ret = _plug_decode(text->utils, context, input, inputlen,
		       &text->decode_buf, &text->decode_buf_len, outputlen,
		       digestmd5_integrity_decode_once);
    
    *output = text->decode_buf;
    
    return ret;
}

static void
digestmd5_common_mech_dispose(void *conn_context, const sasl_utils_t *utils)
{
    context_t *text = (context_t *) conn_context;
    
    if (!text || !utils) return;
    
    if (text->cipher_free) text->cipher_free(text);
    
    /* free the stuff in the context */
    if (text->response_value) utils->free(text->response_value);
    
    if (text->buffer) utils->free(text->buffer);
    if (text->encode_buf) utils->free(text->encode_buf);
    if (text->decode_buf) utils->free(text->decode_buf);
    if (text->decode_once_buf) utils->free(text->decode_once_buf);
    if (text->decode_tmp_buf) utils->free(text->decode_tmp_buf);
    if (text->out_buf) utils->free(text->out_buf);
    if (text->MAC_buf) utils->free(text->MAC_buf);
    
    if (text->enc_in_buf) {
	if (text->enc_in_buf->data) utils->free(text->enc_in_buf->data);
	utils->free(text->enc_in_buf);
    }
    
    utils->free(conn_context);
}

static void
clear_global_context(global_context_t *glob_context, const sasl_utils_t *utils)
{
    if (glob_context->authid) utils->free(glob_context->authid);
    if (glob_context->realm) utils->free(glob_context->realm);
    if (glob_context->nonce) utils->free(glob_context->nonce);
	
	/* Apple: the client nonce is owned by the client context because we've unimplemented reauth */
	/*   and the global context is thread-dangerous */
	/* nothing to free, just set to NULL */
    //if (glob_context->cnonce) utils->free(glob_context->cnonce);
	
    if (glob_context->serverFQDN) utils->free(glob_context->serverFQDN);

    memset(glob_context, 0, sizeof(global_context_t));
}

static void
digestmd5_common_mech_free(void *glob_context, const sasl_utils_t *utils)
{
    global_context_t *glob_text = (global_context_t *) glob_context;
    
    clear_global_context(glob_text, utils);

    utils->free(glob_text);
}

/*****************************  Server Section  *****************************/

typedef struct server_context {
    context_t common;

    char *authid;
    char *realm;
    unsigned char *nonce;
    unsigned int nonce_count;
    unsigned char *cnonce;
    time_t timestamp;

    int stale;				/* last nonce is stale */
    sasl_ssf_t limitssf, requiressf;	/* application defined bounds */
} server_context_t;

static void
DigestCalcHA1FromSecret(context_t * text,
			const sasl_utils_t * utils,
			HASH HA1,
			unsigned char *authorization_id,
			unsigned char *pszNonce,
			unsigned char *pszCNonce,
			HASHHEX SessionKey)
{
    MD5_CTX Md5Ctx;
    
    /* calculate session key */
    utils->MD5Init(&Md5Ctx);
    utils->MD5Update(&Md5Ctx, HA1, HASHLEN);
    utils->MD5Update(&Md5Ctx, COLON, 1);
    utils->MD5Update(&Md5Ctx, pszNonce, strlen((char *) pszNonce));
    utils->MD5Update(&Md5Ctx, COLON, 1);
    utils->MD5Update(&Md5Ctx, pszCNonce, strlen((char *) pszCNonce));
    if (authorization_id != NULL) {
	utils->MD5Update(&Md5Ctx, COLON, 1);
	utils->MD5Update(&Md5Ctx, authorization_id, strlen((char *) authorization_id));
    }
    utils->MD5Final(HA1, &Md5Ctx);
    
    CvtHex(HA1, SessionKey);
    
    
    /* save HA1 because we need it to make the privacy and integrity keys */
    memcpy(text->HA1, HA1, sizeof(HASH));
}

static char *create_response(context_t * text,
			     const sasl_utils_t * utils,
			     unsigned char *nonce,
			     unsigned int ncvalue,
			     unsigned char *cnonce,
			     char *qop,
			     char *digesturi,
			     HASH Secret,
			     char *authorization_id,
			     char **response_value)
{
    HASHHEX         SessionKey;
    HASHHEX         HEntity = "00000000000000000000000000000000";
    HASHHEX         Response;
    char           *result;
    
    if (qop == NULL)
	qop = "auth";
    
    DigestCalcHA1FromSecret(text,
			    utils,
			    Secret,
			    (unsigned char *) authorization_id,
			    nonce,
			    cnonce,
			    SessionKey);
    
    DigestCalcResponse(utils,
		       SessionKey,/* H(A1) */
		       nonce,	/* nonce from server */
		       ncvalue,	/* 8 hex digits */
		       cnonce,	/* client nonce */
		       (unsigned char *) qop,	/* qop-value: "", "auth",
						 * "auth-int" */
		       (unsigned char *) digesturi,	/* requested URL */
		       (unsigned char *) "AUTHENTICATE",
		       HEntity,	/* H(entity body) if qop="auth-int" */
		       Response	/* request-digest or response-digest */
	);
    
    result = utils->malloc(HASHHEXLEN + 1);
    memcpy(result, Response, HASHHEXLEN);
    result[HASHHEXLEN] = 0;
    
    /* response_value (used for reauth i think */
    if (response_value != NULL) {
	DigestCalcResponse(utils,
			   SessionKey,	/* H(A1) */
			   nonce,	/* nonce from server */
			   ncvalue,	/* 8 hex digits */
			   cnonce,	/* client nonce */
			   (unsigned char *) qop,	/* qop-value: "", "auth",
							 * "auth-int" */
			   (unsigned char *) digesturi,	/* requested URL */
			   NULL,
			   HEntity,	/* H(entity body) if qop="auth-int" */
			   Response	/* request-digest or response-digest */
	    );
	
	*response_value = utils->malloc(HASHHEXLEN + 1);
	if (*response_value == NULL)
	    return NULL;
	memcpy(*response_value, Response, HASHHEXLEN);
	(*response_value)[HASHHEXLEN] = 0;
    }
    return result;
}

static int
get_server_realm(sasl_server_params_t * params,
		 char **realm)
{
    /* look at user realm first */
    if (params->user_realm != NULL) {
	if(params->user_realm[0] != '\0') {
	    *realm = (char *) params->user_realm;
	} else {
	    /* Catch improperly converted apps */
	    params->utils->seterror(params->utils->conn, 0,
				    "user_realm is an empty string!");
	    return SASL_BADPARAM;
	}
    } else if (params->serverFQDN != NULL) {
	*realm = (char *) params->serverFQDN;
    } else {
	params->utils->seterror(params->utils->conn, 0,
				"no way to obtain domain");
	return SASL_FAIL;
    }
    
    return SASL_OK;
}

/*
 * Convert hex string to int
 */
static int htoi(unsigned char *hexin, unsigned int *res)
{
    int             lup, inlen;
    inlen = strlen((char *) hexin);
    
    *res = 0;
    for (lup = 0; lup < inlen; lup++) {
	switch (hexin[lup]) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    *res = (*res << 4) + (hexin[lup] - '0');
	    break;
	    
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	    *res = (*res << 4) + (hexin[lup] - 'a' + 10);
	    break;
	    
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	    *res = (*res << 4) + (hexin[lup] - 'A' + 10);
	    break;
	    
	default:
	    return SASL_BADPARAM;
	}
	
    }
    
    return SASL_OK;
}

static int digestmd5_server_mech_new(void *glob_context __attribute__((unused)),
				     sasl_server_params_t * sparams,
				     const char *challenge __attribute__((unused)),
				     unsigned challen __attribute__((unused)),
				     void **conn_context)
{
    context_t *text;
    
    /* holds state are in -- allocate server size */
    text = sparams->utils->malloc(sizeof(server_context_t));
    if (text == NULL)
	return SASL_NOMEM;
    memset(text, 0, sizeof(server_context_t));
    
    text->state = 1;
    text->i_am = SERVER;
    
    *conn_context = text;
    return SASL_OK;
}

static int
digestmd5_server_mech_step1(server_context_t *stext,
			    sasl_server_params_t *sparams,
			    const char *clientin __attribute__((unused)),
			    unsigned clientinlen __attribute__((unused)),
			    const char **serverout,
			    unsigned *serveroutlen,
			    sasl_out_params_t * oparams __attribute__((unused)))
{
    context_t *text = (context_t *) stext;
    int             result;
    char           *realm;
    unsigned char  *nonce;
    char           *charset = "utf-8";
    char qop[1024], cipheropts[1024];
    struct digest_cipher *cipher;
    unsigned       resplen;
    int added_conf = 0;
    
    sparams->utils->log(sparams->utils->conn, SASL_LOG_DEBUG,
			"DIGEST-MD5 server step 1");

    /* get realm */
    result = get_server_realm(sparams, &realm);
    if(result != SASL_OK) return result;
    
    /* what options should we offer the client? */
    qop[0] = '\0';
    cipheropts[0] = '\0';
    if (stext->requiressf == 0) {
	if (*qop) strcat(qop, ",");
	strcat(qop, "auth");
    }
    if (stext->requiressf <= 1 && stext->limitssf >= 1) {
	if (*qop) strcat(qop, ",");
	strcat(qop, "auth-int");
    }
    
    cipher = available_ciphers;
    while (cipher->name) {
	/* do we allow this particular cipher? */
	if (stext->requiressf <= cipher->ssf &&
	    stext->limitssf >= cipher->ssf) {
	    if (!added_conf) {
		if (*qop) strcat(qop, ",");
		strcat(qop, "auth-conf");
		added_conf = 1;
	    }
	    if (*cipheropts) strcat(cipheropts, ",");
	    strcat(cipheropts, cipher->name);
	}
	cipher++;
    }
    
    if (*qop == '\0') {
	/* we didn't allow anything?!? we'll return SASL_TOOWEAK, since
	   that's close enough */
	return SASL_TOOWEAK;
    }
    
    /*
     * digest-challenge  = 1#( realm | nonce | qop-options | stale | maxbuf |
     * charset | cipher-opts | auth-param )
     */
    
    /* FIXME: get nonce XXX have to clean up after self if fail */
    nonce = create_nonce(sparams->utils);
    if (nonce == NULL) {
	SETERROR(sparams->utils, "internal erorr: failed creating a nonce");
	return SASL_FAIL;
    }
    
    resplen = strlen(nonce) + strlen("nonce") + 5;
    result = _plug_buf_alloc(sparams->utils, &(text->out_buf),
			     &(text->out_buf_len), resplen);
    if(result != SASL_OK) return result;
    
    sprintf(text->out_buf, "nonce=\"%s\"", nonce);
    
    /* add to challenge; if we chose not to specify a realm, we won't
     * send one to the client */
    if (realm && add_to_challenge(sparams->utils,
				  &text->out_buf, &text->out_buf_len, &resplen,
				  "realm", (unsigned char *) realm,
				  TRUE) != SASL_OK) {
	SETERROR(sparams->utils, "internal error: add_to_challenge failed");
	return SASL_FAIL;
    }
    /*
     * qop-options A quoted string of one or more tokens indicating the
     * "quality of protection" values supported by the server.  The value
     * "auth" indicates authentication; the value "auth-int" indicates
     * authentication with integrity protection; the value "auth-conf"
     * indicates authentication with integrity protection and encryption.
     */
    
    /* add qop to challenge */
    if (add_to_challenge(sparams->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "qop", 
			 (unsigned char *) qop, TRUE) != SASL_OK) {
	SETERROR(sparams->utils, "internal error: add_to_challenge 3 failed");
	return SASL_FAIL;
    }
    
    /*
     *  Cipheropts - list of ciphers server supports
     */
    /* add cipher-opts to challenge; only add if there are some */
    if (strcmp(cipheropts,"")!=0)
	{
	    if (add_to_challenge(sparams->utils,
				 &text->out_buf, &text->out_buf_len, &resplen,
				 "cipher", (unsigned char *) cipheropts, 
				 TRUE) != SASL_OK) {
		SETERROR(sparams->utils,
			 "internal error: add_to_challenge 4 failed");
		return SASL_FAIL;
	    }
	}
    
    /* "stale" is true if a reauth failed because of a nonce timeout */
    if (stext->stale &&
	add_to_challenge(sparams->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "stale", "true", FALSE) != SASL_OK) {
	SETERROR(sparams->utils, "internal error: add_to_challenge failed");
	return SASL_FAIL;
    }
    
    /*
     * maxbuf A number indicating the size of the largest buffer the server
     * is able to receive when using "auth-int". If this directive is
     * missing, the default value is 65536. This directive may appear at most
     * once; if multiple instances are present, the client should abort the
     * authentication exchange.
     */
    
    if (add_to_challenge(sparams->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "charset", 
			 (unsigned char *) charset, FALSE) != SASL_OK) {
	SETERROR(sparams->utils, "internal error: add_to_challenge 5 failed");
	return SASL_FAIL;
    }
    
    
    /*
     * algorithm 
     *  This directive is required for backwards compatibility with HTTP 
     *  Digest., which supports other algorithms. . This directive is 
     *  required and MUST appear exactly once; if not present, or if multiple 
     *  instances are present, the client should abort the authentication 
     *  exchange. 
     *
     * algorithm         = "algorithm" "=" "md5-sess" 
     */
    
    if (add_to_challenge(sparams->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "algorithm",
			 (unsigned char *) "md5-sess", FALSE)!=SASL_OK) {
	SETERROR(sparams->utils, "internal error: add_to_challenge 6 failed");
	return SASL_FAIL;
    }
    
    /*
     * The size of a digest-challenge MUST be less than 2048 bytes!!!
     */
    if (*serveroutlen > 2048) {
	SETERROR(sparams->utils,
		 "internal error: challenge larger than 2048 bytes");
	return SASL_FAIL;
    }

    stext->authid = NULL;
    _plug_strdup(sparams->utils, realm, &stext->realm, NULL);
    stext->nonce = nonce;
    stext->nonce_count = 1;
    stext->cnonce = NULL;
    stext->timestamp = time(0);
    
    *serveroutlen = strlen(text->out_buf);
    *serverout = text->out_buf;
    
    text->state = 2;
    
    return SASL_CONTINUE;
}

static int
digestmd5_server_mech_step2(server_context_t *stext,
			    sasl_server_params_t *sparams,
			    const char *clientin,
			    unsigned clientinlen,
			    const char **serverout,
			    unsigned *serveroutlen,
			    sasl_out_params_t * oparams)
{
    context_t *text = (context_t *) stext;
    /* verify digest */
    sasl_secret_t  *sec = NULL;
    int             result;
    char           *serverresponse = NULL;
    char           *username = NULL;
    char           *authorization_id = NULL;
    char           *realm = NULL;
    unsigned char  *cnonce = NULL;
    unsigned int   noncecount = 0;
    char           *qop = NULL;
    char           *digesturi = NULL;
    char           *response = NULL;
    
    /* setting the default value (65536) */
    unsigned int    client_maxbuf = 65536;
    int             maxbuf_count = 0;  /* How many maxbuf instaces was found */
    
    char           *charset = NULL;
    char           *cipher = NULL;
    unsigned int   n=0;
    
    HASH            A1;
    
    /* password prop_request */
    const char *password_request[] = { SASL_AUX_PASSWORD,
				       "*cmusaslsecretDIGEST-MD5",
				       NULL };
    unsigned len;
    struct propval auxprop_values[2];
    
    /* can we mess with clientin? copy it to be safe */
    char           *in_start = NULL;
    char           *in = NULL; 
    
    sparams->utils->log(sparams->utils->conn, SASL_LOG_DEBUG,
			"DIGEST-MD5 server step 2");

    in = sparams->utils->malloc(clientinlen + 1);
    
    memcpy(in, clientin, clientinlen);
    in[clientinlen] = 0;
    
    in_start = in;
    
    
    /* parse what we got */
    while (in[0] != '\0') {
	char           *name = NULL, *value = NULL;
	get_pair(&in, &name, &value);
	
	if (name == NULL)
	    break;
	
	/* Extracting parameters */
	
	/*
	 * digest-response  = 1#( username | realm | nonce | cnonce |
	 * nonce-count | qop | digest-uri | response | maxbuf | charset |
	 * cipher | auth-param )
	 */
	
	if (strcasecmp(name, "username") == 0) {
	    if (stext->authid && (strcmp(value, stext->authid) != 0)) {
		SETERROR(sparams->utils,
			 "username changed: authentication aborted");
		result = SASL_FAIL;
		goto FreeAllMem;
	    }

	    _plug_strdup(sparams->utils, value, &username, NULL);
	} else if (strcasecmp(name, "authzid") == 0) {
	    _plug_strdup(sparams->utils, value, &authorization_id, NULL);
	} else if (strcasecmp(name, "cnonce") == 0) {
	    if (stext->cnonce && (strcmp(value, stext->cnonce) != 0)) {
		SETERROR(sparams->utils,
			 "cnonce changed: authentication aborted");
		result = SASL_FAIL;
		goto FreeAllMem;
	    }

	    _plug_strdup(sparams->utils, value, (char **) &cnonce, NULL);
	} else if (strcasecmp(name, "nc") == 0) {
	    if (htoi((unsigned char *) value, &noncecount) != SASL_OK) {
		SETERROR(sparams->utils,
			 "error converting hex to int");
		result = SASL_BADAUTH;
		goto FreeAllMem;
	    }
	    else if (noncecount != stext->nonce_count) {
		SETERROR(sparams->utils,
			 "incorrect nonce-count: authentication aborted");
		result = SASL_BADAUTH;
		goto FreeAllMem;
	    }
	} else if (strcasecmp(name, "realm") == 0) {
	    if (realm) {
		SETERROR(sparams->utils,
			 "duplicate realm: authentication aborted");
		result = SASL_FAIL;
		goto FreeAllMem;
	    } else if (stext->realm && (strcmp(value, stext->realm) != 0)) {
		SETERROR(sparams->utils,
			 "realm changed: authentication aborted");
		result = SASL_FAIL;
		goto FreeAllMem;
	    }
	    
	    _plug_strdup(sparams->utils, value, &realm, NULL);
	} else if (strcasecmp(name, "nonce") == 0) {
	    if (strcmp(value, (char *) stext->nonce) != 0) {
		/*
		 * Nonce changed: Abort authentication!!!
		 */
		SETERROR(sparams->utils,
			 "nonce changed: authentication aborted");
		result = SASL_BADAUTH;
		goto FreeAllMem;
	    }
	} else if (strcasecmp(name, "qop") == 0) {
	    _plug_strdup(sparams->utils, value, &qop, NULL);
	} else if (strcasecmp(name, "digest-uri") == 0) {
	    /* XXX: verify digest-uri format */
	    /*
	     * digest-uri-value  = serv-type "/" host [ "/" serv-name ]
	     */
	    _plug_strdup(sparams->utils, value, &digesturi, NULL);
	} else if (strcasecmp(name, "response") == 0) {
	    _plug_strdup(sparams->utils, value, &response, NULL);
	} else if (strcasecmp(name, "cipher") == 0) {
	    _plug_strdup(sparams->utils, value, &cipher, NULL);
	} else if (strcasecmp(name, "maxbuf") == 0) {
	    maxbuf_count++;
	    if (maxbuf_count != 1) {
		result = SASL_BADAUTH;
		SETERROR(sparams->utils,
			 "duplicate maxbuf: authentication aborted");
		goto FreeAllMem;
	    } else if (sscanf(value, "%u", &client_maxbuf) != 1) {
		result = SASL_BADAUTH;
		SETERROR(sparams->utils, "invalid maxbuf parameter");
		goto FreeAllMem;
	    } else {
		if (client_maxbuf <= 16) {
		    result = SASL_BADAUTH;
		    SETERROR(sparams->utils,
			     "maxbuf parameter too small");
		    goto FreeAllMem;
		}
	    }
	} else if (strcasecmp(name, "charset") == 0) {
	    if (strcasecmp(value, "utf-8") != 0) {
		SETERROR(sparams->utils, "client doesn't support UTF-8");
		result = SASL_FAIL;
		goto FreeAllMem;
	    }
	    _plug_strdup(sparams->utils, value, &charset, NULL);
	} else {
	    sparams->utils->log(sparams->utils->conn, SASL_LOG_DEBUG,
				"DIGEST-MD5 unrecognized pair %s/%s: ignoring",
				name, value);
	}
    }
    
    /* defaulting qop to "auth" if not specified */
    if (qop == NULL) {
	_plug_strdup(sparams->utils, "auth", &qop, NULL);      
    }
    
    /* check which layer/cipher to use */
    if ((!strcasecmp(qop, "auth-conf")) && (cipher != NULL)) {
	/* see what cipher was requested */
	struct digest_cipher *cptr;
	
	cptr = available_ciphers;
	while (cptr->name) {
	    /* find the cipher requested & make sure it's one we're happy
	       with by policy */
	    if (!strcasecmp(cipher, cptr->name) && 
		stext->requiressf <= cptr->ssf &&
		stext->limitssf >= cptr->ssf) {
		/* found it! */
		break;
	    }
	    cptr++;
	}
	
	if (cptr->name) {
	    text->cipher_enc = cptr->cipher_enc;
	    text->cipher_dec = cptr->cipher_dec;
	    text->cipher_init = cptr->cipher_init;
	    text->cipher_free = cptr->cipher_free;
	    oparams->mech_ssf = cptr->ssf;
	    n = cptr->n;
	} else {
	    /* erg? client requested something we didn't advertise! */
	    sparams->utils->log(sparams->utils->conn, SASL_LOG_WARN,
				"protocol violation: client requested invalid cipher");
	    SETERROR(sparams->utils, "client requested invalid cipher");
	    /* Mark that we attempted security layer negotiation */
	    oparams->mech_ssf = 2;
	    result = SASL_FAIL;
	    goto FreeAllMem;
	}
	
	oparams->encode=&digestmd5_privacy_encode;
	oparams->decode=&digestmd5_privacy_decode;
    } else if (!strcasecmp(qop, "auth-int") &&
	       stext->requiressf <= 1 && stext->limitssf >= 1) {
	oparams->encode = &digestmd5_integrity_encode;
	oparams->decode = &digestmd5_integrity_decode;
	oparams->mech_ssf = 1;
    } else if (!strcasecmp(qop, "auth") && stext->requiressf == 0) {
	oparams->encode = NULL;
	oparams->decode = NULL;
	oparams->mech_ssf = 0;
    } else {
	SETERROR(sparams->utils,
		 "protocol violation: client requested invalid qop");
	result = SASL_FAIL;
	goto FreeAllMem;
    }
    
    /*
     * username         = "username" "=" <"> username-value <">
     * username-value   = qdstr-val cnonce           = "cnonce" "=" <">
     * cnonce-value <"> cnonce-value     = qdstr-val nonce-count      = "nc"
     * "=" nc-value nc-value         = 8LHEX qop              = "qop" "="
     * qop-value digest-uri = "digest-uri" "=" digest-uri-value
     * digest-uri-value  = serv-type "/" host [ "/" serv-name ] serv-type
     * = 1*ALPHA host             = 1*( ALPHA | DIGIT | "-" | "." ) service
     * = host response         = "response" "=" <"> response-value <">
     * response-value   = 32LHEX LHEX = "0" | "1" | "2" | "3" | "4" | "5" |
     * "6" | "7" | "8" | "9" | "a" | "b" | "c" | "d" | "e" | "f" cipher =
     * "cipher" "=" cipher-value
     */
    /* Verifing that all parameters was defined */
    if ((username == NULL) ||
	(noncecount == 0) ||
	(cnonce == NULL) ||
	(digesturi == NULL) ||
	(response == NULL)) {
	SETERROR(sparams->utils, "required parameters missing");
	result = SASL_BADAUTH;
	goto FreeAllMem;
    }
    
    result = sparams->utils->prop_request(sparams->propctx, password_request);
    if(result != SASL_OK) {
	SETERROR(sparams->utils, "unable to resquest user password");
	goto FreeAllMem;
    }
    
    /* this will trigger the getting of the aux properties */
    /* Note that if we don't have an authorization id, we don't use it... */
    result = sparams->canon_user(sparams->utils->conn,
				 username, 0, SASL_CU_AUTHID, oparams);
    if (result != SASL_OK) {
	SETERROR(sparams->utils, "unable cannonify user and get auxprops");
	goto FreeAllMem;
    }
    
    if(!authorization_id || !*authorization_id) {
	result = sparams->canon_user(sparams->utils->conn,
				     username, 0, SASL_CU_AUTHZID, oparams);
    } else {
	result = sparams->canon_user(sparams->utils->conn,
				     authorization_id, 0, SASL_CU_AUTHZID,
				     oparams);
    }
    
    if (result != SASL_OK) {
	SETERROR(sparams->utils, "unable authorization ID");
	goto FreeAllMem;
    }
    
    result = sparams->utils->prop_getnames(sparams->propctx, password_request,
					   auxprop_values);
    if(result < 0 ||
       ((!auxprop_values[0].name || !auxprop_values[0].values) &&
	(!auxprop_values[1].name || !auxprop_values[1].values))) {
	/* We didn't find this username */
	sparams->utils->seterror(sparams->utils->conn, 0,
				 "no secret in database");
	result = SASL_NOUSER;
	goto FreeAllMem;
    }
    
    if(auxprop_values[0].name && auxprop_values[0].values) {
	len = strlen(auxprop_values[0].values[0]);
	if (len == 0) {
	    sparams->utils->seterror(sparams->utils->conn,0,
				     "empty secret");
	    result = SASL_FAIL;
	    goto FreeAllMem;
	}
	
	sec = sparams->utils->malloc(sizeof(sasl_secret_t) + len);
	if (!sec) {
	    SETERROR(sparams->utils, "unable to allocate secret");
	    result = SASL_FAIL;
	    goto FreeAllMem;
	}
	
	sec->len = len;
	strncpy(sec->data, auxprop_values[0].values[0], len + 1); 
	
	/*
	 * Verifying response obtained from client
	 * 
	 * H_URP = H({ username-value,":",realm-value,":",passwd}) sec->data
	 * contains H_URP
	 */
	
	/* Calculate the secret from the plaintext password */
	{
	    HASH HA1;
	    
	    DigestCalcSecret(sparams->utils, username,
			     stext->realm, sec->data, sec->len, HA1);
	    
	    /*
	     * A1 = { H( { username-value, ":", realm-value, ":", passwd } ),
	     * ":", nonce-value, ":", cnonce-value }
	     */
	    
	    memcpy(A1, HA1, HASHLEN);
	    A1[HASHLEN] = '\0';
	}
	
	/* We're done with sec now. Let's get rid of it */
	_plug_free_secret(sparams->utils, &sec);
    } else if (auxprop_values[1].name && auxprop_values[1].values) {
	memcpy(A1, auxprop_values[1].values[0], HASHLEN);
	A1[HASHLEN] = '\0';
    } else {
	sparams->utils->seterror(sparams->utils->conn, 0,
				 "Have neither type of secret");
	return SASL_FAIL;
    } 
    
    serverresponse = create_response(text,
				     sparams->utils,
				     stext->nonce,
				     stext->nonce_count,
				     cnonce,
				     qop,
				     digesturi,
				     A1,
				     authorization_id,
				     &text->response_value);
    
    if (serverresponse == NULL) {
	SETERROR(sparams->utils, "internal error: unable to create response");
	result = SASL_NOMEM;
	goto FreeAllMem;
    }
    
    /* if ok verified */
    if (strcmp(serverresponse, response) != 0) {
	SETERROR(sparams->utils,
		 "client response doesn't match what we generated");
	result = SASL_BADAUTH;
	
	goto FreeAllMem;
    }
    
    /* see if our nonce expired (stext->stale = 1) */
    
    /*
     * nothing more to do; authenticated set oparams information
     */
    oparams->doneflag = 1;
    oparams->maxoutbuf = client_maxbuf - 4;
    if(oparams->mech_ssf > 1) {
	/* MAC block (privacy) */
	oparams->maxoutbuf -= 25;
    } else if(oparams->mech_ssf == 1) {
	/* MAC block (integrity) */
	oparams->maxoutbuf -= 16;
    }
    
    oparams->param_version = 0;
    
    text->seqnum = 0;		/* for integrity/privacy */
    text->rec_seqnum = 0;	/* for integrity/privacy */
    text->maxbuf = client_maxbuf;
    text->utils = sparams->utils;
    
    /* used by layers */
    text->size = -1;
    text->needsize = 4;
    text->buffer = NULL;
    
    if (oparams->mech_ssf > 0) {
	char enckey[16];
	char deckey[16];
	
	create_layer_keys(text, sparams->utils,text->HA1,n,enckey,deckey);
	
	/* initialize cipher if need be */
	if (text->cipher_init)
	    if (text->cipher_init(text, enckey, deckey) != SASL_OK) {
		sparams->utils->seterror(sparams->utils->conn, 0,
					 "couldn't init cipher");
	    }
    }
    
    /*
     * The server receives and validates the "digest-response". The server
     * checks that the nonce-count is "00000001". If it supports subsequent
     * authentication, it saves the value of the nonce and the nonce-count.
     */
    
    /*
     * The "username-value", "realm-value" and "passwd" are encoded according
     * to the value of the "charset" directive. If "charset=UTF-8" is
     * present, and all the characters of either "username-value" or "passwd"
     * are in the ISO 8859-1 character set, then it must be converted to
     * UTF-8 before being hashed. A sample implementation of this conversion
     * is in section 8.
     */
    
    /* add to challenge */
    {
	unsigned resplen =
	    strlen(text->response_value) + strlen("rspauth") + 3;
	
	result = _plug_buf_alloc(sparams->utils, &(text->out_buf),
				 &(text->out_buf_len), resplen);
	if(result != SASL_OK) {
	    goto FreeAllMem;
	}
	
	sprintf(text->out_buf, "rspauth=%s", text->response_value);
	
	/* self check */
	if (strlen(text->out_buf) > 2048) {
	    result = SASL_FAIL;
	    goto FreeAllMem;
	}

	/* setup for a potential reauth */
	
	*serveroutlen = strlen(text->out_buf);
	*serverout = text->out_buf;
	
	result = SASL_OK;
    }
    
  FreeAllMem:
    /* free everything */
    /*
     * sparams->utils->free (authorization_id);
     */
    
    if (in_start) sparams->utils->free (in_start);
    
    if (username != NULL)
	sparams->utils->free (username);
    if (realm != NULL)
	sparams->utils->free (realm);
    if (cnonce != NULL)
	sparams->utils->free (cnonce);
    if (response != NULL)
	sparams->utils->free (response);
    if (cipher != NULL)
	sparams->utils->free (cipher);
    if (serverresponse != NULL)
	sparams->utils->free(serverresponse);
    if (charset != NULL)
	sparams->utils->free (charset);
    if (digesturi != NULL)
	sparams->utils->free (digesturi);
    if (qop!=NULL)
	sparams->utils->free (qop);  
    if (sec)
	_plug_free_secret(sparams->utils, &sec);
    
    return result;
}

static int
digestmd5_server_mech_step(void *conn_context,
			   sasl_server_params_t *sparams,
			   const char *clientin,
			   unsigned clientinlen,
			   const char **serverout,
			   unsigned *serveroutlen,
			   sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) conn_context;
    server_context_t *stext = (server_context_t *) conn_context;
    
    if (clientinlen > 4096) return SASL_BADPROT;
    
    *serverout = NULL;
    *serveroutlen = 0;
    
    switch (text->state) {
	
    case 1:
	/* setup SSF limits */
	if (sparams->props.max_ssf < sparams->external_ssf) {
	    stext->limitssf = 0;
	} else {
	    stext->limitssf = sparams->props.max_ssf - sparams->external_ssf;
	}
	if (sparams->props.min_ssf < sparams->external_ssf) {
	    stext->requiressf = 0;
	} else {
	    stext->requiressf = sparams->props.min_ssf - sparams->external_ssf;
	}
#if 0
	/* should we attempt reauth? */
	if (clientin /* && we have reauth info */) {
	    /* setup for reauth attempt */

	    if (digestmd5_server_mech_step2(stext, sparams,
					    clientin, clientinlen,
					    serverout, serveroutlen,
					    oparams) == SASL_OK)
		return SASL_OK;
	    else {
		sparams->utils->log(NULL, SASL_LOG_WARN,
				    "DIGEST-MD5 reauth failed\n");
	    }
	}
#endif
	/* re-initialize everything for a fresh start */
	*serverout = NULL;
	*serveroutlen = 0;
	memset(oparams, 0, sizeof(sasl_out_params_t));
	
	return digestmd5_server_mech_step1(stext, sparams,
					   clientin, clientinlen,
					   serverout, serveroutlen, oparams);
	
    case 2:
	return digestmd5_server_mech_step2(stext, sparams,
					   clientin, clientinlen,
					   serverout, serveroutlen, oparams);
	
    default:
	sparams->utils->log(NULL, SASL_LOG_ERR,
			    "Invalid DIGEST-MD5 server step %d\n", text->state);
	return SASL_FAIL;
    }
    
    return SASL_FAIL; /* should never get here */
}

static void
digestmd5_server_mech_dispose(void *conn_context, const sasl_utils_t *utils)
{
    server_context_t *stext = (server_context_t *) conn_context;
    
    if (!stext || !utils) return;
    
    if (stext->authid) utils->free(stext->authid);
    if (stext->realm) utils->free(stext->realm);
    if (stext->nonce) utils->free(stext->nonce);
    if (stext->cnonce) utils->free(stext->cnonce);

    digestmd5_common_mech_dispose(conn_context, utils);
}

static sasl_server_plug_t digestmd5_server_plugins[] =
{
    {
	"DIGEST-MD5",			/* mech_name */
#ifdef WITH_RC4
	128,				/* max_ssf */
#elif WITH_DES
	112,
#else 
	0,
#endif
	SASL_SEC_NOPLAINTEXT
	| SASL_SEC_NOANONYMOUS
	| SASL_SEC_MUTUAL_AUTH,		/* security_flags */
	0, //SASL_FEAT_ALLOWS_PROXY,		/* features */
	NULL,				/* glob_context */
	&digestmd5_server_mech_new,	/* mech_new */
	&digestmd5_server_mech_step,	/* mech_step */
	&digestmd5_server_mech_dispose,	/* mech_dispose */
	NULL,				/* mech_free */
	NULL,				/* setpass */
	NULL,				/* user_query */
	NULL,				/* idle */
	NULL,				/* mech avail */
	NULL				/* spare */
    }
};

int digestmd5_server_plug_init(sasl_utils_t *utils __attribute__((unused)),
			       int maxversion,
			       int *out_version,
			       sasl_server_plug_t **pluglist,
			       int *plugcount) 
{
    if (maxversion < SASL_SERVER_PLUG_VERSION)
	return SASL_BADVERS;

    *out_version = SASL_SERVER_PLUG_VERSION;
    *pluglist = digestmd5_server_plugins;
    *plugcount = 1;
    
    return SASL_OK;
}

/*****************************  Client Section  *****************************/

typedef struct client_context {
    context_t common;

    char *realm;		/* realm we are in (client) */
    
    sasl_secret_t *password;	/* user password (client) */
    unsigned int free_password; /* set if we need to free password (client) */
	unsigned char *lcnonce;		/* Apple: putting the nonce in the global context doesn't work for multi-threaded */
								/* clients. Let's store it here. */
} client_context_t;

/* calculate H(A1) as per spec */
static void
DigestCalcHA1(context_t * text,
	      const sasl_utils_t * utils,
	      unsigned char *pszUserName,
	      unsigned char *pszRealm,
	      sasl_secret_t * pszPassword,
	      unsigned char *pszAuthorization_id,
	      unsigned char *pszNonce,
	      unsigned char *pszCNonce,
	      HASHHEX SessionKey)
{
    MD5_CTX         Md5Ctx;
    HASH            HA1;
    
    DigestCalcSecret(utils,
		     pszUserName,
		     pszRealm,
		     (unsigned char *) pszPassword->data,
		     pszPassword->len,
		     HA1);
    
    /* calculate the session key */
    utils->MD5Init(&Md5Ctx);
    utils->MD5Update(&Md5Ctx, HA1, HASHLEN);
    utils->MD5Update(&Md5Ctx, COLON, 1);
    utils->MD5Update(&Md5Ctx, pszNonce, strlen((char *) pszNonce));
    utils->MD5Update(&Md5Ctx, COLON, 1);
    utils->MD5Update(&Md5Ctx, pszCNonce, strlen((char *) pszCNonce));
    if (pszAuthorization_id != NULL) {
	utils->MD5Update(&Md5Ctx, COLON, 1);
	utils->MD5Update(&Md5Ctx, pszAuthorization_id, 
			 strlen((char *) pszAuthorization_id));
    }
    utils->MD5Final(HA1, &Md5Ctx);
    
    CvtHex(HA1, SessionKey);
    
    /* xxx rc-* use different n */
    
    /* save HA1 because we'll need it for the privacy and integrity keys */
    memcpy(text->HA1, HA1, sizeof(HASH));
    
}

static char *calculate_response(context_t * text,
				const sasl_utils_t * utils,
				unsigned char *username,
				unsigned char *realm,
				unsigned char *nonce,
				unsigned int ncvalue,
				unsigned char *cnonce,
				char *qop,
				unsigned char *digesturi,
				sasl_secret_t * passwd,
				unsigned char *authorization_id,
				char **response_value)
{
    HASHHEX         SessionKey;
    HASHHEX         HEntity = "00000000000000000000000000000000";
    HASHHEX         Response;
    char           *result;
    
    /* Verifing that all parameters was defined */
    if(!username || !cnonce || !nonce || !ncvalue || !digesturi || !passwd) {
	PARAMERROR( utils );
	return NULL;
    }
    
    if (realm == NULL) {
	/* a NULL realm is equivalent to the empty string */
	realm = (unsigned char *) "";
    }
    
    if (qop == NULL) {
	/* default to a qop of just authentication */
	qop = "auth";
    }
    
    DigestCalcHA1(text,
		  utils,
		  username,
		  realm,
		  passwd,
		  authorization_id,
		  nonce,
		  cnonce,
		  SessionKey);
    
    DigestCalcResponse(utils,
		       SessionKey,/* H(A1) */
		       nonce,	/* nonce from server */
		       ncvalue,	/* 8 hex digits */
		       cnonce,	/* client nonce */
		       (unsigned char *) qop,	/* qop-value: "", "auth",
						 * "auth-int" */
		       digesturi,	/* requested URL */
		       (unsigned char *) "AUTHENTICATE",
		       HEntity,	/* H(entity body) if qop="auth-int" */
		       Response	/* request-digest or response-digest */
	);
    
    result = utils->malloc(HASHHEXLEN + 1);
    memcpy(result, Response, HASHHEXLEN);
    result[HASHHEXLEN] = 0;
    
    if (response_value != NULL) {
	DigestCalcResponse(utils,
			   SessionKey,	/* H(A1) */
			   nonce,	/* nonce from server */
			   ncvalue,	/* 8 hex digits */
			   cnonce,	/* client nonce */
			   (unsigned char *) qop,	/* qop-value: "", "auth",
							 * "auth-int" */
			   (unsigned char *) digesturi,	/* requested URL */
			   NULL,
			   HEntity,	/* H(entity body) if qop="auth-int" */
			   Response	/* request-digest or response-digest */
	    );
	
	*response_value = utils->malloc(HASHHEXLEN + 1);
	if (*response_value == NULL)
	    return NULL;
	
	memcpy(*response_value, Response, HASHHEXLEN);
	(*response_value)[HASHHEXLEN] = 0;
	
    }
    
    return result;
}

static int
make_client_response(context_t *text,
		     sasl_client_params_t *params,
		     sasl_out_params_t *oparams,
		     unsigned int n)
{
    client_context_t *ctext = (client_context_t *) text;
    unsigned char  *digesturi = NULL;
    bool            IsUTF8 = FALSE;
    char           ncvalue[10];
    char           *response = NULL;
    unsigned        resplen = 0;
    int result;

    digesturi = params->utils->malloc(strlen(params->service) + 1 +
				      strlen(params->serverFQDN) + 1 +
				      1);
    if (digesturi == NULL) {
	result = SASL_NOMEM;
	goto FreeAllocatedMem;
    };
    
    /* allocated exactly this. safe */
    strcpy((char *) digesturi, params->service);
    strcat((char *) digesturi, "/");
    strcat((char *) digesturi, params->serverFQDN);
    /*
     * strcat (digesturi, "/"); strcat (digesturi, params->serverFQDN);
     */

    /* response */
    response =
	calculate_response(text,
			   params->utils,
			   (char *) oparams->authid,
			   (unsigned char *) ctext->realm,
			   text->global->nonce,
			   text->global->nonce_count,
			   ctext->lcnonce ? ctext->lcnonce : text->global->cnonce,
			   text->global->qop,
			   digesturi,
			   ctext->password,
			   strcmp(oparams->user, oparams->authid) ?
			   (char *) oparams->user : NULL,
			   &text->response_value);
    
	/* Apple: do not keep calling realloc. Let us try to get close to the right size. */
    //resplen = strlen(oparams->authid) + strlen("username") + 5;
	resplen = strlen(oparams->authid) + strlen("username=''authzid=''nonce=''cnonce=''nc=''qop=''cipher=''charset=''digest-uri=''response=''")*2 + 5;
	
    result =_plug_buf_alloc(params->utils, &(text->out_buf),
			    &(text->out_buf_len),
			    resplen);
    if (result != SASL_OK) goto FreeAllocatedMem;
    
    sprintf(text->out_buf, "username=\"%s\"", oparams->authid);
    
    if (add_to_challenge(params->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "realm", (unsigned char *) ctext->realm,
			 TRUE) != SASL_OK) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }
    if (strcmp(oparams->user, oparams->authid)) {
	if (add_to_challenge(params->utils,
			     &text->out_buf, &text->out_buf_len, &resplen,
			     "authzid", (char *) oparams->user, TRUE) != SASL_OK) {
	    result = SASL_FAIL;
	    goto FreeAllocatedMem;
	}
    }
    if (add_to_challenge(params->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "nonce", text->global->nonce, TRUE) != SASL_OK) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }
    if (add_to_challenge(params->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "cnonce", ctext->lcnonce ? ctext->lcnonce : text->global->cnonce, TRUE) != SASL_OK) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }
    sprintf(ncvalue, "%08x", text->global->nonce_count);
    if (add_to_challenge(params->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "nc", (unsigned char *) ncvalue, FALSE) != SASL_OK) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }
    if (add_to_challenge(params->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "qop", (unsigned char *) text->global->qop, FALSE) != SASL_OK) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }
    if (text->global->bestcipher != NULL)
	if (add_to_challenge(params->utils,
			     &text->out_buf, &text->out_buf_len, &resplen,
			     "cipher", 
			     (unsigned char *) text->global->bestcipher->name,
			     TRUE) != SASL_OK) {
	    result = SASL_FAIL;
	    goto FreeAllocatedMem;
	}
    
    if (IsUTF8) {
	if (add_to_challenge(params->utils,
			     &text->out_buf, &text->out_buf_len, &resplen,
			     "charset", (unsigned char *) "utf-8",
			     FALSE) != SASL_OK) {
	    result = SASL_FAIL;
	    goto FreeAllocatedMem;
	}
    }
    if (add_to_challenge(params->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "digest-uri", digesturi, TRUE) != SASL_OK) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }
    if (add_to_challenge(params->utils,
			 &text->out_buf, &text->out_buf_len, &resplen,
			 "response", (unsigned char *) response,
			 FALSE) != SASL_OK) {
	
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }
    
    /* self check */
    if (strlen(text->out_buf) > 2048) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }

    /* set oparams */
    oparams->maxoutbuf = text->global->server_maxbuf;
    if(oparams->mech_ssf > 1) {
	/* MAC block (privacy) */
	oparams->maxoutbuf -= 25;
    } else if(oparams->mech_ssf == 1) {
	/* MAC block (integrity) */
	oparams->maxoutbuf -= 16;
    }
    
    text->seqnum = 0;	/* for integrity/privacy */
    text->rec_seqnum = 0;	/* for integrity/privacy */
    text->utils = params->utils;
    
    text->maxbuf = text->global->server_maxbuf;
    
    /* used by layers */
    text->size = -1;
    text->needsize = 4;
    text->buffer = NULL;
    
    if (oparams->mech_ssf > 0) {
	char enckey[16];
	char deckey[16];
	
	create_layer_keys(text, params->utils,text->HA1,n,enckey,deckey);
	
	/* initialize cipher if need be */
	if (text->cipher_init)
	    text->cipher_init(text, enckey, deckey);		       
    }
    
    result = SASL_OK;

  FreeAllocatedMem:
    if (digesturi) params->utils->free(digesturi);
    if (response) params->utils->free(response);

    return result;
}

static int
digestmd5_client_mech_new(void *glob_context,
			  sasl_client_params_t * params,
			  void **conn_context)
{
    context_t *text;
    
    /* holds state are in -- allocate client size */
    text = params->utils->malloc(sizeof(client_context_t));
    if (text == NULL)
	return SASL_NOMEM;
    memset(text, 0, sizeof(client_context_t));
    
    text->state = 1;
    text->i_am = CLIENT;
    text->global = glob_context;
    
    *conn_context = text;

    return SASL_OK;
}

/* Apple: reauth is not thread-safe so we are not using it */
#if 0
static int
digestmd5_client_mech_step1(client_context_t *ctext,
			    sasl_client_params_t *params,
			    const char *serverin __attribute__((unused)), 
			    unsigned serverinlen __attribute__((unused)), 
			    sasl_interact_t **prompt_need,
			    const char **clientout,
			    unsigned *clientoutlen,
			    sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) ctext;
    const char *authid = NULL, *userid = NULL;
    int user_result = SASL_OK;
    int auth_result = SASL_OK;
    int pass_result = SASL_OK;
    int result = SASL_FAIL;
    unsigned int n = 0;

    params->utils->log(params->utils->conn, SASL_LOG_DEBUG,
		       "DIGEST-MD5 client step 1");

    /* try to get the authid */
    if (oparams->authid == NULL) {
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
    if (ctext->password == NULL) {
	pass_result = _plug_get_password(params->utils, &ctext->password,
					 &ctext->free_password, prompt_need);
	if ((pass_result != SASL_OK) && (pass_result != SASL_INTERACT))
	    return pass_result;
    }
    
    /* free prompts we got */
    if (prompt_need && *prompt_need) {
	params->utils->free(*prompt_need);
	*prompt_need = NULL;
    }
    
    /* if there are prompts not filled in */
    if ((user_result == SASL_INTERACT) || (auth_result == SASL_INTERACT) ||
	(pass_result == SASL_INTERACT)) {
	/* make the prompt list */
	result =
	    _plug_make_prompts(params->utils, prompt_need,
			       user_result == SASL_INTERACT ?
			       "Please enter your authorization name" : NULL, NULL,
			       auth_result == SASL_INTERACT ?
			       "Please enter your authentication name" : NULL, NULL,
			       pass_result == SASL_INTERACT ?
			       "Please enter your password" : NULL, NULL,
			       NULL, NULL, NULL,
			       NULL, NULL, NULL);
	
	if (result != SASL_OK) return result;
	
	return SASL_INTERACT;
    }
    
    if (!userid || !*userid) {
	result = params->canon_user(params->utils->conn, authid, 0,
				    SASL_CU_AUTHID | SASL_CU_AUTHZID, oparams);
    }
    else {
	result = params->canon_user(params->utils->conn,
				    authid, 0, SASL_CU_AUTHID, oparams);
	if (result != SASL_OK) return result;
    
	result = params->canon_user(params->utils->conn,
				    userid, 0, SASL_CU_AUTHZID, oparams);
    }
    if (result != SASL_OK) return result;

    if (!strcmp(oparams->authid, text->global->authid)) {
	if (!strcmp(text->global->qop, "auth-conf")) {
	    /* confidentiality */
	    oparams->encode = &digestmd5_privacy_encode; 
	    oparams->decode = &digestmd5_privacy_decode;
	    oparams->mech_ssf = text->global->bestcipher->ssf;

	    n = text->global->bestcipher->n;
	    text->cipher_enc = text->global->bestcipher->cipher_enc;
	    text->cipher_dec = text->global->bestcipher->cipher_dec;
	    text->cipher_free = text->global->bestcipher->cipher_free;
	    text->cipher_init = text->global->bestcipher->cipher_init;
	}
	else if (!strcmp(text->global->qop, "auth-int")) {
	    /* integrity */
	    oparams->encode = &digestmd5_integrity_encode;
	    oparams->decode = &digestmd5_integrity_decode;
	    oparams->mech_ssf = 1;
	}
	else {
	    /* no layer */
	    oparams->encode = NULL;
	    oparams->decode = NULL;
	    oparams->mech_ssf = 0;
	}

	ctext->realm = text->global->realm;
	text->global->nonce_count++;
	
	if (make_client_response(text, params, oparams, n) == SASL_OK) {
	    *clientoutlen = strlen(text->out_buf);
	    *clientout = text->out_buf;
	}

	ctext->realm = NULL;
    }

    text->state = 2;
    return SASL_CONTINUE;
}
#endif

static int
digestmd5_client_mech_step2(client_context_t *ctext,
			    sasl_client_params_t *params,
			    const char *serverin,
			    unsigned serverinlen,
			    sasl_interact_t **prompt_need,
			    const char **clientout,
			    unsigned *clientoutlen,
			    sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) ctext;
    char *in = NULL;
    char *in_start;
    sasl_ssf_t limit, musthave = 0;
    sasl_ssf_t external;
    int protection = 0;
    int ciphers = 0;
    struct digest_cipher *bestcipher = NULL;
    unsigned int n = 0;
    char **realms = NULL, *realm_chal = NULL;
    int nrealm = 0;
    unsigned int server_maxbuf = 65536; /* Default value for maxbuf */
    int maxbuf_count = 0;
    bool IsUTF8 = FALSE;
    int result = SASL_FAIL;
    const char *authid = NULL, *userid = NULL;
    int user_result = SASL_OK;
    int auth_result = SASL_OK;
    int pass_result = SASL_OK;
    int realm_result = SASL_OK;
    int algorithm_count = 0;
    
    params->utils->log(params->utils->conn, SASL_LOG_DEBUG,
		       "DIGEST-MD5 client step 2");

    if (params->props.min_ssf > params->props.max_ssf) {
	return SASL_BADPARAM;
    }
    
    in = params->utils->malloc(serverinlen + 1);
    if (in == NULL) return SASL_NOMEM;
    
    memcpy(in, serverin, serverinlen);
    in[serverinlen] = 0;
    
    in_start = in;
    
    /* parse what we got */
    while (in[0] != '\0') {
	char *name, *value;
	
	get_pair(&in, &name, &value);
	
	/* if parse error */
	if (name == NULL) {
	    params->utils->seterror(params->utils->conn, 0, "Parse error");
	    result = SASL_FAIL;
	    goto FreeAllocatedMem;
	}
	
	if (strcasecmp(name, "realm") == 0) {
	    nrealm++;
	    
	    if(!realms)
		realms = params->utils->malloc(sizeof(char *) * (nrealm + 1));
	    else
		realms = params->utils->realloc(realms, 
						sizeof(char *) * (nrealm + 1));
	    
	    if (realms == NULL) {
		result = SASL_NOMEM;
		goto FreeAllocatedMem;
	    }
	    
	    _plug_strdup(params->utils, value, &realms[nrealm-1], NULL);
	    realms[nrealm] = NULL;
	} else if (strcasecmp(name, "nonce") == 0)
	{
		if (text->global->nonce) {
			params->utils->free(text->global->nonce);
			text->global->nonce = NULL;
		}
	    _plug_strdup(params->utils, value, (char **) &text->global->nonce,
			 NULL);
	} else if (strcasecmp(name, "qop") == 0) {
	    while (value && *value) {
		char *comma = strchr(value, ',');
		if (comma != NULL) {
		    *comma++ = '\0';
		}
		
		if (strcasecmp(value, "auth-conf") == 0) {
		    protection |= DIGEST_PRIVACY;
		} else if (strcasecmp(value, "auth-int") == 0) {
		    protection |= DIGEST_INTEGRITY;
		} else if (strcasecmp(value, "auth") == 0) {
		    protection |= DIGEST_NOLAYER;
		} else {
		    params->utils->log(params->utils->conn, SASL_LOG_DEBUG,
				       "Server supports unknown layer: %s\n",
				       value);
		}
		
		value = comma;
	    }
	    
	    if (protection == 0) {
		result = SASL_BADAUTH;
		params->utils->seterror(params->utils->conn, 0,
					"Server doesn't support known qop level");
		goto FreeAllocatedMem;
	    }
	} else if (strcasecmp(name, "cipher") == 0) {
	    while (value && *value) {
		char *comma = strchr(value, ',');
		struct digest_cipher *cipher = available_ciphers;
		
		if (comma != NULL) {
		    *comma++ = '\0';
		}
		
		/* do we support this cipher? */
		while (cipher->name) {
		    if (!strcasecmp(value, cipher->name)) break;
		    cipher++;
		}
		if (cipher->name) {
		    ciphers |= cipher->flag;
		} else {
		    params->utils->log(params->utils->conn, SASL_LOG_DEBUG,
				       "Server supports unknown cipher: %s\n",
				       value);
		}
		
		value = comma;
	    }
	} else if (strcasecmp(name, "stale") == 0) {
	    /* XXX do we need to do something ??? */
	} else if (strcasecmp(name, "maxbuf") == 0) {
	    /* maxbuf A number indicating the size of the largest
	     * buffer the server is able to receive when using
	     * "auth-int". If this directive is missing, the default
	     * value is 65536. This directive may appear at most once;
	     * if multiple instances are present, the client should
	     * abort the authentication exchange.  
	     */
	    maxbuf_count++;
	    
	    if (maxbuf_count != 1) {
		result = SASL_BADAUTH;
		params->utils->seterror(params->utils->conn, 0,
					"At least two maxbuf directives found. Authentication aborted");
		goto FreeAllocatedMem;
	    } else if (sscanf(value, "%u", &server_maxbuf) != 1) {
		result = SASL_BADAUTH;
		params->utils->seterror(params->utils->conn, 0,
					"Invalid maxbuf parameter received from server");
		goto FreeAllocatedMem;
	    } else {
		if (server_maxbuf<=16) {
		    result = SASL_BADAUTH;
		    params->utils->seterror(params->utils->conn, 0,
					    "Invalid maxbuf parameter received from server (too small)");
		    goto FreeAllocatedMem;
		}
	    }
	} else if (strcasecmp(name, "charset") == 0) {
	    if (strcasecmp(value, "utf-8") != 0) {
		result = SASL_BADAUTH;
		params->utils->seterror(params->utils->conn, 0,
					"Charset must be UTF-8");
		goto FreeAllocatedMem;
	    } else {
		IsUTF8 = TRUE;
	    }
	} else if (strcasecmp(name,"algorithm")==0) {
	    if (strcasecmp(value, "md5-sess") != 0)
		{
		    params->utils->seterror(params->utils->conn, 0,
					    "'algorithm' isn't 'md5-sess'");
		    result = SASL_FAIL;
		    goto FreeAllocatedMem;
		}
	    
	    algorithm_count++;
	    if (algorithm_count > 1)
		{
		    params->utils->seterror(params->utils->conn, 0,
					    "Must see 'algorithm' only once");
		    result = SASL_FAIL;
		    goto FreeAllocatedMem;
		}
	} else {
	    params->utils->log(params->utils->conn, SASL_LOG_DEBUG,
			       "DIGEST-MD5 unrecognized pair %s/%s: ignoring",
			       name, value);
	}
    }
    
    if (algorithm_count != 1)
	{
	    params->utils->seterror(params->utils->conn, 0,
				    "Must see 'algorithm' once. Didn't see at all");
	    result = SASL_FAIL;
	    goto FreeAllocatedMem;
	}
    
    /* make sure we have everything we require */
    if (text->global->nonce == NULL) {
	params->utils->seterror(params->utils->conn, 0,
				"Don't have nonce.");
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }

    text->global->server_maxbuf = server_maxbuf;
    
    /* make callbacks */
    
    /* try to get the authid */
    if (oparams->authid == NULL) {
	auth_result = _plug_get_authid(params->utils, &authid, prompt_need);
	
	if ((auth_result != SASL_OK) && (auth_result != SASL_INTERACT)) {
	    result = auth_result;
	    goto FreeAllocatedMem;
	}
    }
    
    /* try to get the userid */
    if (oparams->user == NULL) {
	user_result = _plug_get_userid(params->utils, &userid, prompt_need);
	
	if ((user_result != SASL_OK) && (user_result != SASL_INTERACT))
	    return user_result;
    }
    
    /* try to get the password */
    if (ctext->password == NULL) {
	pass_result = _plug_get_password(params->utils, &ctext->password,
					 &ctext->free_password, prompt_need);
	if ((pass_result != SASL_OK) && (pass_result != SASL_INTERACT)) {
	    result = pass_result;
	    goto FreeAllocatedMem;
	}
    }
    /* try to get the realm, if needed */
    if (nrealm == 1 && ctext->realm == NULL) {
	/* only one choice! */
	ctext->realm = realms[0];
    }
    if (ctext->realm == NULL) {
	realm_result = _plug_get_realm(params->utils, (const char **) realms,
				       (const char **) &ctext->realm,
				       prompt_need);
	
	if ((realm_result != SASL_OK) && (realm_result != SASL_INTERACT)) {
	    /* Fake the realm, if we can. */
	    if (params->serverFQDN) {
		ctext->realm = (char *) params->serverFQDN;
	    } else {
		result = realm_result;
		goto FreeAllocatedMem;
	    }
	}
	/* if realm_result == SASL_OK, ctext->realm has been filled in */
    }
    
    /* free prompts we got */
    if (prompt_need && *prompt_need) {
	params->utils->free(*prompt_need);
	*prompt_need = NULL;
    }
    
    /* if there are prompts not filled in */
    if ((user_result == SASL_INTERACT) ||
	(auth_result == SASL_INTERACT) ||
	(pass_result == SASL_INTERACT) ||
	(realm_result == SASL_INTERACT)) {
	int result = SASL_OK;
	/* make our default realm */
	if ((realm_result == SASL_INTERACT) && params->serverFQDN) {
	    realm_chal = params->utils->malloc(3+strlen(params->serverFQDN));
	    if (realm_chal) {
		sprintf(realm_chal, "{%s}", params->serverFQDN);
	    } else {
		result = SASL_NOMEM;
	    }
	}
	/* make the prompt list */
	if (result == SASL_OK) {
	    result =
		_plug_make_prompts(params->utils, prompt_need,
				   user_result == SASL_INTERACT ?
				   "Please enter your authorization name" : NULL, NULL,
				   auth_result == SASL_INTERACT ?
				   "Please enter your authentication name" : NULL, NULL,
				   pass_result == SASL_INTERACT ?
				   "Please enter your password" : NULL, NULL,
				   NULL, NULL, NULL,
				   realm_chal ? realm_chal : "{}",
				   realm_result == SASL_INTERACT ?
				   "Please enter your realm" : NULL,
				   params->serverFQDN ? params->serverFQDN : NULL);
	}
	
	if ( realm_chal != NULL ) {
		params->utils->free( realm_chal );
		realm_chal = NULL;
	}
	if (in_start) params->utils->free(in_start);
	if (text->global->nonce) {
	    params->utils->free(text->global->nonce);
	    text->global->nonce = NULL;
	}
	if (realms) {
	    int lup;
	    
	    /* need to free all the realms */
	    for (lup = 0; lup < nrealm; lup++)
		params->utils->free(realms[lup]);
	    
	    params->utils->free(realms);
	}
	
	/* need to NULL the realm for the next pass */
	ctext->realm = NULL;
	
	if (result != SASL_OK) return result;
	
	return SASL_INTERACT;
    }
    
    if (oparams->authid == NULL) {
	if (!userid || !*userid) {
	    result = params->canon_user(params->utils->conn, authid, 0,
					SASL_CU_AUTHID | SASL_CU_AUTHZID,
					oparams);
	}
	else {
	    result = params->canon_user(params->utils->conn,
					authid, 0, SASL_CU_AUTHID, oparams);
	    if (result != SASL_OK) goto FreeAllocatedMem;

	    result = params->canon_user(params->utils->conn,
					userid, 0, SASL_CU_AUTHZID, oparams);
	}
	if (result != SASL_OK) goto FreeAllocatedMem;
    }
    
    /*
     * (username | realm | nonce | cnonce | nonce-count | qop digest-uri |
     * response | maxbuf | charset | auth-param )
     */
    
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
    if ((limit > 1) && (protection & DIGEST_PRIVACY)) {
	struct digest_cipher *cipher;
	
	/* let's find an encryption scheme that we like */
	cipher = available_ciphers;
	bestcipher = NULL;
	while (cipher->name) {
	    /* examine each cipher we support, see if it meets our security
	       requirements, and see if the server supports it.
	       choose the best one of these */
	    if ((limit >= cipher->ssf) && (musthave <= cipher->ssf) &&
		(ciphers & cipher->flag) &&
		(!bestcipher || (cipher->ssf > bestcipher->ssf))) {
		bestcipher = cipher;
	    }
	    cipher++;
	}
	
	if (bestcipher) {
	    /* we found a cipher we like */
	    text->global->bestcipher = bestcipher;

	    oparams->encode = &digestmd5_privacy_encode; 
	    oparams->decode = &digestmd5_privacy_decode;
	    oparams->mech_ssf = bestcipher->ssf;
	    
	    text->global->qop = "auth-conf";
	    n = bestcipher->n;
	    text->cipher_enc = bestcipher->cipher_enc;
	    text->cipher_dec = bestcipher->cipher_dec;
	    text->cipher_free = bestcipher->cipher_free;
	    text->cipher_init = bestcipher->cipher_init;
	} else {
	    /* we didn't find any ciphers we like */
	    params->utils->seterror(params->utils->conn, 0,
				    "No good privacy layers");
	    text->global->qop = NULL;
	}
    }
    
    if (text->global->qop == NULL) {
	/* we failed to find an encryption layer we liked;
	   can we use integrity or nothing? */
	
	if ((limit >= 1) && (musthave <= 1) 
	    && (protection & DIGEST_INTEGRITY)) {
	    /* integrity */
	    oparams->encode = &digestmd5_integrity_encode;
	    oparams->decode = &digestmd5_integrity_decode;
	    oparams->mech_ssf = 1;
	    text->global->qop = "auth-int";
	} else if (musthave <= 0) {
	    /* no layer */
	    oparams->encode = NULL;
	    oparams->decode = NULL;
	    oparams->mech_ssf = 0;
	    text->global->qop = "auth";
	    
	    /* See if server supports not having a layer */
	    if ((protection & DIGEST_NOLAYER) != DIGEST_NOLAYER) {
		params->utils->seterror(params->utils->conn, 0, 
					"Server doesn't support \"no layer\"");
		result = SASL_FAIL;
		goto FreeAllocatedMem;
	    }
	} else {
	    params->utils->seterror(params->utils->conn, 0,
				    "Can't find an acceptable layer");
	    result = SASL_TOOWEAK;
	    goto FreeAllocatedMem;
	}
    }
    
    /* get nonce XXX have to clean up after self if fail */
    ctext->lcnonce = create_nonce(params->utils);
	text->global->cnonce = ctext->lcnonce;
    if (text->global->cnonce == NULL) {
	params->utils->seterror(params->utils->conn, 0,
				"failed to create cnonce");
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }

    text->global->nonce_count = 1;

    if (make_client_response(text, params, oparams, n) != SASL_OK) {
	result = SASL_FAIL;
	goto FreeAllocatedMem;
    }

    *clientoutlen = strlen(text->out_buf);
    *clientout = text->out_buf;
    
    /* setup for a potential reauth */
    _plug_strdup(params->utils, oparams->authid,
		 (char **) &text->global->authid, NULL);
    _plug_strdup(params->utils, params->serverFQDN,
		 (char **) &text->global->serverFQDN, NULL);
    _plug_strdup(params->utils, ctext->realm,
		 (char **) &text->global->realm, NULL);

    text->state = 3;
    
    result = SASL_CONTINUE;
    
  FreeAllocatedMem:
    if (in_start) params->utils->free(in_start);
    
    if (realms) {
	int lup;
	
	/* need to free all the realms */
	for (lup = 0;lup < nrealm; lup++)
	    params->utils->free(realms[lup]);
	
	params->utils->free(realms);
    }
    
    /* if we failed, re-initialize everything for a fresh start */
    if ((result != SASL_CONTINUE) && (result != SASL_INTERACT))
	clear_global_context(text->global, params->utils);
    
    return result;
}

static int
digestmd5_client_mech_step3(client_context_t *ctext,
			    sasl_client_params_t *params,
			    const char *serverin,
			    unsigned serverinlen,
			    sasl_interact_t **prompt_need __attribute__((unused)),
			    const char **clientout __attribute__((unused)),
			    unsigned *clientoutlen __attribute__((unused)),
			    sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) ctext;
    char           *in = NULL;
    char           *in_start;
    int result = SASL_FAIL;
    
    params->utils->log(params->utils->conn, SASL_LOG_DEBUG,
		       "DIGEST-MD5 client step 3");

    /* Verify that server is really what he claims to be */
    in = params->utils->malloc(serverinlen + 1);
    if (in == NULL) return SASL_NOMEM;
    memcpy(in, serverin, serverinlen);
    in[serverinlen] = 0;
    
    in_start = in;
	
    /* parse what we got */
    while (in[0] != '\0') {
	char *name, *value;
	get_pair(&in, &name, &value);
	
	if (name == NULL) {
	    params->utils->seterror(params->utils->conn, 0,
				    "DIGEST-MD5 Received Garbage");
	    break;
	}
	
	if (strcasecmp(name, "rspauth") == 0) {
	    
	    if (strcmp(text->response_value, value) != 0) {
		params->utils->seterror(params->utils->conn, 0,
					"DIGEST-MD5: This server wants us to believe that he knows shared secret");
		return SASL_FAIL;
	    } else {
		oparams->doneflag = 1;
		oparams->param_version = 0;
		
		result = SASL_OK;
		break;
	    }
	} else {
	    params->utils->log(params->utils->conn, SASL_LOG_DEBUG,
			       "DIGEST-MD5 unrecognized pair %s/%s: ignoring",
			       name, value);
	}
    }
	
    params->utils->free(in_start);
	
    /* if we failed, re-initialize everything for a fresh start */
    if (result != SASL_OK)
	clear_global_context(text->global, params->utils);
    
    return result;
}

static int
digestmd5_client_mech_step(void *conn_context,
			   sasl_client_params_t *params,
			   const char *serverin,
			   unsigned serverinlen,
			   sasl_interact_t **prompt_need,
			   const char **clientout,
			   unsigned *clientoutlen,
			   sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) conn_context;
    client_context_t *ctext = (client_context_t *) conn_context;
    
    if (serverinlen > 2048) return SASL_BADPROT;
    
    *clientout = NULL;
    *clientoutlen = 0;

    switch (text->state) {

    case 1:
	if (!serverin) {
/* Apple: we know the server isn't going to let us reauth because */
/* it doesn't have a thread-safe implementation, so don't bother */
/* trying. */
#if 0
	    /* here's where we attempt fast reauth if possible */
	    if (text->global->authid &&
		!strcasecmp(text->global->serverFQDN, params->serverFQDN)) {
		/* we've auth'd to this server before, so try reauth */
		return digestmd5_client_mech_step1(ctext, params,
						   serverin, serverinlen,
						   prompt_need,
						   clientout, clientoutlen,
						   oparams);
	    }
#endif	
	    /* we don't have any reauth info, so just return
	     * that there is no initial client send */
	    text->state = 2;
	    return SASL_CONTINUE;
	}
	
	/* otherwise fall through and respond to challenge */
	text->state = 2;
	
    case 2:
	if (strncasecmp(serverin, "rspauth=", 8) != 0) {
	    /* we received a challenge,
	       so re-initialize everything for a fresh start */
	    
	    memset(oparams, 0, sizeof(sasl_out_params_t));
	    clear_global_context(text->global, params->utils);
	    
	    return digestmd5_client_mech_step2(ctext, params,
					       serverin, serverinlen,
					       prompt_need,
					       clientout, clientoutlen,
					       oparams);
	}

	/* otherwise fall through and check rspauth */
	text->state = 3;
    
    case 3:
	return digestmd5_client_mech_step3(ctext, params, serverin, serverinlen,
					   prompt_need, clientout, clientoutlen,
					   oparams);

    default:
	params->utils->log(NULL, SASL_LOG_ERR,
			   "Invalid DIGEST-MD5 client step %d\n", text->state);
	return SASL_FAIL;
    }
    
    return SASL_FAIL; /* should never get here */
}

static void
digestmd5_client_mech_dispose(void *conn_context, const sasl_utils_t *utils)
{
    client_context_t *ctext = (client_context_t *) conn_context;
    
    if (!ctext || !utils) return;
    
    if (ctext->free_password) _plug_free_secret(utils, &ctext->password);

	/* Apple: we're not using reauth, so clean up now */
	((context_t *) conn_context)->global->cnonce = NULL;
	if (ctext->lcnonce) utils->free(ctext->lcnonce);
	
    digestmd5_common_mech_dispose(conn_context, utils);
}

static sasl_client_plug_t digestmd5_client_plugins[] =
{
    {
	"DIGEST-MD5",
#ifdef WITH_RC4				/* mech_name */
	128,				/* max ssf */
#elif WITH_DES
	112,
#else
	0,
#endif
	SASL_SEC_NOPLAINTEXT
	| SASL_SEC_NOANONYMOUS
	| SASL_SEC_MUTUAL_AUTH,		/* security_flags */
	0, //SASL_FEAT_ALLOWS_PROXY, 	/* features */
	NULL,				/* required_prompts */
	NULL,				/* glob_context */
	&digestmd5_client_mech_new,	/* mech_new */
	&digestmd5_client_mech_step,	/* mech_step */
	&digestmd5_client_mech_dispose,	/* mech_dispose */
	&digestmd5_common_mech_free,	/* mech_free */
	NULL,				/* idle */
	NULL,				/* spare1 */
	NULL				/* spare2 */
    }
};

int digestmd5_client_plug_init(sasl_utils_t *utils,
			       int maxversion,
			       int *out_version,
			       sasl_client_plug_t **pluglist,
			       int *plugcount)
{
    global_context_t *glob_text;

    if (maxversion < SASL_CLIENT_PLUG_VERSION)
	return SASL_BADVERS;
    
    /* holds global state are in */
    glob_text = utils->malloc(sizeof(global_context_t));
    if (glob_text == NULL)
	return SASL_NOMEM;
    memset(glob_text, 0, sizeof(global_context_t));
    
    digestmd5_client_plugins[0].glob_context = glob_text;

    *out_version = SASL_CLIENT_PLUG_VERSION;
    *pluglist = digestmd5_client_plugins;
    *plugcount = 1;
    
    return SASL_OK;
}
