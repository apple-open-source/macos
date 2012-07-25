/*
 *  aes.h
 *
 * Copyright © 2010 by Apple, Inc. All rights reserved.
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

/*
 *  aes.h
 *  CommonCrypto shoefly for compatability with older versions
 *
 */

#ifdef CC_Building
#include "CommonCryptor.h"
#include "CommonCryptorSPI.h"
#include "CommonCryptorPriv.h"
#else
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#endif /* CC_Building */

#if !defined( _CC_AES_H_ )
#define _CC_AES_H_

#if defined(__cplusplus)
extern "C" {
#endif	
    
#define AES_BLOCK_SIZE  16  /* the AES block size in bytes          */
    
typedef struct
{
	CCCryptorRef	cref;
	uint32_t		ctx[kCCContextSizeAES128/4];
} aes_encrypt_ctx;
    
typedef struct
{
	CCCryptorRef	cref;
	uint32_t		ctx[kCCContextSizeAES128/4];
} aes_decrypt_ctx;
    
    
    
void aes_encrypt_key128(const unsigned char *in_key, aes_encrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);

void aes_encrypt_key192(const unsigned char *key, aes_encrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);

void aes_encrypt_key256(const unsigned char *in_key, aes_encrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);

void aes_encrypt_cbc(const unsigned char *in_blk, const unsigned char *in_iv, unsigned int num_blk,
                         unsigned char *out_blk, aes_encrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);

void aes_decrypt_key128(const unsigned char *in_key, aes_decrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);

void aes_decrypt_key192(const unsigned char *key, aes_decrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);

void aes_decrypt_key256(const unsigned char *in_key, aes_decrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);

void aes_decrypt_cbc(const unsigned char *in_blk, const unsigned char *in_iv, unsigned int num_blk,
                         unsigned char *out_blk, aes_decrypt_ctx cx[1])
__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_7, __IPHONE_5_0, __IPHONE_5_0);


#if defined(__cplusplus)
}
#endif


#endif	/* _CC_AES_H_ */
