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
// cspclient - client interface to CSSM CSPs and their operations
//
#include <Security/cspclient.h>

using namespace CssmClient;


//
// Manage CSP attachments
//
CSPImpl::CSPImpl(const Guid &guid) : AttachmentImpl(guid, CSSM_SERVICE_CSP)
{
}

CSPImpl::CSPImpl(const Module &module) : AttachmentImpl(module, CSSM_SERVICE_CSP)
{
}

CSPImpl::~CSPImpl()
{
}


//
// Delete a key explicitly
//
void CSPImpl::freeKey(CssmKey &key, const AccessCredentials *cred, bool permanent)
{
    check(CSSM_FreeKey(handle(), cred, &key, permanent));
}


//
// Manage generic context objects
//
CssmClient::Context::Context(const CSP &csp, CSSM_ALGORITHMS alg) 
: ObjectImpl(csp), mAlgorithm(alg), mStaged(false)
{
}

CssmClient::Context::~Context()
{
	try
	{
		deactivate();
	} catch(...) {}
}

void CssmClient::Context::init()
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CssmClient::Context::deactivate()
{
	if (mActive)
	{
		mActive = false;
		check(CSSM_DeleteContext(mHandle));
	}
}


void CssmClient::Context::algorithm(CSSM_ALGORITHMS alg)
{
	if (isActive())
		abort();	//@@@ can't (currently?) change algorithm with active context
	mAlgorithm = alg;
}


//
// The override() method of Context is an expert feature. It replaces the entire
// context with a context object provided. It is up to the caller to keep this context
// consistent with the purpose of the Context subclass he is (mis)using.
// This feature is currently used by the SecurityServer.
//
void CssmClient::Context::override(const Security::Context &ctx)
{
	if (!isActive()) {
		// make a valid context object (it doesn't matter what kind - keep it cheap)
		check(CSSM_CSP_CreateDigestContext(attachment()->handle(), CSSM_ALGID_NONE, &mHandle));
	}
	// now replace everything with the context data provided
	check(CSSM_SetContext(mHandle, &ctx));
	mActive = true;		// now active
}


//
// Manage Digest contexts
//
void Digest::activate()
{
	if (!mActive) {
		check(CSSM_CSP_CreateDigestContext(attachment()->handle(), mAlgorithm, &mHandle));
		mActive = true;
	}
}


void Digest::digest(const CssmData *data, uint32 count, CssmData &digest)
{
	activate();
	if (mStaged)
		Error::throwMe(CSSMERR_CSP_STAGED_OPERATION_IN_PROGRESS);
	check(CSSM_DigestData(handle(), data, count, &digest));
}

void Digest::digest(const CssmData *data, uint32 count)
{
	activate();
	if (!mStaged) {
		check(CSSM_DigestDataInit(handle()));
		mStaged = true;
	}
	check(CSSM_DigestDataUpdate(handle(), data, count));
}

void Digest::operator () (CssmData &digest)
{
	if (!mStaged)
		Error::throwMe(CSSMERR_CSP_STAGED_OPERATION_NOT_STARTED);
	check(CSSM_DigestDataFinal(handle(), &digest));
	mStaged = false;
}


//
// Random number generation
//
void Random::seed(const CssmCryptoData &seedData)
{
	mSeed = &seedData;
	set(CSSM_ATTRIBUTE_SEED, seedData);
}

void Random::size(uint32 sz)
{
	mSize = sz;
	set(CSSM_ATTRIBUTE_OUTPUT_SIZE, sz);
}


void Random::activate()
{
	if (!mActive) {
		check(CSSM_CSP_CreateRandomGenContext(attachment()->handle(), mAlgorithm,
			mSeed, mSize, &mHandle));
		mActive = true;
	}
}


void Random::generate(CssmData &data, uint32 newSize)
{
	if (newSize)
		size(newSize);
	activate();
	assert(!mStaged);	// not a stage-able operation
	check(CSSM_GenerateRandom(handle(), &data));
}
