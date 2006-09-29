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

/* log low-level session info in appleSession.cpp */
#define sslLogSessCacheDebug(args...)	secdebug("sslSessionCache", ## args)

/* log record-level I/O (SSLRead, SSLWrite) */
#define sslLogRecordIo(args...)			secdebug("sslRecordIo", ## args)

/* cert-related info */
#define sslCertDebug(args...)			secdebug("sslCert", ## args)

/* Diffie-Hellman */
#define sslDhDebug(args...)				secdebug("sslDh", ## args)

/* EAP-FAST PAC-based session resumption */
#define sslEapDebug(args...)			secdebug("sslEap", ## args)

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

#endif	/* _SSL_DEBUG_H_ */
