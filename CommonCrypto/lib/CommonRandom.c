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
#include "CommonRandomPriv.h"
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccdrbg.h>
#include <corecrypto/ccrng_CommonCrypto.h>
#include <corecrypto/ccrng_system.h>
#include "ccGlobals.h"
#include "ccMemory.h"
#include "ccdebug.h"


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

static const ccInternalRandom ccRandomDefaultStruct = {
    .rngtype = rng_default,
};

static const ccInternalRandom ccRandomDevRandomStruct = {
    .rngtype = rng_devrandom,
};

const CCRandomRef kCCRandomDefault = (CCRandomRef) &ccRandomDefaultStruct;
const CCRandomRef kCCRandomDevRandom = (CCRandomRef) &ccRandomDevRandomStruct;

/*
 Initialize (if necessary) and return the ccrng_state pointer for
 the /dev/random rng.
 */

struct ccrng_state *
ccDevRandomGetRngState(void)
{
    cc_globals_t globals = _cc_globals();
    dispatch_once(&globals->dev_random_init, ^{
        globals->dev_random.rngtype = rng_devrandom;
        globals->dev_random.state.devrandom.devrandom = (struct ccrng_system_state *)CC_XMALLOC(sizeof(struct ccrng_system_state));
        ccrng_system_init(globals->dev_random.state.devrandom.devrandom);
        
    });
    return (struct ccrng_state *) globals->dev_random.state.devrandom.devrandom;
}

/*
 Read bytes from /dev/random
*/

static int
ccDevRandomReadBytes(void *ptr, size_t length)
{
    struct ccrng_state *dev_rng_state = ccDevRandomGetRngState();
    for (int retries = 5; retries && ccrng_generate(dev_rng_state, length, ptr); retries--)
        if(retries == 0) return -1;
    return 0;
}



/*
 Initialize (if necessary) and return the ccrng_state pointer for
 the DRBG.
 */

#define DRBG_RNG_STATE(X) (X)->state.drbg.rng_state
#define DRBG_DRBG_STATE(X) (X)->state.drbg.drbg_state
#define DRBG_INFO(X) (X)->state.drbg.info

static int
ccInitDRBG(ccInternalRandomRef drbg, struct ccdrbg_nistctr_custom *options, int function_options)
{
    CCRNGStatus retval = kCCSuccess;
    retval = kCCMemoryFailure; // errors following will be memory failures.
    
    drbg->rngtype = rng_created;
    if((DRBG_INFO(drbg) = CC_XMALLOC(sizeof(struct ccdrbg_info))) == NULL) goto errOut;
    ccdrbg_factory_nistctr(DRBG_INFO(drbg), options);
    if((DRBG_DRBG_STATE(drbg) = CC_XMALLOC(DRBG_INFO(drbg)->size)) == NULL) goto errOut;
    if((DRBG_RNG_STATE(drbg) = CC_XMALLOC(sizeof(struct ccrng_CommonCrypto_state))) == NULL) goto errOut;
    
    if(ccrng_CommonCrypto_init(DRBG_RNG_STATE(drbg), DRBG_INFO(drbg), DRBG_DRBG_STATE(drbg), function_options)) {
        retval = kCCDecodeError;
        goto errOut;
    }
    
    return 0;
errOut:
    if(DRBG_INFO(drbg)) {
        if(DRBG_RNG_STATE(drbg)) CC_XFREE(DRBG_RNG_STATE(drbg), sizeof(struct ccrng_CommonCrypto_state));
        if(DRBG_DRBG_STATE(drbg)) CC_XFREE(DRBG_DRBG_STATE(drbg), drbg->info.drbg->size);
        CC_XFREE(DRBG_INFO(drbg), sizeof(struct ccdrbg_info));
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

static const struct ccdrbg_nistctr_custom DRBGcustom = {
    .ecb = &ccaes_ltc_ecb_encrypt_mode,
    .keylen = 16,
    .strictFIPS = 1,
    .use_df = 1
};

#define KNOWN_DRGB_STATE_SIZE 1160

struct ccrng_state *
ccDRBGGetRngState(void)
{
    cc_globals_t globals = _cc_globals();
    ccInternalRandomRef drbg = &globals->drbg;
    
    dispatch_once(&globals->drbg_init, ^{
        drbg->rngtype = rng_default;
        DRBG_INFO(drbg) = calloc(1, sizeof(struct ccdrbg_info));
        ccdrbg_factory_nistctr(DRBG_INFO(drbg), &DRBGcustom);
        DRBG_DRBG_STATE(drbg) = calloc(1, DRBG_INFO(drbg)->size);
        DRBG_RNG_STATE(drbg) = calloc(1, sizeof(struct ccrng_CommonCrypto_state));
        if (ccrng_CommonCrypto_init(DRBG_RNG_STATE(drbg), DRBG_INFO(drbg), DRBG_DRBG_STATE(drbg), 0)) {
            ASSERT(0);
        }
    });
    return (struct ccrng_state *) DRBG_RNG_STATE(drbg);
}


/*
 Read bytes from the DRBG
*/

static int
ccDRBGReadBytes(struct ccrng_CommonCrypto_state *state, void *ptr, size_t length)
{
    ccrng_generate((struct ccrng_state *) state, length, ptr);
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
    
    if((retval = ccInitDRBG(ref, &custom_options, options)) != 0) {
        CC_XFREE(ref, sizeof(ccInternalRandom));
        return retval;
    }
    *rngRef = ref;

    return kCCSuccess;    
}
    
CCRNGStatus
CCRNGRelease(CCRandomRef rng)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(rng->rngtype == rng_created) {
        ccrng_CommonCrypto_done(rng->state.drbg.rng_state);
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
        return ccDRBGReadBytes((struct ccrng_CommonCrypto_state *)rng, bytes, count);
    }
    
    switch(rnd->rngtype) {
        case rng_default:
            rng = ccDRBGGetRngState();
            return ccDRBGReadBytes((struct ccrng_CommonCrypto_state *)rng, bytes, count);
            break;
        case rng_devrandom:
            (void) ccDevRandomGetRngState();
            return ccDevRandomReadBytes(bytes, count);
            break;
        case rng_created:
            return ccDRBGReadBytes(rnd->state.drbg.rng_state, bytes, count);
            break;
        default: // we can get bytes from the DRBG
            rng = ccDRBGGetRngState();
            return ccDRBGReadBytes((struct ccrng_CommonCrypto_state *)rng, bytes, count);
            break;
    }
}



