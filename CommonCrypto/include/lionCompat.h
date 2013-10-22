/*
 * Copyright © 2011 by Apple, Inc. All rights reserved.
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


#if !defined( _COMMON_CRYPTO_LION_COMPAT_ )
#define _COMMON_CRYPTO_LION_COMPAT_

#include <Availability.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonCryptor.h>

typedef struct
{
	CCCryptorRef	cref;
} CAST_KEY;


size_t
CCDigestBlockSize(CCDigestRef ctx)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA);

// might be needed.
void CAST_ecb_encrypt(const unsigned char *in,unsigned char *out, CAST_KEY *key, int enc)
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_NA, __IPHONE_NA);
		


// might be needed.
void CAST_set_key(CAST_KEY *key, int len, const unsigned char *data)
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_NA, __IPHONE_NA);


#endif	/* _COMMON_CRYPTO_LION_COMPAT_ */
