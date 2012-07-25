/*
 * Copyright (c) 2005-2007 Rob Braun
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
 * 3. Neither the name of Rob Braun nor the names of his contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * 03-Apr-2005
 * DRI: Rob Braun <bbraun@synack.net>
 */
/*
 * Portions Copyright 2006, Apple Computer, Inc.
 * Christopher Ryan <ryanc@apple.com>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <zlib.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#else
#include <openssl/evp.h>
#endif

#include "xar.h"
#include "hash.h"
#include "config.h"
#ifndef HAVE_ASPRINTF
#include "asprintf.h"
#endif

#ifdef __APPLE__

// The following value is of coure tweakable but this should serve for now.
#define MAX_HASH_NAME_LENGTH 32 

CCDigestRef digestRef_from_name(const char* name, unsigned int *outHashSize)
{
    CCDigestRef result = NULL;
    
    if (NULL != outHashSize)
    {
        *outHashSize = 0;
    }
    
    if (!strcasecmp(name, "sha") || !strcasecmp(name, "sha1"))
    {
        result = CCDigestCreate(kCCDigestSHA1);
        if (NULL != outHashSize)
        {
            *outHashSize = CC_SHA1_DIGEST_LENGTH;
        }
    }
    else if (!strcasecmp(name, "md5"))
    {
        result = CCDigestCreate(kCCDigestMD5);
        if (NULL != outHashSize)
        {
            *outHashSize = CC_MD5_DIGEST_LENGTH;
        }
    }
    else if (!strcasecmp(name, "md2"))
    {
        result = CCDigestCreate(kCCDigestMD2); 
        if (NULL != outHashSize)
        {
            *outHashSize = CC_MD2_DIGEST_LENGTH;
        }
    }
    
    return result;
                
}
#endif // __APPLE__


struct _hash_context{
#ifdef __APPLE__
    CCDigestRef unarchived_cts;
    const char unarchived_ctx_name[MAX_HASH_NAME_LENGTH];
    unsigned int unarchived_digest_size;
    CCDigestRef archived_cts;
    const char archived_ctx_name[MAX_HASH_NAME_LENGTH];
    unsigned int archived_digest_size;
#else
	EVP_MD_CTX unarchived_cts;
	EVP_MD_CTX archived_cts;
#endif // !__APPLE__
	uint8_t	unarchived;
	uint8_t archived;
	uint64_t count;
};

#define CONTEXT(x) ((struct _hash_context *)(*x))

static char* xar_format_hash(const unsigned char* m,unsigned int len);

int32_t xar_hash_unarchived(xar_t x, xar_file_t f, xar_prop_t p, void **in, size_t *inlen, void **context) {
	return xar_hash_unarchived_out(x,f,p,*in,*inlen,context);
}

int32_t xar_hash_unarchived_out(xar_t x, xar_file_t f, xar_prop_t p, void *in, size_t inlen, void **context) {
	const char *opt;
#ifndef __APPLE__
 	const EVP_MD *md;
#endif // !__APPLE__
    
	xar_prop_t tmpp;

	opt = NULL;
	tmpp = xar_prop_pget(p, "extracted-checksum");
	if( tmpp )
		opt = xar_attr_pget(f, tmpp, "style");
	
	if( !opt ) 	
		opt = xar_opt_get(x, XAR_OPT_FILECKSUM);

	if( !opt || (0 == strcmp(opt, XAR_OPT_VAL_NONE) ) )
		return 0;
	
	if(!CONTEXT(context)){
		*context = calloc(1,sizeof(struct _hash_context));
#ifndef __APPLE__
		OpenSSL_add_all_digests();
#endif // !__APPLE__
	}
	
	if( !CONTEXT(context)->unarchived ){

#ifdef __APPLE__
        CONTEXT(context)->unarchived_cts = digestRef_from_name(opt, &CONTEXT(context)->unarchived_digest_size);
        if (NULL == CONTEXT(context)->unarchived_cts)  return -1;
        strncpy((char *)(CONTEXT(context)->unarchived_ctx_name), opt, MAX_HASH_NAME_LENGTH);
#else
		md = EVP_get_digestbyname(opt);
		if( md == NULL ) return -1;
		EVP_DigestInit(&(CONTEXT(context)->unarchived_cts), md);
#endif // __APPLE__
		CONTEXT(context)->unarchived = 1;	

	}
		
	if( inlen == 0 )
		return 0;
	
	CONTEXT(context)->count += inlen;
#ifdef __APPLE__
    CCDigestUpdate(CONTEXT(context)->unarchived_cts, in, inlen);
#else
	EVP_DigestUpdate(&(CONTEXT(context)->unarchived_cts), in, inlen);
#endif // __APPLE__
	return 0;
}

int32_t xar_hash_archived(xar_t x, xar_file_t f, xar_prop_t p, void **in, size_t *inlen, void **context)
{
	return xar_hash_archived_in(x,f,p,*in,*inlen,context);
}

int32_t xar_hash_archived_in(xar_t x, xar_file_t f, xar_prop_t p, void *in, size_t inlen, void **context) {
	const char *opt;
#ifndef __APPLE__
	const EVP_MD *md;
#endif
	xar_prop_t tmpp;
	
	opt = NULL;
	tmpp = xar_prop_pget(p, "archived-checksum");
	if( tmpp )
		opt = xar_attr_pget(f, tmpp, "style");
	
	if( !opt ) 	
		opt = xar_opt_get(x, XAR_OPT_FILECKSUM);
	
	if( !opt || (0 == strcmp(opt, XAR_OPT_VAL_NONE) ) )
		return 0;
		
	if(!CONTEXT(context)){
		*context = calloc(1,sizeof(struct _hash_context));
#ifndef __APPLE__
		OpenSSL_add_all_digests();
#endif // !__APPLE__
	}
	
	if ( !CONTEXT(context)->archived ){
#ifdef __APPLE__
        CONTEXT(context)->archived_cts = digestRef_from_name(opt, &CONTEXT(context)->archived_digest_size);
        if (NULL == CONTEXT(context)->archived_cts) return -1;
        strncpy((char *)(CONTEXT(context)->archived_ctx_name), opt, MAX_HASH_NAME_LENGTH);
#else
		md = EVP_get_digestbyname(opt);
		if( md == NULL ) return -1;
		EVP_DigestInit(&(CONTEXT(context)->archived_cts), md);	
#endif
		CONTEXT(context)->archived = 1;		
	}

	if( inlen == 0 )
		return 0;

	CONTEXT(context)->count += inlen;
#ifdef __APPLE__
    CCDigestUpdate(CONTEXT(context)->archived_cts, in, inlen);
#else
	EVP_DigestUpdate(&(CONTEXT(context)->archived_cts), in, inlen);
#endif // __APPLE__
	return 0;
}

int32_t xar_hash_done(xar_t x, xar_file_t f, xar_prop_t p, void **context) {
#ifdef __APPLE__
    unsigned char hashstr[CC_SHA512_DIGEST_LENGTH]; // current biggest digest size  This is what OpenSSL uses
#else
	unsigned char hashstr[EVP_MAX_MD_SIZE];
#endif // __APPLE__
	char *str;
	unsigned int len;
	xar_prop_t tmpp;

	if(!CONTEXT(context))
		return 0;

	if( CONTEXT(context)->count == 0 )
		goto DONE;

	if( CONTEXT(context)->unarchived ){
#ifdef __APPLE__
        CCDigestRef ctx  = CONTEXT(context)->unarchived_cts;
        const char *type  = CONTEXT(context)->unarchived_ctx_name;
#else
		EVP_MD_CTX		*ctx = &CONTEXT(context)->unarchived_cts;
		const EVP_MD			*md = EVP_MD_CTX_md(ctx);
        const char *type = EVP_MD_name(md);
#endif // __APPLE__
 
		

		memset(hashstr, 0, sizeof(hashstr));
#ifdef __APPLE__
        CCDigestFinal(CONTEXT(context)->unarchived_cts, hashstr);
        CCDigestDestroy(CONTEXT(context)->unarchived_cts);
        CONTEXT(context)->unarchived_cts = NULL;
        len = CONTEXT(context)->unarchived_digest_size;
#else
		EVP_DigestFinal(&(CONTEXT(context)->unarchived_cts), hashstr, &len);
#endif
		str = xar_format_hash(hashstr,len);
		if( f ) {
			tmpp = xar_prop_pset(f, p, "extracted-checksum", str);
			if( tmpp )
				xar_attr_pset(f, tmpp, "style", type);
		}
		free(str);		
	}

	if( CONTEXT(context)->archived ){
#ifdef __APPLE__
        const char		*type = CONTEXT(context)->archived_ctx_name;
#else
		EVP_MD_CTX				*ctx = &CONTEXT(context)->archived_cts;
		const EVP_MD			*md = EVP_MD_CTX_md(ctx);
		const char		*type = EVP_MD_name(md);
#endif // __APPLE__
		
		memset(hashstr, 0, sizeof(hashstr));
#ifdef __APPLE__
        CCDigestFinal(CONTEXT(context)->archived_cts, hashstr);
        CCDigestDestroy(CONTEXT(context)->archived_cts);
        CONTEXT(context)->archived_cts = NULL;
        len = CONTEXT(context)->archived_digest_size;
#else
		EVP_DigestFinal(&(CONTEXT(context)->archived_cts), hashstr, &len);
#endif
        
		str = xar_format_hash(hashstr,len);
		if( f ) {
			tmpp = xar_prop_pset(f, p, "archived-checksum", str);
			if( tmpp )
				xar_attr_pset(f, tmpp, "style", type);
		}
		free(str);
	}
	
DONE:
	if(*context){
		free(*context);
		*context = NULL;		
	}

	return 0;
}

static char* xar_format_hash(const unsigned char* m,unsigned int len) {
	char* result = malloc((2*len)+1);
	char hexValue[3];
	unsigned int itr = 0;
	
	result[0] = '\0';
	
	for(itr = 0;itr < len;itr++){
		sprintf(hexValue,"%02x",m[itr]);
		strncat(result,hexValue,2);
	}
		
	return result;
}

int32_t xar_hash_out_done(xar_t x, xar_file_t f, xar_prop_t p, void **context) {
    
	const char *uncomp = NULL, *uncompstyle = NULL;
#ifdef __APPLE__
    unsigned char hashstr[CC_SHA512_DIGEST_LENGTH]; // current biggest digest size  This is what OpenSSL uses
#else
	unsigned char hashstr[EVP_MAX_MD_SIZE];
#endif // __APPLE__
    
	unsigned int len;
	char *tmpstr;
#ifndef __APPLE__
	const EVP_MD *md;
#endif
	int32_t err = 0;
	xar_prop_t tmpp;

	if(!CONTEXT(context))
		return 0;

	if( CONTEXT(context)->archived ){	
		tmpp = xar_prop_pget(p, "archived-checksum");
		if( tmpp ) {
			uncompstyle = xar_attr_pget(f, tmpp, "style");
			uncomp = xar_prop_getvalue(tmpp);
		}

#ifdef __APPLE__        
        if (uncomp && uncompstyle && CONTEXT(context)->archived ) {
#else        
		md = EVP_get_digestbyname(uncompstyle);
		if( uncomp && uncompstyle && md && CONTEXT(context)->archived ) {
#endif // __APPLE__

			char *str;
			memset(hashstr, 0, sizeof(hashstr));
#ifdef __APPLE__
            CCDigestFinal(CONTEXT(context)->archived_cts, hashstr);
            CCDigestDestroy(CONTEXT(context)->archived_cts);
            CONTEXT(context)->archived_cts = NULL;
            len = CONTEXT(context)->archived_digest_size;
#else
			EVP_DigestFinal(&(CONTEXT(context)->archived_cts), hashstr, &len);
#endif // __APPLE__
			str = xar_format_hash(hashstr,len);
			if(strcmp(uncomp, str) != 0) {
				xar_err_new(x);
				xar_err_set_file(x, f);
				asprintf(&tmpstr, "archived-checksum %s's do not match",uncompstyle);
				xar_err_set_string(x, tmpstr);
				xar_err_callback(x, XAR_SEVERITY_FATAL, XAR_ERR_ARCHIVE_EXTRACTION);
				err = -1; 
			}
			free(str);
		}
	}
	
	if( CONTEXT(context)->unarchived )
#ifdef __APPLE__
        CCDigestFinal(CONTEXT(context)->unarchived_cts, hashstr);
        CCDigestDestroy(CONTEXT(context)->unarchived_cts);
        CONTEXT(context)->unarchived_cts = NULL;
		len = CONTEXT(context)->unarchived_digest_size;
#else
	    EVP_DigestFinal(&(CONTEXT(context)->unarchived_cts), hashstr, &len);
#endif // __APPLE__

	if(*context){
		free(*context);
		*context = NULL;
	}

	return err;
}
