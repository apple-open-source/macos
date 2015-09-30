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
// cryptoclient - client interface to CSSM CSP encryption/decryption operations
//
#include <security_cdsa_client/cryptoclient.h>

using namespace CssmClient;


Crypt::Crypt(const CSP &csp, CSSM_ALGORITHMS alg)
	: Context(csp, alg), mMode(CSSM_ALGMODE_NONE), mInitVector(NULL),
	  mPadding(CSSM_PADDING_NONE)
{
}

void Crypt::key(const Key &key)
{
	mKey = key;
	set(CSSM_ATTRIBUTE_KEY, static_cast<const CssmKey &>(key));
}


void
Crypt::activate()
{
    StLock<Mutex> _(mActivateMutex);
	if (!mActive)
	{
        // Key is required unless we have a NULL algorithm (cleartext wrap/unwrap),
        // in which case we'll make a symmetric context (it shouldn't matter then).
		if (!mKey && mAlgorithm != CSSM_ALGID_NONE)
			CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_KEY);
		if (!mKey || mKey->keyClass() == CSSM_KEYCLASS_SESSION_KEY)
		{	// symmetric key
			check(CSSM_CSP_CreateSymmetricContext(attachment()->handle(), mAlgorithm,
				mMode, neededCred(), mKey, mInitVector, mPadding, NULL,
				&mHandle));
		}
		else
		{
			check(CSSM_CSP_CreateAsymmetricContext(attachment()->handle(), mAlgorithm,
				neededCred(), mKey, mPadding, &mHandle));
			//@@@ stick mode and initVector explicitly into the context?
		}		
		mActive = true;
	}
}


//
// Manage encryption contexts
//
CSSM_SIZE
Encrypt::encrypt(const CssmData *in, uint32 inCount,
						CssmData *out, uint32 outCount, CssmData &remData)
{
	unstaged();
	CSSM_SIZE total;
	check(CSSM_EncryptData(handle(), in, inCount, out, outCount, &total, &remData));
	return total;
}

void
Encrypt::init()
{
	check(CSSM_EncryptDataInit(handle()));
	mStaged = true;
}

CSSM_SIZE
Encrypt::encrypt(const CssmData *in, uint32 inCount,
	CssmData *out, uint32 outCount)
{
	staged();
	CSSM_SIZE total;
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

CSSM_SIZE
Decrypt::decrypt(const CssmData *in, uint32 inCount,
	CssmData *out, uint32 outCount, CssmData &remData)
{
	unstaged();
	CSSM_SIZE total;
	check(CSSM_DecryptData(handle(), in, inCount, out, outCount, &total, &remData));
	return total;
}

void
Decrypt::init()
{
	check(CSSM_DecryptDataInit(handle()));
	mStaged = true;
}

CSSM_SIZE
Decrypt::decrypt(const CssmData *in, uint32 inCount,
	CssmData *out, uint32 outCount)
{
	staged();
	CSSM_SIZE total;
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
