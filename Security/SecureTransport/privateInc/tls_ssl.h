/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
	File:		tls_ssl.h

	Contains:	Declarations of callout struct to provide indirect calls to
				SSLv3 and TLS routines. 

	Written by:	Doug Mitchell
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
#include "cryptType.h"

/***
 *** Each of {TLS, SSLv3} implements each of these functions.
 ***/

/* unpack, decrypt, validate one record */
typedef OSStatus (*decryptRecordFcn) (
	UInt8 type, 
	SSLBuffer *payload, 
	SSLContext *ctx);
	
/* pack, encrypt, mac, queue one outgoing record */
typedef OSStatus (*writeRecordFcn) (
	SSLRecord rec, 
	SSLContext *ctx);
	
/* initialize a per-CipherContext HashHmacContext for use in MACing each record */
typedef OSStatus (*initMacFcn) (
	CipherContext *cipherCtx,		// macRef, macSecret valid on entry
									// macCtx valid on return
	SSLContext *ctx);
	
/* free per-CipherContext HashHmacContext */
typedef OSStatus (*freeMacFcn) (
	CipherContext *cipherCtx);
	
/* compute MAC on one record */
typedef OSStatus (*computeMacFcn) (
	UInt8 type, 
	SSLBuffer data, 			
	SSLBuffer mac, 					// caller mallocs data
	CipherContext *cipherCtx,		// assumes macCtx, macRef
	sslUint64 seqNo, 
	SSLContext *ctx);
	
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
	SSLBuffer shaMsgState,		// clone of running digest of all handshake msgs
	SSLBuffer md5MsgState, 		// ditto
	Boolean isServer);

typedef OSStatus (*computeCertVfyMacFcn) (
	SSLContext *ctx,
	SSLBuffer finished, 		// output - mallocd by caller 
	SSLBuffer shaMsgState,		// clone of running digest of all handshake msgs
	SSLBuffer md5MsgState); 	// ditto

typedef struct _SslTlsCallouts {
	decryptRecordFcn			decryptRecord;
	writeRecordFcn				writeRecord; 
	initMacFcn					initMac;
	freeMacFcn					freeMac;
	computeMacFcn				computeMac;
	generateKeyMaterialFcn		generateKeyMaterial;
	generateExportKeyAndIvFcn	generateExportKeyAndIv;
	generateMasterSecretFcn		generateMasterSecret;
	computeFinishedMacFcn		computeFinishedMac;
	computeCertVfyMacFcn		computeCertVfyMac;
} SslTlsCallouts;

/* From ssl3Callouts.c and tls1Callouts.c */
extern const SslTlsCallouts	Ssl3Callouts;
extern const SslTlsCallouts	Tls1Callouts;

/* one callout routine used in common (for now) */
OSStatus ssl3WriteRecord(
	SSLRecord rec, 
	SSLContext *ctx);

#ifdef	__cplusplus
}
#endif	

#endif 	/* _TLS_SSL_H_ */
