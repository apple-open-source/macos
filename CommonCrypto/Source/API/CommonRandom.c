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

// #define COMMON_RANDOM_FUNCTIONS
#include "CommonRandomSPI.h"
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccdrbg.h>
#include <corecrypto/ccrng_CommonCrypto.h>
#include <corecrypto/ccrng_system.h>
#include "ccMemory.h"
#include "ccdebug.h"


/*
 This is an internal structure used to represent the two types
 of random number generators we're using.
*/

typedef struct __CCRandom {
    uint32_t rngtype;
    union {
        struct ccrng_system_state *devrandom;
        struct ccrng_CommonCrypto_state *drbg;
    } state;
    union {
        uint8_t *bytes;
        struct ccdrbg_info *drbg;
    } info;
    struct ccdrbg_state *drbg_state;
} ccInternalRandom, *ccInternalRandomRef;

/*
 The types of refs including "undefined" and "user created".
*/

static const uint32_t rng_undefined = 0;
static const uint32_t rng_default = 1;
static const uint32_t rng_devrandom = 2;
static const uint32_t rng_created = 99;

/*
 These are the two internal structures for a DRBG and /dev/random
 based accessor.
*/

ccInternalRandom ccRandomDefaultStruct = {
    .rngtype = rng_default,
};

ccInternalRandom ccRandomDevRandomStruct = {
    .rngtype = rng_devrandom,
};

CCRandomRef kCCRandomDefault = &ccRandomDefaultStruct;
CCRandomRef kCCRandomDevRandom = &ccRandomDevRandomStruct;

/*
 Initialize (if necessary) and return the ccrng_state pointer for
 the /dev/random rng.
 */

struct ccrng_state *
ccDevRandomGetRngState()
{
    static dispatch_once_t rnginit;
    dispatch_once(&rnginit, ^{        
        kCCRandomDevRandom->state.devrandom = (struct ccrng_state *) CC_XMALLOC(sizeof(struct ccrng_system_state));
        ccrng_system_init(kCCRandomDevRandom->state.devrandom);
        
    });
    return kCCRandomDevRandom->state.devrandom;
}

/*
 Read bytes from /dev/random
*/

int
ccDevRandomReadBytes(void *ptr, size_t length)
{    
    for(int retries = 5; retries && ccrng_generate(kCCRandomDevRandom->state.devrandom, length, ptr); retries--)
        if(retries == 0) return -1;
    return 0;
}



/*
 Initialize (if necessary) and return the ccrng_state pointer for
 the DRBG.
 */

static int
ccInitDRBG(ccInternalRandomRef drbg, struct ccdrbg_nistctr_custom *options, int function_options)
{
    CCRNGStatus retval = kCCSuccess;
    //uint8_t entropy[64];
    //struct timeval now;
    
    //gettimeofday(&now, NULL);
    // ccDevRandomGetRngState();
    // if(ccDevRandomReadBytes(entropy, sizeof(entropy))) return kCCDecodeError;
    
    
    retval = kCCMemoryFailure; // errors following will be memory failures.
    
    if((drbg->info.drbg = CC_XMALLOC(sizeof(struct ccdrbg_info))) == NULL) goto errOut;
    ccdrbg_factory_nistctr(drbg->info.drbg, options);
    if((drbg->drbg_state = CC_XMALLOC(drbg->info.drbg->size)) == NULL) goto errOut;
    if((drbg->state.drbg = CC_XMALLOC(sizeof(struct ccrng_CommonCrypto_state))) == NULL) goto errOut;
    
    if(ccrng_CommonCrypto_init(drbg->state.drbg, drbg->info.drbg, drbg->drbg_state, function_options)) {
        retval = kCCDecodeError;
        goto errOut;
    }
    
    return 0;
errOut:
    if(drbg->info.drbg) {
        if(drbg->state.drbg) CC_XFREE(drbg->state.drbg, sizeof(struct ccrng_CommonCrypto_state));
        if(drbg->drbg_state) CC_XFREE(drbg->drbg_state, drbg->info.drbg->size);
        CC_XFREE(drbg->info.drbg, sizeof(struct ccdrbg_info));
    }
    return retval;
}

#ifndef	NDEBUG
#define ASSERT(s)
#else
#define ASSERT(s)	assert(s)
#endif

/*
 Default DRGB setup
 */

static const struct ccdrbg_nistctr_custom CCDRGBcustom = {
    .ecb = &ccaes_ltc_ecb_encrypt_mode,
    .keylen = 16,
    .strictFIPS = 1,
    .use_df = 1
};

static struct ccdrbg_info CCDRGBinfo;
#define KNOWN_DRGB_STATE_SIZE 1160
static uint8_t CCDRGBstate[KNOWN_DRGB_STATE_SIZE];
struct ccrng_CommonCrypto_state CCDRGBrngstate;

struct ccrng_state *
ccDRBGGetRngState()
{
    static dispatch_once_t rnginit;
    
    dispatch_once(&rnginit, ^{
        kCCRandomDefault->info.drbg = &CCDRGBinfo;
        ccdrbg_factory_nistctr(kCCRandomDefault->info.drbg, &CCDRGBcustom);
        ASSERT(kCCRandomDefault->info.drbg->size <= sizeof(CCDRGBstate));    
        kCCRandomDefault->drbg_state = CCDRGBstate;
        kCCRandomDefault->state.drbg = &CCDRGBrngstate;
        if(ccrng_CommonCrypto_init(&CCDRGBrngstate, &CCDRGBinfo, CCDRGBstate, 0)) {
            kCCRandomDefault = NULL;
        }
    });
    ASSERT(kCCRandomDefault != NULL);
    if(kCCRandomDefault == NULL) return NULL;
    return kCCRandomDefault->state.drbg;
}


/*
 Read bytes from the DRBG
*/

int
ccDRBGReadBytes(struct ccrng_CommonCrypto_state *state, void *ptr, size_t length)
{
    ccrng_generate(state, length, ptr);
    return 0;
}


CCRNGStatus
CCRNGCreate(uint32_t options, CCRandomRef *rngRef)
{
    CCRNGStatus retval;
    ccInternalRandomRef ref;
    struct ccdrbg_nistctr_custom custom_options;

    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    ref = CC_XMALLOC(sizeof(ccInternalRandom));
    if(NULL == ref) return kCCMemoryFailure;
    
    ref->rngtype = rng_created;
    // defaults
    custom_options.ecb = &ccaes_ltc_ecb_encrypt_mode;
    custom_options.keylen = 16;
    custom_options.strictFIPS = 1;
    custom_options.use_df = 1;
    
    if(retval = ccInitDRBG(ref, &custom_options, options)) return retval;
    *rngRef = ref;

    return kCCSuccess;    
}
    
CCRNGStatus
CCRNGRelease(CCRandomRef rng)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(rng->rngtype == rng_created) {
        ccrng_CommonCrypto_done(rng->state.drbg);
        CC_XFREE(rng, sizeof(ccInternalRandom));
    }
    return kCCSuccess;        
}

int CCRandomCopyBytes(CCRandomRef rnd, void *bytes, size_t count)
{
    struct ccrng_state *rng;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering rnd(NULL) = %s\n", (rnd == NULL) ? "TRUE": "FALSE");

    
    if(NULL == bytes) return -1;
    if(0 == count) return 0;
    if(NULL == rnd) {
        rng = ccDRBGGetRngState();
        return ccDRBGReadBytes(rng, bytes, count);
    }
    
    switch(rnd->rngtype) {
        case rng_default:
            rng = ccDRBGGetRngState();
            return ccDRBGReadBytes(rng, bytes, count);
            break;
        case rng_devrandom:
            rng = ccDevRandomGetRngState();
            return ccDevRandomReadBytes(bytes, count);
            break;
        case rng_created:
            return ccDRBGReadBytes(rnd->state.drbg, bytes, count);
            break;
        default: // we can get bytes from the DRBG
            rng = ccDRBGGetRngState();
            return ccDRBGReadBytes(rng, bytes, count);
            break;
    }
}



