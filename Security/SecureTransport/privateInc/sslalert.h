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
    File: sslalert.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslalert.h   Alert layer functions and values

    Prototypes for functions in sslalert.c and alert layer equates.

    ****************************************************************** */

#ifndef _SSLALERT_H_
#define _SSLALERT_H_ 1

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
{   alert_warning = 1,
    alert_fatal = 2
} AlertLevel;

typedef enum
{   alert_close_notify = 0,
    alert_unexpected_message = 10,
    alert_bad_record_mac = 20,
    alert_decompression_failure = 30,
    alert_handshake_failure = 40,
    alert_no_certificate = 41,
    alert_bad_certificate = 42,
    alert_unsupported_certificate = 43,
    alert_certificate_revoked = 44,
    alert_certificate_expired = 45,
    alert_certificate_unknown = 46,
    alert_illegal_parameter = 47
} AlertDescription;

SSLErr SSLProcessAlert(SSLRecord rec, SSLContext *ctx);
SSLErr SSLSendAlert(AlertLevel level, AlertDescription desc, SSLContext *ctx);
SSLErr SSLEncodeAlert(SSLRecord *rec, AlertLevel level, AlertDescription desc, SSLContext *ctx);
SSLErr SSLFatalSessionAlert(AlertDescription desc, SSLContext *ctx);

#endif /* _SSLALERT_H_ */
