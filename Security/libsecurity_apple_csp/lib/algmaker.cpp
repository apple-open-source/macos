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
// algmaker - algorithm factory for BSafe 4
//
#include "bsafecspi.h"
#include "bsafecsp.h"
#include "AppleCSPSession.h"

//
// BSafe's Chooser table. 
// These are references to all *BSafe algorithms* we use (and thus must link in)
//
const B_ALGORITHM_METHOD * const BSafe::bsChooser[] = {
    // digests
    &AM_SHA,
    &AM_MD5,
	&AM_MD2,

    // organizational
    &AM_CBC_ENCRYPT,
    &AM_CBC_DECRYPT,
    &AM_ECB_ENCRYPT,
    &AM_ECB_DECRYPT,
    &AM_OFB_ENCRYPT,
    &AM_OFB_DECRYPT,

    // DES & variants
    &AM_DES_ENCRYPT,
    &AM_DES_DECRYPT,
    &AM_DESX_ENCRYPT,
    &AM_DESX_DECRYPT,
    &AM_DES_EDE_ENCRYPT,
    &AM_DES_EDE_DECRYPT,

    // RCn stuff
    &AM_RC2_CBC_ENCRYPT,
    &AM_RC2_CBC_DECRYPT,
    &AM_RC2_ENCRYPT,
    &AM_RC2_DECRYPT,
    &AM_RC4_ENCRYPT,
    &AM_RC4_DECRYPT,
    &AM_RC5_ENCRYPT,
    &AM_RC5_DECRYPT,
    &AM_RC5_CBC_ENCRYPT,
    &AM_RC5_CBC_DECRYPT,

    // RSA
    &AM_RSA_STRONG_KEY_GEN,
    &AM_RSA_KEY_GEN,
    &AM_RSA_CRT_ENCRYPT_BLIND,
    &AM_RSA_CRT_DECRYPT_BLIND,
    &AM_RSA_ENCRYPT,
    &AM_RSA_DECRYPT,

    // DSA
    &AM_DSA_PARAM_GEN,
    &AM_DSA_KEY_GEN,

    // signatures
    &AM_DSA_SIGN,
    &AM_DSA_VERIFY,

    // random number generation
    &AM_MD5_RANDOM,
    &AM_SHA_RANDOM,

    // sentinel
    (B_ALGORITHM_METHOD *)NULL_PTR
};


//
// Makers
//
template <class Ctx>
class Maker0 : public BSafe::MakerBase {
public:
	Ctx *make(AppleCSPSession &session, const Context &context) const
	{ return new Ctx(session, context); }
};

template <class Ctx, class Arg>
class Maker1 : public BSafe::MakerBase {
	Arg arg;
public:
	Maker1(Arg a) : arg(a) { }
	Ctx *make(AppleCSPSession &session, const Context &context) const
	{ return new Ctx(session, context, arg); }
};

template <class Ctx, class Arg1, class Arg2>
class Maker2 : public BSafe::MakerBase {
	Arg1 arg1; Arg2 arg2;
public:
	Maker2(Arg1 a1, Arg2 a2) : arg1(a1), arg2(a2) { }
	Ctx *make(AppleCSPSession &session, const Context &context) const
	{ return new Ctx(session, context, arg1, arg2); }
};

template <class Ctx, class Arg1, class Arg2, class Arg3>
class Maker3 : public BSafe::MakerBase {
	Arg1 arg1; Arg2 arg2; Arg3 arg3;
public:
	Maker3(Arg1 a1, Arg2 a2, Arg3 a3) : 
		arg1(a1), arg2(a2), arg3(a3) { }
	Ctx *make(AppleCSPSession &session, const Context &context) const
	{ return new Ctx(session, context, arg1, arg2, arg3); }
};


bug_const BSafe::MakerTable BSafe::algorithms[] = {
    // signing algorithms
	// constructor args: BSafe algorithm, signature size
    { 
		CSSM_ALGID_SHA1WithDSA, 
		CSSM_ALGCLASS_SIGNATURE,
		new Maker2<SigningContext, B_INFO_TYPE, size_t>
			(AI_DSAWithSHA1, 48) 			// max size of 48 bytes
	},
    { 
		CSSM_ALGID_SHA1WithRSA, 
		CSSM_ALGCLASS_SIGNATURE,
		new Maker2<SigningContext, B_INFO_TYPE, size_t>
			(AI_SHA1WithRSAEncryption, 0) 	// size = RSA key size
	},
	
    { 
		CSSM_ALGID_MD5WithRSA, 
		CSSM_ALGCLASS_SIGNATURE,
		new Maker2<SigningContext, B_INFO_TYPE, size_t>
			(AI_MD5WithRSAEncryption, 0) 	// size = RSA key size
	},
	
    { 
		CSSM_ALGID_MD2WithRSA, 
		CSSM_ALGCLASS_SIGNATURE,
		new Maker2<SigningContext, B_INFO_TYPE, size_t>
			(AI_MD2WithRSAEncryption, 0) 	// size = RSA key size
	},
	
    // MAC algorithms
	// constructor args: BSafe algorithm, signature size
    { 
		CSSM_ALGID_SHA1HMAC,	
		CSSM_ALGCLASS_MAC,
		new Maker2<MacContext, B_INFO_TYPE, size_t>
			(AI_SHA1, 20) 
	},

	// symmetric key generation
	// constructor args: min/max key size in bits, mustBeByteSized
    { 
		CSSM_ALGID_RC2,	
		CSSM_ALGCLASS_KEYGEN,
		new Maker3<SymmetricKeyGenContext, uint32, uint32, bool>
			(1*8, 128*8, true) 
	},
    { 
		CSSM_ALGID_RC4,	
		CSSM_ALGCLASS_KEYGEN,
		new Maker3<SymmetricKeyGenContext, uint32, uint32, bool>
			(1*8, 256*8, true) 
	},
    { 
		CSSM_ALGID_RC5,	
		CSSM_ALGCLASS_KEYGEN,
		new Maker3<SymmetricKeyGenContext, uint32, uint32, bool>
			(1*8, 255*8, true) 
	},
    { 
		CSSM_ALGID_DES,	
		CSSM_ALGCLASS_KEYGEN,
		new Maker3<SymmetricKeyGenContext, uint32, uint32, bool>
			(64, 64, true) 
	},
    { 
		CSSM_ALGID_DESX,	
		CSSM_ALGCLASS_KEYGEN,
		new Maker3<SymmetricKeyGenContext, uint32, uint32, bool>
			(192, 192, true) 
	},
    { 
		CSSM_ALGID_3DES_3KEY,	
		CSSM_ALGCLASS_KEYGEN,
		new Maker3<SymmetricKeyGenContext, uint32, uint32, bool>
			(192, 192, true) 
	},
    { 
		CSSM_ALGID_SHA1HMAC,	
		CSSM_ALGCLASS_KEYGEN,
		new Maker3<SymmetricKeyGenContext, uint32, uint32, bool>
			(160, 2048, true) 
	},
	
    // symmetric encryption algorithms
	// constructor arg: block size (1 ==> stream cipher)
    { 
		CSSM_ALGID_DES,
		CSSM_ALGCLASS_SYMMETRIC,		
		new Maker1<BlockCipherContext, size_t>(8)
	},
    { 
		CSSM_ALGID_DESX,
		CSSM_ALGCLASS_SYMMETRIC,
		new Maker1<BlockCipherContext, size_t>(8)
	},
    { 
		CSSM_ALGID_3DES_3KEY_EDE,
		CSSM_ALGCLASS_SYMMETRIC,
		new Maker1<BlockCipherContext, size_t>(8)
	},
    { 
		CSSM_ALGID_RC2,
		CSSM_ALGCLASS_SYMMETRIC,
		new Maker1<BlockCipherContext, size_t>(8)
	},
    { 
		CSSM_ALGID_RC4,
		CSSM_ALGCLASS_SYMMETRIC,
		new Maker1<BlockCipherContext, size_t>(1)
	},
    { 
		CSSM_ALGID_RC5,
		CSSM_ALGCLASS_SYMMETRIC,
		new Maker1<BlockCipherContext, size_t>(8)
	},

    // asymmetric encryption algorithms
    { 
		CSSM_ALGID_RSA,		
		CSSM_ALGCLASS_ASYMMETRIC,
		new Maker0<PublicKeyCipherContext>()
	},
    { 
		CSSM_ALGID_DSA,
		CSSM_ALGCLASS_ASYMMETRIC,
		new Maker0<PublicKeyCipherContext>()
	},
	
	// key pair generate algorithms
    { 
		CSSM_ALGID_RSA,		
		CSSM_ALGCLASS_KEYGEN,
		new Maker0<BSafeKeyPairGenContext>()
	},
    { 
		CSSM_ALGID_DSA,
		CSSM_ALGCLASS_KEYGEN,
		new Maker0<BSafeKeyPairGenContext>()
	},
	
	// pseudo-random number generators
	{ 
		CSSM_ALGID_MD5Random,	
		CSSM_ALGCLASS_RANDOMGEN,
		new Maker1<RandomContext, B_INFO_TYPE>(AI_MD5Random) 
	},
	{ 
		CSSM_ALGID_SHARandom, 
		CSSM_ALGCLASS_RANDOMGEN,
		new Maker1<RandomContext, B_INFO_TYPE>(AI_SHA1Random) 
	},
};

const unsigned int BSafe::algorithmCount = sizeof(algorithms) / sizeof(algorithms[0]);


//
// BSafeFactory hookup
//
void BSafeFactory::setNormAllocator(Allocator *alloc)
{
	BSafe::setNormAllocator(alloc);
}
void BSafeFactory::setPrivAllocator(Allocator *alloc)
{
	BSafe::setPrivAllocator(alloc);
}

bool BSafeFactory::setup(
	AppleCSPSession &session,
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
	return BSafe::setup(session, cspCtx, context);
}


//
// Algorithm setup
//
bool BSafe::setup(
	AppleCSPSession &session,
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
    for (const BSafe::MakerTable *alg = algorithms; 
	     alg < algorithms + algorithmCount; 
		 alg++) {
        if ((alg->algorithmId == context.algorithm()) &&
		    (alg->algClass == context.type())) {
				if(cspCtx != NULL) {
					/* we allow reuse */
					return true;
				}
				// make new context
				cspCtx = alg->maker->make(session, context);
				return true;
        }
	}
	/* not ours */
    return false;
}
#endif	/* BSAFE_CSP_ENABLE */
