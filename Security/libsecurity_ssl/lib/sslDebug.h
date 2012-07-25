/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslDebug.h - Debugging macros.
 */

#ifndef	_SSL_DEBUG_H_
#define _SSL_DEBUG_H_

#include "sslContext.h"
#include <security_utilities/debugging.h>
#include <assert.h>

#ifndef	NDEBUG
#include <stdio.h>
#endif

/* If SSL_USE_SYSLOG is defined and not 0, use syslog() for debug
 * logging in addition to invoking the secdebug macro (which, as of
 * Snow Leopard, emits a static dtrace probe instead of an actual
 * log message.)
 */
#ifndef SSL_USE_SYSLOG
#define SSL_USE_SYSLOG	0
#endif

#if SSL_USE_SYSLOG
#include <syslog.h>
#define ssl_secdebug(scope, format...) \
{ \
	syslog(LOG_NOTICE, format); \
	secdebug(scope, format); \
}
#else
#define ssl_secdebug(scope, format...) \
	secdebug(scope, format)
#endif

#ifndef NDEBUG

/* log changes in handshake state */
#define sslHdskStateDebug(args...)		ssl_secdebug("sslHdskState", ## args)

/* log handshake and alert messages */
#define sslHdskMsgDebug(args...)		ssl_secdebug("sslHdskMsg", ## args)

/* log negotiated handshake parameters */
#define sslLogNegotiateDebug(args...)	ssl_secdebug("sslLogNegotiate", ## args)

/* log received protocol messsages */
#define sslLogRxProtocolDebug(msgType)	ssl_secdebug("sslLogRxProtocol", \
										"---received protoMsg %s", msgType)

/* log resumable session info */
#define sslLogResumSessDebug(args...)	ssl_secdebug("sslResumSession", ## args)

/* log low-level session info in appleSession.c */
#define sslLogSessCacheDebug(args...)	ssl_secdebug("sslSessionCache", ## args)

/* log record-level I/O (SSLRead, SSLWrite) */
#define sslLogRecordIo(args...)			ssl_secdebug("sslRecordIo", ## args)

/* cert-related info */
#define sslCertDebug(args...)			ssl_secdebug("sslCert", ## args)

/* Diffie-Hellman */
#define sslDhDebug(args...)				ssl_secdebug("sslDh", ## args)

/* EAP-FAST PAC-based session resumption */
#define sslEapDebug(args...)			ssl_secdebug("sslEap", ## args)

/* ECDSA */
#define sslEcdsaDebug(args...)			ssl_secdebug("sslEcdsa", ## args)

#else /* NDEBUG */

/*  deployment build */
#define sslHdskStateDebug(args...)
#define sslHdskMsgDebug(args...)
#define sslLogNegotiateDebug(args...)
#define sslLogRxProtocolDebug(msgType)
#define sslLogResumSessDebug(args...)
#define sslLogSessCacheDebug(args...)
#define sslLogRecordIo(args...)
#define sslCertDebug(args...)
#define sslDhDebug(args...)
#define sslEapDebug(args...)
#define sslEcdsaDebug(args...)

#endif	/*
NDEBUG */

#ifdef	NDEBUG

#define SSLChangeHdskState(ctx, newState) { ctx->state=newState; }
#define SSLLogHdskMsg(msg, sent)

/* all errors logged to stdout for DEBUG config only */
#define sslErrorLog(args...)
#define sslDebugLog(args...)
#define sslDump(d, l)

#else

#include "sslAlertMessage.h"

extern void SSLLogHdskMsg(SSLHandshakeType msg, char sent);
extern char *hdskStateToStr(SSLHandshakeState state);
extern void SSLChangeHdskState(SSLContext *ctx, SSLHandshakeState newState);
extern void SSLDump(const unsigned char *data, unsigned long len);

/* extra debug logging of non-error conditions, if SSL_DEBUG is defined */
#if SSL_DEBUG
//#define sslDebugLog(args...)        printf(args)
#define sslDebugLog(args...)        ssl_secdebug("sslDebug", ## args)
#else
#define sslDebugLog(args...)
#endif
/* all errors logged to stdout for DEBUG config only */
//#define sslErrorLog(args...)        printf(args)
#define sslErrorLog(args...)        ssl_secdebug("sslError", ## args)
#define sslDump(d, l)               SSLDump((d), (l))

#endif	/* NDEBUG */


#ifdef	NDEBUG
#define ASSERT(s)
#else
#define ASSERT(s)	assert(s)
#endif

#endif	/* _SSL_DEBUG_H_ */
