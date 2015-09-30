/*
 * Copyright (c) 2004,2008,2011 Apple Inc. All Rights Reserved.
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
// SDDLSession.h - DL session for security server CSP/DL.
//
#ifndef _H_SDDLSESSION
#define _H_SDDLSESSION

#include <security_cdsa_plugin/DLsession.h>
#include <security_cdsa_utilities/u32handleobject.h>
#include <securityd_client/ssclient.h>

class SDCSPDLPlugin;
class SDCSPDLSession;

class SDDLSession : public DLPluginSession
{
public:
	SDCSPDLSession &mSDCSPDLSession;

	SDDLSession(CSSM_MODULE_HANDLE handle,
				SDCSPDLPlugin &plug,
				const CSSM_VERSION &version,
				uint32 subserviceId,
				CSSM_SERVICE_TYPE subserviceType,
				CSSM_ATTACH_FLAGS attachFlags,
				const CSSM_UPCALLS &upcalls,
				DatabaseManager &databaseManager,
				SDCSPDLSession &ssCSPDLSession);
	~SDDLSession();

	SecurityServer::ClientSession &clientSession()
	{ return mClientSession; }
    void GetDbNames(CSSM_NAME_LIST_PTR &NameList);
    void FreeNameList(CSSM_NAME_LIST &NameList);
    void DbDelete(const char *DbName,
                  const CSSM_NET_ADDRESS *DbLocation,
                  const AccessCredentials *AccessCred);
    void DbCreate(const char *DbName,
                  const CSSM_NET_ADDRESS *DbLocation,
                  const CSSM_DBINFO &DBInfo,
                  CSSM_DB_ACCESS_TYPE AccessRequest,
                  const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
                  const void *OpenParameters,
                  CSSM_DB_HANDLE &DbHandle);
    void DbOpen(const char *DbName,
                const CSSM_NET_ADDRESS *DbLocation,
                CSSM_DB_ACCESS_TYPE AccessRequest,
                const AccessCredentials *AccessCred,
                const void *OpenParameters,
                CSSM_DB_HANDLE &DbHandle);
    void DbClose(CSSM_DB_HANDLE DBHandle);
    void CreateRelation(CSSM_DB_HANDLE DBHandle,
                        CSSM_DB_RECORDTYPE RelationID,
                        const char *RelationName,
                        uint32 NumberOfAttributes,
                        const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
                        uint32 NumberOfIndexes,
                        const CSSM_DB_SCHEMA_INDEX_INFO &pIndexInfo);
    void DestroyRelation(CSSM_DB_HANDLE DBHandle,
                         CSSM_DB_RECORDTYPE RelationID);

    void Authenticate(CSSM_DB_HANDLE DBHandle,
                      CSSM_DB_ACCESS_TYPE AccessRequest,
                      const AccessCredentials &AccessCred);
    void GetDbAcl(CSSM_DB_HANDLE DBHandle,
                  const CSSM_STRING *SelectionTag,
                  uint32 &NumberOfAclInfos,
                  CSSM_ACL_ENTRY_INFO_PTR &AclInfos);
    void ChangeDbAcl(CSSM_DB_HANDLE DBHandle,
                     const AccessCredentials &AccessCred,
                     const CSSM_ACL_EDIT &AclEdit);
    void GetDbOwner(CSSM_DB_HANDLE DBHandle,
                    CSSM_ACL_OWNER_PROTOTYPE &Owner);
    void ChangeDbOwner(CSSM_DB_HANDLE DBHandle,
                       const AccessCredentials &AccessCred,
                       const CSSM_ACL_OWNER_PROTOTYPE &NewOwner);
    void GetDbNameFromHandle(CSSM_DB_HANDLE DBHandle,
                             char **DbName);
    void DataInsert(CSSM_DB_HANDLE DBHandle,
                    CSSM_DB_RECORDTYPE RecordType,
                    const CSSM_DB_RECORD_ATTRIBUTE_DATA *Attributes,
                    const CssmData *Data,
                    CSSM_DB_UNIQUE_RECORD_PTR &UniqueId);
    void DataDelete(CSSM_DB_HANDLE DBHandle,
                    const CSSM_DB_UNIQUE_RECORD &UniqueRecordIdentifier);
    void DataModify(CSSM_DB_HANDLE DBHandle,
                    CSSM_DB_RECORDTYPE RecordType,
                    CSSM_DB_UNIQUE_RECORD &UniqueRecordIdentifier,
                    const CSSM_DB_RECORD_ATTRIBUTE_DATA *AttributesToBeModified,
                    const CssmData *DataToBeModified,
                    CSSM_DB_MODIFY_MODE ModifyMode);
    CSSM_HANDLE DataGetFirst(CSSM_DB_HANDLE DBHandle,
                             const CssmQuery *Query,
                             CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
                             CssmData *Data,
                             CSSM_DB_UNIQUE_RECORD_PTR &UniqueId);
    bool DataGetNext(CSSM_DB_HANDLE DBHandle,
                     CSSM_HANDLE ResultsHandle,
                     CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
                     CssmData *Data,
                     CSSM_DB_UNIQUE_RECORD_PTR &UniqueId);
    void DataAbortQuery(CSSM_DB_HANDLE DBHandle,
                        CSSM_HANDLE ResultsHandle);
    void DataGetFromUniqueRecordId(CSSM_DB_HANDLE DBHandle,
                                   const CSSM_DB_UNIQUE_RECORD &UniqueRecord,
                                   CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
                                   CssmData *Data);
    void FreeUniqueRecord(CSSM_DB_HANDLE DBHandle,
                          CSSM_DB_UNIQUE_RECORD &UniqueRecord);
    void PassThrough(CSSM_DB_HANDLE DBHandle,
                     uint32 PassThroughId,
                     const void *InputParams,
                     void **OutputParams);

	Allocator &allocator() { return *static_cast<DatabaseSession *>(this); }

protected:
	void postGetRecord(SecurityServer::RecordHandle record, U32HandleObject::Handle resultsHandle,
					   CSSM_DB_HANDLE db,
					   CssmDbRecordAttributeData *pAttributes,
					   CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
					   CssmData *inoutData, SecurityServer::KeyHandle hKey);

	CSSM_DB_UNIQUE_RECORD_PTR makeDbUniqueRecord(SecurityServer::RecordHandle recordHandle);
	CSSM_HANDLE findDbUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord);
	void freeDbUniqueRecord(CSSM_DB_UNIQUE_RECORD &inUniqueRecord);

	SecurityServer::ClientSession mClientSession;
    //SecurityServer::AttachmentHandle mAttachment;
};


#endif // _H_SDDLSESSION
