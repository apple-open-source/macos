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
#include <CommonCrypto/CommonRandomSPI.h>
#include "CommonRandomPriv.h"
#include "ccDispatch.h"
#include <corecrypto/ccaes.h>
#include <corecrypto/ccdrbg.h>
#include <corecrypto/ccrng.h>
#include "ccGlobals.h"
#include "ccMemory.h"
#include "ccErrors.h"
#include "ccdebug.h"

/*
 The types of refs including "undefined" and "user created".
*/

// static const uint32_t rng_undefined = 0;
static const uint32_t rng_default = 1;      // Cryptographic rng from corecrypto
static const uint32_t rng_created = 99;

/*
 These are the two internal structures for a DRBG and /dev/random
 based accessor.
*/

static const ccInternalRandom ccRandomDefaultStruct = {
    .rngtype = rng_default,
    .drbgtype = 0,
    .status = 0,
    .rng = NULL
};

const CCRandomRef kCCRandomDefault   = (CCRandomRef) &ccRandomDefaultStruct;
const CCRandomRef kCCRandomDevRandom = (CCRandomRef) &ccRandomDefaultStruct;

/*
 We don't use /dev/random anymore, use the corecrypto rng instead.
 */

struct ccrng_state *
ccDRBGGetRngState(void)
{
    int status;
    struct ccrng_state *rng=ccrng(&status);
    CC_DEBUG_LOG("ccrng returned %d\n", status);
    return rng;
}

struct ccrng_state *
ccDevRandomGetRngState(void)
{
    return ccDRBGGetRngState();
}

static struct ccrng_state *
ccRefGetRngState(CCRandomRef rnd)
{
    struct ccrng_state *p=NULL;
    if (rnd && rnd->status==0 && rnd->rng) {
        p=rnd->rng;
    }
    return p;
}


CCRNGStatus
CCRNGCreate(uint32_t options, CCRandomRef *rngRef)
{
    ccInternalRandomRef ref;
    ref = CC_XMALLOC(sizeof(ccInternalRandom));
    if(NULL == ref) return kCCMemoryFailure;

    ref->rngtype = rng_created;

    /* in the future, we have the possibility to use option to create
     a deterministic DRBG here. All of the fields are available in ccInternalRandom */
    (void) options;

    ref->rng=ccrng(&ref->status);
    if(ref->status != 0) {
        CC_XFREE(ref, sizeof(ccInternalRandom));
        *rngRef = NULL;
        return kCCUnspecifiedError;
    }
    *rngRef = ref;
    return kCCSuccess;
}

CCRNGStatus
CCRNGRelease(CCRandomRef rng)
{
    CC_DEBUG_LOG("Entering\n");
    if(rng->rngtype == rng_created) {
        cc_clear(sizeof(ccInternalRandom),rng);
        CC_XFREE(rng, sizeof(ccInternalRandom));
    }
    return kCCSuccess;
}

/*
 Read bytes from the corecrypto ccrng
*/

static int
ccRNGReadBytes(struct ccrng_state *state, void *ptr, size_t length)
{
    return ccrng_generate(state, length, ptr);
}

int CCRandomCopyBytes(CCRandomRef rnd, void *bytes, size_t count)
{
    struct ccrng_state *rng;

    // Sanity on the input parameters
    if(0 == count) return 0;
    if(NULL == bytes) return -1;

    if(NULL == rnd) {
        // It should really not be NULL, but we deal with NULL.
        CC_DEBUG_LOG("Entering rnd(NULL)");
        rng = ccDRBGGetRngState();
        return ccRNGReadBytes(rng, bytes, count);
    }

    // Initialization failed, stoping here
    if (rnd->status!=0) {
        CC_DEBUG_LOG("Entering rnd(!NULL), type %d, init status %d", rnd->rngtype, rnd->status);
        return rnd->status; // Initialization failed, stoping here
    }

    // Get the random
    switch(rnd->rngtype) {
        case rng_created:

            // Bytes from the created DRBG
            rng = ccRefGetRngState(rnd);
            return ccRNGReadBytes(rng, bytes, count);
            break;

            // Default corecrypto RNG/DRBG
        case rng_default:
        default:
            rng = ccDRBGGetRngState();
            return ccRNGReadBytes(rng, bytes, count);
            break;
    }
}

CCRNGStatus CCRandomGenerateBytes(void *bytes, size_t count) {
    if(CCRandomCopyBytes(kCCRandomDefault, bytes, count) == 0) return kCCSuccess;
    return kCCRNGFailure;
}



