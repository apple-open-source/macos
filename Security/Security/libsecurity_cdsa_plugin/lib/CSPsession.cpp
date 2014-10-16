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
// CSPsession - Plugin framework for CSP plugin modules
//
#include <security_cdsa_plugin/CSPsession.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_utilities/cssmbridge.h>


typedef CSPFullPluginSession::CSPContext CSPContext;


//
// PluginContext construction
//
CSPPluginSession::PluginContext::~PluginContext()
{ /* virtual */ }

CSPFullPluginSession::AlgorithmFactory::~AlgorithmFactory()
{ /* virtual */ }


//
// Internal utilities
//
CssmData CSPFullPluginSession::makeBuffer(size_t size, Allocator &alloc)
{
	return CssmData(alloc.malloc(size), size);
}

inline size_t CSPFullPluginSession::totalBufferSize(const CssmData *data, uint32 count)
{
	size_t size = 0;
	for (uint32 n = 0; n < count; n++)
		size += data[n].length();
	return size;
}


//
// Notify a context that its underlying CSSM context has (well, may have) changed.
// The default reaction is to ask the frame to delete the context and start over.
//
bool CSPPluginSession::PluginContext::changed(const Context &context)
{
    return false;	// delete me, please
}


//
// The Session's init() function calls your setupContext() method to prepare
// it for action, then calls the context's init() method.
//
CSPContext *CSPFullPluginSession::init(CSSM_CC_HANDLE ccHandle,
									CSSM_CONTEXT_TYPE type,
									const Context &context, bool encoding)
{
    CSPContext *ctx = getContext<CSPContext>(ccHandle);
    checkOperation(context.type(), type);

    // ask the implementation to set up an internal context
    setupContext(ctx, context, encoding);
    assert(ctx != NULL);	// must have context now (@@@ throw INTERNAL_ERROR instead?)
    ctx->mType = context.type();
    ctx->mDirection = encoding;
    setContext(ccHandle, ctx);

    // initialize the context and return it
    ctx->init(context, encoding);
    return ctx;
}


//
// Retrieve a context for a staged operation in progress.
//
CSPContext *CSPFullPluginSession::getStagedContext(CSSM_CC_HANDLE ccHandle,
	CSSM_CONTEXT_TYPE type, bool encoding)
{
	CSPContext *ctx = getContext<CSPContext>(ccHandle);
	if (ctx == NULL)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);	//@@@ better diagnostic?
	checkOperation(ctx->type(), type);
	if (ctx->encoding() != encoding)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);
	return ctx;
}


//
// The Session's checkState() function is called for subsequent staged operations
// (update/final) to verify that the user didn't screw up the sequencing.
//
void CSPFullPluginSession::checkOperation(CSSM_CONTEXT_TYPE ctxType, CSSM_CONTEXT_TYPE opType)
{
    switch (opType) {
        case CSSM_ALGCLASS_NONE:	// no check
            return;
        case CSSM_ALGCLASS_CRYPT:	// symmetric or asymmetric encryption
            if (ctxType == CSSM_ALGCLASS_SYMMETRIC ||
                ctxType == CSSM_ALGCLASS_ASYMMETRIC)
                return;
        default:					// plain match
            if (ctxType == opType)
                return;
    }
    CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);
}


//
// The default implementations of the primary context operations throw internal
// errors. You must implement any of these that are actually called by the
// operations involved. The others, of course, can be left alone.
//
void CSPContext::init(const Context &context, bool encoding)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

void CSPContext::update(const CssmData &data)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

void CSPContext::update(void *inp, size_t &inSize, void *outp, size_t &outSize)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

void CSPContext::final(CssmData &out)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

void CSPContext::final(const CssmData &in)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

void CSPContext::generate(const Context &, CssmKey &pubKey, CssmKey &privKey)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

void CSPContext::generate(const Context &, uint32, CssmData &params,
                                                uint32 &attrCount, Context::Attr * &attrs)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

size_t CSPContext::inputSize(size_t outSize)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

size_t CSPContext::outputSize(bool final, size_t inSize)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

void CSPContext::minimumProgress(size_t &in, size_t &out)
{ CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); }

CSPFullPluginSession::CSPContext *CSPContext::clone(Allocator &)
{ CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED); }

void CSPContext::setDigestAlgorithm(CSSM_ALGORITHMS digestAlg)
{ CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM); }

void CSPContext::update(const CssmData *in,
						uint32 inCount, Writer &writer)
{
    const CssmData *lastIn = in + inCount;
    CssmData current;
    for (;;) {
        if (current.length() == 0) {
            if (in == lastIn)
                return;		// all done
            current = *in++;
			continue; // Just in case next block is zero length too.
        }
        // match up current input and output buffers
        void *outP; size_t outSize;
        writer.nextBlock(outP, outSize);
        size_t inSize = inputSize(outSize);
        if (inSize > current.length())
            inSize = current.length();	// cap to remaining input buffer
        if (inSize > 0) {
            // we can stuff into the current output buffer - do it
            update(current.data(), inSize, outP, outSize);
            current.use(inSize);
            writer.use(outSize);
        } else {
            // We have remaining output buffer space, but not enough
            // for the algorithm to make progress with it. We must proceed with
            // a bounce buffer and split it manually into this and the next buffer(s).
            size_t minOutput;
            minimumProgress(inSize, minOutput);
            assert(minOutput > outSize);		// PluginContext consistency (not fatal)
            char splitBuffer[128];
            assert(minOutput <= sizeof(splitBuffer)); // @@@ static buffer for now
            outSize = sizeof(splitBuffer);
            if (current.length() < inSize)
                inSize = current.length();	// cap to data remaining in input buffer
            update(current.data(), inSize, splitBuffer, outSize);
            assert(inSize > 0);				// progress made
            writer.put(splitBuffer, outSize);	// stuff into buffer, the hard way
            current.use(inSize);
        }
    }
}

void CSPContext::final(CssmData &out, Allocator &alloc)
{
    size_t needed = outputSize(true, 0);
    if (out) {
        if (out.length() < needed)
            CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
    } else {
        out = makeBuffer(needed, alloc);
    }
    final(out);
}

void CSPContext::final(Writer &writer, Allocator &alloc)
{
    if (size_t needed = outputSize(true, 0)) {
        // need to generate additional output
        writer.allocate(needed, alloc);		// belt + suspender
    
        void *addr; size_t size;
        writer.nextBlock(addr, size);		// next single block available
        if (needed <= size) {				// rest fits into one block
            CssmData chunk(addr, size);
            final(chunk);
            writer.use(chunk.length());
        } else {							// need to split it up
            char splitBuffer[128];
            assert(needed <= sizeof(splitBuffer));
            CssmData chunk(splitBuffer, sizeof(splitBuffer));
            final(chunk);
            writer.put(chunk.data(), chunk.length());
        }
    }
}


//
// Default context response functions
//
CSPPluginSession::PluginContext *
CSPPluginSession::contextCreate(CSSM_CC_HANDLE, const Context &)
{
	return NULL;	// request no local context
}

void CSPPluginSession::contextUpdate(CSSM_CC_HANDLE ccHandle,
                                     const Context &context, PluginContext * &ctx)
{
    // call update notifier in context object
    if (ctx && !ctx->changed(context)) {
        // context requested that it be removed
        delete ctx;
        ctx = NULL;
    }
}

void CSPPluginSession::contextDelete(CSSM_CC_HANDLE, const Context &, PluginContext *)
{
    // do nothing (you can't prohibit deletion here)
}


//
// Default event notification handler.
// This default handler calls the virtual context* methods to dispose of context actions.
//
void CSPPluginSession::EventNotify(CSSM_CONTEXT_EVENT event,
                                   CSSM_CC_HANDLE ccHandle, const Context &context)
{
    switch (event) {
        case CSSM_CONTEXT_EVENT_CREATE:
            if (PluginContext *ctx = contextCreate(ccHandle, context)) {
				StLock<Mutex> _(contextMapLock);
                assert(contextMap[ccHandle] == NULL);	// check context re-creation
                contextMap[ccHandle] = ctx;
            }
            break;
        case CSSM_CONTEXT_EVENT_UPDATE:
            // note that the handler can change the map entry (even to NULL, if desired)
			{
				StLock<Mutex> _(contextMapLock);
				contextUpdate(ccHandle, context, contextMap[ccHandle]);
			}
            break;
        case CSSM_CONTEXT_EVENT_DELETE:
			{
				StLock<Mutex> _(contextMapLock);
				if (PluginContext *ctx = contextMap[ccHandle]) {
					contextDelete(ccHandle, context, ctx);
					delete ctx;
				}
				contextMap.erase(ccHandle);
			}
			break;
        default:
            CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	// unexpected event code
    }
}


//
// Defaults for methods you *should* implement.
// If you don't, they'll throw UNIMPLEMENTED.
//
void CSPFullPluginSession::getKeySize(const CssmKey &key, CSSM_KEY_SIZE &size)
{ unimplemented(); }
 

//
// Encryption and decryption
//
void CSPFullPluginSession::EncryptData(CSSM_CC_HANDLE ccHandle,
                                       const Context &context,
                                       const CssmData clearBufs[],
                                       uint32 clearBufCount,
                                       CssmData cipherBufs[],
                                       uint32 cipherBufCount,
                                       CSSM_SIZE &bytesEncrypted,
                                       CssmData &remData,
                                       CSSM_PRIVILEGE privilege)
{
    Writer writer(cipherBufs, cipherBufCount, &remData);
    CSPContext *ctx = init(ccHandle, CSSM_ALGCLASS_CRYPT, context, true);
    size_t outNeeded = ctx->outputSize(true, totalBufferSize(clearBufs, clearBufCount));
    writer.allocate(outNeeded, *this);
    ctx->update(clearBufs, clearBufCount, writer);
    ctx->final(writer, *this);
    bytesEncrypted = writer.close();
}

void CSPFullPluginSession::EncryptDataInit(CSSM_CC_HANDLE ccHandle,
                             const Context &context,
                             CSSM_PRIVILEGE Privilege)
{
    init(ccHandle, CSSM_ALGCLASS_CRYPT, context, true);
}

void CSPFullPluginSession::EncryptDataUpdate(CSSM_CC_HANDLE ccHandle,
                                         const CssmData clearBufs[],
                                         uint32 clearBufCount,
                                         CssmData cipherBufs[],
                                         uint32 cipherBufCount,
                                         CSSM_SIZE &bytesEncrypted)
{
    CSPContext *alg = getStagedContext(ccHandle, CSSM_ALGCLASS_CRYPT, true);
    Writer writer(cipherBufs, cipherBufCount);
    size_t outNeeded = alg->outputSize(false, totalBufferSize(clearBufs, clearBufCount));
    writer.allocate(outNeeded, *this);
    alg->update(clearBufs, clearBufCount, writer);
    bytesEncrypted = writer.close();
}

void CSPFullPluginSession::EncryptDataFinal(CSSM_CC_HANDLE ccHandle,
                              CssmData &remData)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_CRYPT, true)->final(remData, *this);
}


void CSPFullPluginSession::DecryptData(CSSM_CC_HANDLE ccHandle,
                                   const Context &context,
                                   const CssmData cipherBufs[],
                                   uint32 cipherBufCount,
                                   CssmData clearBufs[],
                                   uint32 clearBufCount,
                                   CSSM_SIZE &bytesDecrypted,
                                   CssmData &remData,
                                   CSSM_PRIVILEGE privilege)
{
    Writer writer(clearBufs, clearBufCount, &remData);
    CSPContext *ctx = init(ccHandle, CSSM_ALGCLASS_CRYPT, context, false);
    size_t outNeeded = ctx->outputSize(true, totalBufferSize(cipherBufs, cipherBufCount));
    writer.allocate(outNeeded, *this);
    ctx->update(cipherBufs, cipherBufCount, writer);
    ctx->final(writer, *this);
    bytesDecrypted = writer.close();
}

void CSPFullPluginSession::DecryptDataInit(CSSM_CC_HANDLE ccHandle,
                             const Context &context,
                             CSSM_PRIVILEGE Privilege)
{
    init(ccHandle, CSSM_ALGCLASS_CRYPT, context, false);
}

void CSPFullPluginSession::DecryptDataUpdate(CSSM_CC_HANDLE ccHandle,
                               const CssmData cipherBufs[],
                               uint32 cipherBufCount,
                               CssmData clearBufs[],
                               uint32 clearBufCount,
                               CSSM_SIZE &bytesDecrypted)
{
    CSPContext *ctx = getStagedContext(ccHandle, CSSM_ALGCLASS_CRYPT, false);
    Writer writer(clearBufs, clearBufCount);
    size_t outNeeded = ctx->outputSize(false, totalBufferSize(cipherBufs, cipherBufCount));
    writer.allocate(outNeeded, *this);
    ctx->update(cipherBufs, cipherBufCount, writer);
    bytesDecrypted = writer.close();
}

void CSPFullPluginSession::DecryptDataFinal(CSSM_CC_HANDLE ccHandle,
                                      CssmData &remData)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_CRYPT, false)->final(remData, *this);
}
               
void CSPFullPluginSession::QuerySize(CSSM_CC_HANDLE ccHandle,
									 const Context &context,
									 CSSM_BOOL encrypt,
									 uint32 querySizeCount,
									 QuerySizeData *dataBlock)
{
	if (querySizeCount == 0)
		return;	// nothing ventured, nothing gained
	CSPContext *ctx = getContext<CSPContext>(ccHandle);	// existing context?
	if (ctx == NULL)	// force internal context creation (as best we can)
		ctx = init(ccHandle, context.type(), context, encrypt);
	// If QuerySizeCount > 1, we assume this inquires about a staged
	// operation, and the LAST item gets the 'final' treatment.
	//@@@ Intel revised algspec says "use the staged flag" -- TBD
	for (uint32 n = 0; n < querySizeCount; n++) {
		// the outputSize() call might throw CSSMERR_CSP_QUERY_SIZE_UNKNOWN
		dataBlock[n].SizeOutputBlock =
			(uint32)ctx->outputSize(n == querySizeCount-1, dataBlock[n].inputSize());
	}
	//@@@ if we forced a context creation, should we discard it now?
}


//
// Key wrapping and unwrapping.
//
void CSPFullPluginSession::WrapKey(CSSM_CC_HANDLE CCHandle,
								const Context &Context,
								const AccessCredentials &AccessCred,
								const CssmKey &Key,
								const CssmData *DescriptiveData,
								CssmKey &WrappedKey,
								CSSM_PRIVILEGE Privilege)
{
	unimplemented();
}

void CSPFullPluginSession::UnwrapKey(CSSM_CC_HANDLE CCHandle,
								const Context &Context,
								const CssmKey *PublicKey,
								const CssmKey &WrappedKey,
								uint32 KeyUsage,
								uint32 KeyAttr,
								const CssmData *KeyLabel,
								const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
								CssmKey &UnwrappedKey,
								CssmData &DescriptiveData,
								CSSM_PRIVILEGE Privilege)
{
	unimplemented();
}

void CSPFullPluginSession::DeriveKey(CSSM_CC_HANDLE CCHandle,
							const Context &Context,
							CssmData &Param,
							uint32 KeyUsage,
							uint32 KeyAttr,
							const CssmData *KeyLabel,
							const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
							CssmKey &DerivedKey)
{
	unimplemented();
}


//
// Message Authentication Codes.
// Almost like signatures (signatures with symmetric keys), though the
// underlying implementation may be somewhat different.
//
void CSPFullPluginSession::GenerateMac(CSSM_CC_HANDLE ccHandle,
                                   const Context &context,
                                   const CssmData dataBufs[],
                                   uint32 dataBufCount,
                                   CssmData &mac)
{
    GenerateMacInit(ccHandle, context);
    GenerateMacUpdate(ccHandle, dataBufs, dataBufCount);
    GenerateMacFinal(ccHandle, mac);
}

void CSPFullPluginSession::GenerateMacInit(CSSM_CC_HANDLE ccHandle,
                                           const Context &context)
{
    init(ccHandle, CSSM_ALGCLASS_MAC, context, true);
}

void CSPFullPluginSession::GenerateMacUpdate(CSSM_CC_HANDLE ccHandle,
                                             const CssmData dataBufs[],
                                             uint32 dataBufCount)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_MAC, true)->update(dataBufs, dataBufCount);
}

void CSPFullPluginSession::GenerateMacFinal(CSSM_CC_HANDLE ccHandle,
                                            CssmData &mac)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_MAC, true)->final(mac, *this);
}

void CSPFullPluginSession::VerifyMac(CSSM_CC_HANDLE ccHandle,
                                     const Context &context,
                                     const CssmData dataBufs[],
                                     uint32 dataBufCount,
                                     const CssmData &mac)
{
    VerifyMacInit(ccHandle, context);
    VerifyMacUpdate(ccHandle, dataBufs, dataBufCount);
    VerifyMacFinal(ccHandle, mac);
}

void CSPFullPluginSession::VerifyMacInit(CSSM_CC_HANDLE ccHandle,
                                         const Context &context)
{
    init(ccHandle, CSSM_ALGCLASS_MAC, context, false);
}

void CSPFullPluginSession::VerifyMacUpdate(CSSM_CC_HANDLE ccHandle,
                                           const CssmData dataBufs[],
                                           uint32 dataBufCount)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_MAC, false)->update(dataBufs, dataBufCount);
}

void CSPFullPluginSession::VerifyMacFinal(CSSM_CC_HANDLE ccHandle,
                                          const CssmData &mac)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_MAC, false)->final(mac);
}


//
// Signatures
//
void CSPFullPluginSession::SignData(CSSM_CC_HANDLE ccHandle,
                                const Context &context,
                                const CssmData dataBufs[],
                                uint32 dataBufCount,
                                CSSM_ALGORITHMS digestAlgorithm,
                                CssmData &Signature)
{
	SignDataInit(ccHandle, context);
	if(digestAlgorithm != CSSM_ALGID_NONE) {
		getStagedContext(ccHandle, CSSM_ALGCLASS_SIGNATURE, 
			true)->setDigestAlgorithm(digestAlgorithm);
	}
	SignDataUpdate(ccHandle, dataBufs, dataBufCount);
	SignDataFinal(ccHandle, Signature);
}

void CSPFullPluginSession::SignDataInit(CSSM_CC_HANDLE ccHandle,
                                    const Context &context)
{
    init(ccHandle, CSSM_ALGCLASS_SIGNATURE, context, true);
}

void CSPFullPluginSession::SignDataUpdate(CSSM_CC_HANDLE ccHandle,
                    const CssmData dataBufs[],
                    uint32 dataBufCount)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_SIGNATURE, true)->update(dataBufs, dataBufCount);
}

void CSPFullPluginSession::SignDataFinal(CSSM_CC_HANDLE ccHandle,
                           CssmData &signature)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_SIGNATURE, true)->final(signature, *this);
}


void CSPFullPluginSession::VerifyData(CSSM_CC_HANDLE ccHandle,
                                  const Context &context,
                                  const CssmData dataBufs[],
                                  uint32 dataBufCount,
                                  CSSM_ALGORITHMS digestAlgorithm,
                                  const CssmData &Signature)
{
	VerifyDataInit(ccHandle, context);
	if(digestAlgorithm != CSSM_ALGID_NONE) {
		getStagedContext(ccHandle, CSSM_ALGCLASS_SIGNATURE, 
			false)->setDigestAlgorithm(digestAlgorithm);
	}
	VerifyDataUpdate(ccHandle, dataBufs, dataBufCount);
	VerifyDataFinal(ccHandle, Signature);
}

void CSPFullPluginSession::VerifyDataInit(CSSM_CC_HANDLE ccHandle, const Context &context)
{
    init(ccHandle, CSSM_ALGCLASS_SIGNATURE, context, false);
}

void CSPFullPluginSession::VerifyDataUpdate(CSSM_CC_HANDLE ccHandle,
                                        const CssmData dataBufs[],
                                        uint32 dataBufCount)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_SIGNATURE, false)->update(dataBufs, dataBufCount);
}

void CSPFullPluginSession::VerifyDataFinal(CSSM_CC_HANDLE ccHandle,
                     const CssmData &signature)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_SIGNATURE, false)->final(signature);
}


//
// Digesting
//
void CSPFullPluginSession::DigestData(CSSM_CC_HANDLE ccHandle,
               const Context &context,
               const CssmData dataBufs[],
               uint32 DataBufCount,
               CssmData &Digest)
{
    DigestDataInit(ccHandle, context);
    DigestDataUpdate(ccHandle, dataBufs, DataBufCount);
    DigestDataFinal(ccHandle, Digest);
}

void CSPFullPluginSession::DigestDataInit(CSSM_CC_HANDLE ccHandle, const Context &context)
{
    init(ccHandle, CSSM_ALGCLASS_DIGEST, context);
}

void CSPFullPluginSession::DigestDataUpdate(CSSM_CC_HANDLE ccHandle,
                              const CssmData dataBufs[],
                              uint32 dataBufCount)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_DIGEST)->update(dataBufs, dataBufCount);
}

void CSPFullPluginSession::DigestDataFinal(CSSM_CC_HANDLE ccHandle,
                                           CssmData &digest)
{
    getStagedContext(ccHandle, CSSM_ALGCLASS_DIGEST)->final(digest, *this);
}

void CSPFullPluginSession::DigestDataClone(CSSM_CC_HANDLE ccHandle,
                                           CSSM_CC_HANDLE clonedCCHandle)
{
    CSPContext *cloned = getStagedContext(ccHandle, CSSM_ALGCLASS_DIGEST)->clone(*this);
    cloned->mDirection = true;
    cloned->mType = CSSM_ALGCLASS_DIGEST;
    setContext(clonedCCHandle, cloned);
}


//
// Key generation, Derivation, and inquiry
//
void CSPFullPluginSession::GenerateKey(CSSM_CC_HANDLE ccHandle,
                         const Context &context,
                         uint32 keyUsage,
                         uint32 keyAttr,
                         const CssmData *keyLabel,
                         const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
                         CssmKey &key,
                         CSSM_PRIVILEGE privilege)
{
    CSPContext *alg = init(ccHandle, CSSM_ALGCLASS_KEYGEN, context);
    setKey(key, context, CSSM_KEYCLASS_SESSION_KEY, keyAttr, keyUsage);
    CssmKey blank;		// dummy 2nd key (not used)
    alg->generate(context, key, blank);
}

class ContextMinder
{
private:
	CSSM_CC_HANDLE mHandle;

public:
	ContextMinder(CSSM_CC_HANDLE ccHandle) : mHandle(ccHandle) {}
	~ContextMinder() {CSSM_DeleteContext(mHandle);}
};



void CSPFullPluginSession::GenerateKeyPair(CSSM_CC_HANDLE ccHandle,
                             const Context &context,
                             uint32 publicKeyUsage,
                             uint32 publicKeyAttr,
                             const CssmData *publicKeyLabel,
                             CssmKey &publicKey,
                             uint32 privateKeyUsage,
                             uint32 privateKeyAttr,
                             const CssmData *privateKeyLabel,
                             const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
                             CssmKey &privateKey,
                             CSSM_PRIVILEGE privilege)
{
    CSPContext *alg = init(ccHandle, CSSM_ALGCLASS_KEYGEN, context);
    
    setKey(publicKey, context, CSSM_KEYCLASS_PUBLIC_KEY, publicKeyAttr, publicKeyUsage);
    setKey(privateKey, context, CSSM_KEYCLASS_PRIVATE_KEY, privateKeyAttr, privateKeyUsage);
    alg->generate(context, publicKey, privateKey);
	
    //@@@ handle labels
    //@@@ handle reference keys

	bool encryptPublic = publicKeyUsage & CSSM_KEYUSE_ENCRYPT;
	bool encryptPrivate = privateKeyUsage & CSSM_KEYUSE_ENCRYPT;

	if (!(encryptPublic || encryptPrivate))
	{
		return ;
	}
	
	// time to do the FIPS required test!
	CSSM_CSP_HANDLE moduleHandle = handle();
	CSSM_CC_HANDLE encryptHandle;
	CSSM_ACCESS_CREDENTIALS nullCreds;
	memset(&nullCreds, 0, sizeof(nullCreds));

	CSSM_KEY_PTR encryptingKey, decryptingKey;
	if (encryptPublic)
	{
		encryptingKey = &publicKey;
		decryptingKey = &privateKey;
	}
	else
	{
		encryptingKey = &privateKey;
		decryptingKey = &publicKey;
	}
	
	// make data to be encrypted
	unsigned bytesInKey = encryptingKey->KeyHeader.LogicalKeySizeInBits / 8;
	u_int8_t buffer[bytesInKey];
	unsigned i;
	
	for (i = 0; i < bytesInKey; ++i)
	{
		buffer[i] = i;
	}
	
	CSSM_DATA clearBuf = {bytesInKey, buffer};
	CSSM_DATA cipherBuf; // have the CSP allocate the resulting memory
	CSSM_SIZE bytesEncrypted;
	CSSM_DATA remData = {0, NULL};
	CSSM_DATA decryptedBuf = {bytesInKey, buffer};
	
	CSSM_RETURN result = CSSM_CSP_CreateAsymmetricContext(moduleHandle, encryptingKey->KeyHeader.AlgorithmId,  &nullCreds, encryptingKey, CSSM_PADDING_NONE, &encryptHandle);
	if (result != CSSM_OK)
	{
		CssmError::throwMe(result);
	}
	
	ContextMinder encryptMinder(encryptHandle); // auto throw away if we error out
	
	CSSM_QUERY_SIZE_DATA qsData;
	qsData.SizeInputBlock = bytesInKey;
	result = CSSM_QuerySize(encryptHandle, CSSM_TRUE, 1, &qsData);
	if (result == CSSMERR_CSP_INVALID_ALGORITHM)
	{
		return;
	}
	
	uint8 cipherBuffer[qsData.SizeOutputBlock];
	cipherBuf.Length = qsData.SizeOutputBlock;
	cipherBuf.Data = cipherBuffer;

	// do the encryption
	result = CSSM_EncryptData(encryptHandle, &clearBuf, 1, &cipherBuf, 1, &bytesEncrypted, &remData);
	if (result != CSSM_OK)
	{
		CssmError::throwMe(result);
	}
	
	// check the result
	if (memcmp(cipherBuf.Data, clearBuf.Data, clearBuf.Length) == 0)
	{
		// we have a match, that's not good news...
		abort();
	}
	
	// clean up
	if (remData.Data != NULL)
	{
		free(remData.Data);
	}
	
	// make a context to perform the decryption
	CSSM_CC_HANDLE decryptHandle;
	result = CSSM_CSP_CreateAsymmetricContext(moduleHandle, encryptingKey->KeyHeader.AlgorithmId, &nullCreds, decryptingKey, CSSM_PADDING_NONE, &decryptHandle);
	ContextMinder decryptMinder(decryptHandle);

	if (result != CSSM_OK)
	{
		CssmError::throwMe(result);
	}
	
	result = CSSM_DecryptData(decryptHandle, &cipherBuf, 1, &decryptedBuf, 1, &bytesEncrypted, &remData);
	if (result != CSSM_OK)
	{
		CssmError::throwMe(result);
	}
	
	// check the results
	for (i = 0; i < bytesInKey; ++i)
	{
		if (decryptedBuf.Data[i] != (i & 0xFF))
		{
			// bad news
			abort();
		}
	}
	
	if (remData.Data != NULL)
	{
		free(remData.Data);
	}
}

void CSPFullPluginSession::ObtainPrivateKeyFromPublicKey(const CssmKey &PublicKey,
														 CssmKey &PrivateKey)
{
	unimplemented();
}

void CSPFullPluginSession::QueryKeySizeInBits(CSSM_CC_HANDLE ccHandle,
						const Context *context,
						const CssmKey *key,
						CSSM_KEY_SIZE &keySize)
{
	if (context) {
		getKeySize(context->get<CssmKey>(CSSM_ATTRIBUTE_KEY, CSSMERR_CSP_MISSING_ATTR_KEY),
			keySize);
	} else {
		getKeySize(CssmKey::required(key), keySize);
	}
}


//
// Free a key object.
//
void CSPFullPluginSession::FreeKey(const AccessCredentials *AccessCred,
								   CssmKey &key,
								   CSSM_BOOL Delete)
{
	free(key.data());
}


//
// Random number and parameter generation
//
void CSPFullPluginSession::GenerateRandom(CSSM_CC_HANDLE ccHandle,
                                          const Context &context,
                                          CssmData &randomNumber)
{
    init(ccHandle, CSSM_ALGCLASS_RANDOMGEN, context)->final(randomNumber, *this);
}

void CSPFullPluginSession::GenerateAlgorithmParams(CSSM_CC_HANDLE ccHandle,
                                    const Context &context,
                                    uint32 paramBits,
                                    CssmData &param,
                                    uint32 &attrCount,
                                    CSSM_CONTEXT_ATTRIBUTE_PTR &attrs)
{
    Context::Attr *attrList;
    init(ccHandle, CSSM_ALGCLASS_NONE, context)->generate(context, paramBits,
                                                          param, attrCount, attrList);
    attrs = attrList;
}


//
// Login/Logout and token operational maintainance.
// These mean little without support by the actual implementation, but we can help...
// @@@ Should this be in CSP[non-Full]PluginSession?
//
void CSPFullPluginSession::Login(const AccessCredentials &AccessCred,
			const CssmData *LoginName,
			const void *Reserved)
{
	if (Reserved != NULL)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_POINTER);
		
	// default implementation refuses to log in
	//@@@ should hand it to implementation virtual defaulting to this
	CssmError::throwMe(CSSMERR_CSP_INVALID_LOGIN_NAME);
}

void CSPFullPluginSession::Logout()
{
	if (!loggedIn(false))
		CssmError::throwMe(CSSMERR_CSP_NOT_LOGGED_IN);
}

void CSPFullPluginSession::VerifyDevice(const CssmData &DeviceCert)
{
	CssmError::throwMe(CSSMERR_CSP_DEVICE_VERIFY_FAILED);
}

void CSPFullPluginSession::GetOperationalStatistics(CSPOperationalStatistics &statistics)
{
	memset(&statistics, 0, sizeof(statistics));
	statistics.UserAuthenticated = loggedIn();
	//@@@ collect device flags - capability matrix setup?
	//@@@ collect token limitation parameters (static) - capability matrix setup?
	//@@@ collect token statistics (dynamic) - dynamic accounting call-downs?
}


//
// Utterly miscellaneous, rarely used, strange functions
//
void CSPFullPluginSession::RetrieveCounter(CssmData &Counter)
{
	unimplemented();
}

void CSPFullPluginSession::RetrieveUniqueId(CssmData &UniqueID)
{
	unimplemented();
}

void CSPFullPluginSession::GetTimeValue(CSSM_ALGORITHMS TimeAlgorithm, CssmData &TimeData)
{
	unimplemented();
}


//
// ACL retrieval and change operations
//
void CSPFullPluginSession::GetKeyOwner(const CssmKey &Key,
		CSSM_ACL_OWNER_PROTOTYPE &Owner)
{
	unimplemented();
}

void CSPFullPluginSession::ChangeKeyOwner(const AccessCredentials &AccessCred,
		const CssmKey &Key,
		const CSSM_ACL_OWNER_PROTOTYPE &NewOwner)
{
	unimplemented();
}

void CSPFullPluginSession::GetKeyAcl(const CssmKey &Key,
		const CSSM_STRING *SelectionTag,
		uint32 &NumberOfAclInfos,
		CSSM_ACL_ENTRY_INFO_PTR &AclInfos)
{
	unimplemented();
}

void CSPFullPluginSession::ChangeKeyAcl(const AccessCredentials &AccessCred,
		const CSSM_ACL_EDIT &AclEdit,
		const CssmKey &Key)
{
	unimplemented();
}

void CSPFullPluginSession::GetLoginOwner(CSSM_ACL_OWNER_PROTOTYPE &Owner)
{
	unimplemented();
}

void CSPFullPluginSession::ChangeLoginOwner(const AccessCredentials &AccessCred,
		const CSSM_ACL_OWNER_PROTOTYPE &NewOwner)
{
	unimplemented();
}

void CSPFullPluginSession::GetLoginAcl(const CSSM_STRING *SelectionTag,
		uint32 &NumberOfAclInfos,
		CSSM_ACL_ENTRY_INFO_PTR &AclInfos)
{
	unimplemented();
}

void CSPFullPluginSession::ChangeLoginAcl(const AccessCredentials &AccessCred,
		const CSSM_ACL_EDIT &AclEdit)
{
	unimplemented();
}



//
// Passthroughs (by default, unimplemented)
//
void CSPFullPluginSession::PassThrough(CSSM_CC_HANDLE CCHandle,
										const Context &Context,
										uint32 PassThroughId,
										const void *InData,
										void **OutData)
{
	unimplemented();
}


//
// KeyPool -- ReferencedKey management functionality
//
KeyPool::KeyPool()
{
}

KeyPool::~KeyPool()
{
	StLock<Mutex> _(mKeyMapLock);
	// Delete every ReferencedKey in the pool, but be careful to deactivate them first
	// to keep them from calling erase (which would cause deadlock since we already hold mKeyMapLock).
	KeyMap::iterator end = mKeyMap.end();
	for (KeyMap::iterator it = mKeyMap.begin(); it != end; ++it)
	{
		try
		{
			it->second->deactivate();
		}
		catch(...) {}
		delete it->second;
	}
	mKeyMap.clear();
}

void
KeyPool::add(ReferencedKey &referencedKey)
{
	StLock<Mutex> _(mKeyMapLock);
	bool inserted;
    inserted = mKeyMap.insert(KeyMap::value_type(referencedKey.keyReference(), &referencedKey)).second;
	// Since add is only called from the constructor of ReferencedKey we should
	// never add a key that is already in mKeyMap
	assert(inserted);
}

ReferencedKey &
KeyPool::findKey(const CSSM_KEY &key) const
{
	return findKeyReference(ReferencedKey::keyReference(key));
}

ReferencedKey &
KeyPool::findKeyReference(ReferencedKey::KeyReference keyReference) const
{
	StLock<Mutex> _(mKeyMapLock);
	KeyMap::const_iterator it = mKeyMap.find(keyReference);
	if (it == mKeyMap.end())
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);

	return *it->second;
}

void
KeyPool::erase(ReferencedKey &referencedKey)
{
	erase(referencedKey.keyReference());
}

ReferencedKey &
KeyPool::erase(ReferencedKey::KeyReference keyReference)
{
	StLock<Mutex> _(mKeyMapLock);
	KeyMap::iterator it = mKeyMap.find(keyReference);
	if (it == mKeyMap.end())
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);

	ReferencedKey &referencedKey = *it->second;
	mKeyMap.erase(it);
	return referencedKey;
}

// Erase keyReference from mKeyMap, free the ioKey, and delete the ReferencedKey
void
KeyPool::freeKey(Allocator &allocator, CSSM_KEY &ioKey)
{
	delete &erase(ReferencedKey::freeReferenceKey(allocator, ioKey));
}

//
// ReferencedKey class
//
ReferencedKey::ReferencedKey(KeyPool &keyPool) : mKeyPool(&keyPool)
{
	mKeyPool->add(*this);
}

ReferencedKey::~ReferencedKey()
{
	if (isActive())
		mKeyPool->erase(*this);
}

ReferencedKey::KeyReference
ReferencedKey::keyReference()
{
	// @@@ Possibly check isActive() and return an invalid reference if it is not set.
	return reinterpret_cast<ReferencedKey::KeyReference>(this);
}

//
// Making, retrieving and freeing Key references of CssmKeys
//
void
ReferencedKey::makeReferenceKey(Allocator &allocator, KeyReference keyReference, CSSM_KEY &key)
{
	key.KeyHeader.BlobType = CSSM_KEYBLOB_REFERENCE;
	key.KeyHeader.Format = CSSM_KEYBLOB_REF_FORMAT_INTEGER;
	key.KeyData.Length = sizeof(KeyReference);
	key.KeyData.Data = allocator.alloc<uint8>(sizeof(KeyReference));
	uint8 *cp = key.KeyData.Data;
	for (int i = sizeof(KeyReference); --i >= 0;)
	{
		cp[i] = keyReference & 0xff;
		keyReference = keyReference >> 8;
	}
}

ReferencedKey::KeyReference
ReferencedKey::keyReference(const CSSM_KEY &key)
{
	if (key.KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE
		|| key.KeyHeader.Format != CSSM_KEYBLOB_REF_FORMAT_INTEGER
		|| key.KeyData.Length != sizeof(KeyReference)
		|| key.KeyData.Data == NULL)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);

	const uint8 *cp = key.KeyData.Data;
	KeyReference keyReference = 0;
	for (uint32 i = 0; i < sizeof(KeyReference); ++i)
		keyReference = (keyReference << 8) + cp[i];

	return keyReference;
}

ReferencedKey::KeyReference
ReferencedKey::freeReferenceKey(Allocator &allocator, CSSM_KEY &key)
{
	KeyReference aKeyReference = keyReference(key);
	allocator.free(key.KeyData.Data);
	key.KeyData.Data = NULL;
	key.KeyData.Length = 0;
	return aKeyReference;
}
