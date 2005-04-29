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
 * FEESignatureObject.h - FEE-based raw sign/verify classes
 */

#ifdef	CRYPTKIT_CSP_ENABLE

#ifndef	_FEE_SIGNATURE_OBJECT_H_
#define _FEE_SIGNATURE_OBJECT_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <security_cryptkit/feePublicKey.h>
#include <security_cryptkit/feeECDSA.h>
#include "FEECSPUtils.h"
#include "CryptKitSpace.h"
#include <RawSigner.h>
#include <AppleCSPSession.h>

namespace CryptKit {

/* 
 * Common raw FEE sign/verify class.
 */
class FEESigner : public RawSigner {
public:
	FEESigner(
		feeRandFcn		randFcn,
		void			*randRef,
		AppleCSPSession &session,
		Allocator	&alloc) : 
			RawSigner(alloc, CSSM_ALGID_NONE),
			mFeeKey(NULL),
			mWeMallocdFeeKey(false),
			mRandFcn(randFcn),
			mRandRef(randRef),
			mSession(session) { }
			
	virtual ~FEESigner();
	
	/* reusable init */
	void signerInit(
		const Context 	&context,
		bool			isSigning);
	
	/* 
	 * obtain key from context, validate, convert to native FEE key
	 */
	void keyFromContext(
		const Context 	&context);

protected:
		feePubKey		mFeeKey;
		bool			mWeMallocdFeeKey;
		feeRandFcn		mRandFcn;
		void			*mRandRef;
		AppleCSPSession	&mSession;
};

/* 
 * And two implementations.
 *
 * Native FEE signature, ElGamal style.
 */
class FEERawSigner : public FEESigner
{
public:
	FEERawSigner(
		feeRandFcn		randFcn,
		void			*randRef,
		AppleCSPSession &session,
		Allocator	&alloc) : 
			FEESigner(randFcn, randRef, session, alloc) { };
			
	~FEERawSigner() { }
	
	/* sign */
	void sign(
		const void	 	*data, 
		size_t 			dataLen,
		void			*sig,	
		size_t			*sigLen);	/* IN/OUT */
		
	/* verify */
	void verify(
		const void 		*data, 
		size_t 			dataLen,
		const void		*sig,			
		size_t			sigLen);	
		
	/* works for both, but only used for signing */
	size_t maxSigSize();
};

/*
 * FEE signature, ECDSA style.
 */
class FEEECDSASigner : public FEESigner
{
public:
	FEEECDSASigner(
		feeRandFcn		randFcn,
		void			*randRef,
		AppleCSPSession &session,
		Allocator	&alloc) : 
			FEESigner(randFcn, randRef, session, alloc) { };
			
	~FEEECDSASigner() { }
	
	/* sign */
	void sign(
		const void	 	*data, 
		size_t 			dataLen,
		void			*sig,	
		size_t			*sigLen);	/* IN/OUT */
		
	/* verify */
	void verify(
		const void	 	*data, 
		size_t 			dataLen,
		const void		*sig,			
		size_t			sigLen);	
		
	/* works for both, but only used for signing */
	size_t maxSigSize();
};

} /* namespace CryptKit */

#endif	/* _FEE_SIGNATURE_OBJECT_H_ */
#endif	/* CRYPTKIT_CSP_ENABLE */
