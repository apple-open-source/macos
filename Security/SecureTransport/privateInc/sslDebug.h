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

	Change History (most recent first):

		03/10/98	dpm		Created.

*/

#ifndef	_SSL_DEBUG_H_
#define _SSL_DEBUG_H_

#include "sslBuildFlags.h"

#if		SSL_DEBUG || ERROR_LOG_ENABLE

/* any other way? */
#define LOG_VIA_PRINTF		1

#include <stdio.h>
#include <stdlib.h>

#if		!LOG_VIA_PRINTF

#error Hey, figure out a debug mechanism

#include <string.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <TextUtils.h>

/* common log macros */

/* remaining ones can take constant strings */

#ifdef	__cplusplus
extern "C" {
#endif

extern void dblog0(char *str);
extern void dblog1(char *str, void * arg1);
extern void dblog2(char *str, void * arg1, void * arg2);
extern void dblog3(char *str, void * arg1, void * arg2, void * arg3);
extern void dblog4(char *str, void * arg1, void * arg2, void * arg3, void * arg4);

#ifdef	__cplusplus
}
#endif


#else	/* LOG_VIA_PRINTF */

#define dblog0(str)								printf(str)
#define dblog1(str, arg1)						printf(str, arg1)
#define dblog2(str, arg1, arg2)					printf(str, arg1, arg2)
#define dblog3(str, arg1, arg2, arg3)			printf(str, arg1, arg2, arg3)
#define dblog4(str, arg1, arg2, arg3, arg4)		printf(str, arg1, arg2, arg3, arg4)

#endif	/* LOG_VIA_PRINTF */

#else	/* log macros disabled */

#define dblog0(str)
#define dblog1(str, arg1)
#define dblog2(str, arg1, arg2)
#define dblog3(str, arg1, arg2, arg3)
#define dblog4(str, arg1, arg2, arg3, arg4)

#endif	/* SSL_DEBUG || ERROR_LOG_ENABLE */

#if	SSL_DEBUG

#define dprintf0(str)								dblog0(str)
#define dprintf1(str, arg1)							dblog1(str, arg1)
#define dprintf2(str, arg1, arg2)					dblog2(str, arg1, arg2)
#define dprintf3(str, arg1, arg2, arg3)				dblog3(str, arg1, arg2, arg3)
#define dprintf4(str, arg1, arg2, arg3, arg4)		dblog4(str, arg1, arg2, arg3,  arg4)

#ifdef	__cplusplus
extern "C" {
#endif

static inline volatile void sslPanic(const char *str)
{
	printf(str);
	exit(1);
}

#ifdef	__cplusplus
}
#endif

#define CASSERT(expression) 							\
  ((expression) ? (void)0 : 							\
   (dprintf1 ("Assertion failed: " #expression 			\
      ", file " __FILE__ ", line %d.\n", __LINE__), 	\
    sslPanic("Assertion Failure")))

#else	/* SSL_DEBUG */

#define dprintf0(str)
#define dprintf1(str, arg1)
#define dprintf2(str, arg1, arg2)
#define dprintf3(str, arg1, arg2, arg3)
#define dprintf4(str, arg1, arg2, arg3, arg4)

#define CASSERT(expression)
#define sslPanic(s)
#endif	/* SSL_DEBUG */

/*
 * Error logging. This may well be platform dependent.
 */
#if		ERROR_LOG_ENABLE
#define errorLog0(str)								dblog0(str);
#define errorLog1(str, arg1)						dblog1(str, arg1)
#define errorLog2(str, arg1, arg2)					dblog2(str, arg1, arg2)
#define errorLog3(str, arg1, arg2, arg3)			dblog3(str, arg1, arg2, arg3)
#define errorLog4(str, arg1, arg2, arg3, arg4)		dblog4(str, arg1, arg2, arg3, arg4)

#else	/* ERROR_LOG_ENABLE */

#define errorLog0(str)
#define errorLog1(str, arg1)
#define errorLog2(str, arg1, arg2)
#define errorLog3(str, arg1, arg2, arg3)
#define errorLog4(str, arg1, arg2, arg3, arg4)

#endif	/* ERROR_LOG_ENABLE */

/*
 * Override SSLRef macros
 */
#define ERR(x)							(x)
#define DUMP_BUFFER_NAME(name, buf)
#define DUMP_DATA_NAME(name, p, len)
#define ASSERTMSG(m)					sslPanic(m)
#define DEBUGVAL1(str, arg)				errorLog1(str, arg)

/*** SSL-Specific debugging ***/

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

/* Logging Enable Flags */

#if		SSL_DEBUG

/* log changes in handshake state */
#define LOG_HDSK_STATE		0

/* log handshake messages */
#define LOG_HDSK_MSG		0

/* log negotiated handshake parameters */
#define LOG_NEGOTIATE		0

/* log received protocol messsages */
#define LOG_RX_PROTOCOL		0

/* log resumable session info */
#define LOG_RESUM_SESSION 	0

#else	/* !SSL_DEBUG - normal build - all flags disabled */
#define LOG_HDSK_STATE		0
#define LOG_HDSK_MSG		0 
#define LOG_NEGOTIATE		0
#define LOG_RX_PROTOCOL		0
#define LOG_RESUM_SESSION	0
#endif	/* SSL_DEBUG */

#if		LOG_HDSK_STATE
extern void SSLChangeHdskState(SSLContext *ctx, SSLHandshakeState newState);
#else	/* LOG_HDSK_STATE */
#define SSLChangeHdskState(ctx, newState) { ctx->state=newState; }
#endif	/* LOG_HDSK_STATE */

#if		LOG_HDSK_MSG
extern void SSLLogHdskMsg(SSLHandshakeType msg, char sent);
extern char *hdskStateToStr(SSLHandshakeState state);
#else	/* LOG_HDSK_STATE */
#define SSLLogHdskMsg(msg, sent)
#endif	/* LOG_HDSK_STATE */

#if		LOG_RESUM_SESSION
#define SSLLogResumSess(m)	printf(m)
#else
#define SSLLogResumSess(m)
#endif	/* LOG_RESUM_SESSION */

/* 
 * A crufty little routine to write cert blobs to disk.
 * Implemented in appleCdsa.c.
 */
#if	SSL_DEBUG
extern void writeBufBlob(const SSLBuffer *blob, 
	const char *fileName);
#endif

#endif	/* _SSL_DEBUG_H_ */
