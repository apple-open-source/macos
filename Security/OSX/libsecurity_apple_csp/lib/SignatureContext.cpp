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


/*
 * SignatureContext.h - AppleCSPContext sublass for generic sign/verify
 */

#include "SignatureContext.h"
#include "AppleCSPUtils.h"
#include "AppleCSPSession.h"
#include <Security/cssmtype.h>

#include <security_utilities/debugging.h>

#define cspSigDebug(args...)	secinfo("cspSig", ## args)

SignatureContext::~SignatureContext()
{
	delete &mDigest;
	delete &mSigner;
	mInitFlag = false;
}

/* both sign & verify */
void SignatureContext::init(
	const Context &context, 
	bool isSigning)
{
	mDigest.digestInit();
	mSigner.signerInit(context, isSigning);
	mInitFlag = true;
}

/* both sign & verify */
void SignatureContext::update(
	const CssmData &data)
{
	mDigest.digestUpdate(data.Data, data.Length);
}

/* sign only */
void SignatureContext::final(
	CssmData &out)
{	
	void 		*digest;
	size_t		digestLen;
	void		*sig = out.data();
	size_t		sigLen = out.length();
	
	/* first obtain the digest */
	digestLen = mDigest.digestSizeInBytes();
	digest = session().malloc(digestLen);
	mDigest.digestFinal(digest);
	
	/* now sign */
	try {
		mSigner.sign(digest, 
			digestLen,
			sig,
			&sigLen);
	}
	catch(...) {
		session().free(digest);
		throw;
	}
	session().free(digest);
	if(out.length() < sigLen) {
		cspSigDebug("SignatureContext: mallocd sig too small!");
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	out.length(sigLen);
}

/* verify only */
void SignatureContext::final(
	const CssmData &in)
{	
	void 		*digest;
	size_t		digestLen;
	
	/* first obtain the digest */
	digestLen = mDigest.digestSizeInBytes();
	digest = session().malloc(digestLen);
	mDigest.digestFinal(digest);
	
	/* now verify */
	try {
		mSigner.verify(digest, 
			digestLen,
			in.Data,
			in.Length);
	}
	catch(...) {
		session().free(digest);
		throw;
	}
	session().free(digest);
}

size_t SignatureContext::outputSize(
	bool final,
	size_t inSize)
{
	return mSigner.maxSigSize();
}

/* for raw sign/verify - optionally called after init */ 
void SignatureContext::setDigestAlgorithm(
	CSSM_ALGORITHMS digestAlg)
{
	mSigner.setDigestAlg(digestAlg);
}
