/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

#ifndef	_CC_HmacSPI_H_
#define _CC_HmacSPI_H_

#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

#include <stdint.h>
#include <sys/types.h>

#include <Availability.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct CCHmacContext * CCHmacContextRef;

CCHmacContextRef
CCHmacCreate(CCDigestAlg alg, const void *key, size_t keyLength)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/* Update and Final are reused from existing api, type changed from struct CCHmacContext * to CCHmacContextRef though */            

void
CCHmacDestroy(CCHmacContextRef ctx)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

size_t
CCHmacOutputSizeFromRef(CCHmacContextRef ctx)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


size_t
CCHmacOutputSize(CCDigestAlg alg)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

    
#ifdef __cplusplus
}
#endif

#endif /* _CC_HmacSPI_H_ */
