/*
 * Copyright (c) 2000-2002,2011-2012,2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_CDSA_CLIENT_CSPCLIENT
#define _H_CDSA_CLIENT_CSPCLIENT  1

#include <security_cdsa_client/cssmclient.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmacl.h>

namespace Security {
namespace CssmClient {


//
// A CSP attachment
//
class CSPImpl : public AttachmentImpl
{
public:
	CSPImpl(const Guid &guid);
	CSPImpl(const Module &module);
	virtual ~CSPImpl();

    // the least inappropriate place for this one
    void freeKey(CssmKey &key, const AccessCredentials *cred = NULL, bool permanent = false);
};

class CSP : public Attachment
{
public:
	typedef CSPImpl Impl;

	explicit CSP(Impl *impl) : Attachment(impl) {}
	CSP(const Guid &guid) : Attachment(new Impl(guid)) {}
	CSP(const Module &module) : Attachment(new Impl(module)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};

//
// A cryptographic context.
// Contexts always belong to CSPs (CSP attachments).
//
class Context : public ObjectImpl
{
public:
	Context(const CSP &csp, CSSM_ALGORITHMS alg = CSSM_ALGID_NONE);
	~Context();

	CSP attachment() const { return parent<CSP>(); }
	Module module() const { return attachment()->module(); }
	
	CSSM_ALGORITHMS algorithm() const { return mAlgorithm; }
	void algorithm(CSSM_ALGORITHMS alg);

	const AccessCredentials *cred() const	{ return mCred; }
	void cred(const CSSM_ACCESS_CREDENTIALS *cred);
	void cred(const CSSM_ACCESS_CREDENTIALS &cred) { this->cred(&cred); }

public:
	CSSM_CC_HANDLE handle() { activate(); return mHandle; }
    
    uint32 getOutputSize(uint32 inputSize, bool encrypt = true);
    void getOutputSize(CSSM_QUERY_SIZE_DATA &sizes, uint32 count, bool encrypt = true);
	
public:
	// don't use this section unless you know what you're doing!
	void override(const ::Context &ctx);

	template <class T>
	void set(CSSM_ATTRIBUTE_TYPE type, const T &value)
	{
		if (isActive()) {
			::Context::Attr attr(type, value);
			check(CSSM_UpdateContextAttributes(mHandle, 1, &attr));
		}
	}

	void set(CSSM_ATTRIBUTE_TYPE type, uint32 value)
	{
		if (isActive()) {
			::Context::Attr attr(type, value);
			check(CSSM_UpdateContextAttributes(mHandle, 1, &attr));
		}
	}
    
    template <class T>
    void add(CSSM_ATTRIBUTE_TYPE type, const T &value)
    { activate(); set(type, value); }
    
    void add(CSSM_ATTRIBUTE_TYPE type, uint32 value)
    { activate(); set(type, value); }

protected:
	void deactivate();

	virtual void init(); // Subclasses must implement if they support staged operations.

	void unstaged()
	{ activate(); if (mStaged) CssmError::throwMe(CSSMERR_CSP_STAGED_OPERATION_IN_PROGRESS); }
	
	void staged()
	{ if (!mStaged) init(); }

	const AccessCredentials *neededCred()
	{ return AccessCredentials::needed(mCred); }

protected:
	CSSM_ALGORITHMS mAlgorithm;		// intended algorithm
	CSSM_CC_HANDLE mHandle;			// CSSM CC handle
	bool mStaged;					// staged in progress
	const AccessCredentials *mCred;	// if explicitly set
    RecursiveMutex mActivateMutex;
};


//
// An RccBearer holds a ResourceControlContext. Note that this is a composite
// of an AccessCredentials and an AclEntryInput. We allow setting the whole
// thing, or its two components separately. A complete rcc set (via ::rcc)
// overrides any components.
// @@@ Perhaps we should merge components into a specified rcc? Iffy, though...
// Note: We call the credential components "opCred" to distinguish it from
// the "cred" of a CredBearer; some classes are both. As a rule, the "cred" goes
// into the context, while the "opCred" gets passed as an argument.
//
class RccBearer {
public:
	RccBearer() : mOpCred(NULL), mOwner(NULL), mRcc(NULL) { }
	
	const AccessCredentials *opCred() const		{ return mOpCred; }
	void opCred(const CSSM_ACCESS_CREDENTIALS *cred) { mOpCred = AccessCredentials::overlay(cred); }
	void opCred(const CSSM_ACCESS_CREDENTIALS &cred) { this->opCred(&cred); }
	const AclEntryInput *owner() const			{ return mOwner; }
	void owner(const CSSM_ACL_ENTRY_INPUT *owner) { mOwner = AclEntryInput::overlay(owner); }
	void owner(const CSSM_ACL_ENTRY_INPUT &owner) { this->owner(&owner); }
	void owner(const CSSM_ACL_ENTRY_PROTOTYPE *owner);
	void owner(const CSSM_ACL_ENTRY_PROTOTYPE &owner) { this->owner(&owner); }
	const ResourceControlContext *rcc() const	{ return mRcc; }
	void rcc(const CSSM_RESOURCE_CONTROL_CONTEXT *rcc)
			{ mRcc = ResourceControlContext::overlay(rcc); }
	void rcc(const CSSM_RESOURCE_CONTROL_CONTEXT &rcc) { this->rcc(&rcc); }
	
protected:
	const ResourceControlContext &compositeRcc() const;

private:
	// an RCC contains both a cred and entryInput
	// mCred/mAcl are only considered if mRcc is not set (NULL)
	const AccessCredentials *mOpCred;
	const AclEntryInput *mOwner;
	const ResourceControlContext *mRcc;
	
	mutable ResourceControlContext mWorkRcc; // work area
	mutable AclEntryInput mWorkInput;		// work area
};


//
// A PassThough context
//
class PassThrough : public Context
{
public:
	PassThrough(const CSP &csp) : Context(csp) { }

public:
	void operator () (uint32 passThroughId, const void *inData, void **outData);
	
	template <class TIn, class TOut>
	void operator () (uint32 passThroughId, const TIn *inData, TOut **outData)
	{ operator () (passThroughId, (const void *)inData, (void **)outData); }
	
	template <class TIn>
	void operator () (uint32 passThroughId, const TIn *inData)
	{ operator () (passThroughId, (const void *)inData, NULL); }

	const CSSM_KEY *key() const { return mKey; }
	void key(const CSSM_KEY *k) { mKey = k; set(CSSM_ATTRIBUTE_KEY, k); }

protected:
	void activate();

protected:
	const CSSM_KEY *mKey;
};


//
// A Digest context
//
class Digest : public Context
{
public:
	Digest(const CSP &csp, CSSM_ALGORITHMS alg) : Context(csp, alg) { }
	
public:
	// integrated
	void digest(const CssmData &data, CssmData &digest) { this->digest(&data, 1, digest); }
	void digest(const CssmData *data, uint32 count, CssmData &digest);
	
	// staged
	void digest(const CssmData &data) { digest(&data, 1); }
	void digest(const CssmData *data, uint32 count);
	void operator () (CssmData &digest);
	CssmData operator () () { CssmData digest; (*this)(digest); return digest; }
	
protected:
	void activate();
};


//
// A [P]RNG context
//
class Random : public Context
{
public:
	Random(const CSP &csp, CSSM_ALGORITHMS alg) : Context(csp, alg), mSeed(NULL), mSize(1) { }
	Random(const CSP &csp, CSSM_ALGORITHMS alg, const CssmCryptoData &seed)
		: Context(csp, alg), mSeed(&seed), mSize(1) { }
	Random(const CSP &csp, CSSM_ALGORITHMS alg, uint32 size)
		: Context(csp, alg), mSeed(NULL), mSize(size) { }
	Random(const CSP &csp, CSSM_ALGORITHMS alg, const CssmCryptoData &seed, uint32 size)
		: Context(csp, alg), mSeed(&seed), mSize(size) { }
	
	void seed(const CssmCryptoData &data);
	void size(uint32 size);
	
public:
	void generate(CssmData &data, uint32 size = 0);

	// alternate function-call form
	CssmData operator () (uint32 size = 0)
	{ CssmData output; generate(output, size); return output; }
	
protected:
	void activate();
	
private:
	const CssmCryptoData *mSeed;
	uint32 mSize;
};


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_CSPCLIENT
