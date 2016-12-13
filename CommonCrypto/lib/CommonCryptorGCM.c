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
#include <corecrypto/ccn.h>

/*
 typical GCM use case: sending an authenticated packet

 +--------------+-------+--------------+
 |    header    |  seq. |   Data       |
 +--------------+-------+--------------+
        |           |         |
        |           |         |
 Addtl auth data   IV     plain text
        |           |         |
        |           |         V
        |           |     +--------------------+
        |           +---->|                    |
        |           |     | GCM encryption     |
        +---------------->|                    |
        |           |     +--------------------+
        |           |         |            |
        |           |     cipher text    Auth tag
        |           |         |            |
        V           V         V            V
 +--------------+-------+----------------+---------+
 |    header    |  seq. | encrypted data |   ICV   |
 +--------------+-------+----------------+---------+
 */

#define CCCryptorGCMprologue()   CCCryptor *cryptor = getRealCryptor(cryptorRef, 0); \
CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n"); \
if(!cryptor) return kCCParamError;

static inline CCCryptorStatus translate_err_code(int err)
{
    if (err==0) {
        return kCCSuccess;
    } /*else if(err == CCMODE_INVALID_CALL_SEQUENCE){ //unti we can read error codes from corecrypto
        return kCCCallSequenceError;
    } */ else {
        return kCCUnspecifiedError;
    }

}

CCCryptorStatus
CCCryptorGCMAddIV(CCCryptorRef cryptorRef,
                  const void 		*iv,
                  size_t ivLen)
{
    CCCryptorGCMprologue();
    if(ivLen!=0 && iv==NULL) return kCCParamError;
    //it is okay to call with ivLen 0 and/OR iv==NULL
    //infact this needs to be done even with NULL values, otherwise ccgcm_ is going to return call sequence error.
    //currently corecrypto accepts NULL
    //rdar://problem/23523093
    int rc = ccgcm_set_iv_legacy(cryptor->symMode[cryptor->op].gcm,cryptor->ctx[cryptor->op].gcm, ivLen, iv);
    return translate_err_code(rc);

}

//Add additional authentication data
CCCryptorStatus
CCCryptorGCMAddAAD(CCCryptorRef cryptorRef,
                   const void 		*aData,
                   size_t aDataLen)
{
    CCCryptorGCMprologue();
    if(aDataLen!=0 && aData==NULL) return kCCParamError;
    //it is okay to call with aData zero
    int rc = ccgcm_gmac(cryptor->symMode[cryptor->op].gcm,cryptor->ctx[cryptor->op].gcm, aDataLen, aData);
    return translate_err_code(rc);
}

// This is for old iOS5 clients
CCCryptorStatus
CCCryptorGCMAddADD(CCCryptorRef cryptorRef,
                   const void 		*aData,
                   size_t aDataLen)
{
    return  CCCryptorGCMAddAAD(cryptorRef, aData, aDataLen);
}

// This was a temp mistake in MacOSX8
CCCryptorStatus
CCCryptorGCMaddAAD(CCCryptorRef cryptorRef,
                   const void 		*aData,
                   size_t aDataLen)
{
    return CCCryptorGCMAddAAD(cryptorRef, aData, aDataLen);
}

//we are not providing this function to users.
//The reason is that we don't want to create more liability for ourself
//and a new interface function just increases the number of APIs
//without actually helping users
//User's use the old CCCryptorGCMEncrypt() and CCCryptorGCMDecrypt()
static CCCryptorStatus gcm_update(CCCryptorRef cryptorRef,
                                          const void *dataIn,
                                          size_t dataInLength,
                                          void *dataOut)
{
    CCCryptorGCMprologue();
    if(dataInLength!=0 && dataIn==NULL) return kCCParamError;
    //no data is okay
    if(dataOut == NULL) return kCCParamError;
    int rc = ccgcm_update(cryptor->symMode[cryptor->op].gcm,cryptor->ctx[cryptor->op].gcm, dataInLength, dataIn, dataOut);
    return translate_err_code(rc);
}


CCCryptorStatus CCCryptorGCMEncrypt(CCCryptorRef cryptorRef,
                                    const void *dataIn,
                                    size_t dataInLength,
                                    void *dataOut)
{
    return gcm_update(cryptorRef, dataIn, dataInLength, dataOut);

}



CCCryptorStatus CCCryptorGCMDecrypt(CCCryptorRef cryptorRef,
                                    const void *dataIn,
                                    size_t dataInLength,
                                    void *dataOut)
{
    return gcm_update(cryptorRef, dataIn, dataInLength, dataOut);
}


CCCryptorStatus CCCryptorGCMFinal(CCCryptorRef cryptorRef,
                                  void   *tagOut,
                                  size_t *tagLength)
{
    CCCryptorGCMprologue();
    if(tagOut == NULL || tagLength == NULL)  return kCCParamError;
    int rc = ccgcm_finalize(cryptor->symMode[cryptor->op].gcm,cryptor->ctx[cryptor->op].gcm, *tagLength, (void *) tagOut);
    if(rc == -1)
        return kCCUnspecifiedError;
    else
        return kCCSuccess; //this includes 0 and any error message other than -1

   // ccgcm_finalize() returns CCMODE_INTEGRITY_FAILURE (-3) if the expected tag is not coppied to the buffer. but that doesn't mean there is an error
}


CCCryptorStatus CCCryptorGCMReset(CCCryptorRef cryptorRef)
{
    CCCryptorGCMprologue();
    int rc = ccgcm_reset(cryptor->symMode[cryptor->op].gcm,cryptor->ctx[cryptor->op].gcm);
    return translate_err_code(rc);
}

CCCryptorStatus CCCryptorGCM(CCOperation op,				/* kCCEncrypt, kCCDecrypt */
                             CCAlgorithm alg,
                             const void  *key,    size_t keyLength, /* raw key material */
                             const void  *iv,     size_t ivLen,
                             const void  *aData,  size_t aDataLen,
                             const void  *dataIn, size_t dataInLength,
                             void 		 *dataOut,
                             void        *tagOut,    size_t *tagLength)

{
    CCCryptorRef cryptorRef;
    CCCryptorStatus retval;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Op: %d Cipher: %d\n", op, alg);

    retval = CCCryptorCreateWithMode(op, kCCModeGCM, alg, 0, NULL, key, keyLength,
                                         NULL, 0, 0, 0, &cryptorRef);
    if(retval) return retval;

    //call even with NULL pointer and zero length IV
    retval = CCCryptorGCMAddIV(cryptorRef, iv, ivLen);
    if(retval) return retval;

    retval = CCCryptorGCMaddAAD(cryptorRef, aData, aDataLen);
    if(retval) return retval;
    
    retval = gcm_update(cryptorRef, dataIn, dataInLength, dataOut);
    if(retval) return retval;

    retval = CCCryptorGCMFinal(cryptorRef, tagOut, tagLength);
    CCCryptorRelease(cryptorRef);
    
    return retval;
}


