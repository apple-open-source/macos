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
#include <Security/cryptoclient.h>

using namespace CssmClient;


Crypt::Crypt(const CSP &csp, CSSM_ALGORITHMS alg) : Context(csp, alg)
{
	// set defaults
	mMode = CSSM_ALGMODE_NONE;
	mCred = NULL;
	mInitVector = NULL;
	mPadding = CSSM_PADDING_NONE;
}

void Crypt::key(const Key &key)
{
	mKey = key;
	set(CSSM_ATTRIBUTE_KEY, static_cast<const CssmKey &>(key));
}

void
Crypt::activate()
{
	if (!mActive)
	{
        // Some crypto operations require a credential.
        // Use a null credential if none was specified.
        if (!mCred)
            mCred = &AccessCredentials::null;
    
        // Key is required unless we have a NULL algorithm (cleartext wrap/unwrap),
        // in which case we'll make a symmetric context (it shouldn't matter then).
		if (!mKey && mAlgorithm != CSSM_ALGID_NONE)
			CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_KEY);
		if (!mKey || mKey->keyClass() == CSSM_KEYCLASS_SESSION_KEY)
		{	// symmetric key
			check(CSSM_CSP_CreateSymmetricContext(attachment()->handle(), mAlgorithm,
				mMode, mCred, mKey, mInitVector, mPadding, NULL,
				&mHandle));
		}
		else
		{
			check(CSSM_CSP_CreateAsymmetricContext(attachment()->handle(), mAlgorithm,
				mCred, mKey, mPadding, &mHandle));
			//@@@ stick mode and initVector explicitly into the context?
		}		
		mActive = true;
	}
}
void Crypt::cred(const AccessCredentials *c)
{
    if (!(mCred = c))
        mCred = &AccessCredentials::null;
    set(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS, *mCred);
}


//
// Manage encryption contexts
//

uint32
Encrypt::encrypt(const CssmData *in, uint32 inCount,
						CssmData *out, uint32 outCount, CssmData &remData)
{
	unstaged();
	uint32 total;
	check(CSSM_EncryptData(handle(), in, inCount, out, outCount, &total, &remData));
	return total;
}

void
Encrypt::init()
{
	check(CSSM_EncryptDataInit(handle()));
	mStaged = true;
}

uint32
Encrypt::encrypt(const CssmData *in, uint32 inCount,
	CssmData *out, uint32 outCount)
{
	staged();
	uint32 total;
	check(CSSM_EncryptDataUpdate(handle(), in, inCount, out, outCount, &total));
	return total;
}

void
Encrypt::final(CssmData &remData)
{
	staged();
	check(CSSM_EncryptDataFinal(handle(), &remData));
	mStaged = false;
}


//
// Manage Decryption contexts
//

uint32
Decrypt::decrypt(const CssmData *in, uint32 inCount,
	CssmData *out, uint32 outCount, CssmData &remData)
{
	unstaged();
	uint32 total;
	check(CSSM_DecryptData(handle(), in, inCount, out, outCount, &total, &remData));
	return total;
}

void
Decrypt::init()
{
	check(CSSM_DecryptDataInit(handle()));
	mStaged = true;
}

uint32
Decrypt::decrypt(const CssmData *in, uint32 inCount,
	CssmData *out, uint32 outCount)
{
	staged();
	uint32 total;
	check(CSSM_DecryptDataUpdate(handle(), in, inCount, out, outCount, &total));
	return total;
}

void
Decrypt::final(CssmData &remData)
{
	staged();
	check(CSSM_DecryptDataFinal(handle(), &remData));
	mStaged = false;
}
