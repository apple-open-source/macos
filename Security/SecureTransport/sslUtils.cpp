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
	File:		sslutil.c

	Contains:	Misc. SSL utility functions

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "sslUtils.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include <Security/devrandom.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <sys/time.h>

UInt32
SSLDecodeInt(const unsigned char *p, int length)
{   UInt32  val = 0;
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

unsigned char *
SSLEncodeInt(unsigned char *p, UInt32 value, int length)
{   unsigned char   *retVal = p + length;       /* Return pointer to char after int */
    assert(length > 0 && length <= 4);
    while (length--)                /* Assemble backwards */
    {   p[length] = (UInt8)value;   /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
}

UInt8*
SSLEncodeUInt64(UInt8 *p, sslUint64 value)
{   p = SSLEncodeInt(p, value.high, 4);
    return SSLEncodeInt(p, value.low, 4);
}


void
IncrementUInt64(sslUint64 *v)
{   if (++v->low == 0)          /* Must have just rolled over */
        ++v->high;
}

UInt32
SSLGetCertificateChainLength(const SSLCertificate *c)
{   
	UInt32 rtn = 0;
	
    while (c)
    {   
    	rtn++;
        c = c->next;
    }
    return rtn;
}

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
		SSLFreeBuffer(cert->derCert, ctx);
		sslFree(cert);
		cert = nextCert;
	}
	return noErr;
}

#if	SSL_DEBUG

const char *protocolVersStr(SSLProtocolVersion prot)
{
	switch(prot) {
 	case SSL_Version_Undetermined: return "SSL_Version_Undetermined";
 	case SSL_Version_3_0_With_2_0_Hello: return "SSL_Version_3_0_With_2_0_Hello";
 	case SSL_Version_3_0_Only: return "SSL_Version_3_0_Only";
 	case SSL_Version_2_0: return "SSL_Version_2_0";
 	case SSL_Version_3_0: return "SSL_Version_3_0";
 	case TLS_Version_1_0: return "TLS_Version_1_0";
 	case TLS_Version_1_0_Only: return "TLS_Version_1_0_Only";
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
 	UInt32 		dataLength = buf.length;
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
 	UInt32 			dataLength = buf.length;
 	OSStatus		ortn;
 		
	*actualLength = 0;
	ortn = (ctx->ioCtx.write)(ctx->ioCtx.ioRef,
		buf.data,
		&dataLength);
	*actualLength = dataLength;
	return ortn;
}

OSStatus sslTime(UInt32 *tim)
{
	time_t t;
	time(&t);
	*tim = (UInt32)t;
	return noErr;
}

/*
 * Common RNG function.
 */
OSStatus sslRand(SSLContext *ctx, SSLBuffer *buf)
{
	OSStatus		serr = noErr;
	
	assert(ctx != NULL);
	assert(buf != NULL);
	assert(buf->data != NULL);
	
	if(buf->length == 0) {
		sslErrorLog("sslRand: zero buf->length\n");
		return noErr;
	}
	try {
		Security::DevRandomGenerator devRand(false);
		devRand.random(buf->data, buf->length);
	}
	catch(...) {
		serr = errSSLCrypto;
	}
	return serr;
}

