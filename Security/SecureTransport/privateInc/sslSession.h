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
    File: sslSession.h - SSL Session Interface
    *********************************************************************/

#ifndef _SSLSESSION_H_
#define _SSLSESSION_H_ 1

#define SSL_SESSION_ID_LEN  16      /* 16 <= SSL_SESSION_ID_LEN <= 32 */

#ifdef __cplusplus
extern "C" {
#endif

OSStatus SSLAddSessionData(
	const SSLContext *ctx);
OSStatus SSLGetSessionData(
	SSLBuffer *sessionData, 
	const SSLContext *ctx);
OSStatus SSLDeleteSessionData(
	const SSLContext *ctx);
OSStatus SSLRetrieveSessionID(
	const SSLBuffer sessionData, 
	SSLBuffer *identifier, 
	const SSLContext *ctx);
OSStatus SSLRetrieveSessionProtocolVersion(
	const SSLBuffer sessionData, 
	SSLProtocolVersion *version, 
	const SSLContext *ctx);
OSStatus SSLInstallSessionFromData(
	const SSLBuffer sessionData, 
	SSLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _SSLSESSION_H_ */
