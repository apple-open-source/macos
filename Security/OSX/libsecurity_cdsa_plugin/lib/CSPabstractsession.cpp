//
// CSP plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#include <security_cdsa_plugin/CSPsession.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/cssmcspi.h>


CSPAbstractPluginSession::~CSPAbstractPluginSession()
{ /* virtual */ }

static CSSM_RETURN CSSMCSPI cssm_ObtainPrivateKeyFromPublicKey(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_KEY *PublicKey,
         CSSM_KEY_PTR PrivateKey)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).ObtainPrivateKeyFromPublicKey(CssmKey::required(PublicKey),
			CssmKey::required(PrivateKey));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GetOperationalStatistics(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CSP_OPERATIONAL_STATISTICS *Statistics)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GetOperationalStatistics(CSPOperationalStatistics::required(Statistics));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_ChangeLoginOwner(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_ACL_OWNER_PROTOTYPE *NewOwner)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).ChangeLoginOwner(AccessCredentials::required(AccessCred),
			Required(NewOwner));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_EventNotify(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CONTEXT_EVENT Event,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).EventNotify(Event,
			CCHandle,
			Context::required(Context));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DecryptDataInit(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DecryptDataInit(CCHandle,
			Context::required(Context),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_SignDataInit(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).SignDataInit(CCHandle,
			Context::required(Context));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DigestData(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount,
         CSSM_DATA_PTR Digest)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DigestData(CCHandle,
			Context::required(Context),
			&CssmData::required(DataBufs),
			DataBufCount,
			CssmData::required(Digest));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GetKeyOwner(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_KEY *Key,
         CSSM_ACL_OWNER_PROTOTYPE_PTR Owner)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GetKeyOwner(CssmKey::required(Key),
			Required(Owner));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GetLoginAcl(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_STRING *SelectionTag,
         uint32 *NumberOfAclInfos,
         CSSM_ACL_ENTRY_INFO_PTR *AclInfos)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GetLoginAcl(SelectionTag,
			Required(NumberOfAclInfos),
			Required(AclInfos));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyMac(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount,
         const CSSM_DATA *Mac)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyMac(CCHandle,
			Context::required(Context),
			&CssmData::required(DataBufs),
			DataBufCount,
			CssmData::required(Mac));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_SignDataFinal(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         CSSM_DATA_PTR Signature)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).SignDataFinal(CCHandle,
			CssmData::required(Signature));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyDataUpdate(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyDataUpdate(CCHandle,
			&CssmData::required(DataBufs),
			DataBufCount);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateMac(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount,
         CSSM_DATA_PTR Mac)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateMac(CCHandle,
			Context::required(Context),
			&CssmData::required(DataBufs),
			DataBufCount,
			CssmData::required(Mac));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyMacFinal(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *Mac)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyMacFinal(CCHandle,
			CssmData::required(Mac));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateRandom(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         CSSM_DATA_PTR RandomNumber)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateRandom(CCHandle,
			Context::required(Context),
			CssmData::required(RandomNumber));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_RetrieveUniqueId(CSSM_CSP_HANDLE CSPHandle,
         CSSM_DATA_PTR UniqueID)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).RetrieveUniqueId(CssmData::required(UniqueID));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_UnwrapKey(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_KEY *PublicKey,
         const CSSM_WRAP_KEY *WrappedKey,
         uint32 KeyUsage,
         uint32 KeyAttr,
         const CSSM_DATA *KeyLabel,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         CSSM_KEY_PTR UnwrappedKey,
         CSSM_DATA_PTR DescriptiveData,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).UnwrapKey(CCHandle,
			Context::required(Context),
			CssmKey::optional(PublicKey),
			CssmKey::required(WrappedKey),
			KeyUsage,
			KeyAttr,
			CssmData::optional(KeyLabel),
			CredAndAclEntry,
			CssmKey::required(UnwrappedKey),
			CssmData::required(DescriptiveData),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateMacFinal(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         CSSM_DATA_PTR Mac)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateMacFinal(CCHandle,
			CssmData::required(Mac));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_WrapKey(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_KEY *Key,
         const CSSM_DATA *DescriptiveData,
         CSSM_WRAP_KEY_PTR WrappedKey,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).WrapKey(CCHandle,
			Context::required(Context),
			AccessCredentials::required(AccessCred),
			CssmKey::required(Key),
			CssmData::optional(DescriptiveData),
			CssmKey::required(WrappedKey),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DecryptDataFinal(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         CSSM_DATA_PTR RemData)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DecryptDataFinal(CCHandle,
			CssmData::required(RemData));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_SignData(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount,
         CSSM_ALGORITHMS DigestAlgorithm,
         CSSM_DATA_PTR Signature)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).SignData(CCHandle,
			Context::required(Context),
			&CssmData::required(DataBufs),
			DataBufCount,
			DigestAlgorithm,
			CssmData::required(Signature));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_EncryptDataInit(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).EncryptDataInit(CCHandle,
			Context::required(Context),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyData(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount,
         CSSM_ALGORITHMS DigestAlgorithm,
         const CSSM_DATA *Signature)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyData(CCHandle,
			Context::required(Context),
			&CssmData::required(DataBufs),
			DataBufCount,
			DigestAlgorithm,
			CssmData::required(Signature));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateMacUpdate(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateMacUpdate(CCHandle,
			&CssmData::required(DataBufs),
			DataBufCount);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_EncryptDataFinal(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         CSSM_DATA_PTR RemData)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).EncryptDataFinal(CCHandle,
			CssmData::required(RemData));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_ChangeKeyOwner(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_KEY *Key,
         const CSSM_ACL_OWNER_PROTOTYPE *NewOwner)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).ChangeKeyOwner(AccessCredentials::required(AccessCred),
			CssmKey::required(Key),
			Required(NewOwner));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyMacInit(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyMacInit(CCHandle,
			Context::required(Context));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DigestDataClone(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         CSSM_CC_HANDLE ClonedCCHandle)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DigestDataClone(CCHandle,
			ClonedCCHandle);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyDataInit(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyDataInit(CCHandle,
			Context::required(Context));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DecryptDataUpdate(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *CipherBufs,
         uint32 CipherBufCount,
         CSSM_DATA_PTR ClearBufs,
         uint32 ClearBufCount,
         CSSM_SIZE *bytesDecrypted)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DecryptDataUpdate(CCHandle,
			&CssmData::required(CipherBufs),
			CipherBufCount,
			&CssmData::required(ClearBufs),
			ClearBufCount,
			Required(bytesDecrypted));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateAlgorithmParams(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         uint32 ParamBits,
         CSSM_DATA_PTR Param,
         uint32 *NumberOfUpdatedAttibutes,
         CSSM_CONTEXT_ATTRIBUTE_PTR *UpdatedAttributes)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateAlgorithmParams(CCHandle,
			Context::required(Context),
			ParamBits,
			CssmData::required(Param),
			Required(NumberOfUpdatedAttibutes),
			Required(UpdatedAttributes));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GetLoginOwner(CSSM_CSP_HANDLE CSPHandle,
         CSSM_ACL_OWNER_PROTOTYPE_PTR Owner)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GetLoginOwner(Required(Owner));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GetKeyAcl(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_KEY *Key,
         const CSSM_STRING *SelectionTag,
         uint32 *NumberOfAclInfos,
         CSSM_ACL_ENTRY_INFO_PTR *AclInfos)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GetKeyAcl(CssmKey::required(Key),
			SelectionTag,
			Required(NumberOfAclInfos),
			Required(AclInfos));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyDevice(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_DATA *DeviceCert)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyDevice(CssmData::required(DeviceCert));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_EncryptDataUpdate(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *ClearBufs,
         uint32 ClearBufCount,
         CSSM_DATA_PTR CipherBufs,
         uint32 CipherBufCount,
         CSSM_SIZE *bytesEncrypted)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).EncryptDataUpdate(CCHandle,
			&CssmData::required(ClearBufs),
			ClearBufCount,
			&CssmData::required(CipherBufs),
			CipherBufCount,
			Required(bytesEncrypted));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DigestDataFinal(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         CSSM_DATA_PTR Digest)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DigestDataFinal(CCHandle,
			CssmData::required(Digest));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_Login(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_DATA *LoginName,
         const void *Reserved)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).Login(AccessCredentials::required(AccessCred),
			CssmData::optional(LoginName),
			Reserved);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_ChangeKeyAcl(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_ACL_EDIT *AclEdit,
         const CSSM_KEY *Key)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).ChangeKeyAcl(AccessCredentials::required(AccessCred),
			Required(AclEdit),
			CssmKey::required(Key));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_SignDataUpdate(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).SignDataUpdate(CCHandle,
			&CssmData::required(DataBufs),
			DataBufCount);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_QueryKeySizeInBits(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_KEY *Key,
         CSSM_KEY_SIZE_PTR KeySize)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).QueryKeySizeInBits(CCHandle,
			Context::optional(Context),
			CssmKey::optional(Key),
			Required(KeySize));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_Logout(CSSM_CSP_HANDLE CSPHandle)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).Logout();
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DecryptData(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_DATA *CipherBufs,
         uint32 CipherBufCount,
         CSSM_DATA_PTR ClearBufs,
         uint32 ClearBufCount,
         CSSM_SIZE *bytesDecrypted,
         CSSM_DATA_PTR RemData,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DecryptData(CCHandle,
			Context::required(Context),
			&CssmData::required(CipherBufs),
			CipherBufCount,
			&CssmData::required(ClearBufs),
			ClearBufCount,
			Required(bytesDecrypted),
			CssmData::required(RemData),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DigestDataInit(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DigestDataInit(CCHandle,
			Context::required(Context));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyDataFinal(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *Signature)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyDataFinal(CCHandle,
			CssmData::required(Signature));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateKeyPair(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         uint32 PublicKeyUsage,
         uint32 PublicKeyAttr,
         const CSSM_DATA *PublicKeyLabel,
         CSSM_KEY_PTR PublicKey,
         uint32 PrivateKeyUsage,
         uint32 PrivateKeyAttr,
         const CSSM_DATA *PrivateKeyLabel,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         CSSM_KEY_PTR PrivateKey,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateKeyPair(CCHandle,
			Context::required(Context),
			PublicKeyUsage,
			PublicKeyAttr,
			CssmData::optional(PublicKeyLabel),
			CssmKey::required(PublicKey),
			PrivateKeyUsage,
			PrivateKeyAttr,
			CssmData::optional(PrivateKeyLabel),
			CredAndAclEntry,
			CssmKey::required(PrivateKey),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_EncryptData(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         const CSSM_DATA *ClearBufs,
         uint32 ClearBufCount,
         CSSM_DATA_PTR CipherBufs,
         uint32 CipherBufCount,
         CSSM_SIZE *bytesEncrypted,
         CSSM_DATA_PTR RemData,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).EncryptData(CCHandle,
			Context::required(Context),
			&CssmData::required(ClearBufs),
			ClearBufCount,
			&CssmData::required(CipherBufs),
			CipherBufCount,
			Required(bytesEncrypted),
			CssmData::required(RemData),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GetTimeValue(CSSM_CSP_HANDLE CSPHandle,
         CSSM_ALGORITHMS TimeAlgorithm,
         CSSM_DATA *TimeData)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GetTimeValue(TimeAlgorithm,
			CssmData::required(TimeData));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DeriveKey(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         CSSM_DATA_PTR Param,
         uint32 KeyUsage,
         uint32 KeyAttr,
         const CSSM_DATA *KeyLabel,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         CSSM_KEY_PTR DerivedKey)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DeriveKey(CCHandle,
			Context::required(Context),
			CssmData::required(Param),
			KeyUsage,
			KeyAttr,
			CssmData::optional(KeyLabel),
			CredAndAclEntry,
			CssmKey::required(DerivedKey));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateKey(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         uint32 KeyUsage,
         uint32 KeyAttr,
         const CSSM_DATA *KeyLabel,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         CSSM_KEY_PTR Key,
         CSSM_PRIVILEGE Privilege)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateKey(CCHandle,
			Context::required(Context),
			KeyUsage,
			KeyAttr,
			CssmData::optional(KeyLabel),
			CredAndAclEntry,
			CssmKey::required(Key),
			Privilege);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_PassThrough(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         uint32 PassThroughId,
         const void *InData,
         void **OutData)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).PassThrough(CCHandle,
			Context::required(Context),
			PassThroughId,
			InData,
			OutData);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_VerifyMacUpdate(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).VerifyMacUpdate(CCHandle,
			&CssmData::required(DataBufs),
			DataBufCount);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_FreeKey(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         CSSM_KEY_PTR KeyPtr,
         CSSM_BOOL Delete)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).FreeKey(AccessCredentials::optional(AccessCred),
			CssmKey::required(KeyPtr),
			Delete);
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_GenerateMacInit(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).GenerateMacInit(CCHandle,
			Context::required(Context));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_RetrieveCounter(CSSM_CSP_HANDLE CSPHandle,
         CSSM_DATA_PTR Counter)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).RetrieveCounter(CssmData::required(Counter));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_QuerySize(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CONTEXT *Context,
         CSSM_BOOL Encrypt,
         uint32 QuerySizeCount,
         CSSM_QUERY_SIZE_DATA_PTR DataBlock)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).QuerySize(CCHandle,
			Context::required(Context),
			Encrypt,
			QuerySizeCount,
			QuerySizeData::optional(DataBlock));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_ChangeLoginAcl(CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_ACL_EDIT *AclEdit)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).ChangeLoginAcl(AccessCredentials::required(AccessCred),
			Required(AclEdit));
  END_API(CSP)
}

static CSSM_RETURN CSSMCSPI cssm_DigestDataUpdate(CSSM_CSP_HANDLE CSPHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *DataBufs,
         uint32 DataBufCount)
{
  BEGIN_API
  findSession<CSPPluginSession>(CSPHandle).DigestDataUpdate(CCHandle,
			&CssmData::required(DataBufs),
			DataBufCount);
  END_API(CSP)
}


static const CSSM_SPI_CSP_FUNCS CSPFunctionStruct = {
  cssm_EventNotify,
  cssm_QuerySize,
  cssm_SignData,
  cssm_SignDataInit,
  cssm_SignDataUpdate,
  cssm_SignDataFinal,
  cssm_VerifyData,
  cssm_VerifyDataInit,
  cssm_VerifyDataUpdate,
  cssm_VerifyDataFinal,
  cssm_DigestData,
  cssm_DigestDataInit,
  cssm_DigestDataUpdate,
  cssm_DigestDataClone,
  cssm_DigestDataFinal,
  cssm_GenerateMac,
  cssm_GenerateMacInit,
  cssm_GenerateMacUpdate,
  cssm_GenerateMacFinal,
  cssm_VerifyMac,
  cssm_VerifyMacInit,
  cssm_VerifyMacUpdate,
  cssm_VerifyMacFinal,
  cssm_EncryptData,
  cssm_EncryptDataInit,
  cssm_EncryptDataUpdate,
  cssm_EncryptDataFinal,
  cssm_DecryptData,
  cssm_DecryptDataInit,
  cssm_DecryptDataUpdate,
  cssm_DecryptDataFinal,
  cssm_QueryKeySizeInBits,
  cssm_GenerateKey,
  cssm_GenerateKeyPair,
  cssm_GenerateRandom,
  cssm_GenerateAlgorithmParams,
  cssm_WrapKey,
  cssm_UnwrapKey,
  cssm_DeriveKey,
  cssm_FreeKey,
  cssm_PassThrough,
  cssm_Login,
  cssm_Logout,
  cssm_ChangeLoginAcl,
  cssm_ObtainPrivateKeyFromPublicKey,
  cssm_RetrieveUniqueId,
  cssm_RetrieveCounter,
  cssm_VerifyDevice,
  cssm_GetTimeValue,
  cssm_GetOperationalStatistics,
  cssm_GetLoginAcl,
  cssm_GetKeyAcl,
  cssm_ChangeKeyAcl,
  cssm_GetKeyOwner,
  cssm_ChangeKeyOwner,
  cssm_GetLoginOwner,
  cssm_ChangeLoginOwner,
};

static CSSM_MODULE_FUNCS CSPFunctionTable = {
  CSSM_SERVICE_CSP,	// service type
  57,	// number of functions
  (const CSSM_PROC_ADDR *)&CSPFunctionStruct
};

CSSM_MODULE_FUNCS_PTR CSPPluginSession::construct()
{
   return &CSPFunctionTable;
}
