/*
 * Copyright (c) 2004,2008 Apple Inc. All Rights Reserved.
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
// SDCSPSession.cpp - Security Server CSP session.
//
#include "SDCSPSession.h"

#include "SDCSPDLPlugin.h"
#include "SDDLSession.h"
#include "SDKey.h"
#include <security_cdsa_utilities/cssmbridge.h>
#include <memory>

using namespace std;
using namespace SecurityServer;

//
// SDCSPSession -- Security Server CSP session
//
SDCSPSession::SDCSPSession(CSSM_MODULE_HANDLE handle,
						   SDCSPDLPlugin &plug,
						   const CSSM_VERSION &version,
						   uint32 subserviceId,
						   CSSM_SERVICE_TYPE subserviceType,
						   CSSM_ATTACH_FLAGS attachFlags,
						   const CSSM_UPCALLS &upcalls,
						   SDCSPDLSession &ssCSPDLSession,
						   CssmClient::CSP &rawCsp)
: CSPFullPluginSession(handle, plug, version, subserviceId, subserviceType,
					   attachFlags, upcalls),
  mSDCSPDLSession(ssCSPDLSession),
  mSDFactory(plug.mSDFactory),
  mRawCsp(rawCsp),
  mClientSession(Allocator::standard(), *this)
{
}

//
// Called at (CSSM) context create time. This is ignored; we do a full 
// context setup later, at setupContext time. 
//
CSPFullPluginSession::CSPContext *
SDCSPSession::contextCreate(CSSM_CC_HANDLE handle, const Context &context)
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
SDCSPSession::setupContext(CSPContext * &cspCtx,
						   const Context &context,
						   bool encoding)
{
	// note we skip this if this CSPContext is being reused
    if (cspCtx == NULL)
	{

		if (mSDFactory.setup(*this, cspCtx, context, encoding))
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
CSSM_DB_HANDLE
SDCSPSession::getDatabase(const Context &context)
{
	return getDatabase(context.get<CSSM_DL_DB_HANDLE>(CSSM_ATTRIBUTE_DL_DB_HANDLE));
}

CSSM_DB_HANDLE
SDCSPSession::getDatabase(CSSM_DL_DB_HANDLE *aDLDbHandle)
{
	if (aDLDbHandle)
		return aDLDbHandle->DBHandle;
	else
		return CSSM_INVALID_HANDLE;
}


//
// Reference Key management
//
void
SDCSPSession::makeReferenceKey(KeyHandle inKeyHandle, CssmKey &ioKey, CSSM_DB_HANDLE inDBHandle,
							   uint32 inKeyAttr, const CssmData *inKeyLabel)
{
	return mSDCSPDLSession.makeReferenceKey(*this, inKeyHandle, ioKey, inDBHandle, inKeyAttr, inKeyLabel);
}

SDKey &
SDCSPSession::lookupKey(const CssmKey &inKey)
{
	return mSDCSPDLSession.lookupKey(inKey);
}


//
// Key creating and handeling members
//
void
SDCSPSession::WrapKey(CSSM_CC_HANDLE CCHandle,
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
SDCSPSession::UnwrapKey(CSSM_CC_HANDLE CCHandle,
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
	CSSM_DB_HANDLE database = getDatabase(context);
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
	clientSession().unwrapKey(ClientSession::toIPCHandle(database), context, contextKeyHandle,
							  publicKey, WrappedKey, KeyUsage, KeyAttr,
							  cred, owner, DescriptiveData, unwrappedKeyHandle,
							  UnwrappedKey.header(), *this);	
	makeReferenceKey(unwrappedKeyHandle, UnwrappedKey, database, KeyAttr,
					 KeyLabel);
}

void
SDCSPSession::DeriveKey(CSSM_CC_HANDLE ccHandle,
						const Context &context,
						CssmData &param,
						uint32 keyUsage,
						uint32 keyAttr,
						const CssmData *keyLabel,
						const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
						CssmKey &derivedKey)
{
	CSSM_DB_HANDLE database = getDatabase(context);
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
			clientSession().extractMasterKey(ClientSession::toIPCHandle(database), context,
				(DbHandle)getDatabase(param.interpretedAs<CSSM_DL_DB_HANDLE>(CSSMERR_CSP_INVALID_ATTR_DL_DB_HANDLE)),
				keyUsage, keyAttr, cred, owner, keyHandle, derivedKey.header());
		}
		break;
	default:
            clientSession().deriveKey(ClientSession::toIPCHandle(database), context, contextKeyHandle, keyUsage,
					keyAttr, param, cred, owner, keyHandle, derivedKey.header());
		break;
	}
	makeReferenceKey(keyHandle, derivedKey, database, keyAttr, keyLabel);
}

void
SDCSPSession::GenerateKey(CSSM_CC_HANDLE ccHandle,
						  const Context &context,
						  uint32 keyUsage,
						  uint32 keyAttr,
						  const CssmData *keyLabel,
						  const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
						  CssmKey &key,
						  CSSM_PRIVILEGE privilege)
{
	CSSM_DB_HANDLE database = getDatabase(context);
	validateKeyAttr(keyAttr);
	const AccessCredentials *cred = NULL;
	const AclEntryInput *owner = NULL;
	if (credAndAclEntry)
	{
		cred = AccessCredentials::overlay(credAndAclEntry->AccessCred);
		owner = &AclEntryInput::overlay(credAndAclEntry->InitialAclEntry);
	}

	KeyHandle keyHandle;
	clientSession().generateKey(ClientSession::toIPCHandle(database), context, keyUsage,
								keyAttr, cred, owner, keyHandle, key.header());
	makeReferenceKey(keyHandle, key, database, keyAttr, keyLabel);
}

void
SDCSPSession::GenerateKeyPair(CSSM_CC_HANDLE ccHandle,
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
	CSSM_DB_HANDLE database = getDatabase(context);
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
	clientSession().generateKey(ClientSession::toIPCHandle(database), context,
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
SDCSPSession::ObtainPrivateKeyFromPublicKey(const CssmKey &PublicKey,
											CssmKey &PrivateKey)
{
	unimplemented();
}

void
SDCSPSession::QueryKeySizeInBits(CSSM_CC_HANDLE CCHandle,
								 const Context &Context,
								 const CssmKey &Key,
								 CSSM_KEY_SIZE &KeySize)
{
	unimplemented();
}

void
SDCSPSession::FreeKey(const AccessCredentials *accessCred,
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
	    secdebug("freeKey", "CSPDL FreeKey");
		auto_ptr<SDKey> ssKey(&mSDCSPDLSession.find<SDKey>(ioKey));
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
SDCSPSession::GenerateRandom(CSSM_CC_HANDLE ccHandle,
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
SDCSPSession::Login(const AccessCredentials &AccessCred,
					const CssmData *LoginName,
					const void *Reserved)
{
	// @@@ Do a login to the securityServer making keys persistant until it
	// goes away
	unimplemented();
}

void
SDCSPSession::Logout()
{
	unimplemented();
}

void
SDCSPSession::VerifyDevice(const CssmData &DeviceCert)
{
	CssmError::throwMe(CSSMERR_CSP_DEVICE_VERIFY_FAILED);
}

void
SDCSPSession::GetOperationalStatistics(CSPOperationalStatistics &statistics)
{
	unimplemented();
}


//
// Utterly miscellaneous, rarely used, strange functions
//
void
SDCSPSession::RetrieveCounter(CssmData &Counter)
{
	unimplemented();
}

void
SDCSPSession::RetrieveUniqueId(CssmData &UniqueID)
{
	unimplemented();
}

void
SDCSPSession::GetTimeValue(CSSM_ALGORITHMS TimeAlgorithm, CssmData &TimeData)
{
	unimplemented();
}


//
// ACL retrieval and change operations
//
void
SDCSPSession::GetKeyOwner(const CssmKey &Key,
						  CSSM_ACL_OWNER_PROTOTYPE &Owner)
{
	lookupKey(Key).getOwner(Owner, *this);
}

void
SDCSPSession::ChangeKeyOwner(const AccessCredentials &AccessCred,
							 const CssmKey &Key,
							 const CSSM_ACL_OWNER_PROTOTYPE &NewOwner)
{
	lookupKey(Key).changeOwner(AccessCred,
							   AclOwnerPrototype::overlay(NewOwner));
}

void
SDCSPSession::GetKeyAcl(const CssmKey &Key,
						const CSSM_STRING *SelectionTag,
						uint32 &NumberOfAclInfos,
						CSSM_ACL_ENTRY_INFO_PTR &AclInfos)
{
	lookupKey(Key).getAcl(reinterpret_cast<const char *>(SelectionTag),
						  NumberOfAclInfos,
						  reinterpret_cast<AclEntryInfo *&>(AclInfos), *this);
}

void
SDCSPSession::ChangeKeyAcl(const AccessCredentials &AccessCred,
						   const CSSM_ACL_EDIT &AclEdit,
						   const CssmKey &Key)
{
	lookupKey(Key).changeAcl(AccessCred, AclEdit::overlay(AclEdit));
}

void
SDCSPSession::GetLoginOwner(CSSM_ACL_OWNER_PROTOTYPE &Owner)
{
	unimplemented();
}

void
SDCSPSession::ChangeLoginOwner(const AccessCredentials &AccessCred,
							   const CSSM_ACL_OWNER_PROTOTYPE &NewOwner)
{
	unimplemented();
}

void
SDCSPSession::GetLoginAcl(const CSSM_STRING *SelectionTag,
						  uint32 &NumberOfAclInfos,
						  CSSM_ACL_ENTRY_INFO_PTR &AclInfos)
{
	unimplemented();
}

void
SDCSPSession::ChangeLoginAcl(const AccessCredentials &AccessCred,
							 const CSSM_ACL_EDIT &AclEdit)
{
	unimplemented();
}



//
// Passthroughs
//
void
SDCSPSession::PassThrough(CSSM_CC_HANDLE CCHandle,
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
void SDCSPSession::validateKeyAttr(uint32 reqKeyAttr)
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
