//
//  OTRMath.c
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 1/28/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#include "SecOTRMath.h"
#include "SecOTRMathPrivate.h"

#include "SecOTRPacketData.h"

#include <AssertMacros.h>

#include <pthread.h>

#include <Security/SecRandom.h>

#include <corecrypto/ccsha2.h>
#include <corecrypto/cczp.h>

#include <limits.h>

static inline
void cczp_init_from_bytes(cc_size n, cczp_t zp, size_t pSize, const void *p, size_t rSize, const void* r)
{
    CCZP_N(zp) = n;
    zp.zp->mod_prime = cczp_mod;
    ccn_read_uint(n, CCZP_PRIME(zp), pSize, p);
    ccn_read_uint(n+1, CCZP_RECIP(zp), rSize, r);

}

static pthread_once_t   kOTRImportGroupData = PTHREAD_ONCE_INIT;

static cc_unit sOTRGenerator[kOTRDHGroupUnits];
static cczp_decl_n(kOTRDHGroupUnits, sOTRGroup_zp);

static void setupGroupValues()
{
    __Check( kExponentiationUnits == kOTRDHGroupUnits );

    cczp_init_from_bytes(kOTRDHGroupUnits, sOTRGroup_zp,
                         sizeof(kOTRDHGroup), kOTRDHGroup,
                         sizeof(kOTRDHGroup_Recip), kOTRDHGroup_Recip);
    
    ccn_seti(kOTRDHGroupUnits, sOTRGenerator, kOTRGeneratorValue);
}

void OTRExponentiate(cc_unit* res, const cc_unit* base, const cc_unit* exponent)
{
    pthread_once(&kOTRImportGroupData, setupGroupValues);
    
    cczp_power(sOTRGroup_zp, res, base, exponent);
}

void OTRGroupExponentiate(cc_unit* res, const cc_unit* exponent)
{
    pthread_once(&kOTRImportGroupData, setupGroupValues);

    OTRExponentiate(res, sOTRGenerator, exponent);
}

//
// Random Number Generation
//

OSStatus GetRandomBytesInLSBs(size_t bytesOfRandomness, size_t n, cc_unit* place)
{
    OSStatus result = errSecParam;
    require(bytesOfRandomness * 8 <= ccn_bitsof_n(n), fail);
    {
        uint8_t randomBytes[bytesOfRandomness];
        
        result = SecRandomCopyBytes(kSecRandomDefault, sizeof(randomBytes), randomBytes);
        
        require_noerr(result, fail);
        
        ccn_read_uint(n, place, sizeof(randomBytes), randomBytes);
        
        bzero(randomBytes, bytesOfRandomness);
    }
fail:
    return result;
}

OSStatus FillWithRandomBytes(size_t n, cc_unit* place)
{
    return GetRandomBytesInLSBs(ccn_sizeof(n), n, place);
}


static const uint8_t kIVZero[16] = { };

static void AES_CTR_Transform(size_t keySize, const uint8_t* key,
                       const uint8_t iv[16],
                       size_t howMuch, const uint8_t* from, uint8_t* to)
{
    const struct ccmode_ctr* ctr_encrypt = ccaes_ctr_crypt_mode();
    ccctr_ctx_decl(ctr_encrypt->size, ctr_ctx);
    ctr_encrypt->init(ctr_encrypt, ctr_ctx, keySize, key, iv);
    
    ctr_encrypt->ctr(ctr_ctx, howMuch, from, to);
}

void AES_CTR_HighHalf_Transform(size_t keySize, const uint8_t* key,
                                uint64_t highHalf,
                                size_t howMuch, const uint8_t* from, uint8_t* to)
{
    uint8_t iv[16] = { highHalf >> 56, highHalf >> 48, highHalf >> 40, highHalf >> 32,
                       highHalf >> 24, highHalf >> 16, highHalf >> 8 , highHalf >> 0,
                                    0,              0,             0,              0,
                                    0,              0,             0,              0 };
    AES_CTR_Transform(keySize, key, iv, howMuch, from, to);
}

void AES_CTR_IV0_Transform(size_t keySize, const uint8_t* key,
                           size_t howMuch, const uint8_t* from, uint8_t* to)
{
    AES_CTR_Transform(keySize, key, kIVZero, howMuch, from, to);
}


//
// Key Derivation
//

static void HashMPIWithPrefix(uint8_t byte, cc_size sN, const cc_unit* s, uint8_t* buffer)
{
    CFMutableDataRef dataToHash = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataAppendBytes(dataToHash, &byte, 1);
    
    AppendMPI(dataToHash, sN, s);
    
    uint8_t *bytesToHash = CFDataGetMutableBytePtr(dataToHash);
    CFIndex  amountToHash = CFDataGetLength(dataToHash);
    
    /* 64 bits cast: amountToHash is the size of an identity +1 , which is currently hardcoded and never more than 2^32 bytes */
    assert((unsigned long)amountToHash<UINT32_MAX); /* Debug check, Correct as long as CFIndex is a signed long and CC_LONG is a uint32_t */

    (void) CC_SHA256(bytesToHash, (CC_LONG)amountToHash, buffer);
    
    bzero(bytesToHash, (size_t)amountToHash);
    CFReleaseNull(dataToHash);
}

void DeriveOTR256BitsFromS(KeyType whichKey, cc_size sN, const cc_unit* s, size_t keySize, uint8_t* key)
{
    HashMPIWithPrefix(whichKey, sN, s, key);
}

void DeriveOTR128BitPairFromS(KeyType whichKey, size_t sSize, const cc_unit* s,
                              size_t firstKeySize, uint8_t* firstKey,
                              size_t secondKeySize, uint8_t* secondKey)
{
    uint8_t hashBuffer[CCSHA256_OUTPUT_SIZE];
    
    HashMPIWithPrefix(whichKey, sSize, s, hashBuffer);
    
    if (firstKey) {
        firstKeySize = firstKeySize > CCSHA256_OUTPUT_SIZE/2 ? CCSHA256_OUTPUT_SIZE/2 : firstKeySize;
        memcpy(firstKey, hashBuffer, firstKeySize);
    }
    if (secondKey) {
        secondKeySize = secondKeySize > CCSHA256_OUTPUT_SIZE/2 ? CCSHA256_OUTPUT_SIZE/2 : secondKeySize;
        memcpy(secondKey, hashBuffer, secondKeySize);
    }
    
    bzero(hashBuffer, CCSHA256_OUTPUT_SIZE);

}

void DeriveOTR64BitsFromS(KeyType whichKey, size_t sn, const cc_unit* s,
                          size_t topKeySize, uint8_t* topKey)
{
    uint8_t hashBuffer[CCSHA256_OUTPUT_SIZE];
    
    HashMPIWithPrefix(whichKey, sn, s, hashBuffer);
    
    topKeySize = topKeySize > CCSHA256_OUTPUT_SIZE/2 ? CCSHA256_OUTPUT_SIZE/2 : topKeySize;
    memcpy(topKey, hashBuffer, topKeySize);
    
    bzero(hashBuffer, CCSHA256_OUTPUT_SIZE);
}

