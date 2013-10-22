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
// genkey - client interface to CSSM sign/verify contexts
//
#ifndef _H_CDSA_CLIENT_GENKEY
#define _H_CDSA_CLIENT_GENKEY  1

#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/cryptoclient.h>
#include <security_cdsa_client/dlclient.h>
#include <security_cdsa_client/keyclient.h>


namespace Security
{

namespace CssmClient
{

class GenerateKey : public Context, public RccBearer {
public:
	GenerateKey(const CSP &csp, CSSM_ALGORITHMS alg, uint32 size = 0);

public:
	// context parameters
	void size(uint32 s) { mKeySize = s; set(CSSM_ATTRIBUTE_KEY_LENGTH, s); }
	void seed(const CssmCryptoData &s) { mSeed = &s; set(CSSM_ATTRIBUTE_SEED, s); }
	void salt(const CssmData &s) { mSalt = &s;set(CSSM_ATTRIBUTE_SALT, s);  }
	void params(const CssmData &p) { mParams = &p; set(CSSM_ATTRIBUTE_ALG_PARAMS, p); }
	void database(const Db &inDb);

	// symmetric key generation
	Key operator () (const KeySpec &spec);
	void operator () (CssmKey &key, const KeySpec &spec);
	
	// asymmetric key generation
	void operator () (Key &publicKey, const KeySpec &publicSpec,
		Key &privateKey, const KeySpec &privateSpec);
	void operator () (CssmKey &publicKey, const KeySpec &publicSpec,
		CssmKey &privateKey, const KeySpec &privateSpec);

	
protected:
	void activate();
	
private:
	// context parameters
	uint32 mKeySize;
	const CssmCryptoData *mSeed;
	const CssmData *mSalt;
	const CssmData *mParams;
	Db mDb;

	// generation parameters(?) -- Unused
	// const ResourceControlContext *mInitialAcl;
};

} // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_GENKEY
