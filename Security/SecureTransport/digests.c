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
	File:		digests.c

	Contains:	interface between SSL and SHA, MD5 digest libraries

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: digests.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: digests.c    Hashing support functions and data structures

    Contains interface functions which generalize hashing support for MD5
    and SHA1 and a dummy null hash implementation (used before MACing is
    turned on). Also, utility functions for using the hashes.

    ****************************************************************** */

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _CRYPTTYPE_H_
#include "cryptType.h"
#endif

#ifndef SHA_H
#include <stdio.h>      /* sha.h has a prototype with a FILE* */
#include "st_sha.h"
#endif

#ifndef	_SSL_MD5_H_
#include "sslmd5.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef	_DIGESTS_H_
#include "digests.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#include <string.h>

typedef struct
{   SHA_INFO    sha;
    int         bufferPos;
    uint8       dataBuffer[SHA_BLOCKSIZE];
} SSL_SHA_INFO;

uint8   SSLMACPad1[MAX_MAC_PADDING], SSLMACPad2[MAX_MAC_PADDING];

/*
 * Public general hash functions 
 */
void
SSLInitMACPads(void)
{   int     i;
    
    for (i = 0; i < MAX_MAC_PADDING; i++)
    {   SSLMACPad1[i] = 0x36;
        SSLMACPad2[i] = 0x5C;
    }
}

/* FIXME - what's this for, if each alg has its own clone functions? */
SSLErr
CloneHashState(const HashReference *ref, SSLBuffer state, SSLBuffer *newState, SSLContext *ctx)
{   SSLErr      err;
    if ((err = SSLAllocBuffer(newState, state.length, &ctx->sysCtx)) != 0)
        return err;
    memcpy(newState->data, state.data, state.length);
    return SSLNoErr;
}

SSLErr
ReadyHash(const HashReference *ref, SSLBuffer *state, SSLContext *ctx)
{   SSLErr      err;
    if ((err = SSLAllocBuffer(state, ref->contextSize, &ctx->sysCtx)) != 0)
        return err;
    if ((err = ref->init(*state)) != 0)
        return err;
    return SSLNoErr;
}

static SSLErr HashNullInit(SSLBuffer);
static SSLErr HashNullUpdate(SSLBuffer,SSLBuffer);
static SSLErr HashNullFinal(SSLBuffer,SSLBuffer);
static SSLErr HashNullClone(SSLBuffer,SSLBuffer);

static SSLErr HashMD5Init(SSLBuffer digestCtx);
static SSLErr HashMD5Update(SSLBuffer digestCtx, SSLBuffer data);
static SSLErr HashMD5Final(SSLBuffer digestCtx, SSLBuffer digest);
static SSLErr HashMD5Clone(SSLBuffer src, SSLBuffer dest);

static SSLErr HashSHA1Init(SSLBuffer digestCtx);
static SSLErr HashSHA1Update(SSLBuffer digestCtx, SSLBuffer data);
static SSLErr HashSHA1Final(SSLBuffer digestCtx, SSLBuffer digest);
static SSLErr HashSHA1Clone(SSLBuffer src, SSLBuffer dest);

/*
 * These are the handles by which the bulk of digesting work
 * is done.
 */
const HashReference SSLHashNull = 
	{ 	
		0, 
	  	0, 
	  	0, 
	  	HashNullInit, 
	  	HashNullUpdate, 
	  	HashNullFinal, 
	  	HashNullClone 
	};
	
const HashReference SSLHashMD5 = 
	{ 
		sizeof(MD5_CTX), 
		16, 
		48, 
		HashMD5Init, 
		HashMD5Update, 
		HashMD5Final, 
		HashMD5Clone 
	};

const HashReference SSLHashSHA1 = 
	{ 
		sizeof(SSL_SHA_INFO), 
		20, 
		40, 
		HashSHA1Init, 
		HashSHA1Update, 
		HashSHA1Final, 
		HashSHA1Clone 
	};

/*** NULL ***/
static SSLErr HashNullInit(SSLBuffer digestCtx) { 
	return SSLNoErr; 
}

static SSLErr HashNullUpdate(SSLBuffer digestCtx, SSLBuffer data) { 
	return SSLNoErr; 
}

static SSLErr HashNullFinal(SSLBuffer digestCtx, SSLBuffer digest) { 
	return SSLNoErr; 
}

static SSLErr HashNullClone(SSLBuffer src, SSLBuffer dest) { 
	return SSLNoErr; 
}

/*** MD5 ***/

static SSLErr HashMD5Init(SSLBuffer digestCtx)
{   CASSERT(digestCtx.length >= sizeof(MD5_CTX));
    SSLMD5Init((MD5_CTX*)digestCtx.data);
    return SSLNoErr;
}

static SSLErr HashMD5Update(SSLBuffer digestCtx, SSLBuffer data)
{   CASSERT(digestCtx.length >= sizeof(MD5_CTX));
    SSLMD5Update((MD5_CTX*)digestCtx.data, data.data, data.length);
    return SSLNoErr;
}

static SSLErr HashMD5Final(SSLBuffer digestCtx, SSLBuffer digest)
{   CASSERT(digestCtx.length >= sizeof(MD5_CTX));
    CASSERT(digest.length >= 16);
    SSLMD5Final(digest.data, (MD5_CTX*)digestCtx.data);
    digest.length = 16;
    return SSLNoErr;
}

static SSLErr HashMD5Clone(SSLBuffer src, SSLBuffer dest)
{   
	if (src.length != dest.length) {
		errorLog0("HashMD5Clone: length mismatch\n");
        return SSLProtocolErr;
    }
    memcpy(dest.data, src.data, src.length);
    return SSLNoErr;
}   

/*** SHA ***/
static SSLErr HashSHA1Init(SSLBuffer digestCtx)
{   SSL_SHA_INFO    *ctx = (SSL_SHA_INFO*)digestCtx.data;
    CASSERT(digestCtx.length >= sizeof(SSL_SHA_INFO));
    sha_init(&ctx->sha);
    ctx->bufferPos = 0;
    return SSLNoErr;
}

static SSLErr HashSHA1Update(SSLBuffer digestCtx, SSLBuffer data)
{   SSL_SHA_INFO    *ctx = (SSL_SHA_INFO*)digestCtx.data;
    uint32          dataRemaining, processed;
    uint8           *dataPos;
    
    CASSERT(digestCtx.length >= sizeof(SSL_SHA_INFO));
    dataRemaining = data.length;
    dataPos = data.data;
    while (dataRemaining > 0)
    {   processed = SHA_BLOCKSIZE - ctx->bufferPos;
        if (dataRemaining < processed)
            processed = dataRemaining;
        memcpy(ctx->dataBuffer+ctx->bufferPos, dataPos, processed);
        ctx->bufferPos += processed;
        if (ctx->bufferPos == SHA_BLOCKSIZE)
        {   sha_update(&ctx->sha, ctx->dataBuffer, ctx->bufferPos);
            ctx->bufferPos = 0;
        }
        dataRemaining -= processed;
        dataPos += processed;
    }
    //DUMP_BUFFER_PTR("SHA1 data", digestCtx.data, data);
    return SSLNoErr;
}

static SSLErr HashSHA1Final(SSLBuffer digestCtx, SSLBuffer digest)
{   SSL_SHA_INFO    *ctx = (SSL_SHA_INFO*)digestCtx.data;
    CASSERT(digestCtx.length >= sizeof(SSL_SHA_INFO));
    CASSERT(digest.length >= SHA_DIGESTSIZE);
    if (ctx->bufferPos > 0)
        sha_update(&ctx->sha, ctx->dataBuffer, ctx->bufferPos);
    sha_final((SHA_INFO*)digestCtx.data);
    memcpy(digest.data, ((SHA_INFO*)digestCtx.data)->digest, 20);
    //DUMP_BUFFER_PTR("SHA1 final", digestCtx.data, digest);
    return SSLNoErr;
}

static SSLErr HashSHA1Clone(SSLBuffer src, SSLBuffer dest)
{   if (src.length != dest.length) {
		errorLog0("HashSHA1Clone: length mismatch\n");
        return SSLProtocolErr;
    }
    memcpy(dest.data, src.data, src.length);
    return SSLNoErr;
}   
