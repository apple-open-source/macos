//
// CSP plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#ifndef _H_CSPABSTRACTSESSION
#define _H_CSPABSTRACTSESSION

#include <security_cdsa_plugin/pluginsession.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/cssmdb.h>


namespace Security {


//
// A pure abstract class to define the CSP module interface
//
class CSPAbstractPluginSession {
public:
	virtual ~CSPAbstractPluginSession();
  virtual void VerifyMacFinal(CSSM_CC_HANDLE CCHandle,
         const CssmData &Mac) = 0;
  virtual void GenerateRandom(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         CssmData &RandomNumber) = 0;
  virtual void RetrieveUniqueId(CssmData &UniqueID) = 0;
  virtual void SignDataFinal(CSSM_CC_HANDLE CCHandle,
         CssmData &Signature) = 0;
  virtual void VerifyDataUpdate(CSSM_CC_HANDLE CCHandle,
         const CssmData DataBufs[],
         uint32 DataBufCount) = 0;
  virtual void GenerateMac(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmData DataBufs[],
         uint32 DataBufCount,
         CssmData &Mac) = 0;
  virtual void VerifyMac(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmData DataBufs[],
         uint32 DataBufCount,
         const CssmData &Mac) = 0;
  virtual void ObtainPrivateKeyFromPublicKey(const CssmKey &PublicKey,
         CssmKey &PrivateKey) = 0;
  virtual void ChangeLoginOwner(const AccessCredentials &AccessCred,
         const CSSM_ACL_OWNER_PROTOTYPE &NewOwner) = 0;
  virtual void SignDataInit(CSSM_CC_HANDLE CCHandle,
         const Context &Context) = 0;
  virtual void DecryptDataInit(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         CSSM_PRIVILEGE Privilege) = 0;
  virtual void EventNotify(CSSM_CONTEXT_EVENT Event,
         CSSM_CC_HANDLE CCHandle,
         const Context &Context) = 0;
  virtual void GetOperationalStatistics(CSPOperationalStatistics &Statistics) = 0;
  virtual void DigestData(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmData DataBufs[],
         uint32 DataBufCount,
         CssmData &Digest) = 0;
  virtual void GetLoginAcl(const CSSM_STRING *SelectionTag,
         uint32 &NumberOfAclInfos,
         CSSM_ACL_ENTRY_INFO_PTR &AclInfos) = 0;
  virtual void GetKeyOwner(const CssmKey &Key,
         CSSM_ACL_OWNER_PROTOTYPE &Owner) = 0;
  virtual void ChangeKeyOwner(const AccessCredentials &AccessCred,
         const CssmKey &Key,
         const CSSM_ACL_OWNER_PROTOTYPE &NewOwner) = 0;
  virtual void VerifyMacInit(CSSM_CC_HANDLE CCHandle,
         const Context &Context) = 0;
  virtual void DigestDataClone(CSSM_CC_HANDLE CCHandle,
         CSSM_CC_HANDLE ClonedCCHandle) = 0;
  virtual void GenerateMacUpdate(CSSM_CC_HANDLE CCHandle,
         const CssmData DataBufs[],
         uint32 DataBufCount) = 0;
  virtual void EncryptDataFinal(CSSM_CC_HANDLE CCHandle,
         CssmData &RemData) = 0;
  virtual void EncryptDataInit(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         CSSM_PRIVILEGE Privilege) = 0;
  virtual void VerifyData(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmData DataBufs[],
         uint32 DataBufCount,
         CSSM_ALGORITHMS DigestAlgorithm,
         const CssmData &Signature) = 0;
  virtual void UnwrapKey(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmKey *PublicKey,
         const CssmKey &WrappedKey,
         uint32 KeyUsage,
         uint32 KeyAttr,
         const CssmData *KeyLabel,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         CssmKey &UnwrappedKey,
         CssmData &DescriptiveData,
         CSSM_PRIVILEGE Privilege) = 0;
  virtual void GenerateMacFinal(CSSM_CC_HANDLE CCHandle,
         CssmData &Mac) = 0;
  virtual void WrapKey(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const AccessCredentials &AccessCred,
         const CssmKey &Key,
         const CssmData *DescriptiveData,
         CssmKey &WrappedKey,
         CSSM_PRIVILEGE Privilege) = 0;
  virtual void DecryptDataFinal(CSSM_CC_HANDLE CCHandle,
         CssmData &RemData) = 0;
  virtual void SignData(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmData DataBufs[],
         uint32 DataBufCount,
         CSSM_ALGORITHMS DigestAlgorithm,
         CssmData &Signature) = 0;
  virtual void SignDataUpdate(CSSM_CC_HANDLE CCHandle,
         const CssmData DataBufs[],
         uint32 DataBufCount) = 0;
  virtual void Logout() = 0;
  virtual void DecryptData(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmData CipherBufs[],
         uint32 CipherBufCount,
         CssmData ClearBufs[],
         uint32 ClearBufCount,
         CSSM_SIZE &bytesDecrypted,
         CssmData &RemData,
         CSSM_PRIVILEGE Privilege) = 0;
  virtual void QueryKeySizeInBits(CSSM_CC_HANDLE CCHandle,
         const Context *Context,
         const CssmKey *Key,
         CSSM_KEY_SIZE &KeySize) = 0;
  virtual void DigestDataInit(CSSM_CC_HANDLE CCHandle,
         const Context &Context) = 0;
  virtual void DigestDataFinal(CSSM_CC_HANDLE CCHandle,
         CssmData &Digest) = 0;
  virtual void Login(const AccessCredentials &AccessCred,
         const CssmData *LoginName,
         const void *Reserved) = 0;
  virtual void ChangeKeyAcl(const AccessCredentials &AccessCred,
         const CSSM_ACL_EDIT &AclEdit,
         const CssmKey &Key) = 0;
  virtual void GetKeyAcl(const CssmKey &Key,
         const CSSM_STRING *SelectionTag,
         uint32 &NumberOfAclInfos,
         CSSM_ACL_ENTRY_INFO_PTR &AclInfos) = 0;
  virtual void GenerateAlgorithmParams(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         uint32 ParamBits,
         CssmData &Param,
         uint32 &NumberOfUpdatedAttibutes,
         CSSM_CONTEXT_ATTRIBUTE_PTR &UpdatedAttributes) = 0;
  virtual void GetLoginOwner(CSSM_ACL_OWNER_PROTOTYPE &Owner) = 0;
  virtual void VerifyDevice(const CssmData &DeviceCert) = 0;
  virtual void EncryptDataUpdate(CSSM_CC_HANDLE CCHandle,
         const CssmData ClearBufs[],
         uint32 ClearBufCount,
         CssmData CipherBufs[],
         uint32 CipherBufCount,
         CSSM_SIZE &bytesEncrypted) = 0;
  virtual void VerifyDataInit(CSSM_CC_HANDLE CCHandle,
         const Context &Context) = 0;
  virtual void DecryptDataUpdate(CSSM_CC_HANDLE CCHandle,
         const CssmData CipherBufs[],
         uint32 CipherBufCount,
         CssmData ClearBufs[],
         uint32 ClearBufCount,
         CSSM_SIZE &bytesDecrypted) = 0;
  virtual void ChangeLoginAcl(const AccessCredentials &AccessCred,
         const CSSM_ACL_EDIT &AclEdit) = 0;
  virtual void DigestDataUpdate(CSSM_CC_HANDLE CCHandle,
         const CssmData DataBufs[],
         uint32 DataBufCount) = 0;
  virtual void GenerateMacInit(CSSM_CC_HANDLE CCHandle,
         const Context &Context) = 0;
  virtual void QuerySize(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         CSSM_BOOL Encrypt,
         uint32 QuerySizeCount,
         QuerySizeData *DataBlock) = 0;
  virtual void RetrieveCounter(CssmData &Counter) = 0;
  virtual void DeriveKey(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         CssmData &Param,
         uint32 KeyUsage,
         uint32 KeyAttr,
         const CssmData *KeyLabel,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         CssmKey &DerivedKey) = 0;
  virtual void GenerateKey(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         uint32 KeyUsage,
         uint32 KeyAttr,
         const CssmData *KeyLabel,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         CssmKey &Key,
         CSSM_PRIVILEGE Privilege) = 0;
  virtual void FreeKey(const AccessCredentials *AccessCred,
         CssmKey &KeyPtr,
         CSSM_BOOL Delete) = 0;
  virtual void PassThrough(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         uint32 PassThroughId,
         const void *InData,
         void **OutData) = 0;
  virtual void VerifyMacUpdate(CSSM_CC_HANDLE CCHandle,
         const CssmData DataBufs[],
         uint32 DataBufCount) = 0;
  virtual void VerifyDataFinal(CSSM_CC_HANDLE CCHandle,
         const CssmData &Signature) = 0;
  virtual void GenerateKeyPair(CSSM_CC_HANDLE CCHandle,
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
         CSSM_PRIVILEGE Privilege) = 0;
  virtual void GetTimeValue(CSSM_ALGORITHMS TimeAlgorithm,
         CssmData &TimeData) = 0;
  virtual void EncryptData(CSSM_CC_HANDLE CCHandle,
         const Context &Context,
         const CssmData ClearBufs[],
         uint32 ClearBufCount,
         CssmData CipherBufs[],
         uint32 CipherBufCount,
         CSSM_SIZE &bytesEncrypted,
         CssmData &RemData,
         CSSM_PRIVILEGE Privilege) = 0;
};

} // end namespace Security

#endif //_H_CSPABSTRACTSESSION
