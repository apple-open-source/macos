//
// DL plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#include <security_cdsa_plugin/DLsession.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/cssmdli.h>


DLAbstractPluginSession::~DLAbstractPluginSession()
{ /* virtual */ }

static CSSM_RETURN CSSMDLI cssm_DataGetFirst(CSSM_DL_DB_HANDLE DLDBHandle,
         const CSSM_QUERY *Query,
         CSSM_HANDLE_PTR ResultsHandle,
         CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
         CSSM_DATA_PTR Data,
         CSSM_DB_UNIQUE_RECORD_PTR *UniqueId)
{
  BEGIN_API
  if ((Required(ResultsHandle) = findSession<DLPluginSession>(DLDBHandle.DLHandle).DataGetFirst(DLDBHandle.DBHandle,
			CssmQuery::optional(Query),
			Attributes,
			CssmData::optional(Data),
			Required(UniqueId))) == CSSM_INVALID_HANDLE)
    return CSSMERR_DL_ENDOFDATA;
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_GetDbNames(CSSM_DL_HANDLE DLHandle,
         CSSM_NAME_LIST_PTR *NameList)
{
  BEGIN_API
  findSession<DLPluginSession>(DLHandle).GetDbNames(Required(NameList));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DbClose(CSSM_DL_DB_HANDLE DLDBHandle)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).DbClose(DLDBHandle.DBHandle);
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DataInsert(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_DB_RECORDTYPE RecordType,
         const CSSM_DB_RECORD_ATTRIBUTE_DATA *Attributes,
         const CSSM_DATA *Data,
         CSSM_DB_UNIQUE_RECORD_PTR *UniqueId)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).DataInsert(DLDBHandle.DBHandle,
			RecordType,
			Attributes,
			CssmData::optional(Data),
			Required(UniqueId));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_GetDbNameFromHandle(CSSM_DL_DB_HANDLE DLDBHandle,
         char **DbName)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).GetDbNameFromHandle(DLDBHandle.DBHandle,
			DbName);
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_ChangeDbAcl(CSSM_DL_DB_HANDLE DLDBHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_ACL_EDIT *AclEdit)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).ChangeDbAcl(DLDBHandle.DBHandle,
			AccessCredentials::required(AccessCred),
			Required(AclEdit));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_FreeNameList(CSSM_DL_HANDLE DLHandle,
         CSSM_NAME_LIST_PTR NameList)
{
  BEGIN_API
  findSession<DLPluginSession>(DLHandle).FreeNameList(Required(NameList));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_CreateRelation(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_DB_RECORDTYPE RelationID,
         const char *RelationName,
         uint32 NumberOfAttributes,
         const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
         uint32 NumberOfIndexes,
         const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).CreateRelation(DLDBHandle.DBHandle,
			RelationID,
			RelationName,
			NumberOfAttributes,
			pAttributeInfo,
			NumberOfIndexes,
			Required(pIndexInfo));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DataAbortQuery(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_HANDLE ResultsHandle)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).DataAbortQuery(DLDBHandle.DBHandle,
			ResultsHandle);
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DataModify(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_DB_RECORDTYPE RecordType,
         CSSM_DB_UNIQUE_RECORD_PTR UniqueRecordIdentifier,
         const CSSM_DB_RECORD_ATTRIBUTE_DATA *AttributesToBeModified,
         const CSSM_DATA *DataToBeModified,
         CSSM_DB_MODIFY_MODE ModifyMode)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).DataModify(DLDBHandle.DBHandle,
			RecordType,
			Required(UniqueRecordIdentifier),
			AttributesToBeModified,
			CssmData::optional(DataToBeModified),
			ModifyMode);
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DestroyRelation(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_DB_RECORDTYPE RelationID)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).DestroyRelation(DLDBHandle.DBHandle,
			RelationID);
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DbCreate(CSSM_DL_HANDLE DLHandle,
         const char *DbName,
         const CSSM_NET_ADDRESS *DbLocation,
         const CSSM_DBINFO *DBInfo,
         CSSM_DB_ACCESS_TYPE AccessRequest,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
         const void *OpenParameters,
         CSSM_DB_HANDLE *DbHandle)
{
  BEGIN_API
  findSession<DLPluginSession>(DLHandle).DbCreate(DbName,
			DbLocation,
			Required(DBInfo),
			AccessRequest,
			CredAndAclEntry,
			OpenParameters,
			Required(DbHandle));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DbOpen(CSSM_DL_HANDLE DLHandle,
         const char *DbName,
         const CSSM_NET_ADDRESS *DbLocation,
         CSSM_DB_ACCESS_TYPE AccessRequest,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const void *OpenParameters,
         CSSM_DB_HANDLE *DbHandle)
{
  BEGIN_API
  findSession<DLPluginSession>(DLHandle).DbOpen(DbName,
			DbLocation,
			AccessRequest,
			AccessCredentials::optional(AccessCred),
			OpenParameters,
			Required(DbHandle));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DataGetNext(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_HANDLE ResultsHandle,
         CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
         CSSM_DATA_PTR Data,
         CSSM_DB_UNIQUE_RECORD_PTR *UniqueId)
{
  BEGIN_API
  if (!findSession<DLPluginSession>(DLDBHandle.DLHandle).DataGetNext(DLDBHandle.DBHandle,
			ResultsHandle,
			Attributes,
			CssmData::optional(Data),
			Required(UniqueId)))
    return CSSMERR_DL_ENDOFDATA;
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_FreeUniqueRecord(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_DB_UNIQUE_RECORD_PTR UniqueRecord)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).FreeUniqueRecord(DLDBHandle.DBHandle,
			Required(UniqueRecord));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_ChangeDbOwner(CSSM_DL_DB_HANDLE DLDBHandle,
         const CSSM_ACCESS_CREDENTIALS *AccessCred,
         const CSSM_ACL_OWNER_PROTOTYPE *NewOwner)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).ChangeDbOwner(DLDBHandle.DBHandle,
			AccessCredentials::required(AccessCred),
			Required(NewOwner));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DbDelete(CSSM_DL_HANDLE DLHandle,
         const char *DbName,
         const CSSM_NET_ADDRESS *DbLocation,
         const CSSM_ACCESS_CREDENTIALS *AccessCred)
{
  BEGIN_API
  findSession<DLPluginSession>(DLHandle).DbDelete(DbName,
			DbLocation,
			AccessCredentials::optional(AccessCred));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_Authenticate(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_DB_ACCESS_TYPE AccessRequest,
         const CSSM_ACCESS_CREDENTIALS *AccessCred)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).Authenticate(DLDBHandle.DBHandle,
			AccessRequest,
			AccessCredentials::required(AccessCred));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DataDelete(CSSM_DL_DB_HANDLE DLDBHandle,
         const CSSM_DB_UNIQUE_RECORD *UniqueRecordIdentifier)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).DataDelete(DLDBHandle.DBHandle,
			Required(UniqueRecordIdentifier));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_GetDbOwner(CSSM_DL_DB_HANDLE DLDBHandle,
         CSSM_ACL_OWNER_PROTOTYPE_PTR Owner)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).GetDbOwner(DLDBHandle.DBHandle,
			Required(Owner));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_PassThrough(CSSM_DL_DB_HANDLE DLDBHandle,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).PassThrough(DLDBHandle.DBHandle,
			PassThroughId,
			InputParams,
			OutputParams);
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_GetDbAcl(CSSM_DL_DB_HANDLE DLDBHandle,
         const CSSM_STRING *SelectionTag,
         uint32 *NumberOfAclInfos,
         CSSM_ACL_ENTRY_INFO_PTR *AclInfos)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).GetDbAcl(DLDBHandle.DBHandle,
			SelectionTag,
			Required(NumberOfAclInfos),
			Required(AclInfos));
  END_API(DL)
}

static CSSM_RETURN CSSMDLI cssm_DataGetFromUniqueRecordId(CSSM_DL_DB_HANDLE DLDBHandle,
         const CSSM_DB_UNIQUE_RECORD *UniqueRecord,
         CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
         CSSM_DATA_PTR Data)
{
  BEGIN_API
  findSession<DLPluginSession>(DLDBHandle.DLHandle).DataGetFromUniqueRecordId(DLDBHandle.DBHandle,
			Required(UniqueRecord),
			Attributes,
			CssmData::optional(Data));
  END_API(DL)
}


static const CSSM_SPI_DL_FUNCS DLFunctionStruct = {
  cssm_DbOpen,
  cssm_DbClose,
  cssm_DbCreate,
  cssm_DbDelete,
  cssm_CreateRelation,
  cssm_DestroyRelation,
  cssm_Authenticate,
  cssm_GetDbAcl,
  cssm_ChangeDbAcl,
  cssm_GetDbOwner,
  cssm_ChangeDbOwner,
  cssm_GetDbNames,
  cssm_GetDbNameFromHandle,
  cssm_FreeNameList,
  cssm_DataInsert,
  cssm_DataDelete,
  cssm_DataModify,
  cssm_DataGetFirst,
  cssm_DataGetNext,
  cssm_DataAbortQuery,
  cssm_DataGetFromUniqueRecordId,
  cssm_FreeUniqueRecord,
  cssm_PassThrough,
};

static CSSM_MODULE_FUNCS DLFunctionTable = {
  CSSM_SERVICE_DL,	// service type
  23,	// number of functions
  (const CSSM_PROC_ADDR *)&DLFunctionStruct
};

CSSM_MODULE_FUNCS_PTR DLPluginSession::construct()
{
   return &DLFunctionTable;
}
