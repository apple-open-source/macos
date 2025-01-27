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
// wrapkey - client interface for wrapping and unwrapping keys
//
#ifndef _H_CDSA_CLIENT_WRAPKEY
#define _H_CDSA_CLIENT_WRAPKEY  1

#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/cryptoclient.h>
#include <security_cdsa_client/keyclient.h>


namespace Security {
namespace CssmClient {


//
// Wrap a key
//
class WrapKey : public Crypt {
public:
	WrapKey(const CSP &csp, CSSM_ALGORITHMS alg) :
		Crypt(csp, alg), mWrappedKeyFormat(CSSM_KEYBLOB_WRAPPED_FORMAT_NONE) {}

public:
	CSSM_KEYBLOB_FORMAT wrappedKeyFormat() const { return mWrappedKeyFormat; }
	void wrappedKeyFormat(CSSM_KEYBLOB_FORMAT wrappedKeyFormat)
	{ mWrappedKeyFormat = wrappedKeyFormat; set(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT, wrappedKeyFormat); }

	// wrap the key
	Key operator () (Key &keyToBeWrapped, const CssmData *descriptiveData = NULL);
	void operator () (const CssmKey &keyToBeWrapped, CssmKey &wrappedKey,
					  const CssmData *descriptiveData = NULL);

protected:
	void activate();

private:
	CSSM_KEYBLOB_FORMAT mWrappedKeyFormat;
};


//
// Unwrap a key. This creates a new key object
//
class UnwrapKey : public Crypt, public RccBearer {
public:
	UnwrapKey(const CSP &csp, CSSM_ALGORITHMS alg) : Crypt(csp, alg) {}

public:
	// wrap the key
	Key operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec);
	void operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
					  CssmKey &unwrappedKey);

	Key operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
					 Key &optionalPublicKey);
	void operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
					  CssmKey &unwrappedKey, const CssmKey *optionalPublicKey);

	Key operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
					 CssmData *descriptiveData);
	void operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
					  CssmKey &unwrappedKey, CssmData *descriptiveData);

	Key operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
					 const Key &optionalPublicKey, CssmData *descriptiveData);
	void operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
					  CssmKey &unwrappedKey, CssmData *descriptiveData,
					  const CssmKey *optionalPublicKey);
};


//
// Derive a key in various and wonderous ways. Creates a new key object.
//
class DeriveKey : public Crypt, public RccBearer {
public:
	DeriveKey(const CSP &csp, CSSM_ALGORITHMS alg, CSSM_ALGORITHMS target, uint32 size = 0)
    : Crypt(csp, alg), mKeySize(size), mTargetType(target), mIterationCount(0),
      mSeed(NULL), mSalt(NULL) { }

public:
    CSSM_ALGORITHMS targetType() const { return mTargetType; }
    void targetType(CSSM_ALGORITHMS alg) { mTargetType = alg; }
    uint32 iterationCount() const		{ return mIterationCount; }
    void iterationCount(uint32 c)		{ mIterationCount = c; }
    const CssmCryptoData seed() const	{ return *mSeed; }
    void seed(const CssmCryptoData &data) { mSeed = &data; }
    const CssmData salt() const			{ return *mSalt; }
    void salt(const CssmData &data)		{ mSalt = &data; }

	Key operator () (CssmData *param, const KeySpec &spec);
	void operator () (CssmData *param, const KeySpec &spec,
					  CssmKey &derivedKey);
                      
    void activate();
    
private:
	uint32 mKeySize;
    CSSM_ALGORITHMS mTargetType;
    uint32 mIterationCount;
    const CssmCryptoData *mSeed;
    const CssmData *mSalt;
};

} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_WRAPKEY
