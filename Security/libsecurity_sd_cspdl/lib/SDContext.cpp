/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// SDContext - cryptographic contexts for the security server
//
#include "SDContext.h"

#include "SDCSPSession.h"
#include "SDKey.h"
#include <security_utilities/debugging.h>

#define ssCryptDebug(args...)  secdebug("ssCrypt", ## args)

using namespace SecurityServer;

//
// SDContext
//
SDContext::SDContext(SDCSPSession &session)
: mSession(session), mContext(NULL)
{
}

void SDContext::clearOutBuf()
{
	if(mOutBuf.Data) {
		mSession.free(mOutBuf.Data);
		mOutBuf.clear();
	}
}

void SDContext::copyOutBuf(CssmData &out)
{
	if(out.length() < mOutBuf.length()) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	memmove(out.Data, mOutBuf.Data, mOutBuf.Length);
	out.Length = mOutBuf.Length;
	clearOutBuf();
}

void
SDContext::init(const Context &context,
				bool /* encoding */) // @@@ should be removed from API since it's already in mDirection
{
	mContext = &context;
	clearOutBuf();
}

SecurityServer::ClientSession &
SDContext::clientSession()
{
	return mSession.clientSession();
}


//
// SDRandomContext -- Context for GenerateRandom operations
//
SDRandomContext::SDRandomContext(SDCSPSession &session) : SDContext(session) {}

void
SDRandomContext::init(const Context &context, bool encoding)
{
	SDContext::init(context, encoding);

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
SDRandomContext::outputSize(bool final, size_t inSize)
{
	return mOutSize;
}

void
SDRandomContext::final(CssmData &out)
{
	clientSession().generateRandom(*mContext, out);
}


// signature contexts
SDSignatureContext::SDSignatureContext(SDCSPSession &session) 
	: SDContext(session),
		mKeyHandle(noKey),
		mNullDigest(NULL),
		mDigest(NULL)
{
	/* nothing else for now */
}

SDSignatureContext::~SDSignatureContext()
{
	delete mNullDigest;
	delete mDigest;
}

void SDSignatureContext::init(const Context &context, bool signing)
{
	SDContext::init(context, signing);

	/* reusable: skip everything except resetting digest state */
	if((mNullDigest != NULL) || (mDigest != NULL)) {
		if(mNullDigest != NULL) {
			mNullDigest->digestInit();
		}
		return;
	}
	
	/* snag key from context */
 	const CssmKey &keyInContext =
		context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY,
								   CSSMERR_CSP_MISSING_ATTR_KEY);
	mKeyHandle = mSession.lookupKey(keyInContext).keyHandle();
	
	/* get digest alg and sig alg from Context.algorithm */
	switch(context.algorithm()) {
		/*** DSA ***/
		case CSSM_ALGID_SHA1WithDSA:
			mDigestAlg = CSSM_ALGID_SHA1;
			mSigAlg = CSSM_ALGID_DSA;
			break;
		case CSSM_ALGID_DSA:				// Raw
			mDigestAlg = CSSM_ALGID_NONE;
			mSigAlg = CSSM_ALGID_DSA;
			break;
		/*** RSA ***/
		case CSSM_ALGID_SHA1WithRSA:
			mDigestAlg = CSSM_ALGID_SHA1;
			mSigAlg = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_MD5WithRSA:
			mDigestAlg = CSSM_ALGID_MD5;
			mSigAlg = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_MD2WithRSA:
			mDigestAlg = CSSM_ALGID_MD2;
			mSigAlg = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_RSA:				// Raw
			mDigestAlg = CSSM_ALGID_NONE;
			mSigAlg = CSSM_ALGID_RSA;
			break;
		/*** FEE ***/
		case CSSM_ALGID_FEE_SHA1:
			mDigestAlg = CSSM_ALGID_SHA1;
			mSigAlg = CSSM_ALGID_FEE;
			break;
		case CSSM_ALGID_FEE_MD5:
			mDigestAlg = CSSM_ALGID_MD5;
			mSigAlg = CSSM_ALGID_FEE;
			break;
		case CSSM_ALGID_FEE:				// Raw
			mDigestAlg = CSSM_ALGID_NONE;
			mSigAlg = CSSM_ALGID_FEE;
			break;
		/*** ECDSA ***/
		case CSSM_ALGID_SHA1WithECDSA:
			mDigestAlg = CSSM_ALGID_SHA1;
			mSigAlg = CSSM_ALGID_ECDSA;
			break;
		case CSSM_ALGID_SHA224WithECDSA:
			mDigestAlg = CSSM_ALGID_SHA224;
			mSigAlg = CSSM_ALGID_ECDSA;
			break;
		case CSSM_ALGID_SHA256WithECDSA:
			mDigestAlg = CSSM_ALGID_SHA256;
			mSigAlg = CSSM_ALGID_ECDSA;
			break;
		case CSSM_ALGID_SHA384WithECDSA:
			mDigestAlg = CSSM_ALGID_SHA384;
			mSigAlg = CSSM_ALGID_ECDSA;
			break;
		case CSSM_ALGID_SHA512WithECDSA:
			mDigestAlg = CSSM_ALGID_SHA512;
			mSigAlg = CSSM_ALGID_ECDSA;
			break;
		case CSSM_ALGID_ECDSA:				// Raw
			mDigestAlg = CSSM_ALGID_NONE;
			mSigAlg = CSSM_ALGID_ECDSA;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
		
	/* set up mNullDigest or mDigest */
	if(mDigestAlg == CSSM_ALGID_NONE) {
		mNullDigest = new NullDigest();
	}
	else {
		mDigest = new CssmClient::Digest(mSession.mRawCsp, mDigestAlg);
	}
}

/* 
 * for raw sign/verify - optionally called after init.
 * Note that in init (in this case), we set mDigestAlg to ALGID_NONE and set up
 * a NullDigest. We now overwrite mDigestAlg, and we'll useÊthis
 * new value when we do the actual sign/vfy.
 */
void SDSignatureContext::setDigestAlgorithm(CSSM_ALGORITHMS digestAlg)
{
	mDigestAlg = digestAlg;
}

void SDSignatureContext::update(const CssmData &data)
{
	/* Note that for this context, we really can not deal with an out-of-sequence
	 * update --> final(true, 0) --> update since we lose the pending digest state
	 * when we perform the implied final() during outputSize(true, 0). */
	assert(mOutBuf.Data == NULL);
	
	/* add incoming data to digest or accumulator */
	if(mNullDigest) {
		mNullDigest->digestUpdate(data.data(), data.length());
	}
	else {
		mDigest->digest(data);
	}
}

size_t SDSignatureContext::outputSize(bool final, size_t inSize)
{
	if(!final) {
		ssCryptDebug("===sig outputSize !final\n");
		return 0;
	}
	if(!encoding()) {
		ssCryptDebug("===sig outputSize final, !encoding\n");
		/* don't see why this is even called... */
		return 0;
	}
	if(inSize == 0) {
		/* 
		 * This is the implied signal to go for it. Note that in this case,
		 * we can not go back and re-do the op in case of an unexpected
		 * sequence of update/outputSize(final, 0)/final - we lose the digest 
		 * state. Perhaps we should save the digest...? But still it would
		 * be impossible to do another update. 
		 */
		clearOutBuf();
		sign(mOutBuf);
		ssCryptDebug("===sig outputSize(pre-op) %u", (unsigned)mOutBuf.Length);
		return (size_t)mOutBuf.Length;
	}
	else {
		/* out-of-band case, ask CSP via SS */
		uint32 outSize = clientSession().getOutputSize(*mContext, 
			mKeyHandle, 
			/* FIXME - what to use for inSize here - we don't want to 
			 * interrogate mDigest, as that would result in another RPC...
			 * and signature size is not related to input size...right? */
			inSize,
			true);
		ssCryptDebug("===sig outputSize(RPC) %u", (unsigned)outSize);
		return (size_t)outSize;
	}
}

/* sign */

/* first the common routine shared by final and outputSize */
void SDSignatureContext::sign(CssmData &sig)
{
	/* we have to pass down a modified Context, thus.... */
	Context tempContext = *mContext;
	tempContext.AlgorithmType = mSigAlg;
	
	if(mNullDigest) {
		CssmData dData(const_cast<void *>(mNullDigest->digestPtr()), 
			mNullDigest->digestSizeInBytes());	
		clientSession().generateSignature(tempContext,
			mKeyHandle,
			dData, 
			sig,
			mDigestAlg);
	}
	else {
		CssmAutoData d (mDigest->allocator ());
			d.set((*mDigest) ());
			
			clientSession().generateSignature(tempContext,
				mKeyHandle,
				d, 
				sig,
				mDigestAlg);
	}
}

/* this is the one called by CSPFullPluginSession */
void SDSignatureContext::final(CssmData &sig)
{
	if(mOutBuf.Data) {
		/* normal final case in which the actual RPC via SS was done in the
		 * previous outputSize() call. */
		ssCryptDebug("===final via pre-op and copy");
		copyOutBuf(sig);
		return;
	}
	
	ssCryptDebug("===final via RPC");
	sign(sig);
}

/* verify */
void
SDSignatureContext::final(const CssmData &sig)
{
	/* we have to pass down a modified Context, thus.... */
	Context tempContext = *mContext;
	tempContext.AlgorithmType = mSigAlg;

	if(mNullDigest) {
		CssmData dData(const_cast<void *>(mNullDigest->digestPtr()), 
			mNullDigest->digestSizeInBytes());
		clientSession().verifySignature(tempContext,
			mKeyHandle,
			dData, 
			sig,
			mDigestAlg);
	}
	else {
		clientSession().verifySignature(tempContext,
			mKeyHandle,
			(*mDigest)(), 
			sig,
			mDigestAlg);
	}
}


//
// SDCryptContext -- Context for Encrypt and Decrypt operations
//
SDCryptContext::SDCryptContext(SDCSPSession &session)
	: SDContext(session), mKeyHandle(noKey)
{
	/* nothing for now */
}


SDCryptContext::~SDCryptContext()
{
	/* nothing for now */
}

void
SDCryptContext::init(const Context &context, bool encoding)
{
	ssCryptDebug("===init");
	SDContext::init(context, encoding);

	/* reusable; reset accumulator */
	mNullDigest.digestInit();

 	const CssmKey &keyInContext =
		context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY,
								   CSSMERR_CSP_MISSING_ATTR_KEY);
	mKeyHandle = mSession.lookupKey(keyInContext).keyHandle();
}

size_t
SDCryptContext::inputSize(size_t outSize)
{
	ssCryptDebug("===inputSize  outSize=%u", (unsigned)outSize);
	return UINT_MAX;
}

size_t
SDCryptContext::outputSize(bool final, size_t inSize)
{
	ssCryptDebug("===outputSize final %d inSize=%u", final, (unsigned)inSize);
	if(!final) {
		/* we buffer until final; no intermediate output */
		return 0;
	}
	size_t inBufSize = mNullDigest.digestSizeInBytes();
	if(inSize == 0) {
		/* This is the implied signal to go for it */
		clearOutBuf();
		if(inBufSize == 0) {
			return 0;
		}
		const CssmData in(const_cast<void *>(mNullDigest.digestPtr()), inBufSize);
		if (encoding()) {
			clientSession().encrypt(*mContext, mKeyHandle, in, mOutBuf);
		}
		else {
			clientSession().decrypt(*mContext, mKeyHandle, in, mOutBuf);
		}
		/* leave the accumulator as is in case of unexpected sequence */
		ssCryptDebug("   ===outSize(pre-op) %u", (unsigned)mOutBuf.Length);
		return mOutBuf.Length;
	}
	else {
		/* out-of-band case, ask CSP via SS */
		uint32 outSize = clientSession().getOutputSize(*mContext, 
			mKeyHandle, 
			inBufSize + inSize,
			encoding());
		ssCryptDebug("   ===outSize(RPC) %u", (unsigned)outSize);
		return (size_t)outSize;
	}
}

void
SDCryptContext::minimumProgress(size_t &in, size_t &out)
{
	in = 1;
	out = 0;
}

void
SDCryptContext::update(void *inp, size_t &inSize, void *outp, size_t &outSize)
{
	ssCryptDebug("===update inSize=%u", (unsigned)inSize);
	/* add incoming data to accumulator */
	mNullDigest.digestUpdate(inp, inSize);
	outSize = 0;
	clearOutBuf();
}

void
SDCryptContext::final(CssmData &out)
{
	if(mOutBuf.Data != NULL) {
		/* normal final case in which the actual RPC via SS was done in the
		 * previous outputSize() call. A memcpy is needed here because 
		 * CSPFullPluginSession has just allocated the buf size we need. */
		ssCryptDebug("===final via pre-op and copy");
		copyOutBuf(out);
		return;
	}
	
	/* when is this path taken...? */
	ssCryptDebug("===final via RPC");
	size_t inSize = mNullDigest.digestSizeInBytes();
	if(!inSize) return;

	const CssmData in(const_cast<void *>(mNullDigest.digestPtr()), inSize);
	IFDEBUG(unsigned origOutSize = out.length());
	if (encoding()) {
		clientSession().encrypt(*mContext, mKeyHandle, in, out);
	}
	else {
		clientSession().decrypt(*mContext, mKeyHandle, in, out);
	}
	assert(out.length() <= origOutSize);
	mNullDigest.digestInit();
}

// Digest, using raw CSP
SDDigestContext::SDDigestContext(SDCSPSession &session)
	: SDContext(session), mDigest(NULL)
{
	
}

SDDigestContext::~SDDigestContext()
{
	delete mDigest;
}

void SDDigestContext::init(const Context &context, bool encoding)
{
	CSSM_ALGORITHMS alg;
	
	SDContext::init(context, encoding);
	alg = context.algorithm();
	mDigest = new CssmClient::Digest(mSession.mRawCsp, alg);
}

void SDDigestContext::update(const CssmData &data)
{
	mDigest->digest(data);
}

void SDDigestContext::final(CssmData &out)
{
	(*mDigest)(out);
}

size_t SDDigestContext::outputSize(bool final, size_t inSize)
{
	if(!final) {
		return 0;
	}
	else {
		return (size_t)mDigest->getOutputSize(inSize);
	}
}

// MACContext - common class for MAC generate, verify
SDMACContext::SDMACContext(SDCSPSession &session)
	: SDContext(session), mKeyHandle(noKey)
{

}

void SDMACContext::init(const Context &context, bool encoding)
{
	SDContext::init(context, encoding);
	
	/* reusable; reset accumulator */
	mNullDigest.digestInit();
	
	/* snag key from context */
 	const CssmKey &keyInContext =
		context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY,
								   CSSMERR_CSP_MISSING_ATTR_KEY);
	mKeyHandle = mSession.lookupKey(keyInContext).keyHandle();
}

void SDMACContext::update(const CssmData &data)
{
	/* add incoming data to accumulator */
	mNullDigest.digestUpdate(data.data(), data.length());
}

size_t SDMACContext::outputSize(bool final, size_t inSize)
{
	if(!final) {
		ssCryptDebug("===mac outputSize !final\n");
		return 0;
	}
	if(!encoding()) {
		ssCryptDebug("===mac outputSize final, !encoding\n");
		/* don't see why this is even called... */
		return 0;
	}
	if(inSize == 0) {
		/* 
		 * This is the implied signal to go for it.  
		 */
		clearOutBuf();
		genMac(mOutBuf);
		ssCryptDebug("===mac outputSize(pre-op) %u", (unsigned)mOutBuf.Length);
		return (size_t)mOutBuf.Length;
	}
	else {
		/* out-of-band case, ask CSP via SS */
		uint32 outSize = clientSession().getOutputSize(*mContext, 
			mKeyHandle, 
			inSize + mNullDigest.digestSizeInBytes(),
			true);
		ssCryptDebug("===mac outputSize(RPC) %u", (unsigned)outSize);
		return (size_t)outSize;
	}
}

/* generate */

/* first the common routine used by final() and outputSize() */
void SDMACContext::genMac(CssmData &mac)
{
	CssmData allData(const_cast<void *>(mNullDigest.digestPtr()), 
		mNullDigest.digestSizeInBytes());
	clientSession().generateMac(*mContext, mKeyHandle, allData, mac);
}

void SDMACContext::final(CssmData &mac)
{
	genMac(mac);
}

/* verify */
void SDMACContext::final(const CssmData &mac)
{
	CssmData allData(const_cast<void *>(mNullDigest.digestPtr()), 
		mNullDigest.digestSizeInBytes());
	clientSession().verifyMac(*mContext, mKeyHandle, allData, mac);
}
