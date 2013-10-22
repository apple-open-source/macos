/*
 * Copyright (c) 1999-2001,2005-2007,2010-2011 Apple Inc. All Rights Reserved.
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

#include "sslContext.h"

#ifdef __cplusplus
extern "C" {
#endif

    /*
 * Build ctx->validCipherSuites as a copy of all known CipherSpecs.
 */
OSStatus sslBuildCipherSuiteArray(SSLContext *ctx);

/*
 * Initialize dst based on selectedCipher.
 */
void InitCipherSpecParams(SSLContext *ctx);

/*
 * Given a valid ctx->selectedCipher and ctx->validCipherSuites, set
 * ctx->selectedCipherSpec as appropriate. Return an error if
 * ctx->selectedCipher could not be set as the current ctx->selectedCipherSpec.
 */
OSStatus FindCipherSpec(SSLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif	/* _CIPHER_SPECS_H_ */
