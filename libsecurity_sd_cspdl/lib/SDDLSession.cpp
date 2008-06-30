/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
#include "SDDLSession.h"

#include "SDCSPDLPlugin.h"
#include "SDKey.h"
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_utilities/trackingallocator.h>

using namespace CssmClient;
using namespace SecurityServer;
using namespace std;

//
// SDDLSession -- Security Server DL session
//
SDDLSession::SDDLSession(CSSM_MODULE_HANDLE handle,
                         SDCSPDLPlugin &plug,
                         const CSSM_VERSION &version,
                         uint32 subserviceId,
                         CSSM_SERVICE_TYPE subserviceType,
                         CSSM_ATTACH_FLAGS attachFlags,
                         const CSSM_UPCALLS &upcalls,
                         DatabaseManager &databaseManager,
                         SDCSPDLSession &ssCSPDLSession) :
DLPluginSession(handle, plug, version, subserviceId, subserviceType,
                attachFlags, upcalls, databaseManager),
mSDCSPDLSession(ssCSPDLSession),
mClientSession(Allocator::standard(), static_cast<PluginSession &>(*this))
//mAttachment(mClientSession.attach(version, subserviceId, subserviceType, attachFlags))
{
}

SDDLSession::~SDDLSession()
{
}

// Utility functions
void
SDDLSession::GetDbNames(CSSM_NAME_LIST_PTR &outNameList)
{
	outNameList->String = this->PluginSession::alloc<char *>();
	outNameList->NumStrings = 1;
	outNameList->String[0] = (char*) "";	// empty name will trigger dynamic lookup
}


void
SDDLSession::FreeNameList(CSSM_NAME_LIST &inNameList)
{
	this->PluginSession::free(inNameList.String);
}


void
SDDLSession::DbDelete(const char *inDbName,
                      const CSSM_NET_ADDRESS *inDbLocation,
                      const AccessCredentials *inAccessCred)
{
	unimplemented();
}

// DbContext creation and destruction.
void
SDDLSession::DbCreate(const char *inDbName,
                      const CSSM_NET_ADDRESS *inDbLocation,
                      const CSSM_DBINFO &inDBInfo,
                      CSSM_DB_ACCESS_TYPE inAccessRequest,
                      const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
                      const void *inOpenParameters,
                      CSSM_DB_HANDLE &outDbHandle)
{
	unimplemented();
}

void
SDDLSession::DbOpen(const char *inDbName,
                    const CSSM_NET_ADDRESS *inDbLocation,
                    CSSM_DB_ACCESS_TYPE inAccessRequest,
                    const AccessCredentials *inAccessCred,
                    const void *inOpenParameters,
                    CSSM_DB_HANDLE &outDbHandle)
{
    outDbHandle = mClientSession.openToken(subserviceId(), inAccessCred, inDbName);
}

// Operations using DbContext instances.
void
SDDLSession::DbClose(CSSM_DB_HANDLE inDbHandle)
{
    mClientSession.releaseDb(inDbHandle);
}

void
SDDLSession::CreateRelation(CSSM_DB_HANDLE inDbHandle,
                            CSSM_DB_RECORDTYPE inRelationID,
                            const char *inRelationName,
                            uint32 inNumberOfAttributes,
                            const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
                            uint32 inNumberOfIndexes,
                            const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo)
{
	unimplemented();
}

void
SDDLSession::DestroyRelation(CSSM_DB_HANDLE inDbHandle,
                             CSSM_DB_RECORDTYPE inRelationID)
{
	unimplemented();
}

void
SDDLSession::Authenticate(CSSM_DB_HANDLE inDbHandle,
                          CSSM_DB_ACCESS_TYPE inAccessRequest,
                          const AccessCredentials &inAccessCred)
{
    mClientSession.authenticateDb(inDbHandle, inAccessRequest, &inAccessCred);
}


void
SDDLSession::GetDbAcl(CSSM_DB_HANDLE inDbHandle,
                      const CSSM_STRING *inSelectionTag,
                      uint32 &outNumberOfAclInfos,
                      CSSM_ACL_ENTRY_INFO_PTR &outAclInfos)
{
    // @@@ inSelectionTag shouldn't be a CSSM_STRING * but just a CSSM_STRING.
    mClientSession.getDbAcl(inDbHandle, *inSelectionTag, outNumberOfAclInfos,
                            AclEntryInfo::overlayVar(outAclInfos));
}

void
SDDLSession::ChangeDbAcl(CSSM_DB_HANDLE inDbHandle,
                         const AccessCredentials &inAccessCred,
                         const CSSM_ACL_EDIT &inAclEdit)
{
    mClientSession.changeDbAcl(inDbHandle, inAccessCred, AclEdit::overlay(inAclEdit));
}

void
SDDLSession::GetDbOwner(CSSM_DB_HANDLE inDbHandle,
                        CSSM_ACL_OWNER_PROTOTYPE &outOwner)
{
    mClientSession.getDbOwner(inDbHandle, AclOwnerPrototype::overlay(outOwner));
}

void
SDDLSession::ChangeDbOwner(CSSM_DB_HANDLE inDbHandle,
                           const AccessCredentials &inAccessCred,
                           const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner)
{
    mClientSession.changeDbOwner(inDbHandle, inAccessCred, AclOwnerPrototype::overlay(inNewOwner));
}

void
SDDLSession::GetDbNameFromHandle(CSSM_DB_HANDLE inDbHandle,
                                 char **outDbName)
{
	string name;
	mClientSession.getDbName(inDbHandle, name);
	memcpy(Required(outDbName) = static_cast<char *>(this->malloc(name.length() + 1)),
		name.c_str(), name.length() + 1);
}

void
SDDLSession::DataInsert(CSSM_DB_HANDLE inDbHandle,
                        CSSM_DB_RECORDTYPE inRecordType,
                        const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                        const CssmData *inData,
                        CSSM_DB_UNIQUE_RECORD_PTR &outUniqueId)
{
    RecordHandle record;
    record = mClientSession.insertRecord(inDbHandle,
                                         inRecordType,
                                         CssmDbRecordAttributeData::overlay(inAttributes),
                                         inData);
    outUniqueId = makeDbUniqueRecord(record);
}

void
SDDLSession::DataDelete(CSSM_DB_HANDLE inDbHandle,
                        const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
    RecordHandle record = findDbUniqueRecord(inUniqueRecordIdentifier);
    mClientSession.deleteRecord(inDbHandle, record);
}


void
SDDLSession::DataModify(CSSM_DB_HANDLE inDbHandle,
                        CSSM_DB_RECORDTYPE inRecordType,
                        CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
                        const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
                        const CssmData *inDataToBeModified,
                        CSSM_DB_MODIFY_MODE inModifyMode)
{
    RecordHandle record = findDbUniqueRecord(inoutUniqueRecordIdentifier);
    mClientSession.modifyRecord(inDbHandle, record, inRecordType,
		CssmDbRecordAttributeData::overlay(inAttributesToBeModified),
        inDataToBeModified, inModifyMode);
	//@@@ make a (new) unique record out of possibly modified "record"...
}

void
SDDLSession::postGetRecord(RecordHandle record, CSSM_HANDLE resultsHandle,
						   CSSM_DB_HANDLE db,
						   CssmDbRecordAttributeData *pAttributes,
						   CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
						   CssmData *inoutData, KeyHandle hKey)
{
	// If the client didn't ask for data then it doesn't matter
	// if this record is a key or not, just return it.
	if (inoutData)
	{
		CSSM_DB_RECORDTYPE recordType = pAttributes->DataRecordType;
		if (!inoutAttributes)
		{
			// @@@ Free pAttributes
		}

		if (recordType == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| recordType == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| recordType == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		{
			// This record is a key. The data returned is a CSSM_KEY
			// (with empty key data) with the header filled in.
			try
			{
			if (hKey == noKey)	// tokend error - should have returned key handle
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			// Allocate storage for the key.
			CssmKey *outKey = inoutData->interpretedAs<CssmKey>();
			new SDKey(*this, *outKey, hKey, db, record, recordType, *inoutData);
			}
			catch (...)
			{
				try { mClientSession.releaseRecord(record); }
				catch(...) { secdebug("ssCrypt", "releaseRecord threw during catch"); }
				if (resultsHandle != CSSM_INVALID_HANDLE)
				{
					try { mClientSession.releaseSearch(resultsHandle); }
					catch(...) { secdebug("ssCrypt", "releaseSearch threw during catch"); }
				}
				throw;
			}
		} else {	// not a key
			if (hKey != noKey) {
				try { mClientSession.releaseRecord(record); }
				catch(...) { secdebug("ssCrypt", "failed releasing bogus key handle"); }
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
		}
    }
}

CSSM_HANDLE
SDDLSession::DataGetFirst(CSSM_DB_HANDLE inDbHandle,
                          const CssmQuery *inQuery,
                          CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                          CssmData *inoutData,
                          CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
    // Setup so we always retrieve the attributes if the client asks for data,
	// even if the client doesn't want them so we can figure out if we just
	// retrieved a key.
    CssmDbRecordAttributeData attributes;
    CssmDbRecordAttributeData *pAttributes;
    if (inoutAttributes)
		pAttributes = CssmDbRecordAttributeData::overlay(inoutAttributes);
	else
    {
        pAttributes = &attributes;
        memset(pAttributes, 0, sizeof(attributes));
    }

    RecordHandle record;
    CSSM_HANDLE resultsHandle = CSSM_INVALID_HANDLE;
	KeyHandle keyId = noKey;
	record = mClientSession.findFirst(inDbHandle,
										   CssmQuery::required(inQuery),
										   resultsHandle,
										   pAttributes,
										   inoutData, keyId);
    if (!record)
		return CSSM_INVALID_HANDLE;

	postGetRecord(record, resultsHandle, inDbHandle, pAttributes, inoutAttributes, inoutData, keyId);
    outUniqueRecord = makeDbUniqueRecord(record);
	return resultsHandle;
}

bool
SDDLSession::DataGetNext(CSSM_DB_HANDLE inDbHandle,
                         CSSM_HANDLE inResultsHandle,
                         CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                         CssmData *inoutData,
                         CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
    // Setup so we always retrieve the attributes if the client asks for data,
	// even if the client doesn't want them so we can figure out if we just
	// retrieved a key.
    CssmDbRecordAttributeData attributes;
    CssmDbRecordAttributeData *pAttributes;
    if (inoutAttributes)
		pAttributes = CssmDbRecordAttributeData::overlay(inoutAttributes);
	else
    {
        pAttributes = &attributes;
        memset(pAttributes, 0, sizeof(attributes));
    }
	
    RecordHandle record;
	KeyHandle keyId = noKey;
    record = mClientSession.findNext(inResultsHandle,
										  pAttributes,
										  inoutData, keyId);
    if (!record)
		return false;

	postGetRecord(record, CSSM_INVALID_HANDLE, inDbHandle, pAttributes,
		inoutAttributes, inoutData, keyId);
    outUniqueRecord = makeDbUniqueRecord(record);
    return true;
}

void
SDDLSession::DataAbortQuery(CSSM_DB_HANDLE inDbHandle,
                            CSSM_HANDLE inResultsHandle)
{
	mClientSession.releaseSearch(inResultsHandle);
}

void
SDDLSession::DataGetFromUniqueRecordId(CSSM_DB_HANDLE inDbHandle,
                                       const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
                                       CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                                       CssmData *inoutData)
{
    // Setup so we always retrieve the attributes if the client asks for data,
	// even if the client doesn't want them so we can figure out if we just
	// retrieved a key.
    CssmDbRecordAttributeData attributes;
    CssmDbRecordAttributeData *pAttributes;
    if (inoutAttributes)
		pAttributes = CssmDbRecordAttributeData::overlay(inoutAttributes);
	else
    {
        pAttributes = &attributes;
        memset(pAttributes, 0, sizeof(attributes));
    }

	RecordHandle record = findDbUniqueRecord(inUniqueRecord);
	KeyHandle keyId = noKey;
	mClientSession.findRecordHandle(record, pAttributes, inoutData, keyId);
	postGetRecord(record, CSSM_INVALID_HANDLE, inDbHandle, pAttributes,
		inoutAttributes, inoutData, keyId);
}

void
SDDLSession::FreeUniqueRecord(CSSM_DB_HANDLE inDbHandle,
                              CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
    RecordHandle record = findDbUniqueRecord(inUniqueRecordIdentifier);
    freeDbUniqueRecord(inUniqueRecordIdentifier);
	mClientSession.releaseRecord(record);
}

void
SDDLSession::PassThrough(CSSM_DB_HANDLE inDbHandle,
                         uint32 inPassThroughId,
                         const void *inInputParams,
                         void **outOutputParams)
{
    switch (inPassThroughId)
    {
		case CSSM_APPLECSPDL_DB_LOCK:
			mClientSession.lock(inDbHandle);
			break;
		case CSSM_APPLECSPDL_DB_UNLOCK:
		{
			TrackingAllocator track(Allocator::standard());
			AutoCredentials creds(track);
			creds.tag("PIN1");
			if (inInputParams)
				creds += TypedList(track, CSSM_SAMPLE_TYPE_PASSWORD,
					new (track) ListElement(track,
					*reinterpret_cast<const CssmData *>(inInputParams)));
			else
				creds += TypedList(track, CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD,
					new (track) ListElement(track, CssmData()));

			Authenticate(inDbHandle, CSSM_DB_ACCESS_READ, creds);
			break;
		}
		case CSSM_APPLECSPDL_DB_IS_LOCKED:
		{
			if (!outOutputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_OUTPUT_POINTER);

			bool isLocked = mClientSession.isLocked(inDbHandle);
			CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS_PTR params =
				allocator().alloc<CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS>();
			params->isLocked = isLocked;
			*reinterpret_cast<CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS_PTR *>
				(outOutputParams) = params;
			break;
		}
		case CSSM_APPLECSPDL_DB_CHANGE_PASSWORD:
		{
			if (!inInputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

			const CSSM_APPLECSPDL_DB_CHANGE_PASSWORD_PARAMETERS *params =
				reinterpret_cast
				<const CSSM_APPLECSPDL_DB_CHANGE_PASSWORD_PARAMETERS *>
				(inInputParams);

			AutoAclEntryInfoList acls /* (mClientSession.allocator()) */;
			CSSM_STRING tag = { 'P', 'I', 'N', '1' };
			GetDbAcl(inDbHandle, &tag,
				*static_cast<uint32 *>(acls),
				*static_cast<CSSM_ACL_ENTRY_INFO **>(acls));
			if (acls.size() == 0)
				CssmError::throwMe(CSSM_ERRCODE_ACL_ENTRY_TAG_NOT_FOUND);

			const AclEntryInfo &slot = acls.at(0);
			if (acls.size() > 1)
				secdebug("acl",
					"Using entry handle %ld from %d total candidates",
					slot.handle(), acls.size());
			AclEdit edit(slot.handle(), slot.proto());
			ChangeDbAcl(inDbHandle,
				AccessCredentials::required(params->accessCredentials), edit);
			break;
		}
        default:
			CssmError::throwMe(CSSM_ERRCODE_INVALID_PASSTHROUGH_ID);
    }
}

CSSM_DB_UNIQUE_RECORD_PTR
SDDLSession::makeDbUniqueRecord(RecordHandle uniqueId)
{
    CSSM_DB_UNIQUE_RECORD *aUniqueRecord = allocator().alloc<CSSM_DB_UNIQUE_RECORD>();
    memset(aUniqueRecord, 0, sizeof(CSSM_DB_UNIQUE_RECORD));
    aUniqueRecord->RecordIdentifier.Length = sizeof(CSSM_HANDLE);
    try
    {
        aUniqueRecord->RecordIdentifier.Data = allocator().alloc<uint8>(sizeof(CSSM_HANDLE));
        *reinterpret_cast<CSSM_HANDLE *>(aUniqueRecord->RecordIdentifier.Data) = uniqueId;
    }
    catch(...)
    {
        allocator().free(aUniqueRecord);
        throw;
    }
	
    return aUniqueRecord;
}

RecordHandle
SDDLSession::findDbUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
    if (inUniqueRecord.RecordIdentifier.Length != sizeof(CSSM_HANDLE))
        CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);
    
    return *reinterpret_cast<CSSM_HANDLE *>(inUniqueRecord.RecordIdentifier.Data);
}

void
SDDLSession::freeDbUniqueRecord(CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
    if (inUniqueRecord.RecordIdentifier.Length != 0
        && inUniqueRecord.RecordIdentifier.Data != NULL)
    {
        inUniqueRecord.RecordIdentifier.Length = 0;
        allocator().free(inUniqueRecord.RecordIdentifier.Data);
    }
    allocator().free(&inUniqueRecord);
}
