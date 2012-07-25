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
 * RawSigner.h - low-level virtual sign/verify object (no digest)
 */
 
#ifndef	_RAW_SIGNER_H_
#define _RAW_SIGNER_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <security_cdsa_utilities/context.h>
#include <security_utilities/alloc.h>

class RawSigner {
public:
	RawSigner(
		Allocator	&alloc,
		CSSM_ALGORITHMS	digestAlg)	:
			mInitFlag(false), 
			mIsSigning(false),
			mDigestAlg(digestAlg),
			mAlloc(alloc) { }
	virtual ~RawSigner()  	{ }
	
	/* 
	 * The use of our mDigestAlg variable is pretty crufty. For some algs, it's 
	 * known and specified at construction time (e.g., CSSM_ALGID_MD5WithRSA). 
	 * For some algs, it's set by CSPFullPluginSession via 
	 * CSPContext::setDigestAlgorithm during raw sign/verify.
	 */
	void 			setDigestAlg(CSSM_ALGORITHMS alg)
											{ mDigestAlg = alg; }

	/* 
	 * The remaining functions must be implemented by subclass. 
	 */

	/* reusable init */
	virtual void signerInit(
		const Context 	&context,
		bool			isSigning) = 0;
	
	/* sign */
	virtual void sign(
		const void 		*data, 
		size_t 			dataLen,
		void			*sig,	
		size_t			*sigLen) = 0;	/* IN/OUT */
		
	/* verify */
	virtual void verify(
		const void 		*data, 
		size_t 			dataLen,
		const void		*sig,			
		size_t			sigLen) = 0;	
		
	/* works for both, but only used for signing */
	virtual size_t maxSigSize() = 0;

protected:
	bool			mInitFlag;				// true after init
	bool			mOpStarted;				// true after update
	bool			mIsSigning;
	CSSM_ALGORITHMS	mDigestAlg;				// for raw sign/verify
	Allocator	&mAlloc;
	
	bool			initFlag() 				{ return mInitFlag; }
	void			setInitFlag(bool flag) 	{ mInitFlag = flag; }
	bool			opStarted() 			{ return mOpStarted; }
	void			setOpStarted(bool flag) { mOpStarted = flag; }
	bool			isSigning()				{ return mIsSigning; }
	void			setIsSigning(bool signing)
											{ mIsSigning = signing; }
	CSSM_ALGORITHMS	digestAlg()				{ return mDigestAlg; }
	Allocator	&alloc()				{ return mAlloc; }
};


#endif	/* _RAW_SIGNER_H_ */
