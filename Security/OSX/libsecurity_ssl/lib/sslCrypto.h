/*
 * Copyright (c) 2006-2008,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * sslCrypto.h - interface between SSL and crypto libraries
 */

#ifndef	_SSL_CRYPTO_H_
#define _SSL_CRYPTO_H_	1

#include "ssl.h"
#include "sslContext.h"
#include <Security/SecKeyPriv.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef		NDEBUG
extern void stPrintCdsaError(const char *op, OSStatus crtn);
#else
#define stPrintCdsaError(o, cr)
#endif

/*
 * Get algorithm id for a SSLPubKey object.
 */
CFIndex sslPubKeyGetAlgorithmID(SecKeyRef pubKey);

/*
 * Get algorithm id for a SSLPrivKey object.
 */
CFIndex sslPrivKeyGetAlgorithmID(SecKeyRef privKey);

/*
 * Create a new SecTrust object and return it.
 */
OSStatus
sslCreateSecTrust(
	SSLContext				*ctx,
    SecTrustRef             *trust); 	/* RETURNED */

OSStatus sslVerifySelectedCipher(
	SSLContext 		*ctx);

/* Convert DER certs to SecCertificateRefs */
CFArrayRef tls_get_peer_certs(const SSLCertificate *certs);

/*
 * Set the pubkey after receiving the certificate
 */
int tls_set_peer_pubkey(SSLContext *ctx);

/*
 * Verify the peer cert chain (after receiving the server hello or client cert)
 */
int tls_verify_peer_cert(SSLContext *ctx);


#
#ifdef __cplusplus
}
#endif


#endif	/* _SSL_CRYPTO_H_ */
