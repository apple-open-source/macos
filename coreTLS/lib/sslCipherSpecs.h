/*
 * Copyright (c) 1999-2001,2005-2007,2010-2011,2014 Apple Inc. All Rights Reserved.
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
 * cipherSpecs.h - SSLCipherSpec declarations
 */

#ifndef	_SSL_CIPHER_SPECS_H_
#define _SSL_CIPHER_SPECS_H_

#include "tls_handshake.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Helper function to know if a ciphersuite is in a set.
 */
bool cipherSuiteInSet(uint16_t cs, uint16_t *ciphersuites, size_t numCiphersuites);

/* 
 * Set some flags based on enabled ciphersuites 
 */
void sslAnalyzeCipherSpecs(tls_handshake_t ctx);

/*
 * Initialize dst based on selectedCipher.
 */
void InitCipherSpecParams(tls_handshake_t ctx);


/*
 * Server side: given the received requested ciphers and
 * the server config, pick an appropriate ciphersuite
 */
int SelectNewCiphersuite(tls_handshake_t ctx);

/*
 * Client side: Validate the ciphersuite selected by the server.
 * This should be one of the ciphersuite requested by the client.
 */
int ValidateSelectedCiphersuite(tls_handshake_t ctx);

/* 
 * Return true if the given ciphersuite is supported for a specific server/dtls combo
 */
bool tls_handshake_ciphersuite_is_supported(bool server, bool dtls, uint16_t ciphersuite);

/* 
 * Return true, if the given ciphersuite is allowed for a specific config
 */
bool tls_handshake_ciphersuite_is_allowed(tls_handshake_config_t config, uint16_t ciphersuite);

/*
 * Return true, if the given ciphersuite is valid for the current context
 */
bool tls_handshake_ciphersuite_is_valid(tls_handshake_t ctx, uint16_t ciphersuite);

/*
 * Return true if the given ecdh curve is supported
 */
bool tls_handshake_curve_is_supported(uint16_t curve);

/*
 * Return true if the given sigalg is supported
 */
bool tls_handshake_sigalg_is_supported(tls_signature_and_hash_algorithm sigalg);

#ifdef __cplusplus
}
#endif

#endif	/* _CIPHER_SPECS_H_ */
