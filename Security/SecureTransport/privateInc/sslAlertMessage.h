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


/*  *********************************************************************
    File: sslAlertMessage.h
    ****************************************************************** */

#ifndef _SSLALERTMESSAGE_H_
#define _SSLALERTMESSAGE_H_ 1

#ifndef _SECURE_TRANSPORT_H_
#include "SecureTransport.h"
#endif

#include "sslPriv.h"
#include "sslRecord.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{   SSL_AlertLevelWarning = 1,
    SSL_AlertLevelFatal = 2
} AlertLevel;

typedef enum
{   SSL_AlertCloseNotify = 0,
    SSL_AlertUnexpectedMsg = 10,
    SSL_AlertBadRecordMac = 20,
	SSL_AlertDecryptionFail = 21,		/* TLS */
	SSL_AlertRecordOverflow = 22,		/* TLS */
    SSL_AlertDecompressFail = 30,
    SSL_AlertHandshakeFail = 40,
    SSL_AlertNoCert = 41,
    SSL_AlertBadCert = 42,				/* SSLv3 only */
    SSL_AlertUnsupportedCert = 43,
    SSL_AlertCertRevoked = 44,
    SSL_AlertCertExpired = 45,
    SSL_AlertCertUnknown = 46,
    SSL_AlertIllegalParam = 47,
	/* remainder are TLS addenda */
	SSL_AlertUnknownCA = 48,
	SSL_AlertAccessDenied = 49,
	SSL_AlertDecodeError = 50,
	SSL_AlertDecryptError = 51,
	SSL_AlertExportRestriction = 60,
	SSL_AlertProtocolVersion = 70,
	SSL_AlertInsufficientSecurity = 71,
	SSL_AlertInternalError = 80,
	SSL_AlertUserCancelled = 90,
	SSL_AlertNoRenegotiation = 100
} AlertDescription;

OSStatus SSLProcessAlert(
	SSLRecord rec, 
	SSLContext *ctx);
OSStatus SSLSendAlert(
	AlertLevel level, 
	AlertDescription desc, 
	SSLContext *ctx);
OSStatus SSLFatalSessionAlert(
	AlertDescription desc, 
	SSLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _SSLALERTMESSAGE_H_ */
