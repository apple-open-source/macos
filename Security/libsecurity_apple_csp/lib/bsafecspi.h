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

#ifdef	BSAFE_CSP_ENABLE


//
// bsafecspi - implementation layer for C++ BSafe 4 interface
//
#ifndef _H_BSAFECSPI
#define _H_BSAFECSPI

#include <security_cdsa_plugin/CSPsession.h>
#include "bsobjects.h"
#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include <aglobal.h>
#include <bsafe.h>

//
// The BSafe class is more of a namespace than anything else.
// Just think of it as the "static binder" for BSafe's objects.
// Note that we keep a global, static allocator. We have to; BSafe
// doesn't have any state management at that level.
//
class BSafe {
    class BSafeContext; friend class BSafeContext;
	class BSafeFactory; friend class BSafeFactory;

public:
    static void setNormAllocator(Allocator *alloc)
    { assert(!normAllocator); normAllocator = alloc; }
    static void setPrivAllocator(Allocator *alloc)
    { assert(!privAllocator); privAllocator = alloc; }
	
	static bool setup(
		AppleCSPSession &session,
		CSPFullPluginSession::CSPContext * &cspCtx, 
		const Context &context);

private:
    // BSafe's memory allocators
    static Allocator *normAllocator;
    static Allocator *privAllocator;
    friend POINTER T_malloc(unsigned int);
    friend void T_free(POINTER);
    friend POINTER T_realloc(POINTER, unsigned int);
    
    static const B_ALGORITHM_METHOD * const bsChooser[];

private:
	// BSafe-specific BinaryKey class.
	class BSafeBinaryKey : public BinaryKey {
	
	public:
		BSafeBinaryKey(
			bool 			isPub,
			uint32			alg);	// CSSM_ALGID_{RSA,DSA}
		~BSafeBinaryKey();
		void generateKeyBlob(
			Allocator 		&allocator,
			CssmData			&blob,
			CSSM_KEYBLOB_FORMAT	&format,
			AppleCSPSession		&session,
			const CssmKey		*paramKey,		/* optional, unused here */
			CSSM_KEYATTR_FLAGS 	&attrFlags);	/* IN/OUT */
			
		bool			isPublic() 	{ return mIsPublic; }
		uint32			alg()		{ return mAlg; }
		B_KEY_OBJ		bsKey()		{ return mBsKey; }
		
	private:
		bool			mIsPublic;
		uint32			mAlg;		// CSSM_ALGID_{RSA,DSA}
		B_KEY_OBJ		mBsKey;
	};
	
private:
	//
	// The BSafeContext class is the parent of all BSafe-used CSPContext objects.
	// It implements the CSPContext operation functions (init, update, ...) in terms
	// of pointer-to-member fields set by its subclasses. This may not be pretty, but
	// it avoids every subclass having to re-implement all CSPContext operations.
	// Beyond that, we implement a raftload of utility methods for our children.
	//
    class BSafeContext : public AppleCSPContext {
        friend class BSafe;
    public:
        BSafeContext(AppleCSPSession &session);
        virtual ~BSafeContext();
        
		// called by CSPFullPluginSession
        void init(const Context &context, bool encoding = true);
        void update(const CssmData &data);
        void update(void *inp, size_t &inSize, void *outp, size_t &outSize);
        void final(CssmData &out);
        void final(const CssmData &in);
        size_t outputSize(bool final, size_t inSize);

    protected:
		// install a BSafe algorithm into bsAlgorithm
        void setAlgorithm(B_INFO_TYPE bAlgType, const void *info = NULL);
		
		// safely create bsKey 
		void createBsKey();
		
		// set bsKey. The different versions are equivalent
        void setKeyAtom(B_INFO_TYPE bKeyInfo, const void *info);
        void setKeyFromItem(B_INFO_TYPE bKeyInfo, const BSafeItem &item)
			{ setKeyAtom(bKeyInfo, &item); }
        void setKeyFromCssmKey(B_INFO_TYPE bKeyInfo, const CssmKey &key)
			{ BSafeItem item(key.KeyData); setKeyAtom(bKeyInfo, &item); }
        void setKeyFromCssmData(B_INFO_TYPE bKeyInfo, const CssmData &keyData)
			{ BSafeItem item(keyData); setKeyAtom(bKeyInfo, &item); }
        void setKeyFromContext(const Context &context, bool required = true);
	
		void setRefKey(CssmKey &key);
		void setRsaOutSize(bool isPubKey);
		
		// create mRandom to be a suitable random-generator BSafe object (if it isn't yet)
        void setRandom();

		// trackUpdate is called during crypto-output. Hook it to keep track of data flow
        virtual void trackUpdate(size_t in, size_t out);

		// destroy bsAlgorithm and bsKey so we can start over making them
        void reset();

		// clear key state
		void destroyBsKey();
		
		// determine if we can reuse the current bsAlgorithm
        bool reusing(bool encode = true)
        {
            if (initialized && !opStarted && 
				(encode == encoding)) return true;
            encoding = encode;
            return false;
        }

    public:
		//
		// These pointers-to-member are called by the BSafeContext operations
		// (update, final). They must be set by a subclasses's init() method.
		// Not all members are used by all types of operations - check the 
		// source when in doubt.
		//
        int (*inUpdate)(B_ALGORITHM_OBJ, POINTER, unsigned int, A_SURRENDER_CTX *);
        int (*inOutUpdate)(B_ALGORITHM_OBJ, POINTER, unsigned int *, unsigned int,
                            POINTER, unsigned int, B_ALGORITHM_OBJ, A_SURRENDER_CTX *);
        int (*inFinal)(B_ALGORITHM_OBJ, POINTER, unsigned int, A_SURRENDER_CTX *);
        int (*inFinalR)(B_ALGORITHM_OBJ, POINTER, unsigned int,
                        B_ALGORITHM_OBJ, A_SURRENDER_CTX *);
        int (*outFinalR)(B_ALGORITHM_OBJ, POINTER, unsigned int *, unsigned int,
                        B_ALGORITHM_OBJ, A_SURRENDER_CTX *);
        int (*outFinal)(B_ALGORITHM_OBJ, POINTER, unsigned int *, unsigned int,
                        A_SURRENDER_CTX *);
        
    protected:

        // un-consted bsChooser for BSafe's consumption. BSafe's Bad
        static B_ALGORITHM_METHOD **chooser()
        { return const_cast<B_ALGORITHM_METHOD **>(bsChooser); }

		// a placeholder for a surrender context. Not currently used
		// @@@ should perhaps test for pthread cancel? --> thread abstraction
        static A_SURRENDER_CTX * const bsSurrender;

    protected:
        B_ALGORITHM_OBJ bsAlgorithm; // BSafe algorithm object or NULL
        B_ALGORITHM_OBJ bsRandom;	// PRNG algorithm
        bool encoding;				// encoding direction
        bool initialized;			// method init() has completed
		bool opStarted;				// method update() has been called
									// generally means that we can't reuse
									// the current bsAlgorithm
		//
		// We have a binKey only if the caller passed in a reference 
		// key. In that case we avoid deleting bsKey - which is a copy
		// of binKey.bsKey - because a BinaryKey is persistent
		// relative to this context. 
		//
		BSafeBinaryKey	*bsBinKey;
        B_KEY_OBJ 		bsKey;		// BSafe key object or NULL
        
        size_t 			mOutSize; 	// simple output size, if applicable
    };	/* BSafeContext */

	// contexts for BSafe digest operations
    class DigestContext : public BSafeContext {
    public:
		// do all work in constructor. We have no directions; thus default init() works fine
        DigestContext(
			AppleCSPSession &session,
			const Context &, 
			B_INFO_TYPE bAlgInfo, 
			size_t sz);
    };

	// common context features for BSafe cipher operations (both symmetric and asymmetric)
    class CipherContext : public BSafeContext {
	public:
		CipherContext(
			AppleCSPSession &session) :
				BSafeContext(session), 
				pending(0) {}
			
	protected:
        size_t pending;				// bytes not eaten still pending (staged only)
    public:
        void cipherInit();			// common init code (must be called from init())
    };            

	// contexts for block cipher operations using symmetric algorithms
    class BlockCipherContext : public CipherContext {
        size_t blockSize;
		uint32 cssmAlg;
		uint32 cssmMode;
		bool padEnable;
    public:
        BlockCipherContext(
			AppleCSPSession &session,
			const Context &, 
			size_t sz) :
				CipherContext(session),
			 	blockSize(sz) { }
        void init(const Context &context, bool encrypting);
        size_t inputSize(size_t outSize);
        size_t outputSize(bool final, size_t inSize);
        void minimumProgress(size_t &in, size_t &out);
        void trackUpdate(size_t in, size_t out);
	private:
		// special case for RC4
		void RC4init(const Context &context);
    };

	// context for generating public/private key pairs
    class BSafeKeyPairGenContext : public BSafeContext,
					 private AppleKeyPairGenContext  {
	public:
		BSafeKeyPairGenContext(
			AppleCSPSession &session,
			const Context &) :
				BSafeContext(session) {}

		// generate alg params, not handled by PublicKeyGenerateContext
		// For DSA only. 
        void generate(
			const Context 			&context, 
			uint32 					bitSize, 
			CssmData 				&params,
			uint32 					&attrCount, 
			Context::Attr * 		&attrs);
			
		// this one is specified in CSPFullPluginSession
		void generate(
			const Context 	&context, 
			CssmKey 		&pubKey, 
			CssmKey 		&privKey);
			
		// this one in AppleKeyPairGenContext
		void generate(
			const Context 	&context,
			BinaryKey		&pubBinKey,	
			BinaryKey		&privBinKey,
			uint32			&keySize);
	
	private:
		void setupAlgorithm(
			const Context 	&context,
			uint32			&keySize);
			
    };	/* BSafeKeyPairGenContext */

	// public key cipher operations
    class PublicKeyCipherContext : public CipherContext {
    public:
		PublicKeyCipherContext(
			AppleCSPSession &session,
			const Context &) :
				CipherContext(session) { }
        void init(const Context &context, bool encrypting);
        size_t inputSize(size_t outSize);	// unlimited
    };

	// contexts for BSafe signing/verifying operations
    class SigningContext : public BSafeContext {
        B_INFO_TYPE algorithm;
    public:
        SigningContext(
			AppleCSPSession &session,
			const Context &,
			B_INFO_TYPE bAlg, 
			size_t sz) : 
				BSafeContext(session),
				algorithm(bAlg) { mOutSize = sz; }
        void init(const Context &context, bool signing);
    };

	// contexts for BSafe MAC generation and verification
    class MacContext : public BSafeContext {
        B_INFO_TYPE algorithm;
    public:
        MacContext(
			AppleCSPSession &session,
			const Context &,
            B_INFO_TYPE bAlg, 
			size_t sz) : 
				BSafeContext(session),
				algorithm(bAlg) { mOutSize = sz; }
        void init(const Context &context, bool signing);
        void final(const CssmData &in);
    };
	
	// contexts for BSafe's random number generation
	class RandomContext : public BSafeContext {
		B_INFO_TYPE algorithm;
	public:
		RandomContext(
			AppleCSPSession &session,
			const Context &, 
			B_INFO_TYPE alg) :
				BSafeContext(session),
				algorithm(alg) { }
		void init(const Context &context, bool);
		void final(CssmData &data);
	};

	// symmetric key generation context
	class SymmetricKeyGenContext : public BSafeContext,
		private AppleSymmKeyGenContext {
    public:
		SymmetricKeyGenContext(
			AppleCSPSession &session,
			const Context 	&ctx,
			uint32			minSizeInBits,
			uint32			maxSizeInBits,
			bool			mustBeByteSized) :
				BSafeContext(session),
				AppleSymmKeyGenContext(
					minSizeInBits,
					maxSizeInBits,
					mustBeByteSized) { }
					
		void generate(
			const Context 	&context, 
			CssmKey 		&symKey, 
			CssmKey 		&dummyKey);

    };

public:
	/*
	 * Stateless, private function to map a CSSM alg and pub/priv state
	 * to B_INFO_TYPE and format. Returns true on success, false on
	 * "I don't understand this algorithm". 
	 */
	static bool bsafeAlgToInfoType(
		CSSM_ALGORITHMS		alg,
		bool				isPublic,
		B_INFO_TYPE			&infoType,	// RETURNED
		CSSM_KEYBLOB_FORMAT	&format);	// RETURNED

	/* check result of a BSafe call and throw on error */
	static void check(int status, bool isKeyOp = false);
		
	/* moved here from BSafeContext - now works on any key */
	template <class KI_Type>
	static KI_Type *getKey(B_KEY_OBJ bKey, B_INFO_TYPE type)
	{ 
		POINTER p; 
		check(B_GetKeyInfo(&p, bKey, type), true); 
		return reinterpret_cast<KI_Type *>(p); 
	}


    //
    // The context generation table - see algmaker.cpp.
    //
public:
	// Base class for Maker classes
	class MakerBase {
	public:
		virtual ~MakerBase() { }
		virtual BSafeContext *make(
			AppleCSPSession &session,
			const Context &context) const = 0;
	};

	// One entry in Maker table
	struct MakerTable {
		CSSM_ALGORITHMS 	algorithmId;
		CSSM_CONTEXT_TYPE	algClass;
		const MakerBase 	*maker;
		~MakerTable() { delete maker; }
	};
	
private:
	static bug_const MakerTable algorithms[];
	static const unsigned int algorithmCount;
	
	/*
	 * CSPKeyInfoProvider for BSafe keys
	 */
	class BSafeKeyInfoProvider : public CSPKeyInfoProvider 
	{
private:
		BSafeKeyInfoProvider(
			const CssmKey		&cssmKey,
			AppleCSPSession		&session);
	public:
		static CSPKeyInfoProvider *provider(
		const CssmKey 			&cssmKey,
		AppleCSPSession			&session);
		~BSafeKeyInfoProvider() { }
		void CssmKeyToBinary(
			CssmKey				*paramKey,	// optional
			CSSM_KEYATTR_FLAGS	&attrFlags,	// IN/OUT
			BinaryKey			**binKey);	// RETURNED
		void QueryKeySizeInBits(
			CSSM_KEY_SIZE		&keySize);	// RETURNED
	};

}; /* BSAFE namespace */

/*
 * BSAFE Key Info types.
 */
#define BLOB_IS_PUB_KEY_INFO	0

#if		BLOB_IS_PUB_KEY_INFO

/* X beta values */
#define RSA_PUB_KEYINFO_TYPE		KI_RSAPublicBER
#define RSA_PRIV_KEYINFO_TYPE		KI_PKCS_RSAPrivateBER
#define DSA_PUB_KEYINFO_TYPE		KI_DSAPublicBER
#define DSA_PRIV_KEYINFO_TYPE		KI_DSAPrivateBER

#else	/* BLOB_IS_PUB_KEY_INFO */

#define RSA_PUB_KEYINFO_TYPE		KI_RSAPublic
#define RSA_PRIV_KEYINFO_TYPE		KI_PKCS_RSAPrivateBER
#define DSA_PUB_KEYINFO_TYPE		KI_DSAPublicBER
#define DSA_PRIV_KEYINFO_TYPE		KI_DSAPrivateBER

#endif

#endif //_H_BSAFECSP
#endif	/* BSAFE_CSP_ENABLE */
