//
//  OTRMath.h
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 1/28/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#ifndef _SECOTRMATH_H_
#define _SECOTRMATH_H_

#include <CoreFoundation/CFBase.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>

#define kOTRAuthKeyBytes 16
#define kOTRAuthMACKeyBytes 32

#define kOTRMessageKeyBytes 16
#define kOTRMessageMacKeyBytes 20

#define kExponentiationBits 1536
#define kExponentiationUnits ccn_nof(kExponentiationBits)
#define kExponentiationBytes ((kExponentiationBits+7)/8)

#define kSHA256HMAC160Bits  160
#define kSHA256HMAC160Bytes (kSHA256HMAC160Bits/8)

// Result and exponent are expected to be kExponentiationUnits big.
void OTRExponentiate(cc_unit* res, const cc_unit* base, const cc_unit* exponent);
void OTRGroupExponentiate(cc_unit* result, const cc_unit* exponent);

OSStatus GetRandomBytesInLSBs(size_t bytesOfRandomness, size_t n, cc_unit* place);
OSStatus FillWithRandomBytes(size_t n, cc_unit* place);

typedef enum {
    kSSID = 0x00,
    kCs = 0x01,
    kM1 = 0x02,
    kM2 = 0x03,
    kM1Prime = 0x04,
    kM2Prime = 0x05
} KeyType;


void DeriveOTR256BitsFromS(KeyType whichKey, size_t sSize, const cc_unit* s, size_t keySize, uint8_t* key);
void DeriveOTR128BitPairFromS(KeyType whichHalf, size_t sSize, const cc_unit* s,
                              size_t firstKeySize, uint8_t* firstKey,
                              size_t secondKeySize, uint8_t* secondKey);
void DeriveOTR64BitsFromS(KeyType whichKey, size_t sSize, const cc_unit* s,
                          size_t firstKeySize, uint8_t* firstKey);


void AES_CTR_HighHalf_Transform(size_t keySize, const uint8_t* key,
                                uint64_t highHalf,
                                size_t howMuch, const uint8_t* from,
                                uint8_t* to);

void AES_CTR_IV0_Transform(size_t keySize, const uint8_t* key,
                           size_t howMuch, const uint8_t* from,
                           uint8_t* to);

#endif
