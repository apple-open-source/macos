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
// SSContext - cryptographic contexts for the security server
//
#include "SSContext.h"

#include "SSCSPSession.h"
#include "SSKey.h"

using namespace SecurityServer;

//
// SSContext
//
SSContext::SSContext(SSCSPSession &session)
: mSession(session), mContext(NULL)
{
}

void
SSContext::init(const Context &context,
				bool /* encoding */) // @@@ should be removed from API since it's already in mDirection
{
	mContext = &context;
}

SecurityServer::ClientSession &
SSContext::clientSession()
{
	return mSession.clientSession();
}


//
// SSRandomContext -- Context for GenerateRandom operations
//
SSRandomContext::SSRandomContext(SSCSPSession &session) : SSContext(session) {}

void
SSRandomContext::init(const Context &context, bool encoding)
{
	SSContext::init(context, encoding);

	// set/freeze output size
	mOutSize = context.getInt(CSSM_ATTRIBUTE_OUTPUT_SIZE, CSSMERR_CSP_MISSING_ATTR_OUTPUT_SIZE);

#if 0
	// seed the PRNG (if specified)
	if (const CssmCryptoData *seed = context.get<CssmCryptoData>(CSSM_ATTRIBUTE_SEED)) {
		const CssmData &seedValue = (*seed)();
		clientSession().seedRandom(seedValue);
	}
#endif
}

size_t 
SSRandomContext::outputSize(bool final, size_t inSize)
{
	return mOutSize;
}

void
SSRandomContext::final(CssmData &out)
{
	clientSession().generateRandom(out);
}


//
// SSSignContext -- Context for signing and GenerateMac operations
//
SSSignContext::SSSignContext(SSCSPSession &session) : SSContext(session) {}

void
SSSignContext::update(const CssmData &data)
{
}

size_t
SSSignContext::outputSize(bool final, size_t inSize)
{
	return 0;
}

void
SSSignContext::final(CssmData &out)
{
}


//
// SSVerifyContext -- Context for Verify and VerifyMac operations
//
SSVerifyContext::SSVerifyContext(SSCSPSession &session) : SSContext(session) {}

void
SSVerifyContext::update(const CssmData &data)
{
}

void
SSVerifyContext::final(const CssmData &in)
{
}


//
// SSCryptContext -- Context for Encrypt and Decrypt operations
//
SSCryptContext::SSCryptContext(SSCSPSession &session)
: SSContext(session), mKeyHandle(noKey), mCurrent(0), mCapacity(0),
mBuffer(NULL)
{
}


SSCryptContext::~SSCryptContext()
{
	freeBuffer();
}

void
SSCryptContext::freeBuffer()
{
	// @@@ We should probably use CssmAllocator::standard(sensitive) instead of malloc/realloc/free here
	if (mBuffer)
	{
		// Zero out buffer (only on decrypt?)
		if (mCapacity /* && !encoding() */)
		{
			memset(mBuffer, 0, mCapacity);
		}

		free(mBuffer);
		mBuffer = NULL;
		mCapacity = 0;
	}
}

void
SSCryptContext::init(const Context &context, bool encoding)
{
	SSContext::init(context, encoding);
	freeBuffer();

	mCurrent = 0;
	mCapacity = 0;

 	const CssmKey &keyInContext =
		context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY,
								   CSSMERR_CSP_MISSING_ATTR_KEY);

	// @@@ Should return SSKey.
	mKeyHandle = mSession.lookupKey(keyInContext).keyHandle();
}

size_t
SSCryptContext::inputSize(size_t outSize)
{
	return UINT_MAX;
}

size_t
SSCryptContext::outputSize(bool final, size_t inSize)
{
	if (!final)
	{
		mCapacity = mCurrent + inSize;
		mBuffer = realloc(mBuffer, mCapacity);
		return 0;
	}

	// There should not be any remaining input data left when final is true;
	assert(!inSize);

	// Do the actual operation.
	const CssmData in(mBuffer, mCurrent);
	CssmData out;
	if (encoding())
		clientSession().encrypt(*mContext, mKeyHandle, in, out);
	else
		clientSession().decrypt(*mContext, mKeyHandle, in, out);

	freeBuffer();
	mBuffer = out.Data;
	mCapacity = out.Length;
	mCurrent = 0;
	return mCapacity;
}

void
SSCryptContext::minimumProgress(size_t &in, size_t &out)
{
	// This should never be called.
	assert(false);
}

void
SSCryptContext::update(void *inp, size_t &inSize, void *outp, size_t &outSize)
{
	outSize = 0;
	assert(inSize);
	assert(mCurrent + inSize <= mCapacity);
	memcpy(&reinterpret_cast<uint8 *>(mBuffer)[mCurrent], inp, inSize);
	mCurrent += inSize;
}

void
SSCryptContext::final(CssmData &out)
{
	if(!out.Length) return;
	assert(out.Data && out.Length);
	uint32 todo = min(out.Length, mCapacity - mCurrent);
	memcpy(out.Data, &reinterpret_cast<uint8 *>(mBuffer)[mCurrent], todo);
	mCurrent += todo;
	out.Length = todo;

	freeBuffer();
}


#if 0
//
// SSKeyPairGenContext -- Context for key pair generation
//
SSKeyPairGenContext::SSKeyPairGenContext(SSCSPSession &session)
: SSContext(session) {}

void
SSKeyPairGenContext::generate(const Context &context, 
							  CssmKey &pubKey,
							  SSKey *pubBinKey,
							  CssmKey &privKey,
							  SSKey *privBinKey)
{
}

void
SSKeyPairGenContext::generate(const Context &context,
							  SSKey &pubBinKey,
							  SSKey &privBinKey,
							  uint32 &keySize)
{
}


//
// SSSymmKeyGenContext -- Context for symmetric key generation
//
SSSymmKeyGenContext::SSSymmKeyGenContext(SSCSPSession &session,
										 uint32 minSize,
										 uint32 maxSize,
										 bool byteSized)
: SSContext(session),
  minSizeInBits(minSize),
  maxSizeInBits(maxSize),
  mustBeByteSized(byteSized)
{
}
			
void
SSSymmKeyGenContext::generateSymKey(const Context &context, CssmKey &cssmKey)
{
}
#endif
