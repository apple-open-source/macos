/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * sslUtils.c - Misc. SSL utility functions
 */

#include "sslContext.h"
#include "sslUtils.h"
#include "sslMemory.h"
#include "sslDebug.h"

#include <sys/time.h>

#include <fcntl.h> // for open
#include <unistd.h> // for read
#include <errno.h> // for errno
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>

#ifndef NDEBUG
void SSLDump(const unsigned char *data, unsigned long len)
{
    unsigned long i;
    for(i=0;i<len;i++)
    {
        if((i&0xf)==0) printf("%04lx :",i);
        printf(" %02x", data[i]);
        if((i&0xf)==0xf) printf("\n");
    }
    printf("\n");
}
#endif

unsigned int
SSLDecodeInt(const uint8_t *p, size_t length)
{
    unsigned int val = 0;
    assert(length > 0 && length <= 4); //anything else would be an internal error.
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

uint8_t *
SSLEncodeInt(uint8_t *p, unsigned int value, size_t length)
{
    unsigned char *retVal = p + length; /* Return pointer to char after int */
    assert(length > 0 && length <= 4);  //anything else would be an internal error.
    while (length--)                    /* Assemble backwards */
    {   p[length] = (uint8_t)value;     /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
}

size_t
SSLDecodeSize(const uint8_t *p, size_t length)
{
    unsigned int val = 0;
    assert(length > 0 && length <= 4); //anything else would be an internal error.
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

uint8_t *
SSLEncodeSize(uint8_t *p, size_t value, size_t length)
{
    unsigned char *retVal = p + length; /* Return pointer to char after int */
    assert(length > 0 && length <= 4);  //anything else would be an internal error.
    while (length--)                    /* Assemble backwards */
    {   p[length] = (uint8_t)value;     /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
}


uint8_t *
SSLEncodeUInt64(uint8_t *p, sslUint64 value)
{
    p = SSLEncodeInt(p, value.high, 4);
    return SSLEncodeInt(p, value.low, 4);
}


uint8_t *
SSLEncodeHandshakeHeader(SSLContext *ctx, SSLRecord *rec, SSLHandshakeType type, size_t msglen)
{
    uint8_t *charPtr;

    charPtr = rec->contents.data;
    *charPtr++ = type;
    charPtr = SSLEncodeSize(charPtr, msglen, 3);

    if(rec->protocolVersion == DTLS_Version_1_0) {
        charPtr = SSLEncodeInt(charPtr, ctx->hdskMessageSeq, 2);
        /* fragmentation -- we encode header as if unfragmented,
           actual fragmentation happens at lower layer. */
        charPtr = SSLEncodeInt(charPtr, 0, 3);
        charPtr = SSLEncodeSize(charPtr, msglen, 3);
    }

    return charPtr;
}


void
IncrementUInt64(sslUint64 *v)
{   if (++v->low == 0)          /* Must have just rolled over */
        ++v->high;
}

#if ENABLE_DTLS
void
SSLDecodeUInt64(const uint8_t *p, size_t length, sslUint64 *v)
{
    assert(length > 0 && length <= 8);
    if(length<=4) {
        v->low=SSLDecodeInt(p, length);
        v->high=0;
    } else {
        v->high=SSLDecodeInt(p, length-4);
        v->low=SSLDecodeInt(p+length-4, 4);
    }
}
#endif

#ifdef USE_SSLCERTIFICATE
size_t
SSLGetCertificateChainLength(const SSLCertificate *c)
{
	size_t rtn = 0;

    while (c)
    {
    	rtn++;
        c = c->next;
    }
    return rtn;
}

OSStatus sslDeleteCertificateChain(
    SSLCertificate		*certs,
	SSLContext 			*ctx)
{
	SSLCertificate		*cert;
	SSLCertificate		*nextCert;

	assert(ctx != NULL);
	cert=certs;
	while(cert != NULL) {
		nextCert = cert->next;
		SSLFreeBuffer(&cert->derCert, ctx);
		sslFree(cert);
		cert = nextCert;
	}
	return noErr;
}
#endif /* USE_SSLCERTIFICATE */

Boolean sslIsSessionActive(const SSLContext *ctx)
{
	assert(ctx != NULL);
	switch(ctx->state) {
		case SSL_HdskStateUninit:
		case SSL_HdskStateServerUninit:
		case SSL_HdskStateClientUninit:
		case SSL_HdskStateGracefulClose:
		case SSL_HdskStateErrorClose:
			return false;
		default:
			return true;
	}
}

#if	SSL_DEBUG

const char *protocolVersStr(SSLProtocolVersion prot)
{
	switch(prot) {
 	case SSL_Version_Undetermined: return "SSL_Version_Undetermined";
 	case SSL_Version_2_0: return "SSL_Version_2_0";
 	case SSL_Version_3_0: return "SSL_Version_3_0";
 	case TLS_Version_1_0: return "TLS_Version_1_0";
    case TLS_Version_1_1: return "TLS_Version_1_1";
    case TLS_Version_1_2: return "TLS_Version_1_2";
 	default: sslErrorLog("protocolVersStr: bad prot\n"); return "BAD PROTOCOL";
 	}
 	return NULL;	/* NOT REACHED */
}

#endif	/* SSL_DEBUG */

/*
 * Redirect SSLBuffer-based I/O call to user-supplied I/O.
 */
OSStatus sslIoRead(
 	SSLBuffer 		buf,
 	size_t 			*actualLength,
 	SSLContext 		*ctx)
{
 	size_t 		dataLength = buf.length;
 	OSStatus	ortn;

	*actualLength = 0;
	ortn = (ctx->ioCtx.read)(ctx->ioCtx.ioRef,
		buf.data,
		&dataLength);
	*actualLength = dataLength;
	return ortn;
}

OSStatus sslIoWrite(
 	SSLBuffer 		buf,
 	size_t 			*actualLength,
 	SSLContext 		*ctx)
{
 	size_t 			dataLength = buf.length;
 	OSStatus		ortn;

	*actualLength = 0;
	ortn = (ctx->ioCtx.write)(ctx->ioCtx.ioRef,
		buf.data,
		&dataLength);
	*actualLength = dataLength;
	return ortn;
}

OSStatus sslTime(uint32_t *tim)
{
	time_t t;
	time(&t);
	*tim = (uint32_t)t;
	return noErr;
}

/*
 * Common RNG function.
 */
OSStatus sslRand(SSLContext *ctx, SSLBuffer *buf)
{
	assert(buf != NULL);
	assert(buf->data != NULL);

	if(buf->length == 0) {
		sslErrorLog("sslRand: zero buf->length\n");
		return noErr;
	}

	return SecRandomCopyBytes(kSecRandomDefault, buf->length, buf->data) ? errSSLCrypto : noErr;
}

/*
 * Given a protocol version sent by peer, determine if we accept that version
 * and downgrade if appropriate (which can not be done for the client side).
 */
OSStatus sslVerifyProtVersion(
	SSLContext 			*ctx,
	SSLProtocolVersion	peerVersion,	// sent by peer
	SSLProtocolVersion 	*negVersion)	// final negotiated version if return success
{
    if ((ctx->isDTLS)
        ? peerVersion > ctx->minProtocolVersion
        : peerVersion < ctx->minProtocolVersion) {
        return errSSLNegotiation;
    }
    if ((ctx->isDTLS)
        ? peerVersion < ctx->maxProtocolVersion
        : peerVersion > ctx->maxProtocolVersion) {
        if (ctx->protocolSide == kSSLClientSide) {
            return errSSLNegotiation;
        }
        *negVersion = ctx->maxProtocolVersion;
    } else {
        *negVersion = peerVersion;
    }

    return noErr;
}

/*
 * Determine max enabled protocol, i.e., the one we try to negotiate for.
 * Only returns an error (paramErr) if NO protocols are enabled, which can
 * in fact happen by malicious or ignorant use of SSLSetProtocolVersionEnabled().
 */
OSStatus sslGetMaxProtVersion(
	SSLContext 			*ctx,
	SSLProtocolVersion	*version)	// RETURNED
{
    /* This check is here until SSLSetProtocolVersionEnabled() is gone .*/
    if (ctx->maxProtocolVersion == SSL_Version_Undetermined)
        return badReqErr;

    *version = ctx->maxProtocolVersion;
    return noErr;
}
