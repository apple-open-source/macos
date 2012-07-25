/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslPriv.h - Misc. private SSL typedefs
 */

#ifndef	_SSL_PRIV_H_
#define _SSL_PRIV_H_	1

#include "sslBuildFlags.h"
#include "SecureTransportPriv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Diffie-Hellman support */
#define APPLE_DH		1

/*
 * Flags support for ECDSA on the server side.
 * Not implemented as of 6 Aug 2008.
 */
#define SSL_ECDSA_SERVER	0

/*
 * For ease of porting, we'll keep this around for internal use.
 * It's used extensively; eventually we'll convert over to
 * CFData, as in the public API.
 */
typedef struct
{   size_t  length;
    uint8_t *data;
} SSLBuffer;

/*
 * We can make this more Mac-like as well...
 */
typedef struct
{   uint32_t  high;
    uint32_t  low;
}   sslUint64;


typedef enum
{
	/* This value never appears in the actual protocol */
	SSL_Version_Undetermined = 0,
	/* actual protocol values */
    SSL_Version_2_0 = 0x0002,
    SSL_Version_3_0 = 0x0300,
	TLS_Version_1_0 = 0x0301,		/* TLS 1.0 == SSL 3.1 */
    TLS_Version_1_1 = 0x0302,
    TLS_Version_1_2 = 0x0303,
    DTLS_Version_1_0 = 0xfeff,
} SSLProtocolVersion;

/*
 * Clients see an opaque SSLContextRef; internal code uses the
 * following typedef.
 */
typedef struct SSLContext SSLContext;

#ifdef __cplusplus
}
#endif

#endif	/* _SSL_PRIV_H */
