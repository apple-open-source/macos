/* 
 * Copyright (c) 2010-2011 Apple Inc. All Rights Reserved.
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

#ifndef	_SEC_FDE_RECOVERYASYMMETRIC_CRYPTO_H_
#define _SEC_FDE_RECOVERYASYMMETRIC_CRYPTO_H_

#include <Security/cssmtype.h>
#include <Security/SecBase.h>
#include <CoreFoundation/CFData.h>

#ifdef  __cplusplus
extern "C" {
#endif

/*
	See CEncryptedEncoding.h in the DiskImages project
	This structure is only used in libcsfde/lib/CSRecovery.c and 
	SecFDERecoveryAsymmetricCrypto.cpp
*/

typedef struct 
{
	uint32_t				publicKeyHashSize;
	uint8_t					publicKeyHash[32];
	
	CSSM_ALGORITHMS			blobEncryptionAlgorithm;
	CSSM_PADDING			blobEncryptionPadding;
	CSSM_ENCRYPT_MODE		blobEncryptionMode;
	
	uint32_t				encryptedBlobSize;
	uint8_t					encryptedBlob[512];
} FVPrivateKeyHeader;

int SecFDERecoveryWrapCRSKWithPubKey(const uint8_t *crsk, size_t crskLen, 
	SecCertificateRef certificateRef, FVPrivateKeyHeader *outHeader);
CFDataRef SecFDERecoveryUnwrapCRSKWithPrivKey(SecKeychainRef keychain, 
	const FVPrivateKeyHeader *inHeader);

#ifdef  __cplusplus
}
#endif
	
#endif	/* _SEC_FDE_RECOVERYASYMMETRIC_CRYPTO_H_ */
