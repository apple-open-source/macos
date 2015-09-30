/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// bsafeAsymmetric.cpp - asymmetric encrypt/decrypt
//
#include "bsafecspi.h"

#include <stdio.h>	// debug

//
// Public key {en,de}cryption (currently RSA only)
//
// FIXME:
// We really should match the key algorithm to the en/decrypt
// algorithm. Also: verify key usage bits. 
void BSafe::PublicKeyCipherContext::init(const Context &context, bool encrypting)
{
	assert(context.algorithm() == CSSM_ALGID_RSA);
	
    if (reusing(encrypting))
        return;		// all set to go
    
    switch (context.getInt(CSSM_ATTRIBUTE_MODE)) {
        case CSSM_ALGMODE_PUBLIC_KEY:
            setAlgorithm(AI_PKCS_RSAPublic);
            break;
        case CSSM_ALGMODE_PRIVATE_KEY:
            setAlgorithm(AI_PKCS_RSAPrivate);
            break;
        case CSSM_ALGMODE_NONE:	
		{
			/* 
			 * None specified (getInt returns zero in that case) - 
			 * infer from key type 
			 */
			CssmKey &key = context.get<CssmKey>(
				CSSM_ATTRIBUTE_KEY, CSSMERR_CSP_MISSING_ATTR_KEY);
			B_INFO_TYPE bAlgType;
			switch (key.keyClass()) {
				case CSSM_KEYCLASS_PUBLIC_KEY:
					bAlgType = AI_PKCS_RSAPublic;
					break;
				case CSSM_KEYCLASS_PRIVATE_KEY:
					bAlgType = AI_PKCS_RSAPrivate;
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
            setAlgorithm(bAlgType);
            break;
		}
        default:
            CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_MODE);
    }

    // put it all together
    setKeyFromContext(context);		// set BSafe key
    setRandom();					// some PK cryption algs need random input
    cipherInit();					// common cipher init
    //@@@ calculate output buffer size
}

// we assume asymmetric crypto algorithms are one-shot output non-repeating

size_t BSafe::PublicKeyCipherContext::inputSize(size_t outSize)
{
    return 0xFFFFFFFF;	// perhaps not the biggest size_t, but big enough...
}
#endif	/* BSAFE_CSP_ENABLE */
