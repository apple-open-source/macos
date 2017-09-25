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

/*
 This is an internal structure used to represent the two types
 of random number generators we're using.
 */

typedef union {
    struct ccdrbg_nistctr_custom  nistctr_custom;
    struct ccdrbg_nisthmac_custom nisthmac_custom;
} drbg_custom;

typedef struct drbgrng_t {
    struct ccdrbg_info  *info;
    struct ccdrbg_state *drbg_state;
} drbg_context;

typedef enum {
    rng_default=1,  // Cryptographic rng from corecrypto
    rng_created=99
}rng_type_t;

typedef struct __CCRandom {
    rng_type_t rngtype;
    uint32_t drbgtype;
    struct ccrng_state  *rng;   // Handle to a corecrypto random
    drbg_custom         custom; // DRBG custom
    drbg_context        drbg;   // DRBG info and state
    int status;
} ccInternalRandom, *ccInternalRandomRef;


#endif
