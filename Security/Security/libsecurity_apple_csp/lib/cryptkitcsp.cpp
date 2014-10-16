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


//
// cryptkitcsp - top C++ implementation layer for CryptKit
//

#ifdef	CRYPTKIT_CSP_ENABLE

#include "cryptkitcsp.h"
#include "FEESignatureObject.h"			/* raw signer */
#include <SignatureContext.h>
#include "FEEKeys.h"
#include "FEEAsymmetricContext.h"
#include <Security/cssmapple.h>
#include <security_cryptkit/falloc.h>
#include <security_cryptkit/feeFunctions.h>
#include <SHA1_MD5_Object.h>
#include <SHA2_Object.h>
#include <security_cdsa_utilities/digestobject.h>

Allocator *CryptKitFactory::normAllocator;
Allocator *CryptKitFactory::privAllocator;

/*
 * CryptKit-style memory allocator callbacks
 */
static void *ckMalloc(unsigned size)
{
	return CryptKitFactory::privAllocator->malloc(size);
}
static void ckFree(void *data)
{
	CryptKitFactory::privAllocator->free(data);
}
static void *ckRealloc(void *oldPtr, unsigned newSize)
{
	return CryptKitFactory::privAllocator->realloc(oldPtr, newSize);
}

//
// Manage the CryptKit algorithm factory
//

CryptKitFactory::CryptKitFactory(Allocator *normAlloc, Allocator *privAlloc)
{
	setNormAllocator(normAlloc);
	setPrivAllocator(privAlloc);
	/* once-per-address space */
	initCryptKit();
	fallocRegister(ckMalloc, ckFree, ckRealloc);
}

CryptKitFactory::~CryptKitFactory()
{
	terminateCryptKit();
}

bool CryptKitFactory::setup(
	AppleCSPSession &session,	
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
	switch(context.type()) {
		case CSSM_ALGCLASS_SIGNATURE:
			switch(context.algorithm()) {
				case CSSM_ALGID_FEE_MD5:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new MD5Object()),
							*(new FEERawSigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				case CSSM_ALGID_FEE_SHA1:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA1Object()),
							*(new FEERawSigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				case CSSM_ALGID_SHA1WithECDSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA1Object()),
							*(new FEEECDSASigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				case CSSM_ALGID_SHA224WithECDSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA224Object()),
							*(new FEEECDSASigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				case CSSM_ALGID_SHA256WithECDSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA256Object()),
							*(new FEEECDSASigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				case CSSM_ALGID_SHA384WithECDSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA384Object()),
							*(new FEEECDSASigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				case CSSM_ALGID_SHA512WithECDSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA512Object()),
							*(new FEEECDSASigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;

				case CSSM_ALGID_FEE:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new NullDigest()),
							*(new FEERawSigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				case CSSM_ALGID_ECDSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new NullDigest()),
							*(new FEEECDSASigner(feeRandCallback, 
								&session,
								session,
								*privAllocator)));
					}
					return true;
				default:
					break;
			}
			break;		

		case CSSM_ALGCLASS_KEYGEN:
			switch(context.algorithm()) {
				case CSSM_ALGID_FEE:
				case CSSM_ALGID_ECDSA:
					if(cspCtx == NULL) {
						cspCtx = new CryptKit::FEEKeyPairGenContext(session, context);
					}
					return true;
				default:
					break;
			}
			break;		

		case CSSM_ALGCLASS_ASYMMETRIC:
			switch(context.algorithm()) {
				case CSSM_ALGID_FEEDEXP:
					if(cspCtx == NULL) {
						cspCtx = new CryptKit::FEEDExpContext(session);
					}
					return true;
				case CSSM_ALGID_FEED:
					if(cspCtx == NULL) {
						cspCtx = new CryptKit::FEEDContext(session);
					}
					return true;
				default:
					break;
			}
			break;		
		
		/* more here - symmetric, etc. */
		default:
			break;
	}
	/* not implemented here */
	return false;
}

#endif	/* CRYPTKIT_CSP_ENABLE */


