/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

// #define COMMON_SYMMETRIC_KEYWRAP_FUNCTIONS
#include <CommonCrypto/CommonSymmetricKeywrap.h>
#include <CommonCrypto/CommonCryptor.h>
#include "CommonCryptorPriv.h"
#include <AssertMacros.h>
#include "ccdebug.h"
#include <corecrypto/ccwrap.h>


static const uint8_t rfc3394_iv_data[] = {
    0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
};

const uint8_t * const CCrfc3394_iv = rfc3394_iv_data;
const size_t CCrfc3394_ivLen = sizeof(rfc3394_iv_data);

int
CCSymmetricKeyWrap( CCWrappingAlgorithm __unused algorithm,
				   const uint8_t *iv __unused, const size_t ivLen __unused,
				   const uint8_t *kek, size_t kekLen,
				   const uint8_t *rawKey, size_t rawKeyLen,
				   uint8_t  *wrappedKey, size_t *wrappedKeyLen)
{
    CC_DEBUG_LOG("Entering\n");
    int err = kCCUnspecifiedError;

    const struct ccmode_ecb *ccmode = getCipherMode(kCCAlgorithmAES128, kCCModeECB, kCCEncrypt).ecb;
    ccecb_ctx_decl(ccmode->size, ctx);

    require_action((kekLen == CCAES_KEY_SIZE_128 ||
                    kekLen == CCAES_KEY_SIZE_192 ||
                    kekLen == CCAES_KEY_SIZE_256),
                   out, err = kCCParamError);
    require_action(wrappedKeyLen && (*wrappedKeyLen >= ccwrap_wrapped_size(rawKeyLen)), out, err = kCCParamError);

    ccmode->init(ccmode, ctx, kekLen, kek);

    require_action(ccwrap_auth_encrypt(ccmode, ctx, rawKeyLen, rawKey, wrappedKeyLen, wrappedKey) == CCERR_OK, out, err = kCCParamError);

    err = kCCSuccess;

out:
    ccecb_ctx_clear(ccmode->size, ctx);
    return err;
}

int
CCSymmetricKeyUnwrap(CCWrappingAlgorithm __unused algorithm,
					 const uint8_t *iv __unused, const size_t ivLen __unused,
					 const uint8_t *kek, size_t kekLen,
					 const uint8_t  *wrappedKey, size_t wrappedKeyLen,
                     uint8_t  *rawKey, size_t *rawKeyLen)
{
    CC_DEBUG_LOG("Entering\n");
    int err = kCCUnspecifiedError;

    const struct ccmode_ecb *ccmode = getCipherMode(kCCAlgorithmAES128, kCCModeECB, kCCDecrypt).ecb;
    ccecb_ctx_decl(ccmode->size, ctx);

    require_action((kekLen == CCAES_KEY_SIZE_128 ||
                    kekLen == CCAES_KEY_SIZE_192 ||
                    kekLen == CCAES_KEY_SIZE_256),
                   out, err = kCCParamError);
    require_action(rawKeyLen && (*rawKeyLen >= ccwrap_unwrapped_size(wrappedKeyLen)), out, err = kCCParamError);

    ccmode->init(ccmode, ctx, kekLen, kek);

    require_action(ccwrap_auth_decrypt(ccmode, ctx, wrappedKeyLen, wrappedKey, rawKeyLen, rawKey) == CCERR_OK, out, err = kCCDecodeError);

    err = kCCSuccess;

out:
    ccecb_ctx_clear(ccmode->size, ctx);
    return err;
}

size_t
CCSymmetricWrappedSize(CCWrappingAlgorithm __unused algorithm, size_t rawKeyLen)
{
    CC_DEBUG_LOG("Entering\n");
    return ccwrap_wrapped_size(rawKeyLen);
}

size_t
CCSymmetricUnwrappedSize(CCWrappingAlgorithm __unused algorithm, size_t wrappedKeyLen)
{
    CC_DEBUG_LOG("Entering\n");
    return ccwrap_unwrapped_size(wrappedKeyLen);
}
