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
	File:		ssl2.h

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef _SSL2_H_
#define _SSL2_H_

#include "SecureTransport.h"
#include "sslPriv.h"
#include "sslRecord.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {   
	SSL2_MsgError = 0,
    SSL2_MsgClientHello = 1,
    SSL2_MsgClientMasterKey = 2,
    SSL2_MsgClientFinished = 3,
    SSL2_MsgServerHello = 4,
    SSL2_MsgServerVerify = 5,
    SSL2_MsgServerFinished = 6,
    SSL2_MsgRequestCert = 7,
    SSL2_MsgClientCert = 8,
    SSL2_MsgKickstart = 99
} SSL2MessageType;

typedef enum {   
	SSL2_ErrNoCipher = 1,
    SSL2_ErrNoCert = 2,
    SSL2_ErrBadCert = 4,
    SSL2_ErrUnsupportedCert = 6
} SSL2ErrorCode;

typedef enum{   
	SSL2_CertTypeX509 = 1
} SSL2CertTypeCode;

#define SSL2_CONNECTION_ID_LENGTH   16

typedef OSStatus (*EncodeSSL2MessageFunc)(SSLBuffer &msg, SSLContext *ctx);
OSStatus SSL2ReadRecord(SSLRecord &rec, SSLContext *ctx);
OSStatus SSL2WriteRecord(SSLRecord &rec, SSLContext *ctx);
OSStatus SSL2ProcessMessage(SSLRecord &rec, SSLContext *ctx);
OSStatus SSL2SendError(SSL2ErrorCode error, SSLContext *ctx);
OSStatus SSL2AdvanceHandshake(SSL2MessageType msg, SSLContext *ctx);
OSStatus SSL2PrepareAndQueueMessage(EncodeSSL2MessageFunc encodeFunc, SSLContext *ctx);
OSStatus SSL2CompareSessionIDs(SSLContext *ctx);
OSStatus SSL2InstallSessionKey(SSLContext *ctx);
OSStatus SSL2GenerateSessionID(SSLContext *ctx);
OSStatus SSL2InitCiphers(SSLContext *ctx);

OSStatus SSL2ProcessClientHello(SSLBuffer msgContents, SSLContext *ctx);
OSStatus SSL2EncodeClientHello(SSLBuffer &msg, SSLContext *ctx);
OSStatus SSL2ProcessClientMasterKey(SSLBuffer msgContents, SSLContext *ctx);
OSStatus SSL2EncodeClientMasterKey(SSLBuffer &msg, SSLContext *ctx);
OSStatus SSL2ProcessClientFinished(SSLBuffer msgContents, SSLContext *ctx);
OSStatus SSL2EncodeClientFinished(SSLBuffer &msg, SSLContext *ctx);
OSStatus SSL2ProcessServerHello(SSLBuffer msgContents, SSLContext *ctx);
OSStatus SSL2EncodeServerHello(SSLBuffer &msg, SSLContext *ctx);
OSStatus SSL2ProcessServerVerify(SSLBuffer msgContents, SSLContext *ctx);
OSStatus SSL2EncodeServerVerify(SSLBuffer &msg, SSLContext *ctx);
OSStatus SSL2ProcessServerFinished(SSLBuffer msgContents, SSLContext *ctx);
OSStatus SSL2EncodeServerFinished(SSLBuffer &msg, SSLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
