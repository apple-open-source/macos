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
// AppleCSP.cpp - top-level plugin and session implementation 
//
#include "AppleCSP.h"
#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#include <stdio.h> 
#include "cspdebugging.h" 
#include <Security/CSPsession.h>
#include <Security/cssmalloc.h>
#ifdef	BSAFE_CSP_ENABLE
#include <BSafeCSP/bsafecsp.h>
#include <BSafeCSP/bsafecspi.h>
#endif
#ifdef	CRYPTKIT_CSP_ENABLE
#include <CryptKitCSP/cryptkitcsp.h>
#include <CryptKitCSP/FEEKeys.h>
#endif
#include <MiscCSPAlgs/miscAlgFactory.h> 
#ifdef	ASC_CSP_ENABLE
#include <ComCryption/ascFactory.h>
#endif
#include <RSA_DSA/RSA_DSA_csp.h>
#include <RSA_DSA/RSA_DSA_keys.h>
#include <DiffieHellman/DH_csp.h>
#include <DiffieHellman/DH_keys.h>

#include "YarrowConnection.h"

/* 
 * For debugging, we allow use of FEE PRNG when Yarrow is not available
 * (i.e., no SecurityServer is running).
 */
#ifdef	NDEBUG
#define CSP_ALLOW_FEE_RNG		0
#else
#ifdef	CRYPTKIT_CSP_ENABLE
#define CSP_ALLOW_FEE_RNG		1
#else
#define CSP_ALLOW_FEE_RNG		0
#endif
#endif


//
// Make and break the plugin object
//
AppleCSPPlugin::AppleCSPPlugin() :
	normAllocator(CssmAllocator::standard(CssmAllocator::normal)),
	privAllocator(CssmAllocator::standard(CssmAllocator::sensitive)),
	#ifdef	BSAFE_CSP_ENABLE
	bSafe4Factory(new BSafeFactory(&normAllocator, &privAllocator)),
	#endif
	#ifdef	CRYPTKIT_CSP_ENABLE
	cryptKitFactory(new CryptKitFactory(&normAllocator, &privAllocator)),
	#endif
	miscAlgFactory(new MiscAlgFactory(&normAllocator, &privAllocator)),
	#ifdef	ASC_CSP_ENABLE
	ascAlgFactory(new AscAlgFactory(&normAllocator, &privAllocator)),
	#endif
	rsaDsaAlgFactory(new RSA_DSA_Factory(&normAllocator, &privAllocator)),
	dhAlgFactory(new DH_Factory(&normAllocator, &privAllocator))
{
	// misc. once-per-address-space cruft...
}

AppleCSPPlugin::~AppleCSPPlugin()
{
	#ifdef	BSAFE_CSP_ENABLE
	delete bSafe4Factory;
	#endif
	#ifdef	CRYPTKIT_CSP_ENABLE
	delete cryptKitFactory;
	#endif
	delete miscAlgFactory;
	#ifdef	ASC_CSP_ENABLE
	delete ascAlgFactory;
	#endif
	delete rsaDsaAlgFactory;
	delete dhAlgFactory;
}


//
// Create a new plugin session, our way
//
PluginSession *AppleCSPPlugin::makeSession(
	CSSM_MODULE_HANDLE handle,
    const CSSM_VERSION &version,
    uint32 subserviceId,
    CSSM_SERVICE_TYPE subserviceType,
    CSSM_ATTACH_FLAGS attachFlags,
    const CSSM_UPCALLS &upcalls)
{
    switch (subserviceType) {
        case CSSM_SERVICE_CSP:
            return new AppleCSPSession(handle,
				*this,
				version,
				subserviceId,
				subserviceType,
				attachFlags,
				upcalls);
        default:
            CssmError::throwMe(CSSMERR_CSSM_INVALID_SERVICE_MASK);
            return 0;	// placebo
    }
}


//
// Session constructor
//
AppleCSPSession::AppleCSPSession(
	CSSM_MODULE_HANDLE handle,
	AppleCSPPlugin &plug,
	const CSSM_VERSION &version,
	uint32 subserviceId,
	CSSM_SERVICE_TYPE subserviceType,
	CSSM_ATTACH_FLAGS attachFlags,
	const CSSM_UPCALLS &upcalls)
		: CSPFullPluginSession(handle, 
			plug, 
			version, 
			subserviceId, 
			subserviceType,
			attachFlags, 
			upcalls),
		#ifdef	BSAFE_CSP_ENABLE
		bSafe4Factory(*(dynamic_cast<BSafeFactory *>(plug.bSafe4Factory))), 
		#endif
		#ifdef	CRYPTKIT_CSP_ENABLE
		cryptKitFactory(*(dynamic_cast<CryptKitFactory *>(plug.cryptKitFactory))),
		#endif
		miscAlgFactory(*(dynamic_cast<MiscAlgFactory *>(plug.miscAlgFactory))),
		#ifdef	ASC_CSP_ENABLE
		ascAlgFactory(*(dynamic_cast<AscAlgFactory *>(plug.ascAlgFactory))),
		#endif
		rsaDsaAlgFactory(*(dynamic_cast<RSA_DSA_Factory *>(plug.rsaDsaAlgFactory))),
		dhAlgFactory(*(dynamic_cast<DH_Factory *>(plug.dhAlgFactory))),
		normAllocator(*this),
		privAllocator(plug.privAlloc())
{
	// anything? 
}

AppleCSPSession::~AppleCSPSession()
{
	// anything?
}

//
// Called at (CSSM) context create time. This is ignored; we do a full 
// context setup later, at setupContext time. 
//
CSPFullPluginSession::CSPContext *
AppleCSPSession::contextCreate(
	CSSM_CC_HANDLE handle, 
	const Context &context) 
{
	return NULL;
}

//
// Called by CSPFullPluginSession when an op is actually commencing.
// Context can safely assumed to be fully formed and stable for the
// duration of the  op; thus we wait until now to set up our 
// CSPContext as appropriate to the op.
//
void AppleCSPSession::setupContext(
	CSPContext * &cspCtx,
	const Context &context, 
	bool encoding)
{
	/*
	 * Note we leave the decision as to whether it's OK to 
	 * reuse a context to the individual factories.
	 */
	#ifdef	BSAFE_CSP_ENABLE
	/* Give BSAFE the firsrt shot if it's present */
	if (bSafe4Factory.setup(*this, cspCtx, context)) {
		CASSERT(cspCtx != NULL);
		return;
	}
	#endif
	if (rsaDsaAlgFactory.setup(*this, cspCtx, context)) {
		CASSERT(cspCtx != NULL);
		return;
	}
	if (miscAlgFactory.setup(*this, cspCtx, context)) {
		CASSERT(cspCtx != NULL);
		return;
	}
	if (dhAlgFactory.setup(*this, cspCtx, context)) {
		CASSERT(cspCtx != NULL);
		return;
	}
	#ifdef	CRYPTKIT_CSP_ENABLE
	if (cryptKitFactory.setup(*this, cspCtx, context)) {
		CASSERT(cspCtx != NULL);
		return;
	}
	#endif
	#ifdef	ASC_CSP_ENABLE
	if (ascAlgFactory.setup(*this, cspCtx, context)) {
		CASSERT(cspCtx != NULL);
		return;
	}
	#endif
	if(setup(cspCtx, context)) {
		CASSERT(cspCtx != NULL);
		return;
	}
	dprintf0("AppleCSPSession::setupContext: invalid algorithm\n");
	CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
}

/* 
 * Used for generating crypto contexts at this level. 
 * Analogous to AlgorithmFactory.setup().
 */
bool AppleCSPSession::setup(
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
	if (cspCtx) {
		return false;	// not ours or already set
	}
	
	switch(context.type()) {
		case CSSM_ALGCLASS_RANDOMGEN:
			switch (context.algorithm()) {
				case CSSM_ALGID_APPLE_YARROW:
					cspCtx = new YarrowContext(*this);
					return true;
				/* other random algs here */
				default:
					return false;
			}
		/* other contexts here */
		default:
			return false;
	}
	/* NOT REACHED */
	return false;

}

//
// Context for CSSM_ALGID_APPLE_YARROW.
//
YarrowContext::YarrowContext(AppleCSPSession &session)
	: AppleCSPContext(session)
{
	// nothing for now 
}

YarrowContext::~YarrowContext()
{
	// nothing for now 
}

//
// Only job here is to snag the length and process the optional seed argument
//
void YarrowContext::init(
	const Context &context, 
	bool encoding)
{
	/* stash requested length for use later in outputSize() */
	outSize = context.getInt(CSSM_ATTRIBUTE_OUTPUT_SIZE,
		CSSMERR_CSP_INVALID_ATTR_OUTPUT_SIZE);
		
	/* optional seed */
	CssmCryptoData *cseed = context.get<CssmCryptoData>(CSSM_ATTRIBUTE_SEED);
	if(cseed == NULL) {
		/* we're done */
		return;
	}
	CssmData seed = (*cseed)();
	if((seed.Length == 0) ||
	   (seed.Data == NULL)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_SEED);
	}
	session().addEntropy((size_t)seed.Length, seed.Data);
}

void YarrowContext::final(
	CssmData &out)
{
	session().getRandomBytes((size_t)out.Length, out.Data);
}

/***
 *** Binary Key support.
 ***/

// Given a CSSM_DATA, extract its KeyRef. 
static KeyRef CssmDataToKeyRef(
	const CSSM_DATA &data)
{
	if(data.Length != sizeof(KeyRef)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
	}
	
	uint8 *cp = data.Data;
	KeyRef keyRef = cp[0];
	keyRef |= ((KeyRef)cp[1]) << 8;
	keyRef |= ((KeyRef)cp[2]) << 16;
	keyRef |= ((KeyRef)cp[3]) << 24;
	return keyRef;
}

// Place a KeyRef into a CSSM_DATA, mallocing if necessary.
static void keyRefToCssmData(
	KeyRef			keyRef,
	CSSM_DATA		&data,
	CssmAllocator	&allocator)
{
	setUpData(data, sizeof(keyRef), allocator);
	
	uint8 *cp = data.Data;
	cp[0] = keyRef & 0xff;
	cp[1] = (keyRef >> 8) & 0xff;
	cp[2] = (keyRef >> 16) & 0xff;
	cp[3] = (keyRef >> 24) & 0xff;
}

// Look up a BinaryKey by its KeyRef. Returns NULL if not 
// found. refKeyMapLock held on entry and exit.
BinaryKey *AppleCSPSession::lookupKeyRef(
	KeyRef	keyRef)
{
	const BinaryKey *binKey;
	
	// use safe version, don't create new entry if this key
	// isn't there
	keyMap::iterator it = refKeyMap.find(keyRef);
	if(it == refKeyMap.end()) {
		return NULL;
	}
	binKey = it->second;
	assert(binKey == reinterpret_cast<const BinaryKey *>(keyRef));
	assert(binKey->mKeyRef == keyRef);
	return const_cast<BinaryKey *>(binKey);
}

// add a BinaryKey to our refKeyMap. Sets up cssmKey
// as appropriate.
void AppleCSPSession::addRefKey(
	BinaryKey	&binKey,
	CssmKey		&cssmKey)
{
	// for now, KeyRef is just the address of the BinaryKey
	KeyRef			keyRef = reinterpret_cast<KeyRef>(&binKey);
	
	binKey.mKeyRef = keyRef;
	binKey.mKeyHeader = CssmKey::Header::overlay(cssmKey.KeyHeader);
	{
		StLock<Mutex> _(refKeyMapLock);
		assert(lookupKeyRef(keyRef) == NULL);
		refKeyMap[keyRef] = &binKey;
	}
	cssmKey.KeyHeader.BlobType = CSSM_KEYBLOB_REFERENCE;
	cssmKey.KeyHeader.Format = CSSM_KEYBLOB_REF_FORMAT_INTEGER;
	keyRefToCssmData(keyRef, cssmKey.KeyData, normAllocator);
	secdebug("freeKey", "CSP addRefKey key %p keyData %p keyRef %p", 
		&cssmKey, cssmKey.KeyData.Data, &binKey);
}
	
// Given a CssmKey in reference form, obtain the associated
// BinaryKey. Throws CSSMERR_CSP_INVALID_KEY_REFERENCE if
// key not found in session key map.
BinaryKey & AppleCSPSession::lookupRefKey(
	const CssmKey		&cssmKey)
{
	KeyRef 		keyRef;
	BinaryKey	*binKey;

	keyRef = CssmDataToKeyRef(cssmKey.KeyData);
	{
		StLock<Mutex> _(refKeyMapLock);
		binKey = lookupKeyRef(keyRef);
	}
	if(binKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
	}
	assert(Guid::overlay(binKey->mKeyHeader.CspId) == plugin.myGuid());

	/*
	 * Verify sensitive fields have not changed between when the BinaryKey was
	 * created/stored and when the caller passed in the ref key.
	 * Some fields were changed by addRefKey, so make a local copy....
	 */
	CSSM_KEYHEADER localHdr = cssmKey.KeyHeader;
	localHdr.BlobType = binKey->mKeyHeader.BlobType;
	localHdr.Format = binKey->mKeyHeader.Format;
	if(memcmp(&localHdr, &binKey->mKeyHeader, sizeof(CSSM_KEYHEADER))) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
	}
	return (*binKey);
}

// CSPFullPluginSession declares & implements this.
// Note that we ignore the delete argument; since we don't
// store anything, freeing is the same as deleting. 
void AppleCSPSession::FreeKey(
	const AccessCredentials *AccessCred,
	CssmKey &KeyPtr,
	CSSM_BOOL Delete)
{

	if((KeyPtr.blobType() == CSSM_KEYBLOB_REFERENCE) &&
	   (KeyPtr.cspGuid() == plugin.myGuid())) {
		// it's a ref key we generated - delete associated BinaryKey 
		KeyRef keyRef = CssmDataToKeyRef(KeyPtr.KeyData);
		{
			StLock<Mutex> _(refKeyMapLock);
			BinaryKey *binKey = lookupKeyRef(keyRef);
			if(binKey != NULL) {
				secdebug("freeKey", "CSP FreeKey key %p keyData %p binKey %p", 
					&KeyPtr, KeyPtr.KeyData.Data, binKey);
				try {
					refKeyMap.erase(keyRef);
					delete binKey;
				}
				catch (...)  {
					errorLog0("Error deleting/erasing known "
							"ref key\n");
				}
			}
			else {
				secdebug("freeKey", "CSP freeKey unknown key");
			}
		}
	}
	CSPFullPluginSession::FreeKey(AccessCred, KeyPtr, Delete);
}

/* Passthrough, used for key digest */
void AppleCSPSession::PassThrough(
	CSSM_CC_HANDLE CCHandle,
	const Context &Context,
	uint32 PassThroughId,
	const void *InData,
	void **OutData)
{
	*OutData = NULL;
	
	/* validate context */
	if(Context.type() != CSSM_ALGCLASS_NONE) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);
	}

	switch(PassThroughId) {
		case CSSM_APPLECSP_KEYDIGEST:
		{
			CssmKey &key = Context.get<CssmKey>(
				CSSM_ATTRIBUTE_KEY, 
				CSSMERR_CSP_MISSING_ATTR_KEY);
				
			/* validate key as best we can */
			switch(key.keyClass()) {
				case CSSM_KEYCLASS_PUBLIC_KEY:
				case CSSM_KEYCLASS_PRIVATE_KEY:
				case CSSM_KEYCLASS_SESSION_KEY:
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}

			/* 
			 * Ref key: obtain binary, ask it for blob 
			 * Raw key: get info provider, ask it for the blob. This
			 *          allows for an optimized path which avoids
			 *		    converting to a BinaryKey. 
			 */
			CssmData blobToHash;
			switch(key.blobType()) {
				case CSSM_KEYBLOB_RAW:
				{
					CSPKeyInfoProvider *provider = infoProvider(key);
					bool converted = 
						provider->getHashableBlob(privAllocator, blobToHash);
					if(converted) {
						/* took optimized case; proceed */
						delete provider;
						break;
					}
					
					/* convert to BinaryKey and ask it to do the work */
					BinaryKey *binKey;
					CSSM_KEYATTR_FLAGS flags = 0;	// not used
					provider->CssmKeyToBinary(NULL,	// no paramKey
						flags,
						&binKey);
					binKey->mKeyHeader = 
						CssmKey::Header::overlay(key.KeyHeader);
					CSSM_KEYBLOB_FORMAT rawFormat;
					rawFormat = CSSM_KEYBLOB_RAW_FORMAT_DIGEST;
					CSSM_KEYATTR_FLAGS	attrFlags = 0;
					binKey->generateKeyBlob(privAllocator,
							blobToHash,
							rawFormat,
							*this, 
							NULL,
							attrFlags);
					delete binKey;	
					delete provider;
					break;
				}
				case CSSM_KEYBLOB_REFERENCE:
					{
						BinaryKey &binKey = lookupRefKey(key);
						CSSM_KEYBLOB_FORMAT rawFormat;
						rawFormat = CSSM_KEYBLOB_RAW_FORMAT_DIGEST;
						CSSM_KEYATTR_FLAGS attrFlags = 0;
						binKey.generateKeyBlob(privAllocator,
							blobToHash,
							rawFormat,
							*this,
							NULL,
							attrFlags);
					}
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			
			/* obtain sha1 hash of blobToHash */
			
			CSSM_DATA_PTR outHash = NULL;
			try {
				outHash = 
					(CSSM_DATA_PTR)normAllocator.malloc(sizeof(CSSM_DATA));
				outHash->Data = 
					(uint8 *)normAllocator.malloc(SHA1_DIGEST_SIZE);
				outHash->Length = SHA1_DIGEST_SIZE;
			}
			catch(...) {
				freeCssmData(blobToHash, privAllocator);
				throw;
			}
			cspGenSha1Hash(blobToHash.data(), blobToHash.length(), 
				outHash->Data);
			freeCssmData(blobToHash, privAllocator);
			*OutData = outHash;
			return;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_PASSTHROUGH_ID);
	}
	/* NOT REACHED */
}

/*
 * CSPSession version of QueryKeySizeInBits.
 */
void AppleCSPSession::getKeySize(const CssmKey &key, 
	CSSM_KEY_SIZE &size)
{
	CSPKeyInfoProvider *provider = infoProvider(key);
	try {
		provider->QueryKeySizeInBits(size);
	}
	catch(...) {
		/* don't leak this on error */
		delete provider;
		throw;
	}
	delete provider;
}

/*
 * Per-session RNG, which currently just redirects to YarrowConnection.
 * CSP_ALLOW_FEE_RNG enables a fallback to the internal FEE-based PRNG if
 * SecurityServer is not running. 
 */
 
#if	CSP_ALLOW_FEE_RNG
#include <CryptKit/feeRandom.h>

static Mutex feeRngMutex;
static feeRand	feeRng = NULL;

static void cspRandViaFee(size_t length, uint8 *cp)
{
	StLock<Mutex> _(feeRngMutex);
	if(feeRng == NULL) {
		feeRng = feeRandAlloc();
	}
	feeRandBytes(feeRng, cp, length);
}
#endif	/* CSP_ALLOW_FEE_RNG */

void AppleCSPSession::getRandomBytes(size_t length, uint8 *cp)
{
	try {
		cspGetRandomBytes(cp, (unsigned)length);
	}
	catch(...) {
		#if		CSP_ALLOW_FEE_RNG
		errorLog0("CSP: YarrowClient failure; using FEE RNG\n");
		cspRandViaFee(length, cp);
		#else
		errorLog0("CSP: YarrowClient failure\n");
		#endif
	}
}

void AppleCSPSession::addEntropy(size_t length, const uint8 *cp)
{
	try {
		cspAddEntropy(cp, (unsigned)length);
	}
	catch(...) {
		#if		CSP_ALLOW_FEE_RNG
		return;
		#else
		throw;
		#endif
	}
}

/***
 *** CSPKeyInfoProvider support.
 ***/
 
/*
 * Find a CSPKeyInfoProvider subclass for the specified key.
 */
CSPKeyInfoProvider *AppleCSPSession::infoProvider(
	const CssmKey	&key)
{
	CSPKeyInfoProvider *provider = NULL;
	
	#ifdef	BSAFE_CSP_ENABLE
	/* Give BSAFE first shot, if it's here */
	provider = BSafe::BSafeKeyInfoProvider::provider(key, *this);
	if(provider != NULL) {
		return provider;
	}
	#endif
	
	provider = RSAKeyInfoProvider::provider(key, *this);
	if(provider != NULL) {
		return provider;
	}
	
	provider = SymmetricKeyInfoProvider::provider(key, *this);
	if(provider != NULL) {
		return provider;
	}

	#ifdef	CRYPTKIT_CSP_ENABLE
	provider = CryptKit::FEEKeyInfoProvider::provider(key, *this);
	if(provider != NULL) {
		return provider;
	}
	#endif
	
	provider = DSAKeyInfoProvider::provider(key, *this);
	if(provider != NULL) {
		return provider;
	}
	
	provider = DHKeyInfoProvider::provider(key, *this);
	if(provider != NULL) {
		return provider;
	}
	
	CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
}

