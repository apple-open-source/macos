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

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: ssl2.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ssl2.h       SSL 2 functionality header

    This file contains function prototypes and equate values for SSL2.
    The relevant functions are contained in files whose names match
    ssl2*.c

    ****************************************************************** */

#ifndef _SSL2_H_
#define _SSL2_H_

#ifndef _SECURE_TRANSPORT_H_
#include "SecureTransport.h"
#endif

#ifndef	_SSL_PRIV_H_
#include "sslPriv.h"
#endif

#ifndef _SSLREC_H_
#include "sslrec.h"
#endif

typedef enum
{   ssl2_mt_error = 0,
    ssl2_mt_client_hello = 1,
    ssl2_mt_client_master_key = 2,
    ssl2_mt_client_finished = 3,
    ssl2_mt_server_hello = 4,
    ssl2_mt_server_verify = 5,
    ssl2_mt_server_finished = 6,
    ssl2_mt_request_certificate = 7,
    ssl2_mt_client_certificate = 8,
    ssl2_mt_kickstart_handshake = 99
} SSL2MessageType;

typedef enum
{   ssl2_pe_no_cipher = 1,
    ssl2_pe_no_certificate = 2,
    ssl2_pe_bad_certificate = 4,
    ssl2_pe_unsupported_certificate_type = 6
} SSL2ErrorCode;

typedef enum
{   ssl2_ct_x509_certificate = 1
} SSL2CertTypeCode;

#define SSL2_CONNECTION_ID_LENGTH   16

typedef SSLErr (*EncodeSSL2MessageFunc)(SSLBuffer *msg, SSLContext *ctx);
SSLErr SSL2ReadRecord(SSLRecord *rec, SSLContext *ctx);
SSLErr SSL2WriteRecord(SSLRecord rec, SSLContext *ctx);
SSLErr SSL2ProcessMessage(SSLRecord rec, SSLContext *ctx);
SSLErr SSL2SendError(SSL2ErrorCode error, SSLContext *ctx);
SSLErr SSL2AdvanceHandshake(SSL2MessageType msg, SSLContext *ctx);
SSLErr SSL2PrepareAndQueueMessage(EncodeSSL2MessageFunc encodeFunc, SSLContext *ctx);
SSLErr SSL2CompareSessionIDs(SSLContext *ctx);
SSLErr SSL2InstallSessionKey(SSLContext *ctx);
SSLErr SSL2GenerateSessionID(SSLContext *ctx);
SSLErr SSL2InitCiphers(SSLContext *ctx);

SSLErr SSL2ProcessClientHello(SSLBuffer msgContents, SSLContext *ctx);
SSLErr SSL2EncodeClientHello(SSLBuffer *msg, SSLContext *ctx);
SSLErr SSL2ProcessClientMasterKey(SSLBuffer msgContents, SSLContext *ctx);
SSLErr SSL2EncodeClientMasterKey(SSLBuffer *msg, SSLContext *ctx);
SSLErr SSL2ProcessClientFinished(SSLBuffer msgContents, SSLContext *ctx);
SSLErr SSL2EncodeClientFinished(SSLBuffer *msg, SSLContext *ctx);
SSLErr SSL2ProcessServerHello(SSLBuffer msgContents, SSLContext *ctx);
SSLErr SSL2EncodeServerHello(SSLBuffer *msg, SSLContext *ctx);
SSLErr SSL2ProcessServerVerify(SSLBuffer msgContents, SSLContext *ctx);
SSLErr SSL2EncodeServerVerify(SSLBuffer *msg, SSLContext *ctx);
SSLErr SSL2ProcessServerFinished(SSLBuffer msgContents, SSLContext *ctx);
SSLErr SSL2EncodeServerFinished(SSLBuffer *msg, SSLContext *ctx);

#endif
