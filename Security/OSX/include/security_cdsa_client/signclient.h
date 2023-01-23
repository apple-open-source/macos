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
// signclient - client interface to CSSM sign/verify contexts
//
#ifndef _H_CDSA_CLIENT_SIGNCLIENT
#define _H_CDSA_CLIENT_SIGNCLIENT  1

#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/keyclient.h>

namespace Security {
namespace CssmClient {


//
// A signing/verifying context
//
class SigningContext : public Context
{
public:
	SigningContext(const CSP &csp, CSSM_ALGORITHMS alg, CSSM_ALGORITHMS signOnly = CSSM_ALGID_NONE)
	: Context(csp, alg), mSignOnly(signOnly) { }

	Key key() const { assert(mKey); return mKey; }
	void key(const Key &k) { mKey = k; set(CSSM_ATTRIBUTE_KEY, mKey); }
    
    CSSM_ALGORITHMS signOnlyAlgorithm() const	{ return mSignOnly; }
    void signOnlyAlgorithm(CSSM_ALGORITHMS alg)	{ mSignOnly = alg; }

protected:
	void activate();
	CSSM_ALGORITHMS mSignOnly;
	Key mKey;
};


class Sign : public SigningContext
{
public:
	Sign(const CSP &csp, CSSM_ALGORITHMS alg, CSSM_ALGORITHMS signOnly = CSSM_ALGID_NONE)
        : SigningContext(csp, alg, signOnly) { }
	
	// integrated
	void sign(const CssmData &data, CssmData &signature) { sign(&data, 1, signature); }
	void sign(const CssmData *data, uint32 count, CssmData &signature);

	// staged
	void init(); // Optional
	void sign(const CssmData &data) { sign(&data, 1); }
	void sign(const CssmData *data, uint32 count);
	void operator () (CssmData &signature);
	CssmData operator () () { CssmData signature; (*this)(signature); return signature; }
};

class Verify : public SigningContext
{
public:
	Verify(const CSP &csp, CSSM_ALGORITHMS alg, CSSM_ALGORITHMS verifyOnly = CSSM_ALGID_NONE)
        : SigningContext(csp, alg, verifyOnly) { }
	
	// integrated
	void verify(const CssmData &data, const CssmData &signature) { verify(&data, 1, signature); }
	void verify(const CssmData *data, uint32 count, const CssmData &signature);

	// staged
	void init(); // Optional
	void verify(const CssmData &data) { verify(&data, 1); }
	void verify(const CssmData *data, uint32 count);
	void operator () (const CssmData &signature);
};

} // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_SIGNCLIENT
