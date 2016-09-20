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
// SSCSPSession.cpp - Security Server CSP session.
//
#include "SSCSPSession.h"

#include "CSPDLPlugin.h"
#include "SSDatabase.h"
#include "SSDLSession.h"
#include "SSKey.h"
#include <security_cdsa_utilities/cssmbridge.h>
#include <memory>

using namespace std;
using namespace SecurityServer;

//
// SSCSPSession -- Security Server CSP session
//
SSCSPSession::SSCSPSession(CSSM_MODULE_HANDLE handle,
						   CSPDLPlugin &plug,
						   const CSSM_VERSION &version,
						   uint32 subserviceId,
						   CSSM_SERVICE_TYPE subserviceType,
						   CSSM_ATTACH_FLAGS attachFlags,
						   const CSSM_UPCALLS &upcalls,
						   SSCSPDLSession &ssCSPDLSession,
						   CssmClient::CSP &rawCsp)
: CSPFullPluginSession(handle, plug, version, subserviceId, subserviceType,
					   attachFlags, upcalls),
  mSSCSPDLSession(ssCSPDLSession),
  mSSFactory(plug.mSSFactory),
  mRawCsp(rawCsp),
  mClientSession(Allocator::standard(), *this)
{
	mClientSession.registerForAclEdits(SSCSPDLSession::didChangeKeyAclCallback, &mSSCSPDLSession);
}

//
// Called at (CSSM) context create time. This is ignored; we do a full 
// context setup later, at setupContext time. 
//
CSPFullPluginSession::CSPContext *
SSCSPSession::contextCreate(CSSM_CC_HANDLE handle, const Context &context)
{
	return NULL;
}


//
// Called by CSPFullPluginSession when an op is actually commencing.
// Context can safely assumed to be fully formed and stable for the
// duration of the  op; thus we wait until now to set up our 
// CSPContext as appropriate to the op.
//
void
SSCSPSession::setupContext(CSPContext * &cspCtx,
						   const Context &context,
						   bool encoding)
{
	// note we skip this if this CSPContext is being reused
    if (cspCtx == NULL)
	{

		if (mSSFactory.setup(*this, cspCtx, context, encoding))
			return;

#if 0
		if (mBSafe4Factory.setup(*this, cspCtx, context))
			return;

		if (mCryptKitFactory.setup(*this, cspCtx, context))
			return;
#endif

        CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
}


//
// DL interaction
//
SSDatabase
SSCSPSession::getDatabase(const Context &context)
{
	return getDatabase(context.get<CSSM_DL_DB_HANDLE>(CSSM_ATTRIBUTE_DL_DB_HANDLE));
}

SSDatabase
SSCSPSession::getDatabase(CSSM_DL_DB_HANDLE *aDLDbHandle)
{
	if (aDLDbHandle)
		return findSession<SSDLSession>(aDLDbHandle->DLHandle).findDbHandle(aDLDbHandle->DBHandle);
	else
		return SSDatabase();
}


//
// Reference Key management
//
void
SSCSPSession::makeReferenceKey(KeyHandle inKeyHandle, CssmKey &ioKey, SSDatabase &inSSDatabase,
							   uint32 inKeyAttr, const CssmData *inKeyLabel)
{
	return mSSCSPDLSession.makeReferenceKey(*this, inKeyHandle, ioKey, inSSDatabase, inKeyAttr, inKeyLabel);
}

SSKey &
SSCSPSession::lookupKey(const CssmKey &inKey)
{
	return mSSCSPDLSession.lookupKey(inKey);
}


//
// Key creating and handeling members
//
void
SSCSPSession::WrapKey(CSSM_CC_HANDLE CCHandle,
					  const Context &context,
					  const AccessCredentials &AccessCred,
					  const CssmKey &Key,
					  const CssmData *DescriptiveData,
					  CssmKey &WrappedKey,
					  CSSM_PRIVILEGE Privilege)
{
	// @@@ Deal with permanent keys
 	const CssmKey *keyInContext =
		context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY);

	KeyHandle contextKeyHandle = (keyInContext
								  ? lookupKey(*keyInContext).keyHandle()
								  : noKey);
	clientSession().wrapKey(context, contextKeyHandle,
							lookupKey(Key).keyHandle(), &AccessCred,
							DescriptiveData, WrappedKey, *this);
}

void
SSCSPSession::UnwrapKey(CSSM_CC_HANDLE CCHandle,
						const Context &context,
						const CssmKey *PublicKey,
						const CssmWrappedKey &WrappedKey,
						uint32 KeyUsage,
						uint32 KeyAttr,
						const CssmData *KeyLabel,
						const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
						CssmKey &UnwrappedKey,
						CssmData &DescriptiveData,
						CSSM_PRIVILEGE Privilege)
{
	SSDatabase database = getDatabase(context);
	validateKeyAttr(KeyAttr);
	const AccessCredentials *cred = NULL;
	const AclEntryInput *owner = NULL;
	if (CredAndAclEntry)
	{
		cred = AccessCredentials::overlay(CredAndAclEntry->AccessCred);
		owner = &AclEntryInput::overlay(CredAndAclEntry->InitialAclEntry);
	}

	KeyHandle publicKey = noKey;
	if (PublicKey)
	{
		if (PublicKey->blobType() == CSSM_KEYBLOB_RAW)
		{
			// @@@ We need to unwrap the publicKey into the SecurityServer
			// before continuing
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
		}
		else
			publicKey = lookupKey(*PublicKey).keyHandle();
	}

	// @@@ Deal with permanent keys
 	const CssmKey *keyInContext =
		context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY);

	KeyHandle contextKeyHandle =
		keyInContext ? lookupKey(*keyInContext).keyHandle() : noKey;

	KeyHandle unwrappedKeyHandle;
	clientSession().unwrapKey(database.dbHandle(), context, contextKeyHandle,
							  publicKey, WrappedKey, KeyUsage, KeyAttr,
							  cred, owner, DescriptiveData, unwrappedKeyHandle,
							  UnwrappedKey.header(), *this);	
	makeReferenceKey(unwrappedKeyHandle, UnwrappedKey, database, KeyAttr,
					 KeyLabel);
}

void
SSCSPSession::DeriveKey(CSSM_CC_HANDLE ccHandle,
						const Context &context,
						CssmData &param,
						uint32 keyUsage,
						uint32 keyAttr,
						const CssmData *keyLabel,
						const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
						CssmKey &derivedKey)
{
	SSDatabase database = getDatabase(context);
	validateKeyAttr(keyAttr);
	const AccessCredentials *cred = NULL;
	const AclEntryInput *owner = NULL;
	if (credAndAclEntry)
	{
		cred = AccessCredentials::overlay(credAndAclEntry->AccessCred);
		owner = &AclEntryInput::overlay(credAndAclEntry->InitialAclEntry);
	}

	/* optional BaseKey */
 	const CssmKey *keyInContext =
		context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY);
	KeyHandle contextKeyHandle =
		keyInContext ? lookupKey(*keyInContext).keyHandle() : noKey;
	KeyHandle keyHandle;
	switch(context.algorithm()) {
	case CSSM_ALGID_KEYCHAIN_KEY:
		{
			// special interpretation: take DLDBHandle -> DbHandle from params
			clientSession().extractMasterKey(database.dbHandle(), context,
				getDatabase(param.interpretedAs<CSSM_DL_DB_HANDLE>(CSSMERR_CSP_INVALID_ATTR_DL_DB_HANDLE)).dbHandle(),
				keyUsage, keyAttr, cred, owner, keyHandle, derivedKey.header());
		}
		break;
	default:
		clientSession().deriveKey(database.dbHandle(), context, contextKeyHandle, keyUsage,
					keyAttr, param, cred, owner, keyHandle, derivedKey.header());
		break;
	}
	makeReferenceKey(keyHandle, derivedKey, database, keyAttr, keyLabel);
}

void
SSCSPSession::GenerateKey(CSSM_CC_HANDLE ccHandle,
						  const Context &context,
						  uint32 keyUsage,
						  uint32 keyAttr,
						  const CssmData *keyLabel,
						  const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
						  CssmKey &key,
						  CSSM_PRIVILEGE privilege)
{
	SSDatabase database = getDatabase(context);
	validateKeyAttr(keyAttr);
	const AccessCredentials *cred = NULL;
	const AclEntryInput *owner = NULL;
	if (credAndAclEntry)
	{
		cred = AccessCredentials::overlay(credAndAclEntry->AccessCred);
		owner = &AclEntryInput::overlay(credAndAclEntry->InitialAclEntry);
	}

	KeyHandle keyHandle;
	clientSession().generateKey(database.dbHandle(), context, keyUsage,
								keyAttr, cred, owner, keyHandle, key.header());
	makeReferenceKey(keyHandle, key, database, keyAttr, keyLabel);
}

void
SSCSPSession::GenerateKeyPair(CSSM_CC_HANDLE ccHandle,
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
	SSDatabase database = getDatabase(context);
	validateKeyAttr(publicKeyAttr);
	validateKeyAttr(privateKeyAttr);
	const AccessCredentials *cred = NULL;
	const AclEntryInput *owner = NULL;
	if (credAndAclEntry)
	{
		cred = AccessCredentials::overlay(credAndAclEntry->AccessCred);
		owner = &AclEntryInput::overlay(credAndAclEntry->InitialAclEntry);
	}

	/* 
	 * Public keys must be extractable in the clear - that's the Apple
	 * policy. The raw CSP is unable to enforce the extractable
	 * bit since it always sees that as true (it's managed and forced
	 * true by the SecurityServer). So...
	 */
	if(!(publicKeyAttr & CSSM_KEYATTR_EXTRACTABLE)) {
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	}
	KeyHandle pubKeyHandle, privKeyHandle;
	clientSession().generateKey(database.dbHandle(), context,
							   publicKeyUsage, publicKeyAttr,
							   privateKeyUsage, privateKeyAttr,
							   cred, owner,
							   pubKeyHandle, publicKey.header(),
							   privKeyHandle, privateKey.header());
	makeReferenceKey(privKeyHandle, privateKey, database, privateKeyAttr,
					 privateKeyLabel);
	// @@@ What if this throws, we need to free privateKey.
	makeReferenceKey(pubKeyHandle, publicKey, database, publicKeyAttr,
					 publicKeyLabel);
}

void
SSCSPSession::ObtainPrivateKeyFromPublicKey(const CssmKey &PublicKey,
											CssmKey &PrivateKey)
{
	unimplemented();
}

void
SSCSPSession::QueryKeySizeInBits(CSSM_CC_HANDLE CCHandle,
								 const Context *Context,
								 const CssmKey *Key,
								 CSSM_KEY_SIZE &KeySize)
{
	unimplemented();
}

void
SSCSPSession::FreeKey(const AccessCredentials *accessCred,
					  CssmKey &ioKey, CSSM_BOOL deleteKey)
{
	if (ioKey.blobType() == CSSM_KEYBLOB_REFERENCE)
	{
		// @@@ Note that this means that detaching a session should free
		// all keys ascociated with it or else...
		// -- or else what?
		// exactly!

		// @@@ There are thread safety issues when deleting a key that is
		// in use by another thread, but the answer to that is: Don't do
		// that!

		// Find the key in the map.  Tell tell the key to free itself
		// (when the auto_ptr deletes the key it removes itself from the map). 
	    secinfo("freeKey", "CSPDL FreeKey");
		auto_ptr<SSKey> ssKey(&mSSCSPDLSession.find<SSKey>(ioKey));
		ssKey->free(accessCred, ioKey, deleteKey);
	}
	else
	{
		CSPFullPluginSession::FreeKey(accessCred, ioKey, deleteKey);
	}
}


//
// Generation stuff.
//
void
SSCSPSession::GenerateRandom(CSSM_CC_HANDLE ccHandle,
							 const Context &context,
							 CssmData &randomNumber)
{
    checkOperation(context.type(), CSSM_ALGCLASS_RANDOMGEN);
	// if (context.algorithm() != @@@) CssmError::throwMe(ALGORITHM_NOT_SUPPORTED);
	uint32 needed = context.getInt(CSSM_ATTRIBUTE_OUTPUT_SIZE, CSSMERR_CSP_MISSING_ATTR_OUTPUT_SIZE);

	// @@@ What about the seed?
    if (randomNumber.length())
	{
        if (randomNumber.length() < needed)
            CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
		clientSession().generateRandom(context, randomNumber);
    }
	else
	{
        randomNumber.Data = alloc<uint8>(needed);
		try
		{
			clientSession().generateRandom(context, randomNumber);
		}
		catch(...)
		{
			free(randomNumber.Data);
			randomNumber.Data = NULL;
			throw;
		}
	}
}

//
// Login/Logout and token operational maintainance.  These mean little
// without support by the actual implementation, but we can help...
// @@@ Should this be in CSP[non-Full]PluginSession?
//
void
SSCSPSession::Login(const AccessCredentials &AccessCred,
					const CssmData *LoginName,
					const void *Reserved)
{
	// @@@ Do a login to the securityServer making keys persistent until it
	// goes away
	unimplemented();
}

void
SSCSPSession::Logout()
{
	unimplemented();
}

void
SSCSPSession::VerifyDevice(const CssmData &DeviceCert)
{
	CssmError::throwMe(CSSMERR_CSP_DEVICE_VERIFY_FAILED);
}

void
SSCSPSession::GetOperationalStatistics(CSPOperationalStatistics &statistics)
{
	unimplemented();
}


//
// Utterly miscellaneous, rarely used, strange functions
//
void
SSCSPSession::RetrieveCounter(CssmData &Counter)
{
	unimplemented();
}

void
SSCSPSession::RetrieveUniqueId(CssmData &UniqueID)
{
	unimplemented();
}

void
SSCSPSession::GetTimeValue(CSSM_ALGORITHMS TimeAlgorithm, CssmData &TimeData)
{
	unimplemented();
}


//
// ACL retrieval and change operations
//
void
SSCSPSession::GetKeyOwner(const CssmKey &Key,
						  CSSM_ACL_OWNER_PROTOTYPE &Owner)
{
	lookupKey(Key).getOwner(Owner, *this);
}

void
SSCSPSession::ChangeKeyOwner(const AccessCredentials &AccessCred,
							 const CssmKey &Key,
							 const CSSM_ACL_OWNER_PROTOTYPE &NewOwner)
{
	lookupKey(Key).changeOwner(AccessCred,
							   AclOwnerPrototype::overlay(NewOwner));
}

void
SSCSPSession::GetKeyAcl(const CssmKey &Key,
						const CSSM_STRING *SelectionTag,
						uint32 &NumberOfAclInfos,
						CSSM_ACL_ENTRY_INFO_PTR &AclInfos)
{
	lookupKey(Key).getAcl(reinterpret_cast<const char *>(SelectionTag),
						  NumberOfAclInfos,
						  reinterpret_cast<AclEntryInfo *&>(AclInfos), *this);
}

void
SSCSPSession::ChangeKeyAcl(const AccessCredentials &AccessCred,
						   const CSSM_ACL_EDIT &AclEdit,
						   const CssmKey &Key)
{
	lookupKey(Key).changeAcl(AccessCred, AclEdit::overlay(AclEdit));
}

void
SSCSPSession::GetLoginOwner(CSSM_ACL_OWNER_PROTOTYPE &Owner)
{
	unimplemented();
}

void
SSCSPSession::ChangeLoginOwner(const AccessCredentials &AccessCred,
							   const CSSM_ACL_OWNER_PROTOTYPE &NewOwner)
{
	unimplemented();
}

void
SSCSPSession::GetLoginAcl(const CSSM_STRING *SelectionTag,
						  uint32 &NumberOfAclInfos,
						  CSSM_ACL_ENTRY_INFO_PTR &AclInfos)
{
	unimplemented();
}

void
SSCSPSession::ChangeLoginAcl(const AccessCredentials &AccessCred,
							 const CSSM_ACL_EDIT &AclEdit)
{
	unimplemented();
}



//
// Passthroughs
//
void
SSCSPSession::PassThrough(CSSM_CC_HANDLE CCHandle,
						  const Context &context,
						  uint32 passThroughId,
						  const void *inData,
						  void **outData)
{
    checkOperation(context.type(), CSSM_ALGCLASS_NONE);
	switch (passThroughId) {
	case CSSM_APPLESCPDL_CSP_GET_KEYHANDLE:
	{
		// inData unused, must be NULL
		if (inData)
			CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

		// outData required, must be pointer-to-pointer-to-KeyHandle
		KeyHandle &result = Required(reinterpret_cast<KeyHandle *>(outData));

		// we'll take the key from the context
		const CssmKey &key =
			context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY,  CSSMERR_CSP_MISSING_ATTR_KEY);

		// all ready
		result = lookupKey(key).keyHandle();
		break;
	}
	case CSSM_APPLECSP_KEYDIGEST:
	{
		// inData unused, must be NULL
		if (inData)
			CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

		// outData required
		Required(outData);

		// take the key from the context, convert to KeyHandle
		const CssmKey &key =
			context.get<const CssmKey>(CSSM_ATTRIBUTE_KEY,  CSSMERR_CSP_MISSING_ATTR_KEY); 
		KeyHandle keyHandle = lookupKey(key).keyHandle();

		// allocate digest holder on app's behalf
		CSSM_DATA *digest = alloc<CSSM_DATA>(sizeof(CSSM_DATA));
		digest->Data = NULL;
		digest->Length = 0;
				
		// go
		try {
			clientSession().getKeyDigest(keyHandle, CssmData::overlay(*digest));
		}
		catch(...) {
			free(digest);
			throw;
		}
		*outData = digest;
		break;
	}
	
	default:
		CssmError::throwMe(CSSM_ERRCODE_INVALID_PASSTHROUGH_ID);
	}
}

/* Validate requested key attr flags for newly generated keys */
void SSCSPSession::validateKeyAttr(uint32 reqKeyAttr)
{
	if(reqKeyAttr & (CSSM_KEYATTR_RETURN_DATA)) {
		/* CSPDL only supports reference keys */
		CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK);
	}
	if(reqKeyAttr & (CSSM_KEYATTR_ALWAYS_SENSITIVE | 
				     CSSM_KEYATTR_NEVER_EXTRACTABLE)) {
		/* invalid for any CSP */
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	}
	/* There may be more, but we'll leave it to SS and CSP to decide */
}
