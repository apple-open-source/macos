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
// AppleCSPSession.h - top-level session class
//
#ifndef _APPLE_CSP_SESSION_H_
#define _APPLE_CSP_SESSION_H_

#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_plugin/pluginsession.h>
#include <security_cdsa_plugin/CSPsession.h>
#include <security_utilities/threading.h>
#include "BinaryKey.h"
#include "AppleCSPUtils.h"

class CSPKeyInfoProvider;

/* avoid unnecessary includes.... */
class AppleCSPPlugin;
#ifdef	BSAFE_CSP_ENABLE
class BSafeFactory;
#endif
#ifdef	CRYPTKIT_CSP_ENABLE
class CryptKitFactory;
#endif
class MiscAlgFactory;
#ifdef	ASC_CSP_ENABLE
class AscAlgFactory;
#endif
class RSA_DSA_Factory;
class DH_Factory;

/* one per attach/detach */
class AppleCSPSession : public CSPFullPluginSession {
public:
	
	AppleCSPSession(
		CSSM_MODULE_HANDLE 	handle,
		AppleCSPPlugin 		&plug,
		const CSSM_VERSION 	&Version,
		uint32 				SubserviceID,
		CSSM_SERVICE_TYPE 	SubServiceType,
		CSSM_ATTACH_FLAGS 	AttachFlags,
		const CSSM_UPCALLS 	&upcalls);

	~AppleCSPSession();
	
	CSPContext *contextCreate(
		CSSM_CC_HANDLE 		handle, 
		const Context 		&context);
	void setupContext(
		CSPContext * 		&cspCtx, 
		const Context 		&context, 
		bool 				encoding);

	// Functions declared in CSPFullPluginSession which we override.
	
	// Free a key. If this is a reference key
	// we generated, remove it from refKeyMap. 
	void FreeKey(const AccessCredentials *AccessCred,
		CssmKey &KeyPtr,
		CSSM_BOOL Delete);
	
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
	void WrapKey(CSSM_CC_HANDLE CCHandle,
        const Context &Context,
        const AccessCredentials &AccessCred,
        const CssmKey &Key,
        const CssmData *DescriptiveData,
        CssmKey &WrappedKey,
        CSSM_PRIVILEGE Privilege);
 	void DeriveKey(CSSM_CC_HANDLE CCHandle,
		const Context &Context,
		CssmData &Param,
		uint32 KeyUsage,
		uint32 KeyAttr,
		const CssmData *KeyLabel,
		const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
		CssmKey &DerivedKey);
	void PassThrough(CSSM_CC_HANDLE CCHandle,
		const Context &Context,
		uint32 PassThroughId,
		const void *InData,
		void **OutData);
	void getKeySize(const CssmKey &key, 
		CSSM_KEY_SIZE &size);

	// add a BinaryKey to our refKeyMap. Sets up cssmKey
	// as appropriate.
	void addRefKey(
		BinaryKey			&binKey,
		CssmKey				&cssmKey);
		
	// Given a CssmKey in reference form, obtain the associated
	// BinaryKey. 
	BinaryKey &lookupRefKey(
		const CssmKey		&cssmKey);

	// CSP's RNG. This redirects to Yarrow.
	void					getRandomBytes(size_t length, uint8 *cp);
	void					addEntropy(size_t length, const uint8 *cp);  
 
	Allocator 			&normAlloc()  { return normAllocator; }	
    Allocator 			&privAlloc()  { return privAllocator; }
		
	#ifdef	BSAFE_CSP_ENABLE
	BSafeFactory 			&bSafe4Factory;
	#endif
	#ifdef	CRYPTKIT_CSP_ENABLE
	CryptKitFactory			&cryptKitFactory;
	#endif
	MiscAlgFactory			&miscAlgFactory;
	#ifdef	ASC_CSP_ENABLE
	AscAlgFactory			&ascAlgFactory;
	#endif
	RSA_DSA_Factory			&rsaDsaAlgFactory;
	DH_Factory				&dhAlgFactory;
	
private:
	// storage of binary keys (which apps know as reference keys)
	typedef std::map<KeyRef, const BinaryKey *> keyMap;
	keyMap					refKeyMap;
	Mutex					refKeyMapLock;
    Allocator 			&normAllocator;	
    Allocator 			&privAllocator;	
	
	BinaryKey 				*lookupKeyRef(KeyRef keyRef);
	void 					DeriveKey_PBKDF2(
								const Context &Context,
								const CssmData &Param,
								CSSM_DATA *keyData);
	
	void					DeriveKey_PKCS5_V1_5(
								const Context &context,
								CSSM_ALGORITHMS algId,
								const CssmData &Param,
								CSSM_DATA *keyData);	

	void					DeriveKey_OpenSSH1(
								const Context &context,
								CSSM_ALGORITHMS algId,
								const CssmData &Param,
								CSSM_DATA *keyData);	

	/* CMS wrap/unwrap, called out from standard wrap/unwrap */
	void WrapKeyCms(
		CSSM_CC_HANDLE CCHandle,
		const Context &Context,
		const AccessCredentials &AccessCred,
		const CssmKey &UnwrappedKey,
		CssmData &rawBlob,
		bool allocdRawBlob,			// callee has to free rawBlob
		const CssmData *DescriptiveData,
		CssmKey &WrappedKey,
		CSSM_PRIVILEGE Privilege);
		
	void UnwrapKeyCms(
		CSSM_CC_HANDLE CCHandle,
		const Context &Context,
		const CssmKey &WrappedKey,
		const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
		CssmKey &UnwrappedKey,
		CssmData &DescriptiveData,
		CSSM_PRIVILEGE Privilege,
		cspKeyStorage keyStorage);

	/* OpenSSHv1 wrap/unwrap, called out from standard wrap/unwrap */
	void WrapKeyOpenSSH1(
		CSSM_CC_HANDLE CCHandle,
		const Context &Context,
		const AccessCredentials &AccessCred,
		BinaryKey &unwrappedBinKey,
		CssmData &rawBlob,
		bool allocdRawBlob,			// callee has to free rawBlob
		const CssmData *DescriptiveData,
		CssmKey &WrappedKey,
		CSSM_PRIVILEGE Privilege);
		
	void UnwrapKeyOpenSSH1(
		CSSM_CC_HANDLE CCHandle,
		const Context &Context,
		const CssmKey &WrappedKey,
		const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
		CssmKey &UnwrappedKey,
		CssmData &DescriptiveData,
		CSSM_PRIVILEGE Privilege,
		cspKeyStorage keyStorage);

	/* 
	 * Used for generating crypto contexts at this level. 
	 * Analogous to AlgorithmFactory.setup().
	 */
	bool setup(
		CSPFullPluginSession::CSPContext * &cspCtx, 
		const Context &context);

	/*
	 * Find a CSPKeyInfoProvider subclass for the specified key.
	 */
	CSPKeyInfoProvider *infoProvider(
		const CssmKey	&key);
		
	void pkcs8InferKeyHeader(
		CssmKey			&key);
	
	void opensslInferKeyHeader(
		CssmKey			&key);
	
};	/* AppleCSPSession */


#endif //_APPLE_CSP_SESSION_H_
