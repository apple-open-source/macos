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

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: sslutil.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslutil.c    Utility functions for encoding structures

    Handles encoding endian-independant wire representation of 2, 3, or 4
    byte integers.

    ****************************************************************** */

#ifndef	_SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

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
    CASSERT(length > 0 && length <= 4);
    while (length--)                /* Assemble backwards */
    {   p[length] = (UInt8)value;   /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
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
	CASSERT(ctx != NULL);
	switch(ctx->state) {
		case SSLUninitialized:
		case HandshakeServerUninit:
		case HandshakeClientUninit:
		case SSLGracefulClose:
		case SSLErrorClose:
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
	
	CASSERT(ctx != NULL);
	cert=certs;
	while(cert != NULL) {
		nextCert = cert->next;
		SSLFreeBuffer(&cert->derCert, &ctx->sysCtx);
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
 	default: sslPanic("protocolVersStr: bad prot");
 	}
 	return NULL;	/* NOT REACHED */
}

#endif	/* SSL_DEBUG */
