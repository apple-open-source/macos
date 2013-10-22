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

// #define COMMON_GCM_FUNCTIONS
#include "ccMemory.h"
#include "ccdebug.h"
#include "CommonCryptor.h"
#include "CommonCryptorSPI.h"
#include "CommonCryptorPriv.h"
#include <corecrypto/ccmode_factory.h>


CCCryptorStatus
CCCryptorGCMAddIV(CCCryptorRef cryptorRef,
                	const void 		*iv,
                    size_t ivLen)
{
    CCCryptor   *cryptor = getRealCryptor(cryptorRef, 0);
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!cryptor) return kCCParamError;
    ccmode_gcm_set_iv(cryptor->ctx[cryptor->op].gcm, ivLen, iv);
 	return kCCSuccess;
}


CCCryptorStatus
CCCryptorGCMAddAAD(CCCryptorRef cryptorRef,
                	const void 		*aData,
                    size_t aDataLen)
{
    CCCryptor   *cryptor = getRealCryptor(cryptorRef, 0);
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");    
    if(!cryptor) return kCCParamError;
    ccmode_gcm_gmac(cryptor->ctx[cryptor->op].gcm, aDataLen, aData);
 	return kCCSuccess;
}

// This is for old iOS5 clients
CCCryptorStatus
CCCryptorGCMAddADD(CCCryptorRef cryptorRef,
                   const void 		*aData,
                   size_t aDataLen)
{
    return CCCryptorGCMAddAAD(cryptorRef, aData, aDataLen);
}

// This was a temp mistake in MacOSX8
CCCryptorStatus
CCCryptorGCMaddAAD(CCCryptorRef cryptorRef,
                   const void 		*aData,
                   size_t aDataLen)
{
    return CCCryptorGCMAddAAD(cryptorRef, aData, aDataLen);
}



CCCryptorStatus CCCryptorGCMEncrypt(
	CCCryptorRef cryptorRef,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
{
    CCCryptor   *cryptor = getRealCryptor(cryptorRef, 0);
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!cryptor) return kCCParamError;
    if(dataIn == NULL || dataOut == NULL) return kCCParamError;
    ccmode_gcm_encrypt(cryptor->ctx[cryptor->op].gcm, dataInLength, dataIn, dataOut);
 	return kCCSuccess;
}



CCCryptorStatus CCCryptorGCMDecrypt(
	CCCryptorRef cryptorRef,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
{
    CCCryptor   *cryptor = getRealCryptor(cryptorRef, 0);
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!cryptor) return kCCParamError;
    if(dataIn == NULL || dataOut == NULL) return kCCParamError;
    ccmode_gcm_decrypt(cryptor->ctx[cryptor->op].gcm, dataInLength, dataIn, dataOut);
 	return kCCSuccess;
}



CCCryptorStatus CCCryptorGCMFinal(
	CCCryptorRef cryptorRef,
	const void *tag,
	size_t *tagLength)
{
    CCCryptor   *cryptor = getRealCryptor(cryptorRef, 0);
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!cryptor) return kCCParamError;
	if(tag == NULL || tagLength == NULL)  return kCCParamError;
    ccmode_gcm_finalize(cryptor->ctx[cryptor->op].gcm, *tagLength, (void *)tag);
 	return kCCSuccess;
}



CCCryptorStatus CCCryptorGCMReset(
	CCCryptorRef cryptorRef)
{
    CCCryptor   *cryptor = getRealCryptor(cryptorRef, 0);
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!cryptor) return kCCParamError;
    ccmode_gcm_reset(cryptor->ctx[cryptor->op].gcm);
 	return kCCSuccess;
}



CCCryptorStatus CCCryptorGCM(
	CCOperation 	op,				/* kCCEncrypt, kCCDecrypt */
	CCAlgorithm		alg,
	const void 		*key,			/* raw key material */
	size_t 			keyLength,	
	const void 		*iv,
	size_t 			ivLen,
	const void 		*aData,
	size_t 			aDataLen,
	const void 		*dataIn,
	size_t 			dataInLength,
  	void 			*dataOut,
	const void 		*tag,
	size_t 			*tagLength)
{
    CCCryptorRef cryptorRef;
    CCCryptorStatus retval;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Op: %d Cipher: %d\n", op, alg);

    retval = CCCryptorCreateWithMode(op, kCCModeGCM, alg, 0, NULL, key, keyLength,
                                         NULL, 0, 0, 0, &cryptorRef);
    if(retval) return retval;
    
    // IV is optional
    if(ivLen) {
        retval = CCCryptorGCMAddIV(cryptorRef, iv, ivLen);
        if(retval) return retval;
    }
    
    // This must always be called - even with no aData.
    retval = CCCryptorGCMaddAAD(cryptorRef, aData, aDataLen);
    if(retval) return retval;
    
    if(op == kCCEncrypt)
        retval = CCCryptorGCMEncrypt(cryptorRef, dataIn, dataInLength, dataOut);
    else if(op == kCCDecrypt)
        retval = CCCryptorGCMDecrypt(cryptorRef, dataIn, dataInLength, dataOut);
    else return kCCParamError;
    if(retval) return retval;

    retval = CCCryptorGCMFinal(cryptorRef, tag, tagLength);
    CCCryptorRelease(cryptorRef);
    
    return retval;
}


