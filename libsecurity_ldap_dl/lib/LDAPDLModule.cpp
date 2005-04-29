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

#include "Relation.h"
#include "LDAPDLModule.h"
#include "CommonCode.h"

TableRelation *LDAPDLModule::mSchemaRelationRelation = NULL,
			  *LDAPDLModule::mSchemaAttributeRelation = NULL,
			  *LDAPDLModule::mSchemaIndexRelation = NULL,
			  *LDAPDLModule::mSchemaParsingModuleRelation = NULL;
DSX509Relation *LDAPDLModule::mX509Relation = NULL;
RelationMap *LDAPDLModule::mRelationMap;


void LDAPDLModule::SetupSchemaRelationRelation ()
{
	// setup the CSSM_DL_DB_SCHEMA_INDEXES
	mSchemaRelationRelation = new TableRelation (CSSM_DL_DB_SCHEMA_INFO, 2);
	mSchemaRelationRelation->SetColumnNames ("RelationID", "RelationName");
	mSchemaRelationRelation->SetColumnIDs (0, 1);
	mSchemaRelationRelation->SetColumnFormats (CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_STRING);
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INFO), new StringValue ("CSSM_DL_DB_SCHEMA_INFO"));
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES), new StringValue ("CSSM_DL_DB_SCHEMA_ATTRIBUTES"));
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INDEXES), new StringValue ("CSSM_DL_DB_SCHEMA_INDEXES"));
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE), new StringValue ("CSSM_DL_DB_SCHEMA_PARSING_MODULE"));
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INFO] = mSchemaRelationRelation;
}



void LDAPDLModule::SetupSchemaAttributeRelation ()
{
	mSchemaAttributeRelation = new TableRelation (CSSM_DL_DB_SCHEMA_ATTRIBUTES, 6);
	mSchemaAttributeRelation->SetColumnNames ("RelationID", "AttributeID", "AttributeNameFormat", "AttributeName", "AttributeNameID", "AttributeFormat");
	mSchemaAttributeRelation->SetColumnFormats (CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32,
												CSSM_DB_ATTRIBUTE_FORMAT_STRING, CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32);
	mSchemaAttributeRelation->SetColumnIDs (0, 1, 2, 3, 4, 5);
												
	// setup the index
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INFO),
										new UInt32Value (0),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("RelationID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INFO),
										new UInt32Value (1),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("RelationName"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_STRING));
	
	// setup the attribute table
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES),
										new UInt32Value (0),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("RelationID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES),
										new UInt32Value (1),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AttributeID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES),
										new UInt32Value (2),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AttributeNameFormat"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES),
										new UInt32Value (3),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AttributeName"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES),
										new UInt32Value (4),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AttributeName"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_STRING));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES),
										new UInt32Value (5),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AttributeNameID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	
	// setup the index table
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INDEXES),
										new UInt32Value (0),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("RelationID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INDEXES),
										new UInt32Value (1),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("IndexID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INDEXES),
										new UInt32Value (2),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AttributeID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INDEXES),
										new UInt32Value (3),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("IndexType"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INDEXES),
										new UInt32Value (4),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("IndexedDataLocation"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	
	// setup the schema parsing module
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE),
										new UInt32Value (0),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("RelationID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE),
										new UInt32Value (1),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AttributeID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE),
										new UInt32Value (2),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("ModuleID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_BLOB));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE),
										new UInt32Value (3),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("AddInVersion"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_STRING));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE),
										new UInt32Value (4),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("SSID"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE),
										new UInt32Value (5),
										new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
										new StringValue ("SubserviceType"),
										NULL,
										new UInt32Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32));
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_ATTRIBUTES] = mSchemaAttributeRelation;
}



void LDAPDLModule::SetupSchemaIndexRelation ()
{
	mSchemaIndexRelation = new TableRelation (CSSM_DL_DB_SCHEMA_INDEXES, 5);
	mSchemaIndexRelation->SetColumnNames ("RelationID", "IndexID", "AttributeID", "IndexType", "IndexedDataLocation");
	mSchemaIndexRelation->SetColumnFormats (CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32,
											CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32);
	mSchemaIndexRelation->SetColumnIDs (0, 1, 2, 3, 4);

	// none of our table relations is indexed, but the certificate relation is, sort of.  Add an index relation for the certificate relation
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (0),
									new UInt32Value ('ctyp'),
									new UInt32Value (0),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (0),
									new UInt32Value ('issu'),
									new UInt32Value (0),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (0),
									new UInt32Value ('snbr'),
									new UInt32Value (0),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (1),
									new UInt32Value ('ctyp'),
									new UInt32Value (1),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (2),
									new UInt32Value ('alis'),
									new UInt32Value (1),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (3),
									new UInt32Value ('subj'),
									new UInt32Value (1),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (4),
									new UInt32Value ('issu'),
									new UInt32Value (1),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (5),
									new UInt32Value ('snbr'),
									new UInt32Value (1),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (6),
									new UInt32Value ('skid'),
									new UInt32Value (1),
									new UInt32Value (1));

	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
									new UInt32Value (7),
									new UInt32Value ('hpky'),
									new UInt32Value (1),
									new UInt32Value (1));

	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INDEXES] = mSchemaIndexRelation;
}



void LDAPDLModule::SetupSchemaParsingModuleRelation ()
{
	mSchemaParsingModuleRelation = new TableRelation (CSSM_DL_DB_SCHEMA_PARSING_MODULE, 6);
	mSchemaParsingModuleRelation->SetColumnNames ("RelationID", "AttributeID", "ModuleID", "AddinVersion", "SSID", "SubserviceType");
	mSchemaParsingModuleRelation->SetColumnIDs (0, 1, 2, 3, 4, 5);
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_PARSING_MODULE] = mSchemaParsingModuleRelation;
}



void LDAPDLModule::SetupX509Relation ()
{
	mX509Relation = new DSX509Relation ();
	
	// add the relation to the attributes table
	int i;
	int max = mX509Relation->GetNumberOfColumns ();
	uint32* columnIDs = mX509Relation->GetColumnIDs ();
	Tuple* columnNames = mX509Relation->GetColumnNames ();

	for (i = 0; i < max; ++i)
	{
		mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
											new UInt32Value (columnIDs[i]),
											new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
											columnNames->GetValue (i),
											NULL,
											new UInt32Value (mX509Relation->GetColumnFormat (i)));
	}
	(*mRelationMap)[CSSM_DL_DB_RECORD_X509_CERTIFICATE] = mX509Relation;

	// add to the attribute info database
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new StringValue ("CSSM_DL_DB_RECORD_X509_CERTIFICATE"));

}



void LDAPDLModule::InitializeRelations ()
{
	mRelationMap = new RelationMap;
	SetupSchemaRelationRelation ();
	SetupSchemaAttributeRelation ();
	SetupSchemaIndexRelation ();
	SetupSchemaParsingModuleRelation ();
	SetupX509Relation ();
}



LDAPDLModule::LDAPDLModule (pthread_mutex_t *globalLock, CSSM_SPI_ModuleEventHandler CssmNotifyCallback, void* CssmNotifyCallbackCtx) : 
	DataStorageLibrary (globalLock, CssmNotifyCallback, CssmNotifyCallbackCtx)
{
	if (mSchemaRelationRelation == NULL)
	{
		InitializeRelations ();
	}
}



LDAPDLModule::~LDAPDLModule ()
{
}



AttachedInstance* LDAPDLModule::MakeAttachedInstance ()
{
	return new LDAPAttachedInstance ();
}



Relation* LDAPDLModule::LookupRelation (CSSM_DB_RECORDTYPE recordType)
{
	RelationMap::iterator r = (*mRelationMap).find (recordType);
	if (r == (*mRelationMap).end ())
	{
		throw CSSMError (CSSMERR_DL_INVALID_RECORDTYPE);
	}
	
	return (*r).second;
}



Database* LDAPAttachedInstance::MakeDatabaseObject ()
{
	return new LDAPDatabase (this);
}



LDAPDatabase::LDAPDatabase (AttachedInstance *ai) : Database (ai), mNextHandle (0)
{
}



LDAPDatabase::~LDAPDatabase ()
{
}



void LDAPDatabase::DbOpen (const char* DbName, const CSSM_NET_ADDRESS *dbLocation, const CSSM_DB_ACCESS_TYPE accessRequest,
						   const CSSM_ACCESS_CREDENTIALS *accessCredentials, const void* openParameters)
{
	if (DbName == NULL)
	{
		throw CSSMError (CSSMERR_CSSM_INVALID_POINTER);
	}
	
	// this database can only be opened read only.  Any attempt to gain write permissions will be dealt with severely
	if (accessRequest != CSSM_DB_ACCESS_READ)
	{
		throw CSSMError (CSSMERR_DL_INVALID_ACCESS_REQUEST);
	}
	
	mDatabaseName = DbName;
}



void LDAPDatabase::DbClose ()
{
	// we're about to disappear...
}



void LDAPDatabase::DbGetDbNameFromHandle (char** dbName)
{
	// return the name of our storage library
	
	*dbName = (char*) mAttachedInstance->malloc (mDatabaseName.length () + 1);
	strcpy (*dbName, mDatabaseName.c_str ());
}



void LDAPDatabase::CopyAttributes (Relation *r, Tuple *t, CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes)
{
	// fill out each attribute requested
	uint32 i;
	CSSM_DB_ATTRIBUTE_DATA* d = attributes->AttributeData;

	for (i = 0; i < attributes->NumberOfAttributes; ++i)
	{
		int columnNumber;
		switch (d->Info.AttributeNameFormat)
		{
			case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
			{
				columnNumber = r->GetColumnNumber (d->Info.Label.AttributeName);
				break;
			}
			
			case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
			{
				columnNumber = r->GetColumnNumber (d->Info.Label.AttributeID);
				break;
			}
			
			default:
			{
				throw CSSMError (CSSMERR_DL_INVALID_FIELD_NAME);
			}
		}
		
		// copy the value from the tuple into the field
		Value *v = t->GetValue (columnNumber);
		d->Value = (CSSM_DATA_PTR) mAttachedInstance->malloc (sizeof (CSSM_DATA));
		if (v != NULL)
		{
			d->Info.AttributeFormat = v->GetValueType();
			uint32 numItems, length;
			uint8* value;
			
			value = v->CloneContents (mAttachedInstance, numItems, length);
			d->Value->Data = value;
			d->Value->Length = length;
			d->NumberOfValues = numItems;
		}
		else
		{
			d->Value->Data = NULL;
			d->Value->Length = 0;
			d->NumberOfValues = 0;
		}
		
		d += 1;
	}
}



const uint32 kMaximumSelectionPredicates = 1000;



void LDAPDatabase::GetDataFromTuple (Tuple *t, CSSM_DATA &data)
{
	// get the data from the tuple
	CSSM_DATA tmpData;
	t->GetData (tmpData);
	
	// clone it
	data.Data = (uint8 *)mAttachedInstance->malloc (tmpData.Length);
	data.Length = tmpData.Length;
	memmove(data.Data, tmpData.Data, data.Length);
}



CSSM_HANDLE LDAPDatabase::DbDataGetFirst (const CSSM_QUERY *query,
										  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
										  CSSM_DATA_PTR data,
										  CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	// do error checking on the attributes
	if (attributes && attributes->SemanticInformation != 0)
	{
		throw CSSMError (CSSMERR_DL_INVALID_QUERY);
	}
	
	// set an arbitrary limit on the number of selection predicates -- mostly for range checking
	if (query->NumSelectionPredicates > kMaximumSelectionPredicates)
	{
		throw CSSMError (CSSMERR_DL_UNSUPPORTED_NUM_SELECTION_PREDS);
	}
	
	// lookup our relation in the relation map
	Relation* r = LDAPDLModule::LookupRelation (query->RecordType);
	
	// make a query for this request
	Query *q = r->MakeQuery (query);
	
	UniqueIdentifier *id;
	Tuple* t = q->GetNextTuple (id);
	
	if (t == NULL)
	{
		*uniqueID = NULL;
		delete q;
		throw CSSMError (CSSMERR_DL_ENDOFDATA);
	}
	else
	{
		if (attributes != NULL)
		{
			CopyAttributes (r, t, attributes);
		}
		
		if (data != NULL)
		{
			GetDataFromTuple (t, *data);
		}

		// make a new handle for this query
		CSSM_HANDLE h = mNextHandle++;
		mQueryMap[h] = q;
		*uniqueID = new CSSM_DB_UNIQUE_RECORD;
		ExportUniqueID (id, uniqueID);
		return h; // for now
	}
}



void LDAPDatabase::ExportUniqueID (UniqueIdentifier *id, CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	CSSM_DB_UNIQUE_RECORD *ur = new CSSM_DB_UNIQUE_RECORD;
	id->Export (*ur);
	ur->RecordIdentifier.Length = sizeof (id);
	ur->RecordIdentifier.Data = (uint8*) id;
	*uniqueID = ur;
}



void LDAPDatabase::DbDataGetNext (CSSM_HANDLE resultsHandle,
								  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
								  CSSM_DATA_PTR data,
								  CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	QueryMap::iterator it = mQueryMap.find (resultsHandle);
	if (it == mQueryMap.end ())
	{
		throw CSSMError (CSSMERR_DL_INVALID_DB_HANDLE);
	}
	
	Query* q = (*it).second;
	
	UniqueIdentifier* id;
	Tuple *t = q->GetNextTuple (id);
	Relation *r = q->GetRelation ();

	if (t == NULL)
	{
		delete q;
		mQueryMap.erase (resultsHandle);
		throw CSSMError (CSSMERR_DL_ENDOFDATA);
	}
	else
	{
		if (attributes)
		{
			CopyAttributes (r, t, attributes);
		}
		
		// make a new handle for this query
		ExportUniqueID (id, uniqueID);
	}
}



void LDAPDatabase::DbDataAbortQuery (CSSM_HANDLE resultsHandle)
{
	QueryMap::iterator it = mQueryMap.find (resultsHandle);
	if (it == mQueryMap.end ())
	{
		throw CSSMError (CSSMERR_DL_INVALID_DB_HANDLE);
	}
	
	Query* q = (*it).second;
	delete q;
	mQueryMap.erase (resultsHandle);
}



void LDAPDatabase::DbFreeUniqueRecord (CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord)
{
	UniqueIdentifier* id = (UniqueIdentifier*) uniqueRecord->RecordIdentifier.Data;
	delete id;
	delete uniqueRecord;
}



void LDAPDatabase::DbDataGetFromUniqueRecordID (const CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord,
												CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
												CSSM_DATA_PTR data)
{
	// recover the identifier
	UniqueIdentifier* id = (UniqueIdentifier*) uniqueRecord->RecordIdentifier.Data;
	CSSM_DB_RECORDTYPE recordType = id->GetRecordType ();
	Relation* r = LDAPDLModule::LookupRelation (recordType);
	Tuple* t = r->GetTupleFromUniqueIdentifier (id);
	if (attributes)
	{
		CopyAttributes (r, t, attributes);
	}
	t->GetData (*data);
}
