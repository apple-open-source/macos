/*
 * Copyright (c) 1999-2001,2005-2007,2010-2014 Apple Inc. All Rights Reserved.
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
 * sslRecord.c - Encryption, decryption and MACing of data
*/

#include <SecureTransport.h>
#include "ssl.h"
#include "sslRecord.h"
#include "sslMemory.h"
#include "sslContext.h"
#include "sslDebug.h"
#include "SSLRecordInternal.h"

#include <string.h>
#include <assert.h>

#include <utilities/SecIOFormat.h>

/*
 * Lots of servers fail to provide closure alerts when they disconnect.
 * For now we'll just accept it as long as it occurs on a clean record boundary
 * (and the handshake is complete).
 */
#define SSL_ALLOW_UNNOTICED_DISCONNECT	1


static OSStatus errorTranslate(int recordErr)
{
    switch(recordErr) {
        case errSecSuccess:
            return errSecSuccess;
        case errSSLRecordInternal:
            return errSSLInternal;
        case errSSLRecordWouldBlock:
            return errSSLWouldBlock;
        case errSSLRecordProtocol:
            return errSSLProtocol;
        case errSSLRecordNegotiation:
            return errSSLNegotiation;
        case errSSLRecordClosedAbort:
            return errSSLClosedAbort;
        case errSSLRecordConnectionRefused:
            return errSSLConnectionRefused;
        case errSSLRecordDecryptionFail:
            return errSSLDecryptionFail;
        case errSSLRecordBadRecordMac:
            return errSSLBadRecordMac;
        case errSSLRecordRecordOverflow:
            return errSSLRecordOverflow;
        case errSSLRecordUnexpectedRecord:
            return errSSLUnexpectedRecord;
        default:
            sslErrorLog("unknown error code returned in sslErrorTranslate: %d\n", recordErr);
            return recordErr;
    }
}

/* SSLWriteRecord
 *  Attempt to encrypt and queue an SSL record.
 */
OSStatus
SSLWriteRecord(SSLRecord rec, SSLContext *ctx)
{
    OSStatus    err;

    err=errorTranslate(ctx->recFuncs->write(ctx->recCtx, rec));

    switch(err) {
        case errSecSuccess:
            break;
        default:
            sslErrorLog("unexpected error code returned in SSLWriteRecord: %d\n", (int)err);
            break;
    }

    return err;
}

/* SSLFreeRecord
 *  Free a record returned by SSLReadRecord.
 */
OSStatus
SSLFreeRecord(SSLRecord rec, SSLContext *ctx)
{
    return ctx->recFuncs->free(ctx->recCtx, rec);
}

/* SSLReadRecord
 *  Attempt to read & decrypt an SSL record.
 *  Record content should be freed using SSLFreeRecord
 */
OSStatus
SSLReadRecord(SSLRecord *rec, SSLContext *ctx)
{
    return errorTranslate(ctx->recFuncs->read(ctx->recCtx, rec));
}

OSStatus SSLServiceWriteQueue(SSLContext *ctx)
{
    return errorTranslate(ctx->recFuncs->serviceWriteQueue(ctx->recCtx));
}
