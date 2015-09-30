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
 * sslSession.h - SSL Session Interface
 */

#ifndef _SSLSESSION_H_
#define _SSLSESSION_H_ 1

#define SSL_SESSION_ID_LEN  16      /* 16 <= SSL_SESSION_ID_LEN <= 32 */

#ifdef __cplusplus
extern "C" {
#endif

int  SSLAddSessionData(const tls_handshake_t ctx);
int  SSLDeleteSessionData(const tls_handshake_t ctx);

int  SSLRetrieveSessionID(
	const tls_buffer sessionData,
	tls_buffer *identifier);

int  SSLRetrieveSessionTicket(
    const tls_buffer sessionData,
    tls_buffer *ticket);
/*
int  SSLRetrieveSessionProtocolVersion(
	const tls_buffer sessionData,
	tls_protocol_version *version,
	const tls_handshake_t ctx);
*/

int SSLInstallSessionFromData(const tls_buffer sessionData, tls_handshake_t ctx);

int SSLServerValidateSessionData(const tls_buffer sessionData, tls_handshake_t ctx);
int SSLClientValidateSessionDataBefore(const tls_buffer sessionData, tls_handshake_t ctx);
int SSLClientValidateSessionDataAfter(const tls_buffer sessionData, tls_handshake_t ctx);


#ifdef __cplusplus
}
#endif

#endif /* _SSLSESSION_H_ */
