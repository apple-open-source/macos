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
	File:		sslDebug.h

	Contains:	Debugging macros.

	Written by:	Doug Mitchell

	Copyright:	(c) 1998, 1999 by Apple Computer, Inc., all rights reserved.
*/

#ifndef	_SSL_DEBUG_H_
#define _SSL_DEBUG_H_

#include "sslContext.h"
#include <security_utilities/debugging.h>
#include <assert.h>

/* Dtrace just ain't working well enough for me to waste my time on it */
#define SSL_DTRACE		0

#if		SSL_DTRACE
/* log changes in handshake state */
#define sslHdskStateDebug(args...)		secdebug("sslHdskState", ## args)

/* log handshake and alert messages */
#define sslHdskMsgDebug(args...)		secdebug("sslHdskMsg", ## args)

/* log negotiated handshake parameters */
#define sslLogNegotiateDebug(args...)	secdebug("sslLogNegotiate", ## args)

/* log received protocol messsages */
#define sslLogRxProtocolDebug(msgType)	secdebug("sslLogRxProtocol", \
										"---received protoMsg %s", msgType)
		
/* log resumable session info */
#define sslLogResumSessDebug(args...)	secdebug("sslResumSession", ## args)

/* log low-level session info in appleSession.c */
#define sslLogSessCacheDebug(args...)	secdebug("sslSessionCache", ## args)

/* log record-level I/O (SSLRead, SSLWrite) */
#define sslLogRecordIo(args...)			secdebug("sslRecordIo", ## args)

/* cert-related info */
#define sslCertDebug(args...)			secdebug("sslCert", ## args)

/* Diffie-Hellman */
#define sslDhDebug(args...)				secdebug("sslDh", ## args)

/* EAP-FAST PAC-based session resumption */
#define sslEapDebug(args...)			secdebug("sslEap", ## args)

/* ECDSA */
#define sslEcdsaDebug(args...)			secdebug("sslEcdsa", ## args)

#else	/* !SSL_DTRACE */

/* the old fashioned way */

#ifdef NDEBUG
/*  deployment build */
#define sslHdskStateDebug(args...)	
#define sslHdskMsgDebug(args...)	
#define sslLogNegotiateDebug(args...)	
#define sslLogNegotiateVerbDebug(args...)	
#define sslLogRxProtocolDebug(msgType)											
#define sslLogResumSessDebug(args...)	
#define sslLogSessCacheDebug(args...)	
#define sslLogRecordIo(args...)			
#define sslCertDebug(args...)			
#define sslDhDebug(args...)				
#define sslEapDebug(args...)			
#define sslEcdsaDebug(args...)			

#else
/* !NDEBUG - tweak these */
#define SD_HSHAKE_STATE		0	/* handshake state */
#define SD_HSHAKE_MSG		0	/* handshakle msg */
#define SD_NEGOTIATE		0	/* negotiated params */
#define SD_NEGOTIATE_VERB	0	/* negotiated params, verbose */
#define SD_RX_PROT			0	/* received protocol msgs */
#define SD_RESUME_SESSION	0	/* resumable session */
#define SD_SESSION_CACHE	0	/* session cache */
#define SD_RECORD_IO		0	/* low level record I/O */
#define SD_CERT				0	/* certificate related */
#define SD_DH				0	/* Diffie-Hellman */
#define SD_EAP				0	/* EAP */
#define SD_ECDSA			0	/* ECDSA */

#define COND_PRINT(cond, args...)	\
	if(cond) {						\
		printf(args);				\
		putchar('\n');				\
	}
	

#define sslHdskStateDebug(args...)			COND_PRINT(SD_HSHAKE_STATE, ## args)
#define sslHdskMsgDebug(args...)			COND_PRINT(SD_HSHAKE_MSG, ## args)
#define sslLogNegotiateDebug(args...)		COND_PRINT(SD_NEGOTIATE, ## args)
#define sslLogNegotiateVerbDebug(args...)	COND_PRINT(SD_NEGOTIATE_VERB, ## args)
#define sslLogRxProtocolDebug(msgType)		COND_PRINT(SD_RX_PROT, \
												"---received protoMsg %s", msgType)
#define sslLogResumSessDebug(args...)		COND_PRINT(SD_RESUME_SESSION, ## args)
#define sslLogSessCacheDebug(args...)		COND_PRINT(SD_SESSION_CACHE, ## args)
#define sslLogRecordIo(args...)				COND_PRINT(SD_RECORD_IO, ## args)
#define sslCertDebug(args...)				COND_PRINT(SD_CERT, ## args)
#define sslDhDebug(args...)					COND_PRINT(SD_DH, ## args)
#define sslEapDebug(args...)				COND_PRINT(SD_EAP, ## args)
#define sslEcdsaDebug(args...)				COND_PRINT(SD_ECDSA, ## args)

#endif	/* !NDEBUG */

#endif	/* SSL_DTRACE */

#ifdef	NDEBUG

#define SSLChangeHdskState(ctx, newState) { ctx->state=newState; }
#define SSLLogHdskMsg(msg, sent)

/* all errors logged to stdout for DEBUG config only */
#define sslErrorLog(args...)			

#else

#include "sslAlertMessage.h"

extern void SSLLogHdskMsg(SSLHandshakeType msg, char sent);
extern char *hdskStateToStr(SSLHandshakeState state);
extern void SSLChangeHdskState(SSLContext *ctx, SSLHandshakeState newState);

/* all errors logged to stdout for DEBUG config only */
#define sslErrorLog(args...)			printf(args)

#endif	/* NDEBUG */


#ifdef	NDEBUG
#define ASSERT(s)
#else
#define ASSERT(s)	assert(s)
#endif

/* Enable Some hacks for ECDSA testing.
 * ALL SHOULD BE REMOVED BEFORE PROJECT COMPLETION.
 * Note that with this OFF, Safari will not be able to reliably access
 *    ECDSA servers due to Radar 6133465. 
 * With this ON, regression tests will fail due to forcing SSLv2 to always
 *    be off. 
 */
#ifdef	NDEBUG
/* ON to make Safari work with ECDSA */
#define SSL_ECDSA_HACK		0
#else
/* normally OFF for regress tests */
#define SSL_ECDSA_HACK		0
#endif

#endif	/* _SSL_DEBUG_H_ */
