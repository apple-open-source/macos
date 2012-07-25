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
// macclient - client interface to CSSM sign/verify mac contexts
//
#ifndef _H_CDSA_CLIENT_MACCLIENT
#define _H_CDSA_CLIENT_MACCLIENT  1

#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/keyclient.h>

namespace Security
{

namespace CssmClient
{

//
// A signing/verifying mac context
//
class MacContext : public Context
{
public:
	MacContext(const CSP &csp, CSSM_ALGORITHMS alg)
		: Context(csp, alg) { }

	// preliminary interface
	Key key() const { assert(mKey); return mKey; }
	void key(const Key &k) { mKey = k; set(CSSM_ATTRIBUTE_KEY, mKey); }

protected:
	void activate();
	Key mKey;
};


class GenerateMac : public MacContext
{
public:
	GenerateMac(const CSP &csp, CSSM_ALGORITHMS alg) : MacContext(csp, alg) { }
	
	// integrated
	void sign(const CssmData &data, CssmData &mac) { sign(&data, 1, mac); }
	void sign(const CssmData *data, uint32 count, CssmData &mac);
	
	// staged
	void init(); // Optional
	void sign(const CssmData &data) { sign(&data, 1); }
	void sign(const CssmData *data, uint32 count);
	void operator () (CssmData &mac);
	CssmData operator () () { CssmData mac; (*this)(mac); return mac; }
};

class VerifyMac : public MacContext
{
public:
	VerifyMac(const CSP &csp, CSSM_ALGORITHMS alg) : MacContext(csp, alg) { }
	
	// integrated
	void verify(const CssmData &data, const CssmData &mac) { verify(&data, 1, mac); }
	void verify(const CssmData *data, uint32 count, const CssmData &mac);
	
	// staged
	void init(); // Optional
	void verify(const CssmData &data) { verify(&data, 1); }
	void verify(const CssmData *data, uint32 count);
	void operator () (const CssmData &mac);
};

} // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_MACCLIENT
