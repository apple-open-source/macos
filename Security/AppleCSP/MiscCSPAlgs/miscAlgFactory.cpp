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


//
// miscAlgFactory.h - miscellaneous algorithm factory
// Written by Doug Mitchell 3/28/2001
//

#include "miscAlgFactory.h"
#include <AES/aescspi.h>
#include "desContext.h"
#include "rc2Context.h"
#include "rc4Context.h"
#include "rc5Context.h"
#include "MacContext.h"
#include "DigestContext.h"
#include "SHA1_MD5_Object.h"			/* raw digest */
#include "MD2Object.h"
#include <Security/cssmapple.h>

/* 
 * normally CSSM_ALGID_SHA1HMAC_LEGACY maps to a MacLegacyContext if 
 * CRYPTKIT_CSP_ENABLE is true. For quick testing, we also map 
 * CSSM_ALGID_SHA1HMAC to MacLegacyContext.
 */
#define HMAC_BOGUS_ENABLE	0

/*
 * These #defines are mainly to facilitate measuring the performance of our own
 * implementation vs. the ones in BSafe. This factory gets called first; if
 * we disable e.g. DES here the BSAFE version will be used.
 */
#ifdef	BSAFE_CSP_ENABLE

#define MAF_DES_ENABLE		0
#define MAF_DES3_ENABLE		0
#define MAF_RC2_ENABLE		0
#define MAF_RC4_ENABLE		0
#define MAF_RC5_ENABLE		0
#define MAF_MAC_ENABLE		0

#else	/* !BSAFE_CSP_ENABLE, normal case */

#define MAF_DES_ENABLE		1
#define MAF_DES3_ENABLE		1
#define MAF_RC2_ENABLE		1
#define MAF_RC4_ENABLE		1
#define MAF_RC5_ENABLE		1
#define MAF_MAC_ENABLE		1

#endif	/* BSAFE_CSP_ENABLE */

#if	(!MAF_DES_ENABLE || !MAF_DES3_ENABLE || !MAF_RC2_ENABLE || !MAF_RC4_ENABLE || \
		!MAF_RC5_ENABLE || !MAF_MAC_ENABLE)
#warning	Internal DES/RC2/RC4/RC5/Mac implementation disabled! 
#endif

bool MiscAlgFactory::setup(
	AppleCSPSession &session,
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
	CSSM_CONTEXT_TYPE ctype = context.type();
	CSSM_ALGORITHMS alg = context.algorithm();
	
	switch(ctype) {
		case CSSM_ALGCLASS_SYMMETRIC:
			switch(alg) {
				case CSSM_ALGID_AES:
					if(cspCtx == NULL) {
						cspCtx = new AESContext(session);
					}
					return true;

				#if		MAF_DES_ENABLE
				case CSSM_ALGID_DES:
					if(cspCtx == NULL) {
						cspCtx = new DESContext(session);
					}
					return true;
				#endif	/* MAF_DES_ENABLE */
				
				#if		MAF_DES3_ENABLE
				/*
				 * TripleDES: for some reason, cssmtype.h defines different symbols
				 * for CSSM_ALGID_3DES_3KEY (key gen) and CSSM_ALGID_3DES_3KEY_EDE
				 * (an encrypt alg with mode), but they define to the same value.
				 */
				case CSSM_ALGID_3DES_3KEY_EDE:
					if(cspCtx == NULL) {
						cspCtx = new DES3Context(session);
					}
					return true;
				#endif
				
				#if		MAF_RC2_ENABLE
				case CSSM_ALGID_RC2:
					if(cspCtx == NULL) {
						cspCtx = new RC2Context(session);
					}
					return true;
				#endif
				
				#if		MAF_RC4_ENABLE
				case CSSM_ALGID_RC4:
					if(cspCtx == NULL) {
						cspCtx = new RC4Context(session);
					}
					return true;
				#endif
				
				#if		MAF_RC5_ENABLE
				case CSSM_ALGID_RC5:
					if(cspCtx == NULL) {
						cspCtx = new RC5Context(session);
					}
					return true;
				#endif
				
				default:
					break;	// not our symmetric alg
			}				// switch alg for symmetric 
			break;			// from case CSSM_ALGCLASS_SYMMETRIC 
			
		/* digest algorithms always enabled here */
		case CSSM_ALGCLASS_DIGEST:
			switch(alg) {
				case CSSM_ALGID_SHA1:
					if(cspCtx == NULL) {
						/* reuse is OK */
						cspCtx = new DigestContext(session, 
								*(new SHA1Object));
					}
					return true;
				case CSSM_ALGID_MD5:
					if(cspCtx == NULL) {
						/* reuse is OK */
						cspCtx = new DigestContext(session, 
								*(new MD5Object));
					}
					return true;
				case CSSM_ALGID_MD2:
					if(cspCtx == NULL) {
						/* reuse is OK */
						cspCtx = new DigestContext(session, 
								*(new MD2Object));
					}
					return true;
				default:
					break;		// not our digest alg
			}					// switch digest alg
			break;				// from case CSSM_ALGCLASS_DIGEST
			
		case CSSM_ALGCLASS_KEYGEN:
			switch(alg) {
				case CSSM_ALGID_AES:
					if(cspCtx == NULL) {
						cspCtx = new AESKeyGenContext(session);
					}
					return true;

				#if		MAF_DES_ENABLE
				case CSSM_ALGID_DES:
					if(cspCtx == NULL) {
						cspCtx = new AppleSymmKeyGenerator(session,
							DES_KEY_SIZE_BITS_EXTERNAL,
							DES_KEY_SIZE_BITS_EXTERNAL,
							true);				// must be byte size
					}
					return true;
				#endif	/* MAF_DES_ENABLE */
				
				#if		MAF_DES3_ENABLE
				case CSSM_ALGID_3DES_3KEY_EDE:
					if(cspCtx == NULL) {
						cspCtx = new AppleSymmKeyGenerator(session,
							DES3_KEY_SIZE_BYTES * 8,
							DES3_KEY_SIZE_BYTES * 8,
							true);			// must be byte size
					}
					return true;
				#endif
				
				#if		MAF_RC2_ENABLE
				case CSSM_ALGID_RC2:
					if(cspCtx == NULL) {
						cspCtx = new AppleSymmKeyGenerator(session,
							RC2_MIN_KEY_SIZE_BYTES * 8,
							RC2_MAX_KEY_SIZE_BYTES * 8,
							true);				// must be byte size
					}
					return true;
				#endif
				
				#if		MAF_RC4_ENABLE
				case CSSM_ALGID_RC4:
					if(cspCtx == NULL) {
						cspCtx = new AppleSymmKeyGenerator(session,
							RC4_MIN_KEY_SIZE_BYTES * 8,
							RC4_MAX_KEY_SIZE_BYTES * 8,
							true);				// must be byte size
					}
					return true;
				#endif
				
				#if		MAF_RC5_ENABLE
				case CSSM_ALGID_RC5:
					if(cspCtx == NULL) {
						cspCtx = new AppleSymmKeyGenerator(session,
							RC5_MIN_KEY_SIZE_BYTES * 8,
							RC5_MAX_KEY_SIZE_BYTES * 8,
							true);				// must be byte size
					}
					return true;
				#endif
				
				#if		MAF_MAC_ENABLE
				case CSSM_ALGID_SHA1HMAC:
					if(cspCtx == NULL) {
						cspCtx = new AppleSymmKeyGenerator(session,
							HMAC_MIN_KEY_SIZE * 8,
							HMAC_MAX_KEY_SIZE * 8,
							true);				// must be byte size
					}
					return true;
				#endif
				
				default:
					break;	// not our keygen alg
			}				// switch alg for keygen
			break;			// from case CSSM_ALGCLASS_KEYGEN
			
		case CSSM_ALGCLASS_MAC:
			switch(alg) {
				#if		MAF_MAC_ENABLE
				case CSSM_ALGID_SHA1HMAC:
					if(cspCtx == NULL) {
						#if		HMAC_BOGUS_ENABLE
						/* quick hack for Keychain Access testing */
						cspCtx = new MacLegacyContext(session);
						#else
						cspCtx = new MacContext(session);
						#endif
					}
					return true;
				#endif
				#if		CRYPTKIT_CSP_ENABLE
				case CSSM_ALGID_SHA1HMAC_LEGACY:
					if(cspCtx == NULL) {
						cspCtx = new MacLegacyContext(session);
					}
					return true;
				#endif
				default:
					/* not our mac alg */
					break;
			}
			break;
			
		default:
			break;			// not our context type
	}						// switch context type
	
	/* not ours */
	return false;
}
