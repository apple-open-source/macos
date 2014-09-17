/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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
 *
 */

#ifndef HEADER_X509_H
#include <openssl/x509.h>
#endif

#ifndef HEADER_X509_VFY_APPLE_H
#define HEADER_X509_VFY_APPLE_H

/* Environment variable name to disable TEA. */
#define X509_TEA_ENV_DISABLE "OPENSSL_X509_TEA_DISABLE"

/*
 * X509_verify_cert
 *
 * Originally located in x509_vfy.c.
 *
 * Verify certificate with OpenSSL created X509_verify_cert. If and only if
 * OpenSSL cannot get certificate issuer locally then OS X security API will
 * verify the certificate, using Trust Evaluation Agent.
 *
 * Return values:
 * --------------
 * -1: Null was passed for either ctx or ctx->cert.
 *  0: Certificate is trusted.
 *  1: Certificate is not trusted.
 */
int X509_verify_cert(X509_STORE_CTX *ctx);

/*
 * X509_TEA_is_enabled
 *
 * Is the Trust Evaluation Agent (TEA) used for certificate verification when
 * the issuer cannot be verified.
 *
 * Returns 0 if TEA is disabled and 1 if TEA is enabled.
 */
int X509_TEA_is_enabled();

/*
 * X509_TEA_set_state
 *
 * Enables/disables certificate verification with Trust Evaluation Agent (TEA)
 * when the issuer cannot be verified.
 *
 * Pass 0 to disable TEA and non-zero to enable TEA.
 */
void X509_TEA_set_state(int change);

int X509_verify_cert_orig(X509_STORE_CTX *ctx);

#endif /* HEADER_X509_VFY_APPLE_H */
