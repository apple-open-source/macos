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
	File:		appleGlue.c

	Contains:	Glue layer between Apple SecureTransport and 
				original SSLRef code. 

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_SSL_H_
#include "ssl.h"
#endif

#ifndef	_SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef	_SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef	_APPLE_GLUE_H_
#include "appleGlue.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <time.h>
#include <string.h>

/*
 * Cruft used to map between private SSLErr's and the SSL-specific 
 * OSStatus values in SecureTransport.h. Eventually we should do 
 * away with SSLErr....
 */
typedef struct {
	SSLErr		serr;
	OSStatus	oerr;
} _sslErrMap;

static const _sslErrMap sslErrMap[] = {
	{ SSLNoErr, 					noErr						},
	{ SSLMemoryErr, 				memFullErr					},
	{ SSLUnsupportedErr, 			unimpErr					},
	{ SSLProtocolErr, 				errSSLProtocol				},
	{ SSLNegotiationErr, 			errSSLNegotiation			},
	{ SSLFatalAlert, 				errSSLFatalAlert			},
	{ SSLWouldBlockErr, 			errSSLWouldBlock			},
	{ SSLIOErr, 					ioErr						},
	{ SSLSessionNotFoundErr, 		errSSLSessionNotFound		},
	{ SSLConnectionClosedGraceful, 	errSSLClosedGraceful		},
	{ SSLConnectionClosedError, 	errSSLClosedAbort			},
   	{ X509CertChainInvalidErr, 		errSSLXCertChainInvalid		},
    { SSLBadCert,					errSSLBadCert				},
    { SSLCryptoError,				errSSLCrypto				},
    { SSLInternalError,				errSSLInternal				},
    { SSLDataOverflow,				errSSLCrypto				},
    { SSLAttachFailure,				errSSLModuleAttach			},
    { SSLUnknownRootCert,			errSSLUnknownRootCert		},
    { SSLNoRootCert,				errSSLNoRootCert			},
    { SSLCertExpired,				errSSLCertExpired			},
	{ SSLCertNotYetValid,			errSSLCertNotYetValid		},
    { SSLBadStateErr,				badReqErr					},
    { SSLConnectionClosedNoNotify,	errSSLClosedNoNotify		},
};

#define SIZEOF_ERR_MAP	(sizeof(sslErrMap) / sizeof(_sslErrMap))

/*
 * Functions to allow old code to use SSLBuffer-based I/O calls.
 * We redirect the calls here to an SSL{Write,Read}Func.
 * This is of course way inefficient due to an extra copy for
 * each I/O, but let's do it this way until the port settles down.
 */ 
SSLErr sslIoRead(
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
	return sslErrFromOsStatus(ortn);
 }
 
 SSLErr sslIoWrite(
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
	return sslErrFromOsStatus(ortn);
 }

 /*
  * Convert between SSLErr and OSStatus.
  * These will go away eventually.
  */
SSLErr sslErrFromOsStatus(OSStatus o)
{
	int i;
	const _sslErrMap *emap = sslErrMap;
	
	for(i=0; i<SIZEOF_ERR_MAP; i++) {
		if(emap->oerr == o) {
			return emap->serr;
		}
		emap++;
	}
	return SSLIOErr;			/* normal: bad error */
}
 
OSStatus sslErrToOsStatus(SSLErr s)
{
	int i;
	const _sslErrMap *emap = sslErrMap;
	
	for(i=0; i<SIZEOF_ERR_MAP; i++) {
		if(emap->serr == s) {
			return emap->oerr;
		}
		emap++;
	}
	CASSERT(0);					/* Debug: panic */
	return paramErr;			/* normal: bad error */
}

/*
 * Time functions - replaces SSLRef's SSLTimeFunc, SSLConvertTimeFunc
 */
SSLErr sslTime(UInt32 *tim)
{
	time_t t;
	time(&t);
	*tim = (UInt32)t;
	return SSLNoErr;
}

#ifdef	notdef
/* not used.... */
SSLErr sslConvertTime(UInt32 *time)
{
	return SSLUnsupportedErr;
}
#endif
