/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * sslKeychain.h - Apple Keychain routines
 */

#ifndef	_SSL_KEYCHAIN_H_
#define _SSL_KEYCHAIN_H_


#include "sslContext.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Free the tls_private_key_t struct and associated SecKeyRef context that were created by parseIncomingCerts */
void sslFreePrivKey(tls_private_key_t *sslPrivKey);

/* Create a tls_private_key_t struct and SSLCertificate list, from a CFArray */
OSStatus
parseIncomingCerts(
	SSLContext			*ctx,
	CFArrayRef			certs,
	SSLCertificate      **destCertChain,/* &ctx->{localCertChain,encryptCertChain} */
    tls_private_key_t   *privKeyRef);	/* &ctx->signingPrivKeyRef, etc. */

#ifdef __cplusplus
}
#endif

#endif	/* _SSL_KEYCHAIN_H_ */
