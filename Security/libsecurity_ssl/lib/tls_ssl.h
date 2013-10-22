/*
 * Copyright (c) 2002,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * tls_ssl.h - Declarations of handshake layer callout struct to provide indirect calls to
 *     SSLv3 and TLS routines. 
 */

#ifndef	_TLS_SSL_H_
#define _TLS_SSL_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include "ssl.h"
#include "sslPriv.h"
#include "sslContext.h"
#include "sslRecord.h"

/***
 *** Each of {TLS, SSLv3} implements each of these functions.
 ***/

typedef OSStatus (*generateKeyMaterialFcn) (
	SSLBuffer key, 					// caller mallocs and specifies length of
									//   required key material here
	SSLContext *ctx);

typedef OSStatus (*generateExportKeyAndIvFcn) (
	SSLContext *ctx,				// clientRandom, serverRandom valid
	const SSLBuffer clientWriteKey,
	const SSLBuffer serverWriteKey,
	SSLBuffer finalClientWriteKey,	// RETURNED, mallocd by caller
	SSLBuffer finalServerWriteKey,	// RETURNED, mallocd by caller
	SSLBuffer finalClientIV,		// RETURNED, mallocd by caller
	SSLBuffer finalServerIV);		// RETURNED, mallocd by caller

/*
 * On entry: clientRandom, serverRandom, preMasterSecret valid
 * On return: masterSecret valid
 */
typedef OSStatus (*generateMasterSecretFcn) (
	SSLContext *ctx);

typedef OSStatus (*computeFinishedMacFcn) (
	SSLContext *ctx,
	SSLBuffer finished, 		// output - mallocd by caller
	Boolean isServer);

typedef OSStatus (*computeCertVfyMacFcn) (
	SSLContext *ctx,
    SSLBuffer *finished,		// output - mallocd by caller
    SSL_HashAlgorithm hash);    //only used in TLS 1.2


typedef struct _SslTlsCallouts {
	generateKeyMaterialFcn		generateKeyMaterial;
	generateMasterSecretFcn		generateMasterSecret;
	computeFinishedMacFcn		computeFinishedMac;
	computeCertVfyMacFcn		computeCertVfyMac;
} SslTlsCallouts;


/* From ssl3Callouts.c and tls1Callouts.c */
extern const SslTlsCallouts	Ssl3Callouts;
extern const SslTlsCallouts	Tls1Callouts;
extern const SslTlsCallouts Tls12Callouts;

#ifdef	__cplusplus
}
#endif

#endif 	/* _TLS_SSL_H_ */
