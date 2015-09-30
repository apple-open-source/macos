/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
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
#include <security_cdsa_client/genkey.h>

using namespace CssmClient;


GenerateKey::GenerateKey(const CSP &csp, CSSM_ALGORITHMS alg, uint32 size)
: Context(csp, alg), mKeySize(size), mSeed(NULL), mSalt(NULL), mParams(NULL)
{
}

void
GenerateKey::database(const Db &inDb)
{
	mDb = inDb;
	if (mDb && isActive())
		set(CSSM_ATTRIBUTE_DL_DB_HANDLE, mDb->handle());
}

void GenerateKey::activate()
{
    StLock<Mutex> _(mActivateMutex);
	if (!mActive)
	{
		check(CSSM_CSP_CreateKeyGenContext(attachment()->handle(), mAlgorithm,
			mKeySize, mSeed, mSalt, NULL, NULL, mParams, &mHandle));
		// Must be done before calling set() since is does nothing unless we are active.
		// Also we are technically active even if set() throws since we already created a context.
		mActive = true;
		if (mDb)
			set(CSSM_ATTRIBUTE_DL_DB_HANDLE, mDb->handle());
	}
}

Key GenerateKey::operator () (const KeySpec &spec)
{
	Key key;
	
	check(CSSM_GenerateKey(handle(), spec.usage, spec.attributes, spec.label,
		   &compositeRcc(), key.makeNewKey(attachment())));
		   
	key->activate();
	
	return key;
}

void GenerateKey::operator () (CssmKey &key, const KeySpec &spec)
{
	check(CSSM_GenerateKey(handle(), spec.usage, spec.attributes, spec.label, &compositeRcc(), &key));

}

void GenerateKey::operator () (Key &publicKey, const KeySpec &pubSpec,
		Key &privateKey, const KeySpec &privSpec)
{
	check(CSSM_GenerateKeyPair(handle(),
		pubSpec.usage, pubSpec.attributes,
		pubSpec.label, publicKey.makeNewKey(attachment()),
		privSpec.usage, privSpec.attributes,
		privSpec.label, &compositeRcc(), privateKey.makeNewKey(attachment())));

	publicKey->activate();
	privateKey->activate();

}

void GenerateKey::operator () (CssmKey &publicKey, const KeySpec &pubSpec,
		CssmKey &privateKey, const KeySpec &privSpec)
{
	check(CSSM_GenerateKeyPair(handle(),
		pubSpec.usage, pubSpec.attributes, pubSpec.label, &publicKey,
		privSpec.usage, privSpec.attributes, privSpec.label, &compositeRcc(), &privateKey));
}

