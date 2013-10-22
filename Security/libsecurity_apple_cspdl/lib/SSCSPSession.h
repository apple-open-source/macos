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
// SSDLSession.h - CSP session for security server CSP/DL.
//
#ifndef _H_SSCSPSESSION
#define _H_SSCSPSESSION

#include "SSCSPDLSession.h"

#include <securityd_client/ssclient.h>
#include <security_cdsa_client/cspclient.h>


class CSPDLPlugin;
class SSFactory;
class SSDatabase;
class SSKey;

class SSCSPSession : public CSPFullPluginSession
{
public:
	SSCSPDLSession &mSSCSPDLSession;
	SSFactory &mSSFactory;
	CssmClient::CSP &mRawCsp;
	
	SSCSPSession(CSSM_MODULE_HANDLE handle,
				 CSPDLPlugin &plug,
				 const CSSM_VERSION &version,
				 uint32 subserviceId,
				 CSSM_SERVICE_TYPE subserviceType,
				 CSSM_ATTACH_FLAGS attachFlags,
				 const CSSM_UPCALLS &upcalls,
				 SSCSPDLSession &ssCSPDLSession,
				 CssmClient::CSP &rawCsp);

	SecurityServer::ClientSession &clientSession()
	{ return mClientSession; }

	CSPContext *contextCreate(CSSM_CC_HANDLE handle, const Context &context);
#if 0
	void contextUpdate(CSSM_CC_HANDLE handle, const Context &context,
					   PluginContext *ctx);
	void contextDelete(CSSM_CC_HANDLE handle, const Context &context,
					   PluginContext *ctx);
#endif

	void setupContext(CSPContext * &ctx, const Context &context,
					  bool encoding);
	
	SSDatabase getDatabase(CSSM_DL_DB_HANDLE *aDLDbHandle);
	SSDatabase getDatabase(const Context &context);

	void makeReferenceKey(SecurityServer::KeyHandle inKeyHandle,
						  CssmKey &outKey, SSDatabase &inSSDatabase,
						  uint32 inKeyAttr, const CssmData *inKeyLabel);
	SSKey &lookupKey(const CssmKey &inKey);

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
	void GenerateKey(CSSM_CC_HANDLE ccHandle,
					const Context &context,
					uint32 keyUsage,
					uint32 keyAttr,
					const CssmData *keyLabel,
					const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
					CssmKey &key,
					CSSM_PRIVILEGE privilege);
	void GenerateKeyPair(CSSM_CC_HANDLE ccHandle,
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
						CSSM_PRIVILEGE privilege);
	void ObtainPrivateKeyFromPublicKey(const CssmKey &PublicKey,
									CssmKey &PrivateKey);
	void QueryKeySizeInBits(CSSM_CC_HANDLE CCHandle,
							const Context *Context,
							const CssmKey *Key,
							CSSM_KEY_SIZE &KeySize);
	void FreeKey(const AccessCredentials *AccessCred,
				CssmKey &key, CSSM_BOOL Delete);
	void GenerateRandom(CSSM_CC_HANDLE ccHandle,
						const Context &context,
						CssmData &randomNumber);
	void Login(const AccessCredentials &AccessCred,
			const CssmData *LoginName,
			const void *Reserved);
	void Logout();
	void VerifyDevice(const CssmData &DeviceCert);
	void GetOperationalStatistics(CSPOperationalStatistics &statistics);
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
private:
	/* Validate requested key attr flags for newly generated keys */
	void validateKeyAttr(uint32 reqKeyAttr);

	SecurityServer::ClientSession mClientSession;
};


#endif // _H_SSCSPSESSION
