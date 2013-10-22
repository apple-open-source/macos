/*
 * Copyright (c) 1999-2001,2005-2008,2010-2011 Apple Inc. All Rights Reserved.
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
 * symCipherParams.c - symmetric cipher parameters
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "sslBuildFlags.h"
#include "symCipher.h"

/*
 * The parameters for the symmetric ciphers currently supported (in addition to the
 * NULL cipher in nullciph.c).
 */

#if	ENABLE_DES
const SSLSymmetricCipherParams SSLCipherDES_CBCParams = {
    .keyAlg = SSL_CipherAlgorithmDES_CBC,
    .keySize = 8,
    .ivSize = 8,
    .blockSize = 8,
    .cipherType = blockCipherType,
};
#endif	/* ENABLE_DES */

#if	ENABLE_3DES
const SSLSymmetricCipherParams SSLCipher3DES_CBCParams = {
    .keyAlg = SSL_CipherAlgorithm3DES_CBC,
    .keySize = 24,
    .ivSize = 8,
    .blockSize = 8,
    .cipherType = blockCipherType,
};
#endif	/* ENABLE_3DES */

#if		ENABLE_RC4
const SSLSymmetricCipherParams SSLCipherRC4_128Params = {
    .keyAlg = SSL_CipherAlgorithmRC4_128,
    .keySize = 16,
    .cipherType = streamCipherType,
};
#endif	/* ENABLE_RC4 */

#if		ENABLE_RC2
const SSLSymmetricCipherParams SSLCipherRC2_128Params = {
    .keyAlg = SSL_CipherAlgorithmRC2_128,
    .keySize = 16,
    .ivSize = 8,
    .blockSize = 8,
    .cipherType = blockCipherType,
};
#endif	/* ENABLE_RC2*/

#if		ENABLE_AES
const SSLSymmetricCipherParams SSLCipherAES_128_CBCParams = {
    .keyAlg = SSL_CipherAlgorithmAES_128_CBC,
    .keySize = 16,
    .ivSize = 16,
    .blockSize = 16,
    .cipherType = blockCipherType,
};
#endif	/* ENABLE_AES */

#if ENABLE_AES256
const SSLSymmetricCipherParams SSLCipherAES_256_CBCParams = {
    .keyAlg = SSL_CipherAlgorithmAES_256_CBC,
    .keySize = 32,
    .ivSize = 16,
    .blockSize = 16,
    .cipherType = blockCipherType,
};
#endif /* ENABLE_AES256 */

#if ENABLE_AES_GCM
const SSLSymmetricCipherParams SSLCipherAES_128_GCMParams = {
    .keyAlg = SSL_CipherAlgorithmAES_128_GCM,
    .keySize = 16,
    .ivSize = 16,
    .blockSize = 16,
    .cipherType = aeadCipherType,
};

const SSLSymmetricCipherParams SSLCipherAES_256_GCMParams = {
    .keyAlg = SSL_CipherAlgorithmAES_256_GCM,
    .keySize = 32,
    .ivSize = 16,
    .blockSize = 16,
    .cipherType = aeadCipherType,
};
#endif /* ENABLE_AES_GCM */
