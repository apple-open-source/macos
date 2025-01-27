//
// DL plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#ifndef _H_DLABSTRACTSESSION
#define _H_DLABSTRACTSESSION

#include <security_cdsa_plugin/pluginsession.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/cssmdb.h>


namespace Security {


//
// A pure abstract class to define the DL module interface
//
class DLAbstractPluginSession {
public:
	virtual ~DLAbstractPluginSession();
  virtual void FreeNameList(CSSM_NAME_LIST &NameList) = 0;
  virtual void CreateRelation(CSSM_DB_HANDLE DBHandle,
         CSSM_DB_RECORDTYPE RelationID,
         const char *RelationName,
         uint32 NumberOfAttributes,
         const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
         uint32 NumberOfIndexes,
         const CSSM_DB_SCHEMA_INDEX_INFO &pIndexInfo) = 0;
  virtual void DataModify(CSSM_DB_HANDLE DBHandle,
         CSSM_DB_RECORDTYPE RecordType,
         CSSM_DB_UNIQUE_RECORD &UniqueRecordIdentifier,
         const CSSM_DB_RECORD_ATTRIBUTE_DATA *AttributesToBeModified,
         const CssmData *DataToBeModified,
         CSSM_DB_MODIFY_MODE ModifyMode) = 0;
  virtual void DataAbortQuery(CSSM_DB_HANDLE DBHandle,
         CSSM_HANDLE ResultsHandle) = 0;
  virtual void DestroyRelation(CSSM_DB_HANDLE DBHandle,
         CSSM_DB_RECORDTYPE RelationID) = 0;
  virtual void DbCreate(const char *DbName,
         const CSSM_NET_ADDRESS *DbLocation,
         const CSSM_DBINFO &DBInfo,
         CSSM_DB_ACCESS_TYPE AccessRequest,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         const void *OpenParameters,
         CSSM_DB_HANDLE &DbHandle) = 0;
  virtual CSSM_HANDLE DataGetFirst(CSSM_DB_HANDLE DBHandle,
         const CssmQuery *Query,
                  CSSM_DB_RECORD_ATTRIBUTE_DATA *Attributes,
         CssmData *Data,
         CSSM_DB_UNIQUE_RECORD_PTR &UniqueId) = 0;
  virtual void GetDbNames(CSSM_NAME_LIST_PTR &NameList) = 0;
  virtual void DbClose(CSSM_DB_HANDLE DBHandle) = 0;
  virtual void GetDbNameFromHandle(CSSM_DB_HANDLE DBHandle,
         char **DbName) = 0;
  virtual void DataInsert(CSSM_DB_HANDLE DBHandle,
         CSSM_DB_RECORDTYPE RecordType,
         const CSSM_DB_RECORD_ATTRIBUTE_DATA *Attributes,
         const CssmData *Data,
         CSSM_DB_UNIQUE_RECORD_PTR &UniqueId) = 0;
  virtual void ChangeDbAcl(CSSM_DB_HANDLE DBHandle,
         const AccessCredentials &AccessCred,
         const CSSM_ACL_EDIT &AclEdit) = 0;
  virtual void PassThrough(CSSM_DB_HANDLE DBHandle,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams) = 0;
  virtual void GetDbAcl(CSSM_DB_HANDLE DBHandle,
         const CSSM_STRING *SelectionTag,
         uint32 &NumberOfAclInfos,
         CSSM_ACL_ENTRY_INFO_PTR &AclInfos) = 0;
  virtual void DataGetFromUniqueRecordId(CSSM_DB_HANDLE DBHandle,
         const CSSM_DB_UNIQUE_RECORD &UniqueRecord,
         CSSM_DB_RECORD_ATTRIBUTE_DATA *Attributes,
         CssmData *Data) = 0;
  virtual void DbOpen(const char *DbName,
         const CSSM_NET_ADDRESS *DbLocation,
         CSSM_DB_ACCESS_TYPE AccessRequest,
         const AccessCredentials *AccessCred,
         const void *OpenParameters,
         CSSM_DB_HANDLE &DbHandle) = 0;
  virtual bool DataGetNext(CSSM_DB_HANDLE DBHandle,
         CSSM_HANDLE ResultsHandle,
         CSSM_DB_RECORD_ATTRIBUTE_DATA *Attributes,
         CssmData *Data,
         CSSM_DB_UNIQUE_RECORD_PTR &UniqueId) = 0;
  virtual void FreeUniqueRecord(CSSM_DB_HANDLE DBHandle,
         CSSM_DB_UNIQUE_RECORD &UniqueRecord) = 0;
  virtual void ChangeDbOwner(CSSM_DB_HANDLE DBHandle,
         const AccessCredentials &AccessCred,
         const CSSM_ACL_OWNER_PROTOTYPE &NewOwner) = 0;
  virtual void Authenticate(CSSM_DB_HANDLE DBHandle,
         CSSM_DB_ACCESS_TYPE AccessRequest,
         const AccessCredentials &AccessCred) = 0;
  virtual void DbDelete(const char *DbName,
         const CSSM_NET_ADDRESS *DbLocation,
         const AccessCredentials *AccessCred) = 0;
  virtual void DataDelete(CSSM_DB_HANDLE DBHandle,
         const CSSM_DB_UNIQUE_RECORD &UniqueRecordIdentifier) = 0;
  virtual void GetDbOwner(CSSM_DB_HANDLE DBHandle,
         CSSM_ACL_OWNER_PROTOTYPE &Owner) = 0;
};

} // end namespace Security

#endif //_H_DLABSTRACTSESSION
