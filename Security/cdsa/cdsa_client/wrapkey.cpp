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
// wrapkey - client interface for wrapping and unwrapping keys
//
#include <Security/wrapkey.h>

using namespace CssmClient;


Key
WrapKey::operator () (Key &keyToBeWrapped, const CssmData *descriptiveData)
{
	Key wrappedKey;

	check(CSSM_WrapKey(handle(), mCred, keyToBeWrapped, descriptiveData,
					   wrappedKey.makeNewKey(attachment())));
	wrappedKey->activate();

	return wrappedKey;
}

void
WrapKey::operator () (const CssmKey &keyToBeWrapped, CssmKey &wrappedKey,
					  const CssmData *descriptiveData)
{
	check(CSSM_WrapKey(handle(), mCred, &keyToBeWrapped, descriptiveData, &wrappedKey));
}

void
WrapKey::activate()
{
	if (!mActive)
	{
		Crypt::activate();
		if (mWrappedKeyFormat != CSSM_KEYBLOB_WRAPPED_FORMAT_NONE);
			set(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT, mWrappedKeyFormat);
	}
}

Key
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec)
{
	Key unwrappedKey;

	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));
	CssmData data(reinterpret_cast<uint8 *>(1), 0);

	check(CSSM_UnwrapKey(handle(), NULL,
						 &keyToBeUnwrapped, spec.usage, spec.attributes,
						 spec.label, &resourceControlContext,
						 unwrappedKey.makeNewKey(attachment()), &data));
	unwrappedKey->activate();

	return unwrappedKey;
}

void
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
						CssmKey &unwrappedKey)
{
	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));
	CssmData data(reinterpret_cast<uint8 *>(1), 0);

	check(CSSM_UnwrapKey(handle(), NULL, &keyToBeUnwrapped, spec.usage,
						 spec.attributes, spec.label, &resourceControlContext,
						 &unwrappedKey, &data));
}

Key
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
						Key &optionalPublicKey)
{
	Key unwrappedKey;

	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));
	CssmData data(reinterpret_cast<uint8 *>(1), 0);

	check(CSSM_UnwrapKey(handle(), optionalPublicKey,
						 &keyToBeUnwrapped, spec.usage, spec.attributes,
						 spec.label, &resourceControlContext,
						 unwrappedKey.makeNewKey(attachment()), &data));

	unwrappedKey->activate();

	return unwrappedKey;
}

void
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
						CssmKey &unwrappedKey,
						const CssmKey *optionalPublicKey)
{
	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));
	CssmData data(reinterpret_cast<uint8 *>(1), 0);

	check(CSSM_UnwrapKey(handle(), optionalPublicKey, &keyToBeUnwrapped,
						 spec.usage, spec.attributes, spec.label,
						 &resourceControlContext, &unwrappedKey, &data));
}


Key
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
						CssmData *descriptiveData)
{
	Key unwrappedKey;

	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));

	check(CSSM_UnwrapKey(handle(), NULL, &keyToBeUnwrapped, spec.usage,
						 spec.attributes, spec.label, &resourceControlContext,
						 unwrappedKey.makeNewKey(attachment()),
						 descriptiveData));
	unwrappedKey->activate();

	return unwrappedKey;
}

void
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
						CssmKey &unwrappedKey, CssmData *descriptiveData)
{
	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));

	check(CSSM_UnwrapKey(handle(), NULL, &keyToBeUnwrapped, spec.usage,
						 spec.attributes, spec.label, &resourceControlContext,
						 &unwrappedKey, descriptiveData));
}

Key
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
						Key &optionalPublicKey, CssmData *descriptiveData)
{
	Key unwrappedKey;

	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));

	check(CSSM_UnwrapKey(handle(), optionalPublicKey, &keyToBeUnwrapped,
						 spec.usage, spec.attributes, spec.label,
						 &resourceControlContext,
						 unwrappedKey.makeNewKey(attachment()),
						 descriptiveData));
	unwrappedKey->activate();

	return unwrappedKey;
}

void
UnwrapKey::operator () (const CssmKey &keyToBeUnwrapped, const KeySpec &spec,
						CssmKey &unwrappedKey, CssmData *descriptiveData,
						const CssmKey *optionalPublicKey)
{
	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));

	check(CSSM_UnwrapKey(handle(), optionalPublicKey, &keyToBeUnwrapped,
						 spec.usage, spec.attributes, spec.label,
						 &resourceControlContext, &unwrappedKey,
						 descriptiveData));
}


void DeriveKey::activate()
{
	if (!mActive)
	{
        check(CSSM_CSP_CreateDeriveKeyContext(attachment()->handle(), mAlgorithm,
            mTargetType, mKeySize, mCred, mKey, mIterationCount, mSalt, mSeed, &mHandle));
		mActive = true;
    }
}


Key
DeriveKey::operator () (CssmData *param, const KeySpec &spec)
{
	Key derivedKey;

	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));

	check(CSSM_DeriveKey(handle(), param, spec.usage, spec.attributes,
						 spec.label, &resourceControlContext,
						 derivedKey.makeNewKey(attachment())));
	derivedKey->activate();

	return derivedKey;
}

void
DeriveKey::operator () (CssmData *param, const KeySpec &spec,
						CssmKey &derivedKey)
{
	const ResourceControlContext resourceControlContext
		(mAclEntry, const_cast<AccessCredentials *>(mCred));

	check(CSSM_DeriveKey(handle(), param, spec.usage, spec.attributes,
						 spec.label, &resourceControlContext, &derivedKey));
}
