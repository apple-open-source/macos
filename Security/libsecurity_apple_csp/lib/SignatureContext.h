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
 * SignatureContext.h - AppleCSPContext subclass for generic sign/verify
 *
 * This class performs all of the sign/verify operations in the CSP. The general
 * scheme is that an instance of this class has references to one DigestObject
 * and one RawSigner. Sign and Verify "updates" go to the DigestObject. The "final"
 * operation consists of obtaining the final digest from the DigestObject and 
 * performing a sign or verify on that data via the RawSigner. 
 *
 * This class knows nothing about any of the algorithms involved; all sign and 
 * verify operations follow this same scheme. Various modules' AlgorithmFactories
 * construct one of these objects by providing the appropriate DigestObject and
 * RawSigner. 
 *
 * The seemingly special case of "raw RSA sign", in which the app calculates the 
 * digest separately from the sign operation, is handled via the NullDigest object.
 */
 
#ifndef	_SIGNATURE_CONTEXT_H_
#define _SIGNATURE_CONTEXT_H_

#include <RawSigner.h>
#include <security_cdsa_utilities/digestobject.h>
#include <AppleCSPContext.h>

class SignatureContext : public AppleCSPContext  {
public:
	SignatureContext(
		AppleCSPSession &session,
		DigestObject	&digest,
		RawSigner		&signer) : 
			AppleCSPContext(session), 
			mDigest(digest), 
			mSigner(signer),
			mInitFlag(false) { }
			
	~SignatureContext();
	
	/* called out from CSPFullPluginSession....
	 * both sign & verify: */
	void init(const Context &context, bool isSigning);
	void update(const CssmData &data);
	
	/* sign only */
	void final(CssmData &out);	
	
	/* verify only */
	void final(const CssmData &in);	

	size_t outputSize(bool final, size_t inSize);

	/* for raw sign/verify - optionally called after init */ 
	virtual void setDigestAlgorithm(CSSM_ALGORITHMS digestAlg);


private:
	DigestObject	&mDigest;
	RawSigner		&mSigner;
	bool			mInitFlag;			// true after init
};


#endif	/* _SIGNATURE_CONTEXT_H_ */
