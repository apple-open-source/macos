//
//  CommonRandomPriv.h
//  CommonCrypto
//
//  Created by Richard Murphy on 6/11/12.
//  Copyright (c) 2012 Platform Security. All rights reserved.
//

#ifndef CommonCrypto_CommonRandomPriv_h
#define CommonCrypto_CommonRandomPriv_h

#include <corecrypto/ccdrbg.h>
#include <corecrypto/ccrng_CommonCrypto.h>
#include <corecrypto/ccrng_system.h>

/*
 This is an internal structure used to represent the two types
 of random number generators we're using.
 */

typedef struct drbgrng_t {
    struct ccrng_CommonCrypto_state *rng_state;
    struct ccdrbg_info *info;
    struct ccdrbg_state *drbg_state;
} drbg_rng;

typedef struct devrandom_t {
    struct ccrng_system_state *devrandom;
    uint8_t *bytes;
} devrandom_rng;

typedef struct __CCRandom {
    uint32_t rngtype;
    union {
        drbg_rng drbg;
        devrandom_rng devrandom;
    } state;
} ccInternalRandom, *ccInternalRandomRef;


#endif
