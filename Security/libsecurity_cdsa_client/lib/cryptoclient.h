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
// cryptoclient - client interface to CSSM CSP encryption/decryption operations
//
#ifndef _H_CDSA_CLIENT_CRYPTOCLIENT
#define _H_CDSA_CLIENT_CRYPTOCLIENT  1

#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/keyclient.h>

namespace Security {
namespace CssmClient {


//
// Common features of various cryptographic operations contexts.
// These all use symmetric or asymmetric contexts.
//
class Crypt : public Context {
public:
	Crypt(const CSP &csp, CSSM_ALGORITHMS alg);
	
public:
    // Context attributes
	CSSM_ENCRYPT_MODE mode() const			{ return mMode; }
	void mode(CSSM_ENCRYPT_MODE m)			{ mMode = m; set(CSSM_ATTRIBUTE_MODE, m); }
	Key key() const							{ return mKey; }
	void key(const Key &k);
	const CssmData &initVector() const		{ return *mInitVector; }
	void initVector(const CssmData &v)		{ mInitVector = &v; set(CSSM_ATTRIBUTE_INIT_VECTOR, v); }
	CSSM_PADDING padding() const			{ return mPadding; }
	void padding(CSSM_PADDING p)			{ mPadding = p; set(CSSM_ATTRIBUTE_PADDING, p); }

protected:
	void activate();
	
protected:
	CSSM_ENCRYPT_MODE mMode;
	Key mKey;
	const CssmData *mInitVector;
	CSSM_PADDING mPadding;
};



//
// An encryption context
//
class Encrypt : public Crypt
{
public:
	Encrypt(const CSP &csp, CSSM_ALGORITHMS alg) : Crypt(csp, alg) {};
	
public:
	// integrated
	CSSM_SIZE encrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount,
		CssmData &remData);
	CSSM_SIZE encrypt(const CssmData &in, CssmData &out, CssmData &remData)
	{ return encrypt(&in, 1, &out, 1, remData); }
	
	// staged update
	void init(); // Optional
	CSSM_SIZE encrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount);
	CSSM_SIZE encrypt(const CssmData &in, CssmData &out)
	{ return encrypt(&in, 1, &out, 1); }
	// staged final
	void final(CssmData &remData);
};

//
// An Decryption context
//
class Decrypt : public Crypt
{
public:
	Decrypt(const CSP &csp, CSSM_ALGORITHMS alg) : Crypt(csp, alg) {};
	
public:
	// integrated
	CSSM_SIZE decrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount,
		CssmData &remData);
	CSSM_SIZE decrypt(const CssmData &in, CssmData &out, CssmData &remData)
	{ return decrypt(&in, 1, &out, 1, remData); }

	// staged update
	void init(); // Optional
	CSSM_SIZE decrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount);
	CSSM_SIZE decrypt(const CssmData &in, CssmData &out)
	{ return decrypt(&in, 1, &out, 1); }
	// staged final
	void final(CssmData &remData);
};


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_CRYPTOCLIENT
