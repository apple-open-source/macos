/*
 * Copyright (c) 2000-2001,2011-2014 Apple Inc. All Rights Reserved.
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
// SSDLSession.h - DL session for security server CSP/DL.
//
#include "SSDLSession.h"

#include "CSPDLPlugin.h"
#include "SSKey.h"
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/cssmapplePriv.h>

using namespace CssmClient;
using namespace SecurityServer;
using namespace std;

//
// SSDLSession -- Security Server DL session
//
SSDLSession::SSDLSession(CSSM_MODULE_HANDLE handle,
						 CSPDLPlugin &plug,
						 const CSSM_VERSION &version,
						 uint32 subserviceId,
						 CSSM_SERVICE_TYPE subserviceType,
						 CSSM_ATTACH_FLAGS attachFlags,
						 const CSSM_UPCALLS &upcalls,
						 DatabaseManager &databaseManager,
						 SSCSPDLSession &ssCSPDLSession)
: DLPluginSession(handle, plug, version, subserviceId, subserviceType,
				  attachFlags, upcalls, databaseManager),
  mSSCSPDLSession(ssCSPDLSession),
  mDL(Module(gGuidAppleFileDL, Cssm::standard())),
  mClientSession(Allocator::standard(), static_cast<PluginSession &>(*this))
{
	mClientSession.registerForAclEdits(SSCSPDLSession::didChangeKeyAclCallback, &mSSCSPDLSession);
	// @@@ mDL.allocator(*static_cast<DatabaseSession *>(this));
	mDL->allocator(allocator());
	mDL->version(version);
	mDL->subserviceId(subserviceId);
	mDL->flags(attachFlags);
	// fprintf(stderr, "%p: Created %p\n", pthread_self(), this);
}

SSDLSession::~SSDLSession()
try
{
	StLock<Mutex> _1(mSSUniqueRecordLock);
	mSSUniqueRecordMap.clear();

	StLock<Mutex> _2(mDbHandleLock);
	DbHandleMap::iterator end = mDbHandleMap.end();
	for (DbHandleMap::iterator it = mDbHandleMap.begin(); it != end; ++it)
		it->second->close();

	mDbHandleMap.clear();
	mDL->detach();
}
catch (...)
{
}

// Utility functions
void
SSDLSession::GetDbNames(CSSM_NAME_LIST_PTR &outNameList)
{
	// @@@ Fix client lib
	CSSM_DL_GetDbNames(mDL->handle(), &outNameList);
}


void
SSDLSession::FreeNameList(CSSM_NAME_LIST &inNameList)
{
	// @@@ Fix client lib
	CSSM_DL_FreeNameList(mDL->handle(), &inNameList);
}


void
SSDLSession::DbDelete(const char *inDbName,
					  const CSSM_NET_ADDRESS *inDbLocation,
					  const AccessCredentials *inAccessCred)
{
	SSDatabase db(mClientSession, mDL, inDbName, inDbLocation);
	db->accessCredentials(inAccessCred);
	db->deleteDb();
}

// DbContext creation and destruction.
void
SSDLSession::DbCreate(const char *inDbName,
					  const CSSM_NET_ADDRESS *inDbLocation,
					  const CSSM_DBINFO &inDBInfo,
					  CSSM_DB_ACCESS_TYPE inAccessRequest,
					  const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
					  const void *inOpenParameters,
					  CSSM_DB_HANDLE &outDbHandle)
{
	SSDatabase db(mClientSession, mDL, inDbName, inDbLocation);
	db->dbInfo(&inDBInfo);
	db->accessRequest(inAccessRequest);
	db->resourceControlContext(inCredAndAclEntry);
	db->openParameters(inOpenParameters);
	db->create(DLDbIdentifier(CssmSubserviceUid(plugin.myGuid(), &version(), subserviceId(),
												CSSM_SERVICE_DL | CSSM_SERVICE_CSP),
							  inDbName, inDbLocation));
	db->dbInfo(NULL);
	outDbHandle = makeDbHandle(db);
	// fprintf(stderr, "%p %p was created for %s in session %p\n", pthread_self(), (void*) outDbHandle, inDbName, this);
}

void
SSDLSession::CreateWithBlob(const char *DbName,
							const CSSM_NET_ADDRESS *DbLocation,
							const CSSM_DBINFO &DBInfo,
							CSSM_DB_ACCESS_TYPE AccessRequest,
							const void *OpenParameters,
							const CSSM_DATA &blob,
							CSSM_DB_HANDLE &DbHandle)
{
	SSDatabase db(mClientSession, mDL, DbName, DbLocation);
	db->dbInfo(&DBInfo);
	db->accessRequest(AccessRequest);
	db->resourceControlContext(NULL);
	db->openParameters(OpenParameters);
	db->createWithBlob(DLDbIdentifier(CssmSubserviceUid(plugin.myGuid(), &version(), subserviceId(),
									  CSSM_SERVICE_DL | CSSM_SERVICE_CSP),
									  DbName, DbLocation),
					   blob);
	db->dbInfo(NULL);
	DbHandle = makeDbHandle(db);
	// fprintf(stderr, "%p %p was created with a blob in session %p\n", pthread_self(), (void*) DbHandle, this);
}

void
SSDLSession::DbOpen(const char *inDbName,
					const CSSM_NET_ADDRESS *inDbLocation,
					CSSM_DB_ACCESS_TYPE inAccessRequest,
					const AccessCredentials *inAccessCred,
					const void *inOpenParameters,
					CSSM_DB_HANDLE &outDbHandle)
{
	SSDatabase db(mClientSession, mDL, inDbName, inDbLocation);
	db->accessRequest(inAccessRequest);
	db->accessCredentials(inAccessCred);
	db->openParameters(inOpenParameters);
	db->open(DLDbIdentifier(CssmSubserviceUid(plugin.myGuid(), &version(), subserviceId(),
											  CSSM_SERVICE_DL | CSSM_SERVICE_CSP),
							inDbName, inDbLocation));
	outDbHandle = makeDbHandle(db);
	// fprintf(stderr, "%p %p was opened for %s in session %p\n", pthread_self(), (void*) outDbHandle, inDbName, this);
}

// Operations using DbContext instances.
void
SSDLSession::DbClose(CSSM_DB_HANDLE inDbHandle)
{
	killDbHandle(inDbHandle)->close();
}

void
SSDLSession::CreateRelation(CSSM_DB_HANDLE inDbHandle,
							CSSM_DB_RECORDTYPE inRelationID,
							const char *inRelationName,
							uint32 inNumberOfAttributes,
							const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
							uint32 inNumberOfIndexes,
							const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo)
{
	SSDatabase db = findDbHandle(inDbHandle);
	// @@@ Fix inAttributeInfo and inIndexInfo arguments (might be NULL if NumberOf = 0)
	db->createRelation(inRelationID, inRelationName,
					  inNumberOfAttributes, inAttributeInfo,
					  inNumberOfIndexes, &inIndexInfo);
}

void
SSDLSession::DestroyRelation(CSSM_DB_HANDLE inDbHandle,
							 CSSM_DB_RECORDTYPE inRelationID)
{
	// @@@ Check credentials.
	SSDatabase db = findDbHandle(inDbHandle);
	db->destroyRelation(inRelationID);
}

void
SSDLSession::Authenticate(CSSM_DB_HANDLE inDbHandle,
						  CSSM_DB_ACCESS_TYPE inAccessRequest,
						  const AccessCredentials &inAccessCred)
{
	SSDatabase db = findDbHandle(inDbHandle);
	db->authenticate(inAccessRequest, &inAccessCred);
}


void
SSDLSession::GetDbAcl(CSSM_DB_HANDLE inDbHandle,
					  const CSSM_STRING *inSelectionTag,
					  uint32 &outNumberOfAclInfos,
					  CSSM_ACL_ENTRY_INFO_PTR &outAclInfos)
{
	SSDatabase db = findDbHandle(inDbHandle);
	mClientSession.getDbAcl(db->dbHandle(),
		inSelectionTag ? *inSelectionTag : NULL,
		outNumberOfAclInfos, AclEntryInfo::overlayVar(outAclInfos), allocator());
}

void
SSDLSession::ChangeDbAcl(CSSM_DB_HANDLE inDbHandle,
						 const AccessCredentials &inAccessCred,
						 const CSSM_ACL_EDIT &inAclEdit)
{
	SSDatabase db = findDbHandle(inDbHandle);
	mClientSession.changeDbAcl(db->dbHandle(), inAccessCred, AclEdit::overlay(inAclEdit));
}

void
SSDLSession::GetDbOwner(CSSM_DB_HANDLE inDbHandle,
						CSSM_ACL_OWNER_PROTOTYPE &outOwner)
{
	SSDatabase db = findDbHandle(inDbHandle);
	mClientSession.getDbOwner(db->dbHandle(),
		AclOwnerPrototype::overlay(outOwner), allocator());
}

void
SSDLSession::ChangeDbOwner(CSSM_DB_HANDLE inDbHandle,
						   const AccessCredentials &inAccessCred,
						   const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner)
{
	SSDatabase db = findDbHandle(inDbHandle);
	mClientSession.changeDbOwner(db->dbHandle(), inAccessCred,
		AclOwnerPrototype::overlay(inNewOwner));
}

void
SSDLSession::GetDbNameFromHandle(CSSM_DB_HANDLE inDbHandle,
                                 char **outDbName)
{
	SSDatabase db = findDbHandle(inDbHandle);
	// @@@ Fix this functions signature.
	db->name(*outDbName);
}

void
SSDLSession::DataInsert(CSSM_DB_HANDLE inDbHandle,
						CSSM_DB_RECORDTYPE inRecordType,
						const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
						const CssmData *inData,
						CSSM_DB_UNIQUE_RECORD_PTR &outUniqueId)
{
	SSDatabase db = findDbHandle(inDbHandle);
	// @@@ Fix client lib.
    SSUniqueRecord uniqueId = db->insert(inRecordType, inAttributes, inData, true); // @@@ Fix me
	outUniqueId = makeSSUniqueRecord(uniqueId);
	// @@@ If this is a key do the right thing.
}

void
SSDLSession::DataDelete(CSSM_DB_HANDLE inDbHandle,
						const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
	SSDatabase db = findDbHandle(inDbHandle);
	SSUniqueRecord uniqueId = findSSUniqueRecord(inUniqueRecordIdentifier);
	uniqueId->deleteRecord();
	// @@@ If this is a key do the right thing.
}


void
SSDLSession::DataModify(CSSM_DB_HANDLE inDbHandle,
						CSSM_DB_RECORDTYPE inRecordType,
						CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
						const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
						const CssmData *inDataToBeModified,
						CSSM_DB_MODIFY_MODE inModifyMode)
{
	SSDatabase db = findDbHandle(inDbHandle);
	SSUniqueRecord uniqueId = findSSUniqueRecord(inoutUniqueRecordIdentifier);
	uniqueId->modify(inRecordType, inAttributesToBeModified, inDataToBeModified, inModifyMode);
	// @@@ If this is a key do the right thing.
}

CSSM_HANDLE
SSDLSession::DataGetFirst(CSSM_DB_HANDLE inDbHandle,
						  const CssmQuery *inQuery,
						  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
						  CssmData *inoutData,
						  CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	SSDatabase db = findDbHandle(inDbHandle);
	CSSM_HANDLE resultsHandle = CSSM_INVALID_HANDLE;
    SSUniqueRecord uniqueId(db);

	// Setup so we always retrive the attributes even if the client
	// doesn't want them so we can figure out if we just retrived a key.
	CSSM_DB_RECORD_ATTRIBUTE_DATA attributes;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR pAttributes;
	if (inoutAttributes)
		pAttributes = inoutAttributes;
	else
	{
		pAttributes = &attributes;
		memset(pAttributes, 0, sizeof(attributes));
	}

	// Retrive the record.
	CSSM_RETURN result = CSSM_DL_DataGetFirst(db->handle(), inQuery, &resultsHandle,
											  pAttributes, inoutData, uniqueId);
	if (result)
	{
		if (result == CSSMERR_DL_ENDOFDATA)
			return CSSM_INVALID_HANDLE;

		CssmError::throwMe(result);
	}

	uniqueId->activate();

	// If we the client didn't ask for data then it doesn't matter
	// if this record is a key or not, just return it.
	if (inoutData)
	{
		if (pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		{
			// This record is a key, do the right thing (tm).
			// Allocate storage for the key.
            CssmKey *outKey = DatabaseSession::alloc<CssmKey>();
			new SSKey(*this, *outKey, db, uniqueId, pAttributes->DataRecordType, *inoutData);

			// Free the data we retrived (keyblob)
			allocator().free(inoutData->Data);

			// Set the length and data on the data we return to the client
			inoutData->Length = sizeof(*outKey);
			inoutData->Data = reinterpret_cast<uint8 *>(outKey);
		}
	}

	outUniqueRecord = makeSSUniqueRecord(uniqueId);
	return resultsHandle;
}

bool
SSDLSession::DataGetNext(CSSM_DB_HANDLE inDbHandle,
						 CSSM_HANDLE inResultsHandle,
						 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
						 CssmData *inoutData,
						 CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	// @@@ If this is a key do the right thing.
	SSDatabase db = findDbHandle(inDbHandle);
    SSUniqueRecord uniqueId(db);

	// Setup so we always retrive the attributes even if the client
	// doesn't want them so we can figure out if we just retrived a key.
	CSSM_DB_RECORD_ATTRIBUTE_DATA attributes;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR pAttributes;
	if (inoutAttributes)
		pAttributes = inoutAttributes;
	else
	{
		pAttributes = &attributes;
		memset(pAttributes, 0, sizeof(attributes));
	}

	CSSM_RETURN result = CSSM_DL_DataGetNext(db->handle(), inResultsHandle,
											 inoutAttributes, inoutData, uniqueId);
	if (result)
	{
		if (result == CSSMERR_DL_ENDOFDATA)
			return false;

		CssmError::throwMe(result);
	}

	uniqueId->activate();

	// If we the client didn't ask for data then it doesn't matter
	// if this record is a key or not, just return it.
	if (inoutData)
	{
		if (pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		{
			// This record is a key, do the right thing (tm).
			// Allocate storage for the key.
			CssmKey *outKey = DatabaseSession::alloc<CssmKey>();
			new SSKey(*this, *outKey, db, uniqueId, pAttributes->DataRecordType, *inoutData);

			// Free the data we retrived (keyblob)
			allocator().free(inoutData->Data);

			// Set the length and data on the data we return to the client
			inoutData->Length = sizeof(*outKey);
			inoutData->Data = reinterpret_cast<uint8 *>(outKey);
		}
	}

	outUniqueRecord = makeSSUniqueRecord(uniqueId);

	return true;
}

void
SSDLSession::DataAbortQuery(CSSM_DB_HANDLE inDbHandle,
							CSSM_HANDLE inResultsHandle)
{
	// @@@ If this is a key do the right thing.
	SSDatabase db = findDbHandle(inDbHandle);
	CSSM_RETURN result = CSSM_DL_DataAbortQuery(db->handle(), inResultsHandle);
	if (result)
		CssmError::throwMe(result);
}

void
SSDLSession::DataGetFromUniqueRecordId(CSSM_DB_HANDLE inDbHandle,
									   const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
									   CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
									   CssmData *inoutData)
{
	SSDatabase db = findDbHandle(inDbHandle);
	const SSUniqueRecord uniqueId = findSSUniqueRecord(inUniqueRecord);

	// Setup so we always retrive the attributes even if the client
	// doesn't want them so we can figure out if we just retrived a key.
	CSSM_DB_RECORD_ATTRIBUTE_DATA attributes;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR pAttributes;
	if (inoutAttributes)
		pAttributes = inoutAttributes;
	else
	{
		pAttributes = &attributes;
		memset(pAttributes, 0, sizeof(attributes));
	}

	CSSM_RETURN result = CSSM_DL_DataGetFromUniqueRecordId(db->handle(),
		uniqueId, pAttributes, inoutData);
	if (result)
		CssmError::throwMe(result);

	if (inoutData)
	{
		if (pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		{
			// This record is a key, do the right thing (tm).
			// Allocate storage for the key.
			CssmKey *outKey = DatabaseSession::alloc<CssmKey>();
			new SSKey(*this, *outKey, db, uniqueId, pAttributes->DataRecordType, *inoutData);

			// Free the data we retrived (keyblob)
			allocator().free(inoutData->Data);

			// Set the length and data on the data we return to the client
			inoutData->Length = sizeof(*outKey);
			inoutData->Data = reinterpret_cast<uint8 *>(outKey);
		}
	}
}

void
SSDLSession::FreeUniqueRecord(CSSM_DB_HANDLE inDbHandle,
							  CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
	killSSUniqueRecord(inUniqueRecordIdentifier);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

static const uint32 kGenericAttributeNames[] =
{
	'cdat', 'mdat', 'desc', 'icmt', 'crtr', 'type', 'scrp', 7, 8, 'invi', 'nega', 'cusi', 'prot', 'acct', 'svce',
	'gena'
};

const uint32 kNumGenericAttributes = sizeof (kGenericAttributeNames) / sizeof (uint32);

static const uint32 kApplesharePasswordNames[] =
{
	'cdat', 'mdat', 'desc', 'icmt', 'crtr', 'type', 'scrp', 7, 8, 'invi', 'nega', 'cusi', 'prot', 'acct', 'vlme',
	'srvr', 'ptcl', 'addr', 'ssig'
};

const uint32 kNumApplesharePasswordAttributes = sizeof (kApplesharePasswordNames) / sizeof (uint32);

static const uint32 kInternetPasswordNames[] =
{
	'cdat', 'mdat', 'desc', 'icmt', 'crtr', 'type', 'scrp', 7, 8, 'invi', 'nega', 'cusi', 'prot', 'acct', 'sdmn',
	'srvr', 'ptcl', 'atyp', 'port', 'path'
};

const uint32 kNumInternetPasswordAttributes = sizeof (kInternetPasswordNames) / sizeof (uint32);

const uint32 kKeyAttributeNames[] =
{
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26
};

const uint32 kNumKeyAttributes = sizeof (kKeyAttributeNames) / sizeof (uint32);

const uint32 kCertificateAttributeNames[] =
{
	'ctyp', 'cenc', 'labl', 'alis', 'subj', 'issu', 'snbr', 'skid', 'hpky'
};

const uint32 kNumCertificateAttributes = sizeof (kCertificateAttributeNames) / sizeof (uint32);

const unsigned kSymmetricKeyLabel = 6; // record id for the symmetric key
const unsigned kLabelSize = 20;
const unsigned kNumSymmetricAttributes = 27; // number of attributes to request

#pragma clang diagnostic pop

static void appendUInt32ToData (const uint32 value, CssmDataContainer &data)
{
	data.append (CssmPolyData (uint32 (htonl (value))));
}

static inline uint32 GetUInt32AtFinger (uint8 *&finger)
{
	uint32 a = ((finger[0] << 24) | (finger[1] << 16) | (finger[2] << 8) | finger[3]);
	finger += sizeof (uint32);
	return a;
}

void
SSDLSession::unwrapAttributesAndData (uint32 &numAttributes,
									  CSSM_DB_ATTRIBUTE_DATA_PTR &attributes,
									  CSSM_DATA &data,
									  CSSM_DATA &input)
{
	// get the number of attributes
	uint8* finger = input.Data;
	numAttributes = GetUInt32AtFinger (finger);

	// compute the end of the data for sanity checking later
	uint8* maximum = input.Data + input.Length;

	// make the attribute array
	attributes = (CSSM_DB_ATTRIBUTE_DATA*) allocator ().malloc (numAttributes * sizeof (CSSM_DB_ATTRIBUTE_DATA));

	// for each attribute, retrieve the name format, name, type, and number of values
	unsigned i;
	for (i = 0; i < numAttributes; ++i)
	{
		attributes[i].Info.AttributeNameFormat = GetUInt32AtFinger (finger);
		attributes[i].Info.Label.AttributeID = GetUInt32AtFinger (finger);
		attributes[i].Info.AttributeFormat = GetUInt32AtFinger (finger);
		attributes[i].NumberOfValues = GetUInt32AtFinger (finger);

		// for each value, get the length and data
		attributes[i].Value = (CSSM_DATA*) allocator ().malloc (sizeof (CSSM_DATA) * attributes[i].NumberOfValues);
		unsigned j;
		for (j = 0; j < attributes[i].NumberOfValues; ++j)
		{
			attributes[i].Value[j].Length = GetUInt32AtFinger (finger);
			if (attributes[i].Value[j].Length != 0)
			{
				// sanity check what we are about to do
				if (finger > maximum || finger + attributes[i].Value[j].Length > maximum)
				{
					CssmError::throwMe (CSSM_ERRCODE_INVALID_POINTER);
				}

				attributes[i].Value[j].Data = (uint8*) allocator ().malloc (attributes[i].Value[j].Length);

				switch (attributes[i].Info.AttributeFormat)
				{
					default:
					{
						memmove (attributes[i].Value[j].Data, finger, attributes[i].Value[j].Length);
						finger += attributes[i].Value[j].Length;
						break;
					}

					case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
					case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
					{
						*(uint32*) attributes[i].Value[j].Data = GetUInt32AtFinger (finger);
						break;
					}

					case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
					{
						uint32* d = (uint32*) attributes[i].Value[j].Data;
						unsigned long numValues = attributes[i].Value[j].Length / sizeof (UInt32);
						while (numValues--)
						{
							*d++ = GetUInt32AtFinger (finger);
						}
						break;
					}
				}
			}
			else
			{
				attributes[i].Value[j].Data = NULL;
			}
		}
	}

	// get the data
	data.Length = GetUInt32AtFinger (finger);
	if (data.Length != 0)
	{
		// sanity check the pointer
		if (finger + data.Length > maximum)
		{
			CssmError::throwMe (CSSM_ERRCODE_INVALID_POINTER);
		}

		data.Data = (uint8*) allocator ().malloc (data.Length);
		memmove (data.Data, finger, data.Length);
		finger += data.Length;
	}
	else
	{
		data.Data = NULL;
	}
}

void
SSDLSession::getWrappedAttributesAndData (SSDatabase &db,
										  CSSM_DB_RECORDTYPE recordType,
										  CSSM_DB_UNIQUE_RECORD_PTR recordPtr,
										  CssmDataContainer &output,
										  CSSM_DATA *dataBlob)
{
	// figure out which attributes to use
	const uint32* attributeNameArray;
	uint32 numAttributeNames;

	switch (recordType)
	{
		case CSSM_DL_DB_RECORD_GENERIC_PASSWORD:
		{
			attributeNameArray = kGenericAttributeNames;
			numAttributeNames = kNumGenericAttributes;
			break;
		}

		case CSSM_DL_DB_RECORD_INTERNET_PASSWORD:
		{
			attributeNameArray = kInternetPasswordNames;
			numAttributeNames = kNumInternetPasswordAttributes;
			break;
		}

		case CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD:
		{
			attributeNameArray = kApplesharePasswordNames;
			numAttributeNames = kNumApplesharePasswordAttributes;
			break;
		}

		case CSSM_DL_DB_RECORD_X509_CERTIFICATE:
		{
			attributeNameArray = kCertificateAttributeNames;
			numAttributeNames = kNumCertificateAttributes;
			break;
		}

		case CSSM_DL_DB_RECORD_PUBLIC_KEY:
		case CSSM_DL_DB_RECORD_PRIVATE_KEY:
		case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
		{
			attributeNameArray = kKeyAttributeNames;
			numAttributeNames = kNumKeyAttributes;
			break;
		}

		default:
		{
			CssmError::throwMe (CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED);
		}
	}

	// make the attribute array
	size_t arraySize = numAttributeNames * sizeof (CSSM_DB_ATTRIBUTE_DATA);

	CSSM_DB_ATTRIBUTE_DATA_PTR attributes =
		(CSSM_DB_ATTRIBUTE_DATA_PTR) allocator ().malloc (arraySize);

	// initialize the array
	memset (attributes, 0, arraySize);
	unsigned i;
	for (i = 0; i < numAttributeNames; ++i)
	{
		attributes[i].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
		attributes[i].Info.Label.AttributeID = attributeNameArray[i];
	}

	// make the attribute record
	CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;
	attrData.DataRecordType = recordType;
	attrData.SemanticInformation = 0;
	attrData.NumberOfAttributes = numAttributeNames;
	attrData.AttributeData = attributes;

	// get the data
	CssmDataContainer data;
	CSSM_RETURN result = CSSM_DL_DataGetFromUniqueRecordId (db->handle (),
															recordPtr,
															&attrData,
															&data);
	if (result != 0)
	{
		CssmError::throwMe (result);
	}

	// wrap the data -- write the number of attributes
	appendUInt32ToData (numAttributeNames, output);

	// for each attribute, write the type and number of values
	for (i = 0; i < numAttributeNames; ++i)
	{
		appendUInt32ToData (attributes[i].Info.AttributeNameFormat, output);
		appendUInt32ToData (attributes[i].Info.Label.AttributeID, output);
		appendUInt32ToData (attributes[i].Info.AttributeFormat, output);
		appendUInt32ToData (attributes[i].NumberOfValues, output);

		// for each value, write the name format, name, length and the data
		unsigned j;
		for (j = 0; j < attributes[i].NumberOfValues; ++j)
		{
			appendUInt32ToData ((uint32)attributes[i].Value[j].Length, output);
			if (attributes[i].Value[j].Length != 0)
			{
				switch (attributes[i].Info.AttributeFormat)
				{
					default:
					{
						output.append (CssmPolyData (attributes[i].Value[j]));
						break;
					}

					case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
					case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
					{
						uint32 n = htonl (*(uint32*) attributes[i].Value[j].Data);
						CSSM_DATA d;
						d.Length = sizeof (uint32);
						d.Data = (uint8*) &n;
						output.append (CssmPolyData (d));
						break;
					}
				}
			}
		}
	}

	// write the length of the data
	appendUInt32ToData ((uint32)data.Length, output);

	// write the data itself
	if (data.Length != 0)
	{
		output.append (CssmPolyData (data));
	}

	// clean up
	for (i = 0; i < numAttributeNames; ++i)
	{
		unsigned j;
		for (j = 0; j < attributes[i].NumberOfValues; ++j)
		{
			allocator ().free (attributes[i].Value[j].Data);
		}

		allocator ().free (attributes[i].Value);
	}

	allocator ().free (attributes);

	// copy out the data if the caller needs it
	if (dataBlob)
	{
		dataBlob->Data = data.Data;
		dataBlob->Length = data.Length;
		data.Data = NULL;
		data.Length = 0;
	}
}

void
SSDLSession::getUniqueIdForSymmetricKey (SSDatabase &db, CSSM_DATA &label,
										 CSSM_DB_UNIQUE_RECORD_PTR &uniqueRecord)
{
	// set up a query to get the key
	CSSM_SELECTION_PREDICATE predicate;
	predicate.DbOperator = CSSM_DB_EQUAL;
	predicate.Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
	predicate.Attribute.Info.Label.AttributeID = kSymmetricKeyLabel;
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	predicate.Attribute.NumberOfValues = 1;
	// the label of the corresponding key is the first 20 bytes of the blob we returned
	predicate.Attribute.Value = &label;

	CSSM_QUERY query;
	query.RecordType = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	query.SelectionPredicate = &predicate;

	// fill out the record data
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttributeData;
	recordAttributeData.DataRecordType = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
	recordAttributeData.SemanticInformation = 0;
	recordAttributeData.NumberOfAttributes = 0;
	recordAttributeData.AttributeData = NULL;

	// get the data
	CSSM_HANDLE handle;
	CSSM_RETURN result = CSSM_DL_DataGetFirst (db->handle (), &query, &handle, &recordAttributeData, NULL,
											   &uniqueRecord);
	if (result)
	{
		CssmError::throwMe (result);
	}

	// clean up
	CSSM_DL_DataAbortQuery (db->handle (), handle);
}

void
SSDLSession::getCorrespondingSymmetricKey (SSDatabase &db, CSSM_DATA &labelData, CssmDataContainer &data)
{
	// get the unique ID
	CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord;
	getUniqueIdForSymmetricKey (db, labelData, uniqueRecord);

	// from this. get the wrapped attributes and data
	getWrappedAttributesAndData (db, CSSM_DL_DB_RECORD_SYMMETRIC_KEY, uniqueRecord, data, NULL);

	// clean up after the query
	CSSM_DL_FreeUniqueRecord (db->handle (), uniqueRecord);
}

void SSDLSession::doGetWithoutEncryption (SSDatabase &db, const void *inInputParams, void **outOutputParams)
{
	CSSM_APPLECSPDL_DB_GET_WITHOUT_ENCRYPTION_PARAMETERS* params =
		(CSSM_APPLECSPDL_DB_GET_WITHOUT_ENCRYPTION_PARAMETERS*) inInputParams;

	SSUniqueRecord uniqueID = findSSUniqueRecord(*(params->uniqueID));

	CSSM_DATA *outputData = (CSSM_DATA*) outOutputParams;
	CssmDataContainer output;

	// get the record type and requested attributes from the DL
	CssmDataContainer data;
	CSSM_RETURN result = CSSM_DL_DataGetFromUniqueRecordId(db->handle(),
														   uniqueID,
														   params->attributes,
														   NULL);

	if (result)
	{
		CssmError::throwMe(result);
	}

	// get the real data and all of the attributes from the DL
	CssmDataContainer blobData;
	getWrappedAttributesAndData (db, params->attributes->DataRecordType, uniqueID, data, &blobData);

	// write out the data blob
	appendUInt32ToData ((uint32)data.Length, output);
	output.append (CssmPolyData (data));

	// figure out what we need to do with the key blob
	CssmDataContainer key;
	switch (params->attributes->DataRecordType)
	{
		case CSSM_DL_DB_RECORD_GENERIC_PASSWORD:
		case CSSM_DL_DB_RECORD_INTERNET_PASSWORD:
		case CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD:
		{
			// the label is the first kLabelSize bytes of the resultant data blob
			CSSM_DATA label = {kLabelSize, blobData.Data};

			// get the key
			getCorrespondingSymmetricKey (db, label, key);
		}
		break;

		default:
		{
			break;
		}
	}


	// write out the length of the key blob
	appendUInt32ToData ((uint32)key.Length, output);

	if (key.Length != 0)
	{
		// write the key
		output.append (CssmPolyData (key));
	}

	// copy out the results
	outputData->Data = output.Data;
	output.Data = NULL;
	outputData->Length = output.Length;
	output.Length = 0;
}

void
SSDLSession::cleanupAttributes (uint32 numAttributes, CSSM_DB_ATTRIBUTE_DATA_PTR attributes)
{
	unsigned i;
	for (i = 0; i < numAttributes; ++i)
	{
		unsigned j;
		for (j = 0; j < attributes[i].NumberOfValues; ++j)
		{
			free (attributes[i].Value[j].Data);
		}

		free (attributes[i].Value);
	}

	free (attributes);
}

void
SSDLSession::doModifyWithoutEncryption (SSDatabase &db, const void* inInputParams, void** outOutputParams)
{
	CSSM_RETURN result;
	CSSM_APPLECSPDL_DB_MODIFY_WITHOUT_ENCRYPTION_PARAMETERS* params =
		(CSSM_APPLECSPDL_DB_MODIFY_WITHOUT_ENCRYPTION_PARAMETERS*) inInputParams;

	// extract the data for this modify.
	uint8* finger = params->data->Data;
	CSSM_DATA data;
	data.Length = GetUInt32AtFinger (finger);
	data.Data = finger;
	if (data.Length + sizeof (UInt32) > params->data->Length)
	{
		CssmError::throwMe (CSSM_ERRCODE_INVALID_POINTER);
	}

	// point to the key
	finger += data.Length;

	// reconstruct the attributes and data
	uint32 numAttributes;
	CSSM_DB_ATTRIBUTE_DATA_PTR attributes;
	CssmDataContainer dataBlob;

	unwrapAttributesAndData (numAttributes, attributes, dataBlob, data);

	CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;
	attrData.DataRecordType = params->attributes->DataRecordType;
	attrData.SemanticInformation = 0;
	attrData.NumberOfAttributes = numAttributes;
	attrData.AttributeData = attributes;

	// get the unique ID for this record (from the db's perspective)
	SSUniqueRecord uniqueID = findSSUniqueRecord(*(params->uniqueID));
	CSSM_DB_UNIQUE_RECORD *uniqueIDPtr = uniqueID; // for readability.  There's cast overloading
												   // going on here.

	switch (attrData.DataRecordType)
	{
		case CSSM_DL_DB_RECORD_GENERIC_PASSWORD:
		case CSSM_DL_DB_RECORD_INTERNET_PASSWORD:
		case CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD:
		{
			// read off the data so that we can update the key
			CssmDataContainer oldData;
			result = CSSM_DL_DataGetFromUniqueRecordId (db->handle(),
														uniqueIDPtr,
														NULL,
														&oldData);
			if (result)
			{
				CssmError::throwMe (result);
			}

			CSSM_DB_MODIFY_MODE modifyMode = params->modifyMode;

			// parse the key data blob
			CssmDataContainer keyBlob;
			data.Length = GetUInt32AtFinger (finger);
			data.Data = finger;

			CSSM_DB_RECORD_ATTRIBUTE_DATA* attrDataPtr = NULL;
			CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;

			CSSM_DATA labelData = {kLabelSize, oldData.Data};
			CSSM_DB_UNIQUE_RECORD_PTR recordID;
			getUniqueIdForSymmetricKey (db, labelData, recordID);

			CSSM_DB_ATTRIBUTE_DATA_PTR keyAttributes;
			uint32 numKeyAttributes;
			unwrapAttributesAndData (numKeyAttributes, keyAttributes, keyBlob, data);

			// make the attribute data
			attrData.DataRecordType = params->recordType;
			attrData.SemanticInformation = 0;
			attrData.NumberOfAttributes = numKeyAttributes;
			attrData.AttributeData = keyAttributes;

			attrDataPtr = &attrData;

			result = CSSM_DL_DataModify (db->handle(),
							 CSSM_DL_DB_RECORD_SYMMETRIC_KEY,
							 recordID,
							 attrDataPtr,
							 &keyBlob,
							 modifyMode);

			// clean up
			CSSM_DL_FreeUniqueRecord (db->handle (), recordID);

			cleanupAttributes (numKeyAttributes, keyAttributes);
			break;
		}

		default:
		{
			break;
		}
	}

	// save off the new data
	result = CSSM_DL_DataModify(db->handle(),
								params->recordType,
								uniqueIDPtr,
								&attrData,
								&dataBlob,
								params->modifyMode);

	// clean up
	cleanupAttributes (numAttributes, attributes);

	if (result)
	{
		CssmError::throwMe(result);
	}

}

void
SSDLSession::doInsertWithoutEncryption (SSDatabase &db, const void* inInputParams, void** outOutputParams)
{
	CSSM_RETURN result;

	CSSM_APPLECSPDL_DB_INSERT_WITHOUT_ENCRYPTION_PARAMETERS* params =
		(CSSM_APPLECSPDL_DB_INSERT_WITHOUT_ENCRYPTION_PARAMETERS*) inInputParams;

	// extract the data for this insert.
	uint8* finger = params->data.Data;
	CSSM_DATA data;
	data.Length = GetUInt32AtFinger (finger);
	data.Data = finger;
	finger += data.Length;

	// reconstruct the attributes and data
	uint32 numAttributes;
	CSSM_DB_ATTRIBUTE_DATA_PTR attributes;
	CSSM_DATA dataBlob;

	unwrapAttributesAndData (numAttributes, attributes, dataBlob, data);

	// make the attribute data
	CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;
	attrData.DataRecordType = params->recordType;
	attrData.SemanticInformation = 0;
	attrData.NumberOfAttributes = numAttributes;
	attrData.AttributeData = attributes;

	// insert into the database
	SSUniqueRecord uniqueID (db);
	result = CSSM_DL_DataInsert (db->handle(), params->recordType,
								 &attrData,
								 &dataBlob,
								 uniqueID);

	// cleanup
	allocator ().free (dataBlob.Data);
	cleanupAttributes (numAttributes, attributes);

	// attach into the CSP/DL mechanism
	CSSM_DB_UNIQUE_RECORD_PTR newRecord = makeSSUniqueRecord(uniqueID);
	*(CSSM_DB_UNIQUE_RECORD_PTR*) outOutputParams = newRecord;

	if (result)
	{
		CssmError::throwMe(result);
	}

	// Get the key data for this insert
	data.Length = GetUInt32AtFinger (finger);
	if (data.Length != 0)
	{
		data.Data = finger;

		// parse the key data blob
		unwrapAttributesAndData (numAttributes, attributes, dataBlob, data);

		// make the attribute data
		CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;
		attrData.DataRecordType = params->recordType;
		attrData.SemanticInformation = 0;
		attrData.NumberOfAttributes = numAttributes;
		attrData.AttributeData = attributes;

		// insert the key data into the symmetric key table
		CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord;
		result = CSSM_DL_DataInsert (db->handle(), CSSM_DL_DB_RECORD_SYMMETRIC_KEY, &attrData, &dataBlob,
									 &uniqueRecord);
		if (result)
		{
			CssmError::throwMe (result);
		}

		// clean up after inserting the key
		CSSM_DL_FreeUniqueRecord (db->handle (), uniqueRecord);
		allocator ().free (dataBlob.Data);
		cleanupAttributes (numAttributes, attributes);
	}
}

void
SSDLSession::doConvertRecordIdentifier (SSDatabase &db, const void *inInputParams, void **outOutputParams)
{
	SSUniqueRecord uniqueId (db);

	// clone the unique record
	CSSM_DB_UNIQUE_RECORD_PTR clone = (CSSM_DB_UNIQUE_RECORD_PTR) allocator ().malloc (sizeof (CSSM_DB_UNIQUE_RECORD));
	*clone = *(CSSM_DB_UNIQUE_RECORD_PTR) inInputParams;

	// set the value of the unique record
	uniqueId->setUniqueRecordPtr (clone);

	// byte swap the retrieved record pointer to host order
	uint32* idArray = (uint32*) clone->RecordIdentifier.Data;
	idArray[0] = ntohl (idArray[0]);
	idArray[1] = ntohl (idArray[1]);
	idArray[2] = ntohl (idArray[2]);

	CSSM_DB_UNIQUE_RECORD_PTR newRecord = makeSSUniqueRecord(uniqueId);
	*(CSSM_DB_UNIQUE_RECORD_PTR*) outOutputParams = newRecord;
}

void
SSDLSession::PassThrough(CSSM_DB_HANDLE inDbHandle,
						 uint32 inPassThroughId,
						 const void *inInputParams,
						 void **outOutputParams)
{
	if (inPassThroughId == CSSM_APPLECSPDL_DB_CREATE_WITH_BLOB)
	{
		CSSM_APPLE_CSPDL_DB_CREATE_WITH_BLOB_PARAMETERS* params = (CSSM_APPLE_CSPDL_DB_CREATE_WITH_BLOB_PARAMETERS*) inInputParams;
		CreateWithBlob(params->dbName, params->dbLocation, *params->dbInfo, params->accessRequest, params->openParameters, *params->blob,
					   * (CSSM_DB_HANDLE*) outOutputParams);
		return;
	}

	SSDatabase db = findDbHandle(inDbHandle);
	switch (inPassThroughId)
	{
		case CSSM_APPLECSPDL_DB_LOCK:
			db->lock();
			break;
		case CSSM_APPLECSPDL_DB_UNLOCK:
			if (inInputParams)
				db->unlock(*reinterpret_cast<const CSSM_DATA *>(inInputParams));
			else
				db->unlock();
			break;
		case CSSM_APPLECSPDL_DB_STASH:
            db->stash();
            break;
        case CSSM_APPLECSPDL_DB_STASH_CHECK:
            db->stashCheck();
            break;
        case CSSM_APPLECSPDL_DB_GET_SETTINGS:
		{
			if (!outOutputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_OUTPUT_POINTER);

			CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS_PTR params =
            DatabaseSession::alloc<CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS>();
			try
			{
				uint32 idleTimeout;
				bool lockOnSleep;
				db->getSettings(idleTimeout, lockOnSleep);
				params->idleTimeout = idleTimeout;
				params->lockOnSleep = lockOnSleep;
			}
			catch(...)
			{
				allocator().free(params);
				throw;
			}
			*reinterpret_cast<CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS_PTR *>(outOutputParams) = params;
			break;
		}
		case CSSM_APPLECSPDL_DB_SET_SETTINGS:
		{
			if (!inInputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

			const CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS *params =
				reinterpret_cast<const CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS *>(inInputParams);
			db->setSettings(params->idleTimeout, params->lockOnSleep);
			break;
		}
		case CSSM_APPLECSPDL_DB_IS_LOCKED:
		{
			if (!outOutputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_OUTPUT_POINTER);

			CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS_PTR params =
            DatabaseSession::alloc<CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS>();
			try
			{
				params->isLocked = db->isLocked();
			}
			catch(...)
			{
				allocator().free(params);
				throw;
			}
			*reinterpret_cast<CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS_PTR *>(outOutputParams) = params;
			break;
		}
		case CSSM_APPLECSPDL_DB_CHANGE_PASSWORD:
		{
			if (!inInputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

			const CSSM_APPLECSPDL_DB_CHANGE_PASSWORD_PARAMETERS *params =
				reinterpret_cast<const CSSM_APPLECSPDL_DB_CHANGE_PASSWORD_PARAMETERS *>(inInputParams);
			db->changePassphrase(params->accessCredentials);
			break;
		}
		case CSSM_APPLECSPDL_DB_GET_HANDLE:
		{
			using SecurityServer::DbHandle;
			Required(outOutputParams, CSSM_ERRCODE_INVALID_OUTPUT_POINTER);
			*reinterpret_cast<CSSM_DL_DB_HANDLE *>(outOutputParams) = db->handle();
			break;
		}
		case CSSM_APPLECSPDL_CSP_RECODE:
		{
			if (!inInputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

			const CSSM_APPLECSPDL_RECODE_PARAMETERS *params =
				reinterpret_cast<const CSSM_APPLECSPDL_RECODE_PARAMETERS *>(inInputParams);

			db->recode(CssmData::overlay(params->dbBlob),
				CssmData::overlay(params->extraData));
			break;
		}
		case CSSM_APPLECSPDL_DB_GET_RECORD_IDENTIFIER:
		{
			SSUniqueRecord uniqueID = findSSUniqueRecord(*(CSSM_DB_UNIQUE_RECORD_PTR) inInputParams);
			db->getRecordIdentifier(uniqueID, *reinterpret_cast<CSSM_DATA *>(outOutputParams));
			break;
		}
		case CSSM_APPLECSPDL_DB_COPY_BLOB:
		{
			// make the output parameters
			db->copyBlob(*reinterpret_cast<CSSM_DATA *>(outOutputParams));
			break;
		}
		case CSSM_APPLECSPDL_DB_INSERT_WITHOUT_ENCRYPTION:
		{
			doInsertWithoutEncryption (db, inInputParams, outOutputParams);
			break;
		}
		case CSSM_APPLECSPDL_DB_MODIFY_WITHOUT_ENCRYPTION:
		{
			doModifyWithoutEncryption (db, inInputParams, outOutputParams);
			break;
		}
		case CSSM_APPLECSPDL_DB_GET_WITHOUT_ENCRYPTION:
		{
			doGetWithoutEncryption (db, inInputParams, outOutputParams);
			break;
		}
		case CSSM_APPLECSPDL_DB_CONVERT_RECORD_IDENTIFIER:
		{
			doConvertRecordIdentifier (db, inInputParams, outOutputParams);
			break;
		}
		default:
		{
			CSSM_RETURN result = CSSM_DL_PassThrough(db->handle(), inPassThroughId, inInputParams, outOutputParams);
			if (result)
				CssmError::throwMe(result);
			break;
		}
	}
}

CSSM_DB_HANDLE
SSDLSession::makeDbHandle(SSDatabase &inDb)
{
	StLock<Mutex> _(mDbHandleLock);
	CSSM_DB_HANDLE aDbHandle = inDb->handle().DBHandle;
	bool inserted;
    inserted = mDbHandleMap.insert(DbHandleMap::value_type(aDbHandle, inDb)).second;
	assert(inserted);
	// fprintf(stderr, "%p Added %p to %p\n", pthread_self(), (void*) aDbHandle, (void*) this);
	return aDbHandle;
}

SSDatabase
SSDLSession::killDbHandle(CSSM_DB_HANDLE inDbHandle)
{
	StLock<Mutex> _(mDbHandleLock);
	DbHandleMap::iterator it = mDbHandleMap.find(inDbHandle);
	if (it == mDbHandleMap.end())
	{
		// fprintf(stderr, "Can't find %p in %p\n", (void*) inDbHandle, this);
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_HANDLE);
	}

	SSDatabase db = it->second;
	// fprintf(stderr, "%p Removed %p from %p\n", pthread_self(), (void*) it->first, (void*) this);
	mDbHandleMap.erase(it);
	return db;
}

SSDatabase
SSDLSession::findDbHandle(CSSM_DB_HANDLE inDbHandle)
{
	StLock<Mutex> _(mDbHandleLock);
	// fprintf(stderr, "%p Looking for %p in %p\n", pthread_self(), (void*) inDbHandle, (void*) this);
	DbHandleMap::iterator it = mDbHandleMap.find(inDbHandle);
	if (it == mDbHandleMap.end())
	{
		// fprintf(stderr, "%p Can't find %p in %p\n", pthread_self(), (void*) inDbHandle, this);
		DbHandleMap::iterator it = mDbHandleMap.begin();
		while (it != mDbHandleMap.end())
		{
			// fprintf(stderr, "\t%p\n", (void*) it->first);
			it++;
		}

		CssmError::throwMe(CSSMERR_DL_INVALID_DB_HANDLE);
	}

	return it->second;
}

CSSM_DB_UNIQUE_RECORD_PTR
SSDLSession::makeSSUniqueRecord(SSUniqueRecord &uniqueId)
{
	StLock<Mutex> _(mSSUniqueRecordLock);
	CSSM_HANDLE ref = CSSM_HANDLE(static_cast<CSSM_DB_UNIQUE_RECORD *>(uniqueId));
	bool inserted;
    inserted = mSSUniqueRecordMap.insert(SSUniqueRecordMap::value_type(ref, uniqueId)).second;
	assert(inserted);
	return createUniqueRecord(ref);
}

SSUniqueRecord
SSDLSession::killSSUniqueRecord(CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	CSSM_HANDLE ref = parseUniqueRecord(inUniqueRecord);
	StLock<Mutex> _(mSSUniqueRecordLock);
	SSUniqueRecordMap::iterator it = mSSUniqueRecordMap.find(ref);
	if (it == mSSUniqueRecordMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	SSUniqueRecord uniqueRecord = it->second;
	mSSUniqueRecordMap.erase(it);
	freeUniqueRecord(inUniqueRecord);
	return uniqueRecord;
}

SSUniqueRecord
SSDLSession::findSSUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	CSSM_HANDLE ref = parseUniqueRecord(inUniqueRecord);
	StLock<Mutex> _(mSSUniqueRecordLock);
	SSUniqueRecordMap::iterator it = mSSUniqueRecordMap.find(ref);
	if (it == mSSUniqueRecordMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	return it->second;
}

CSSM_DB_UNIQUE_RECORD_PTR
SSDLSession::createUniqueRecord(CSSM_HANDLE ref)
{
	CSSM_DB_UNIQUE_RECORD *aUniqueRecord = DatabaseSession::alloc<CSSM_DB_UNIQUE_RECORD>();
	memset(aUniqueRecord, 0, sizeof(CSSM_DB_UNIQUE_RECORD));
	aUniqueRecord->RecordIdentifier.Length = sizeof(CSSM_HANDLE);
	try
	{
		aUniqueRecord->RecordIdentifier.Data = DatabaseSession::alloc<uint8>(sizeof(CSSM_HANDLE));
		*reinterpret_cast<CSSM_HANDLE *>(aUniqueRecord->RecordIdentifier.Data) = ref;
	}
	catch(...)
	{
		free(aUniqueRecord);
		throw;
	}

	return aUniqueRecord;
}

CSSM_HANDLE
SSDLSession::parseUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	if (inUniqueRecord.RecordIdentifier.Length != sizeof(CSSM_HANDLE))
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	return *reinterpret_cast<CSSM_HANDLE *>(inUniqueRecord.RecordIdentifier.Data);
}

void
SSDLSession::freeUniqueRecord(CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	if (inUniqueRecord.RecordIdentifier.Length != 0
		&& inUniqueRecord.RecordIdentifier.Data != NULL)
	{
		inUniqueRecord.RecordIdentifier.Length = 0;
		allocator().free(inUniqueRecord.RecordIdentifier.Data);
	}
	allocator().free(&inUniqueRecord);
}
