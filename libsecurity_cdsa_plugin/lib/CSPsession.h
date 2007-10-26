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
// CSPsession.h - Framework for CSP plugin modules
//
#ifndef _H_CSPSESSION
#define _H_CSPSESSION

#include <security_cdsa_plugin/CSPabstractsession.h>
#include <map>


namespace Security {

//
// The CSPPluginSession provides a general bed for CSP plugin session objects.
// Derive from this if you want to write your CSP, effectively, from scratch.
// We still provide a framework for managing local cryptographic contexts and
// (module) logins.
//
class CSPPluginSession : public PluginSession, public CSPAbstractPluginSession {
public:
    CSPPluginSession(CSSM_MODULE_HANDLE theHandle,
                    CssmPlugin &plug,
                    const CSSM_VERSION &version,
                    uint32 subserviceId,
                    CSSM_SERVICE_TYPE subserviceType,
                    CSSM_ATTACH_FLAGS attachFlags,
                    const CSSM_UPCALLS &upcalls)
      : PluginSession(theHandle, plug, version, subserviceId, subserviceType, attachFlags, upcalls) { }

    // methods implemented here that you should not override in a subclass
    void EventNotify(CSSM_CONTEXT_EVENT e,
                     CSSM_CC_HANDLE ccHandle, const Context &context);
    CSSM_MODULE_FUNCS_PTR construct();

public:
    class PluginContext {
    public:
        virtual bool changed(const Context &context);
        virtual ~PluginContext();
    };

public:
    bool loggedIn() const { return mLoggedIn; }
    bool loggedIn(bool li) { bool old = mLoggedIn; mLoggedIn = li; return old; }

    template <class Ctx> Ctx *getContext(CSSM_CC_HANDLE handle)
    { StLock<Mutex> _(contextMapLock); return safe_cast<Ctx *>(contextMap[handle]); }

    void setContext(CSSM_CC_HANDLE handle, PluginContext *ctx)
    { StLock<Mutex> _(contextMapLock); contextMap[handle] = ctx; }

public:
    // context management methods - override as needed
    virtual PluginContext *contextCreate(CSSM_CC_HANDLE handle, const Context &context);
    virtual void contextUpdate(CSSM_CC_HANDLE handle,
                               const Context &context, PluginContext * &ctx);
    virtual void contextDelete(CSSM_CC_HANDLE handle, const Context &context, PluginContext *ctx);

private:
    bool mLoggedIn;
	
    map<CSSM_CC_HANDLE, PluginContext *> contextMap;
	Mutex contextMapLock;
};


//
// On the other hand, for most CSP modules, this subclass of CSPPluginSession provides
// much more convenient embedding facilities. The theory of operation is too complicated
// to explain here; refer to the accompanying documentation.
//
class CSPFullPluginSession : public CSPPluginSession {
public:
    class CSPContext;
    class AlgorithmFactory;

    CSPFullPluginSession(CSSM_MODULE_HANDLE theHandle,
                    CssmPlugin &plug,
                    const CSSM_VERSION &version,
                    uint32 subserviceId,
                    CSSM_SERVICE_TYPE subserviceType,
                    CSSM_ATTACH_FLAGS attachFlags,
                    const CSSM_UPCALLS &upcalls)
    : CSPPluginSession(theHandle, plug, version,
                             subserviceId, subserviceType, attachFlags, upcalls) { }

    // final context preparation (called by secondary transition layer)
    CSPContext *init(CSSM_CC_HANDLE ccHandle, CSSM_CONTEXT_TYPE type,
                     const Context &context, bool encoding = true);

    // verify proper state on continuation (update/final) calls
    CSPContext *getStagedContext(CSSM_CC_HANDLE ccHandle,
                                 CSSM_CONTEXT_TYPE type, bool encoding = true);

    static const uint32 CSSM_ALGCLASS_CRYPT = 1001;	// internally added to CONTEXT_TYPE

protected:
	// validate operation type against context class
    void checkOperation(CSSM_CONTEXT_TYPE ctxType, CSSM_CONTEXT_TYPE opType);
	
protected:
	//
	// The Writer class encapsulates staged-output destinations with optional overflow
	//
	class Writer {
	public:
		Writer(CssmData *v, uint32 n, CssmData *rem = NULL);
	
		// can this buffer be extended?
		bool isExtensible() const
		{ return !*vec || remData && !*remData; }
	
		// increase size if necessary (and possible)
		void allocate(size_t needed, Allocator &alloc);
	
		// straight-forward buffer writing
		void put(void *addr, size_t size);
	
		// locate-mode output (deliver buffer mode)
		void nextBlock(void * &p, size_t &sz);
		void use(size_t sz);
	
		// wrap up and return total number of bytes written
		size_t close();
	
	private:
		CssmData *vec;						// current buffer descriptor (the one in use)
        CssmData *firstVec;					// first buffer descriptor
		CssmData *lastVec;					// last buffer descriptor (NOT one past it)
		CssmData *remData;					// overflow buffer, if any
	
		void *currentBuffer;				// next free byte in vec
		size_t currentSize;					// free bytes in vec
	
		size_t written;						// bytes written
		
		void useData(CssmData *data)
		{ currentBuffer = data->data(); currentSize = data->length(); }
	};
	
public:
	// internal utilities (used by our own subclasses)
	static CssmData makeBuffer(size_t size, Allocator &alloc);
	static size_t totalBufferSize(const CssmData *data, uint32 count);
    void setKey(CssmKey &key,
                const Context &context, CSSM_KEYCLASS keyClass,
                CSSM_KEYATTR_FLAGS attrs, CSSM_KEYUSE use);

public:
	//
	// All contexts from CSPFullPluginSession's subclasses must derive from CSPContext.
	// CSPFullPluginSession reformulates CSSM operations in terms of virtual methods of
	// the context class.
	//
    class CSPContext : public PluginContext {
        friend class CSPFullPluginSession;
    public:
        CSSM_CONTEXT_TYPE type() const { return mType; }
        bool encoding() const { return mDirection; }
        
        // init() is called for all algorithms
        virtual void init(const Context &context, bool encoding = true);

        // the following methods will be called for some but not all algorithms
        virtual void update(const CssmData &data);	// all block-input algorithms
        virtual void update(void *inp, size_t &inSize, void *outp, size_t &outSize); // cryption algs
        virtual void final(CssmData &out);		// output-data producing algorithms
        virtual void final(const CssmData &in);	// verifying algorithms
        virtual void generate(const Context &context, CssmKey &pubKey, CssmKey &privKey);
        virtual void generate(const Context &context, uint32,
                              CssmData &params, uint32 &attrCount, Context::Attr * &attrs);
		virtual CSPContext *clone(Allocator &);	// clone internal state
		virtual void setDigestAlgorithm(CSSM_ALGORITHMS digestAlg);
		
        virtual size_t inputSize(size_t outSize);	// input for given output size
        virtual size_t outputSize(bool final = false, size_t inSize = 0); // output for given input size
        virtual void minimumProgress(size_t &in, size_t &out); // minimum progress chunks

    protected:
        // convenience forms of the above
        void update(const CssmData *in, uint32 inCount, Writer &writer);
        void final(CssmData &out, Allocator &alloc);
        void final(Writer &writer, Allocator &alloc);

        void update(const CssmData *in, uint32 inCount)
        { for (uint32 n = 0; n < inCount; n++) update(in[n]); }

        void checkOperation(CSSM_CONTEXT_TYPE type);
        void checkOperation(CSSM_CONTEXT_TYPE type, bool encode);

        CSSM_CONTEXT_TYPE mType;		// CSSM context type
        bool mDirection;			// operation direction (true if irrelevant)
    };

protected:
    virtual void setupContext(CSPContext * &ctx, const Context &context, bool encoding) = 0;
	
	virtual void getKeySize(const CssmKey &key, CSSM_KEY_SIZE &size);
    
public:
    // an algorithm factory. This is an optional feature
    class AlgorithmFactory {
    public:
		virtual ~AlgorithmFactory();
		
        // set ctx and return true if you can handle this
        virtual bool setup(CSPContext * &ctx, const Context &context) = 0;
    };

public:
    void EncryptData(CSSM_CC_HANDLE CCHandle,
                     const Context &Context,
                     const CssmData ClearBufs[],
                     uint32 ClearBufCount,
                     CssmData CipherBufs[],
                     uint32 CipherBufCount,
                     CSSM_SIZE &bytesEncrypted,
                     CssmData &RemData,
                     CSSM_PRIVILEGE Privilege);
    void EncryptDataInit(CSSM_CC_HANDLE CCHandle,
                         const Context &Context,
                         CSSM_PRIVILEGE Privilege);
    void EncryptDataUpdate(CSSM_CC_HANDLE CCHandle,
                           const CssmData ClearBufs[],
                           uint32 ClearBufCount,
                           CssmData CipherBufs[],
                           uint32 CipherBufCount,
                           CSSM_SIZE &bytesEncrypted);
    void EncryptDataFinal(CSSM_CC_HANDLE CCHandle,
                          CssmData &RemData);

    void DecryptData(CSSM_CC_HANDLE CCHandle,
                     const Context &Context,
                     const CssmData CipherBufs[],
                     uint32 CipherBufCount,
                     CssmData ClearBufs[],
                     uint32 ClearBufCount,
                     CSSM_SIZE &bytesDecrypted,
                     CssmData &RemData,
                     CSSM_PRIVILEGE Privilege);
    void DecryptDataInit(CSSM_CC_HANDLE CCHandle,
                         const Context &Context,
                         CSSM_PRIVILEGE Privilege);
    void DecryptDataUpdate(CSSM_CC_HANDLE CCHandle,
                           const CssmData CipherBufs[],
                           uint32 CipherBufCount,
                           CssmData ClearBufs[],
                           uint32 ClearBufCount,
                           CSSM_SIZE &bytesDecrypted);
    void DecryptDataFinal(CSSM_CC_HANDLE CCHandle,
                          CssmData &RemData);
               
	void QuerySize(CSSM_CC_HANDLE CCHandle,
				  const Context &Context,
				  CSSM_BOOL Encrypt,
				  uint32 QuerySizeCount,
				  QuerySizeData *DataBlock);

	void WrapKey(CSSM_CC_HANDLE CCHandle,
				const Context &Context,
				const AccessCredentials &AccessCred,
				const CssmKey &Key,
				const CssmData *DescriptiveData,
				CssmKey &WrappedKey,
				CSSM_PRIVILEGE Privilege);
	void UnwrapKey(CSSM_CC_HANDLE CCHandle,
				const Context &Context,
				const CssmKey *PublicKey,
				const CssmKey &WrappedKey,
				uint32 KeyUsage,
				uint32 KeyAttr,
				const CssmData *KeyLabel,
				const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
				CssmKey &UnwrappedKey,
				CssmData &DescriptiveData,
				CSSM_PRIVILEGE Privilege);
	void DeriveKey(CSSM_CC_HANDLE CCHandle,
				const Context &Context,
				CssmData &Param,
				uint32 KeyUsage,
				uint32 KeyAttr,
				const CssmData *KeyLabel,
				const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
				CssmKey &DerivedKey);

    void GenerateMac(CSSM_CC_HANDLE CCHandle,
                     const Context &Context,
                     const CssmData DataBufs[],
                     uint32 DataBufCount,
                     CssmData &Mac);
    void GenerateMacInit(CSSM_CC_HANDLE CCHandle,
                                 const Context &Context);
    void GenerateMacUpdate(CSSM_CC_HANDLE CCHandle,
                                   const CssmData DataBufs[],
                                   uint32 DataBufCount);
    void GenerateMacFinal(CSSM_CC_HANDLE CCHandle,
                                  CssmData &Mac);
    
    void VerifyMac(CSSM_CC_HANDLE CCHandle,
                   const Context &Context,
                   const CssmData DataBufs[],
                   uint32 DataBufCount,
                   const CssmData &Mac);
    virtual void VerifyMacInit(CSSM_CC_HANDLE CCHandle,
                               const Context &Context);
    virtual void VerifyMacUpdate(CSSM_CC_HANDLE CCHandle,
                                 const CssmData DataBufs[],
                                 uint32 DataBufCount);
    virtual void VerifyMacFinal(CSSM_CC_HANDLE CCHandle,
                                const CssmData &Mac);

    void SignData(CSSM_CC_HANDLE CCHandle,
                  const Context &Context,
                  const CssmData DataBufs[],
                  uint32 DataBufCount,
                  CSSM_ALGORITHMS DigestAlgorithm,
                  CssmData &Signature);
    void SignDataInit(CSSM_CC_HANDLE CCHandle,
                              const Context &Context);
    void SignDataUpdate(CSSM_CC_HANDLE CCHandle,
                                const CssmData DataBufs[],
                                uint32 DataBufCount);
    void SignDataFinal(CSSM_CC_HANDLE CCHandle,
                               CssmData &Signature);

    void VerifyData(CSSM_CC_HANDLE CCHandle,
                    const Context &Context,
                    const CssmData DataBufs[],
                    uint32 DataBufCount,
                    CSSM_ALGORITHMS DigestAlgorithm,
                    const CssmData &Signature);
    virtual void VerifyDataInit(CSSM_CC_HANDLE CCHandle,
                                const Context &Context);
    virtual void VerifyDataUpdate(CSSM_CC_HANDLE CCHandle,
                                  const CssmData DataBufs[],
                                  uint32 DataBufCount);
    virtual void VerifyDataFinal(CSSM_CC_HANDLE CCHandle,
                                 const CssmData &Signature);

    void DigestData(CSSM_CC_HANDLE CCHandle,
                    const Context &Context,
                    const CssmData DataBufs[],
                    uint32 DataBufCount,
                    CssmData &Digest);
    void DigestDataInit(CSSM_CC_HANDLE CCHandle,
                                const Context &Context);
    void DigestDataUpdate(CSSM_CC_HANDLE CCHandle,
                                  const CssmData DataBufs[],
                                  uint32 DataBufCount);
    void DigestDataFinal(CSSM_CC_HANDLE CCHandle,
                                 CssmData &Digest);
    void DigestDataClone(CSSM_CC_HANDLE CCHandle,
						CSSM_CC_HANDLE ClonedCCHandle);

    void GenerateKey(CSSM_CC_HANDLE CCHandle,
                             const Context &Context,
                             uint32 KeyUsage,
                             uint32 KeyAttr,
                             const CssmData *KeyLabel,
                             const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
                             CssmKey &Key,
                             CSSM_PRIVILEGE Privilege);
    void GenerateKeyPair(CSSM_CC_HANDLE CCHandle,
                                 const Context &Context,
                                 uint32 PublicKeyUsage,
                                 uint32 PublicKeyAttr,
                                 const CssmData *PublicKeyLabel,
                                 CssmKey &PublicKey,
                                 uint32 PrivateKeyUsage,
                                 uint32 PrivateKeyAttr,
                                 const CssmData *PrivateKeyLabel,
                                 const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
                                 CssmKey &PrivateKey,
                                 CSSM_PRIVILEGE Privilege);
        
	void ObtainPrivateKeyFromPublicKey(const CssmKey &PublicKey,
									   CssmKey &PrivateKey);
	void QueryKeySizeInBits(CSSM_CC_HANDLE CCHandle,
							const Context *Context,
							const CssmKey *Key,
							CSSM_KEY_SIZE &KeySize);
                              
	void FreeKey(const AccessCredentials *AccessCred,
				 CssmKey &KeyPtr,
				 CSSM_BOOL Delete);

    void GenerateRandom(CSSM_CC_HANDLE CCHandle,
                                const Context &Context,
                                CssmData &RandomNumber);
    void GenerateAlgorithmParams(CSSM_CC_HANDLE CCHandle,
                                         const Context &Context,
                                         uint32 ParamBits,
                                         CssmData &Param,
                                         uint32 &NumberOfUpdatedAttibutes,
                                         CSSM_CONTEXT_ATTRIBUTE_PTR &UpdatedAttributes);
										 
	void Login(const AccessCredentials &AccessCred,
			   const CssmData *LoginName,
			   const void *Reserved);
	void Logout();
	void VerifyDevice(const CssmData &DeviceCert);
	void GetOperationalStatistics(CSPOperationalStatistics &Statistics);
	
	void RetrieveCounter(CssmData &Counter);
	void RetrieveUniqueId(CssmData &UniqueID);
	void GetTimeValue(CSSM_ALGORITHMS TimeAlgorithm, CssmData &TimeData);
	
	void GetKeyOwner(const CssmKey &Key,
			CSSM_ACL_OWNER_PROTOTYPE &Owner);
	void ChangeKeyOwner(const AccessCredentials &AccessCred,
			const CssmKey &Key,
			const CSSM_ACL_OWNER_PROTOTYPE &NewOwner);
	void GetKeyAcl(const CssmKey &Key,
			const CSSM_STRING *SelectionTag,
			uint32 &NumberOfAclInfos,
			CSSM_ACL_ENTRY_INFO_PTR &AclInfos);
	void ChangeKeyAcl(const AccessCredentials &AccessCred,
			const CSSM_ACL_EDIT &AclEdit,
			const CssmKey &Key);
			
	void GetLoginOwner(CSSM_ACL_OWNER_PROTOTYPE &Owner);
	void ChangeLoginOwner(const AccessCredentials &AccessCred,
			const CSSM_ACL_OWNER_PROTOTYPE &NewOwner);
	void GetLoginAcl(const CSSM_STRING *SelectionTag,
			uint32 &NumberOfAclInfos,
			CSSM_ACL_ENTRY_INFO_PTR &AclInfos);
	void ChangeLoginAcl(const AccessCredentials &AccessCred,
			const CSSM_ACL_EDIT &AclEdit);
       
	void PassThrough(CSSM_CC_HANDLE CCHandle,
               const Context &Context,
               uint32 PassThroughId,
               const void *InData,
               void **OutData);
};


//
// Classes for dealing with reference keys.
//

// Forward declaration.
class KeyPool;

//
// A ReferencedKey -- The private (to the CSP) part of a Reference Key.
//
class ReferencedKey
{
	friend class KeyPool; // So it can call deactivate()
public:
	// What we use to reference a ReferencedKey.
	typedef CSSM_INTPTR KeyReference;
	ReferencedKey(KeyPool &session); // Calls KeyPool::add()
	virtual ~ReferencedKey(); // Calls KeyPool::erase()

	KeyReference keyReference();
	bool isActive() { return mKeyPool != NULL; }

	template <class SubPool>
	SubPool &keyPool() { assert(mKeyPool); return safer_cast<SubPool &>(*mKeyPool); }
public:
	// Making, retrieving and freeing CSSM_KEYBLOB_REF_FORMAT_INTEGER CSSM_KEY type reference keys
	// NOTE: that none of these functions affect mKeyMap.
	static void makeReferenceKey(Allocator &allocator, KeyReference keyReference, CSSM_KEY &ioKey);
	static KeyReference keyReference(const CSSM_KEY &key);
	static KeyReference freeReferenceKey(Allocator &allocator, CSSM_KEY &ioKey);

private:
	void deactivate() { mKeyPool = NULL; }

	// Will be NULL iff this key is not active
	KeyPool *mKeyPool;
};


//
// KeyPool -- a mixin class to manage a pool of ReferencedKeys
//
class KeyPool
{
public:
	friend class ReferencedKey; // So it can call add() and erase()
public:
	KeyPool();
	virtual ~KeyPool();

	// Type safe ReferencedKey subclass lookup
	template <class Subclass>
	Subclass &find(const CSSM_KEY &key) const;

	// Free the ioKey, erase keyReference from mKeyMap, and delete the ReferencedKey
	void freeKey(Allocator &allocator, CSSM_KEY &key);

protected:
	// Called by the constructor of ReferencedKey -- add referencedKey to mKeyMap
 	void add(ReferencedKey &referencedKey);

	ReferencedKey &findKey(const CSSM_KEY &key) const;
	ReferencedKey &findKeyReference(ReferencedKey::KeyReference keyReference) const;

	// Called by the destructor of ReferencedKey -- erase keyReference from mKeyMap
 	void erase(ReferencedKey &referencedKey);

	// Erase keyReference from mKeyMap, and return it (for deletion)
 	ReferencedKey &erase(ReferencedKey::KeyReference keyReference);

protected:
	typedef map<ReferencedKey::KeyReference, ReferencedKey *> KeyMap;
	KeyMap mKeyMap;
	mutable Mutex mKeyMapLock;
};

// Implementation of type safe ReferencedKey subclass lookup.
template <class Subclass>
Subclass &
KeyPool::find(const CSSM_KEY &key) const
{
    Subclass *sub;
    if (!(sub = dynamic_cast<Subclass *>(&findKey(key))))
        CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
    return *sub;
}

} // end namespace Security

#endif //_H_CSPSESSION
