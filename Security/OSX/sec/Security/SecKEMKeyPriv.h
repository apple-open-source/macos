/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECKEMKEYPRIV_H_
#define _SECURITY_SECKEMKEYPRIV_H_

#import <Foundation/Foundation.h>

#include <Security/SecBase.h>
#include <corecrypto/cckem.h>

__BEGIN_DECLS

void SecKEMPublicKeyDestroy(SecKeyRef key);
CFDataRef SecKEMPublicKeyCopyData(cckem_pub_ctx_t ctx, CFErrorRef *error);
size_t SecKEMPublicKeyBlockSize(SecKeyRef key);

void SecKEMPrivateKeyDestroy(SecKeyRef key);
CFDataRef SecKEMPrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error);
size_t SecKEMPrivateKeyBlockSize(SecKeyRef key);

#if __OBJC__

NSString *SecKEMGenerateHexDump(cckem_pub_ctx_t ctx);
NSDictionary *SecKEMCreateKeyAttributeDictionary(id keyType,
                                                 id keySizeType,
                                                 id keyClass,
                                                 NSData *applicationLabel,
                                                 NSData *valueData);
NSData *SecKEMDecapsulateSharedKey(SecKeyRef key,
                                   CFDataRef encapsulatedKey,
                                   CFErrorRef *error);

#endif

__END_DECLS

#endif /* !_SECURITY_SECKEMKEYPRIV_H_ */
