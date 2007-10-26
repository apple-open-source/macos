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


  /************************************************************
    File: sslUtils.h
   ************************************************************/

#ifndef _SSLUTILS_H_
#define _SSLUTILS_H_ 1

#include "SecureTransport.h"
#include "sslPriv.h"

#ifdef	__cplusplus
extern "C" {
#endif

UInt32 SSLDecodeInt(
	const unsigned char *p, 
	int 				length);
unsigned char *SSLEncodeInt(
	unsigned char 		*p, 
	UInt32 				value, 
	int length);
UInt8* SSLEncodeUInt64(
	UInt8 				*p, 
	sslUint64 			value);
void IncrementUInt64(
	sslUint64 			*v);

UInt32 SSLGetCertificateChainLength(
	const SSLCertificate *c);
Boolean sslIsSessionActive(
	const SSLContext 	*ctx);
OSStatus sslDeleteCertificateChain(
	SSLCertificate 		*certs, 
	SSLContext 			*ctx);

OSStatus sslTime(
	UInt32 				*tim);

#if	SSL_DEBUG
extern const char *protocolVersStr(
	SSLProtocolVersion 	prot);
#endif

/*
 * Redirect SSLBuffer-based I/O call to user-supplied I/O. 
 */ 
OSStatus sslIoRead(
 	SSLBuffer 		buf, 
 	size_t			*actualLength, 
 	SSLContext 		*ctx);
 
OSStatus sslIoWrite(
 	SSLBuffer 		buf, 
 	size_t			*actualLength, 
 	SSLContext 		*ctx);
 
/*
 * Common RNG function.
 */
OSStatus sslRand(
	SSLContext 		*ctx, 
	SSLBuffer 		*buf);
	
OSStatus sslVerifyProtVersion(
	SSLContext 			*ctx,
	SSLProtocolVersion	peerVersion,
	SSLProtocolVersion 	*negVersion);

OSStatus sslGetMaxProtVersion(
	SSLContext 			*ctx,
	SSLProtocolVersion	*version);	// RETURNED

#define SET_SSL_BUFFER(buf, d, l)   do { (buf).data = (d); (buf).length = (l); } while (0)

#ifdef	__cplusplus
}
#endif

#endif
