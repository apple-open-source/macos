/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

#ifdef	BSAFE_CSP_ENABLE

//
// miscalgorithms - miscellaneous BSafe context creators and managers
//
#include "bsafecspi.h"

#include <stdio.h>	// debug


//
// Digest algorithms.
// NOTE: There is no init() method, since BSafe digest algorithms re-initialize
// automatically and there is no directional difference.
//
BSafe::DigestContext::DigestContext(
	AppleCSPSession &session,
	const Context &, 
	B_INFO_TYPE bAlgInfo, 
	size_t sz)
	: BSafeContext(session)
{
    mOutSize = sz;
    inUpdate = B_DigestUpdate;
    outFinal = B_DigestFinal;
    setAlgorithm(bAlgInfo);
    check(B_DigestInit(bsAlgorithm, bsKey, chooser(), bsSurrender));
    initialized = true;
}


//
// Signing/Verifying algorithms
//
// FIXME:
// We really should match the key algorithm to the sign/vfy
// algorithm. Also: verify key usage bits. 
void BSafe::SigningContext::init(
	const Context &context, 
	bool signing)
{
    if (reusing(signing))
        return;		// all set to go

    setAlgorithm(algorithm, NULL);
    setKeyFromContext(context);	// may set outSize for some keys

    if (signing) {
        check(B_SignInit(bsAlgorithm, bsKey, chooser(), bsSurrender));
        setRandom();	// needed by some signing algorithms
        inUpdate = B_SignUpdate;
        outFinalR = B_SignFinal;
        outFinal = NULL;
    } else {
        check(B_VerifyInit(bsAlgorithm, bsKey, chooser(), bsSurrender));
        inUpdate = B_VerifyUpdate;
        inFinalR = B_VerifyFinal;
        inFinal = NULL;
    }
}


//
// MAC algorithms.
// Note that BSafe treats MACs as digest algorithms - it has no MAC algorithm
// class. Thus, verifying consists of "digesting" followed by comparing the result.
//
// FIXME : what kind of key do we expect here? For now, any old 
// symmetric key will work...
//
void BSafe::MacContext::init(
	const Context &context, 
	bool signing)
{
    if (reusing(signing))
        return;		// all set to go

    B_DIGEST_SPECIFIER digestSpec;
    digestSpec.digestInfoType = algorithm;
    digestSpec.digestInfoParams = NULL;

    setAlgorithm(AI_HMAC, &digestSpec);
    setKeyFromContext(context);
    check(B_DigestInit(bsAlgorithm, bsKey, chooser(), bsSurrender));
    
    if (signing) {
        inUpdate = B_DigestUpdate;
        outFinal = B_DigestFinal;
    } else {
        inUpdate = B_DigestUpdate;
        // need not set xxFinal - we override final().
    }
}

void BSafe::MacContext::final(const CssmData &in)
{
    // we need to perform a DigestFinal step into a temp buffer and compare to 'in'
    void *digest = normAllocator->malloc(in.length());
    unsigned int length;
    check(B_DigestFinal(bsAlgorithm, POINTER(digest), &length, in.length(), bsSurrender));
    bool verified = length == in.length() && !memcmp(digest, in.data(), in.length());
    normAllocator->free(digest);
	initialized = false;
    if (!verified)
        CssmError::throwMe(CSSMERR_CSP_VERIFY_FAILED);
}


//
// Random-number generation algorithms.
// Note that we don't use bsRandom, since that's our internal fixed "best to use" method,
// not the one the user asked for.
// NOTE: We freeze the output size at init().
//
void BSafe::RandomContext::init(const Context &context, bool)
{
	reset();	// throw away, we need to re-seed anyway
	setAlgorithm(algorithm, NULL);	// MD5 generator mode (RSA proprietary)
	check(B_RandomInit(bsAlgorithm, chooser(), bsSurrender));
	
	// set/freeze output size
	mOutSize = context.getInt(CSSM_ATTRIBUTE_OUTPUT_SIZE, CSSMERR_CSP_MISSING_ATTR_OUTPUT_SIZE);
	
	// seed the PRNG (if specified)
	if (const CssmCryptoData *seed = context.get<CssmCryptoData>(CSSM_ATTRIBUTE_SEED)) {
		const CssmData &seedValue = (*seed)();
		check(B_RandomUpdate(bsAlgorithm, POINTER(seedValue.data()), seedValue.length(), bsSurrender));
	}
}

void BSafe::RandomContext::final(CssmData &data)
{
	check(B_GenerateRandomBytes(bsAlgorithm, POINTER(data.data()), mOutSize, bsSurrender));
}
#endif	/* BSAFE_CSP_ENABLE */
