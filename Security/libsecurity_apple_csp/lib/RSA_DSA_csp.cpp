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


/*
 * RSA_DSA_csp.cpp - Algorithm factory for RSA/DSA 
 */
 
#include "RSA_DSA_csp.h"
#include "RSA_DSA_signature.h"					/* raw signer */
#include <SHA1_MD5_Object.h>		/* raw digest */
#include <SignatureContext.h>
#include <security_cdsa_utilities/digestobject.h>
#include "RSA_DSA_keys.h"
#include "RSA_asymmetric.h"
#include <MD2Object.h>
#include <SHA2_Object.h>
#include <Security/cssmapple.h>

#define OPENSSL_DSA_ENABLE	1

Allocator *RSA_DSA_Factory::normAllocator;
Allocator *RSA_DSA_Factory::privAllocator;

/* normally found in crypto.h, which has way too much useless cruft....move these to
 * a local header.... */
extern "C" {
extern int CRYPTO_set_mem_functions(
	void *(*m)(size_t),
	void *(*r)(void *,size_t), 
	void (*f)(void *));
int CRYPTO_set_locked_mem_functions(
	void *(*m)(size_t), 
	void (*free_func)(void *));
}

/*
 * openssl-style memory allocator callbacks
 */
static void *osMalloc(size_t size)
{
	return RSA_DSA_Factory::privAllocator->malloc(size);
}
static void osFree(void *data)
{
	RSA_DSA_Factory::privAllocator->free(data);
}
static void *osRealloc(void *oldPtr, size_t newSize)
{
	return RSA_DSA_Factory::privAllocator->realloc(oldPtr, newSize);
}

RSA_DSA_Factory::RSA_DSA_Factory(Allocator *normAlloc, Allocator *privAlloc)
{
	setNormAllocator(normAlloc);
	setPrivAllocator(privAlloc);
	/* once-per-address space */
	CRYPTO_set_mem_functions(osMalloc, osRealloc, osFree);
	CRYPTO_set_locked_mem_functions(osMalloc, osFree);
	/* these should go in a lib somewhere */
	ERR_load_RSA_strings();
	ERR_load_BN_strings();
	ERR_load_DSA_strings();
}

RSA_DSA_Factory::~RSA_DSA_Factory()
{
	// TBD terminateCryptKit();
}

bool RSA_DSA_Factory::setup(
	AppleCSPSession &session,	
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
	switch(context.type()) {
		case CSSM_ALGCLASS_SIGNATURE:
			switch(context.algorithm()) {
				case CSSM_ALGID_SHA1WithRSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA1Object()),
							*(new RSASigner(*privAllocator,	
								session,
								CSSM_ALGID_SHA1)));
					}
					return true;
				case CSSM_ALGID_MD5WithRSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new MD5Object()),
							*(new RSASigner(*privAllocator,	
								session,
								CSSM_ALGID_MD5)));
					}
					return true;
				case CSSM_ALGID_MD2WithRSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new MD2Object()),
							*(new RSASigner(*privAllocator,	
								session,
								CSSM_ALGID_MD2)));
					}
					return true;
				#if	OPENSSL_DSA_ENABLE
				case CSSM_ALGID_SHA1WithDSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA1Object()),
							*(new DSASigner(*privAllocator,	
								session,
								CSSM_ALGID_SHA1)));
					}
					return true;
				case CSSM_ALGID_DSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new NullDigest()),
							*(new DSASigner(*privAllocator,	
								session,
								// set later via setDigestAlgorithm but not used by DSA
								CSSM_ALGID_NONE)));	
					}
					return true;
				#endif
				case CSSM_ALGID_RSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new NullDigest()),
							*(new RSASigner(*privAllocator,	
								session,
								// set later via setDigestAlgorithm
								CSSM_ALGID_NONE)));	
					}
					return true;
				case CSSM_ALGID_SHA256WithRSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA256Object()),
							*(new RSASigner(*privAllocator,	
								session,
								CSSM_ALGID_SHA256)));
					}
					return true;
				case CSSM_ALGID_SHA224WithRSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA224Object()),
							*(new RSASigner(*privAllocator,	
								session,
								CSSM_ALGID_SHA224)));
					}
					return true;
				case CSSM_ALGID_SHA384WithRSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA384Object()),
							*(new RSASigner(*privAllocator,	
								session,
								CSSM_ALGID_SHA384)));
					}
					return true;
				case CSSM_ALGID_SHA512WithRSA:
					if(cspCtx == NULL) {
						cspCtx = new SignatureContext(session,
							*(new SHA512Object()),
							*(new RSASigner(*privAllocator,	
								session,
								CSSM_ALGID_SHA512)));
					}
					return true;
				default:
					break;
			}
			break;		
		
		case CSSM_ALGCLASS_KEYGEN:
			switch(context.algorithm()) {
				case CSSM_ALGID_RSA:
				case CSSM_ALGMODE_PKCS1_EME_OAEP:
					if(cspCtx == NULL) {
						cspCtx = new RSAKeyPairGenContext(session, context);
					}
					return true;
				#if	OPENSSL_DSA_ENABLE
				case CSSM_ALGID_DSA:
					if(cspCtx == NULL) {
						cspCtx = new DSAKeyPairGenContext(session, context);
					}
					return true;
				#endif
				default:
					break;
			}
			break;		

		case CSSM_ALGCLASS_ASYMMETRIC:
			switch(context.algorithm()) {
				case CSSM_ALGID_RSA:
				case CSSM_ALGMODE_PKCS1_EME_OAEP:
					if(cspCtx == NULL) {
						cspCtx = new RSA_CryptContext(session);
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



