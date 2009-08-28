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
#include "DotMacDLModule.h"
#include "CommonCode.h"
#include <security_utilities/debugging.h>

TableRelation *DotMacDLModule::mSchemaRelationRelation = NULL,
			  *DotMacDLModule::mSchemaAttributeRelation = NULL,
			  *DotMacDLModule::mSchemaIndexRelation = NULL,
			  *DotMacDLModule::mSchemaParsingModuleRelation = NULL;
DotMacRelation *DotMacDLModule::mDotMacRelation = NULL;
RelationMap *DotMacDLModule::mRelationMap;


void DotMacDLModule::SetupSchemaRelationRelation ()
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
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new StringValue ("CSSM_DL_DB_RECORD_X509_CERTIFICATE"));
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INFO] = mSchemaRelationRelation;
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INDEXES] = mSchemaRelationRelation;
}



void DotMacDLModule::SetupSchemaAttributeRelation ()
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



void DotMacDLModule::SetupSchemaIndexRelation ()
{
	mSchemaIndexRelation = new TableRelation (CSSM_DL_DB_SCHEMA_INDEXES, 5);
	mSchemaIndexRelation->SetColumnNames ("RelationID", "IndexID", "AttributeID", "IndexType", "IndexedDataLocation");
	mSchemaIndexRelation->SetColumnFormats (CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32,
											CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32);
	mSchemaIndexRelation->SetColumnIDs (0, 1, 2, 3, 4);
	// none of our table relations is indexed, but the certificate relation is, sort of.  Add an index relation for the certificate relation
	//
	//                                                          RelationID                              IndexID               AttributeID            IndexType         IndexedDataLocation
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (0), new UInt32Value ('ctyp'), new UInt32Value (0), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (1), new UInt32Value ('issu'), new UInt32Value (0), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (2), new UInt32Value ('snbr'), new UInt32Value (0), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (3), new UInt32Value ('alis'), new UInt32Value (1), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (4), new UInt32Value ('subj'), new UInt32Value (1), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (5), new UInt32Value ('issu'), new UInt32Value (1), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (6), new UInt32Value ('snbr'), new UInt32Value (1), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (7), new UInt32Value ('skid'), new UInt32Value (1), new UInt32Value (1));
	mSchemaIndexRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (8), new UInt32Value ('hpky'), new UInt32Value (1), new UInt32Value (1));


	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INDEXES] = mSchemaIndexRelation;
}



void DotMacDLModule::SetupSchemaParsingModuleRelation ()
{
	mSchemaParsingModuleRelation = new TableRelation (CSSM_DL_DB_SCHEMA_PARSING_MODULE, 6);
	mSchemaParsingModuleRelation->SetColumnNames ("RelationID", "AttributeID", "ModuleID", "AddinVersion", "SSID", "SubserviceType");
	mSchemaParsingModuleRelation->SetColumnIDs (0, 1, 2, 3, 4, 5);
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_PARSING_MODULE] = mSchemaParsingModuleRelation;
}



void DotMacDLModule::SetupDotMacRelation ()
{
	mDotMacRelation = new DotMacRelation ();
	
	// add the relation to the attributes table
	int i;
	
	/* CSSM_DL_DB_RECORD_X509_CERTIFICATE */
	int max = mDotMacRelation->GetNumberOfColumns ();
	uint32* columnIDs = mDotMacRelation->GetColumnIDs ();
	Tuple* columnNames = mDotMacRelation->GetColumnNames ();

	for (i = 0; i < max; ++i)
	{
		mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE),
											new UInt32Value (columnIDs[i]),
											new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),
											columnNames->GetValue (i),
											NULL,
											new UInt32Value (mDotMacRelation->GetColumnFormat (i)));
	}
	(*mRelationMap)[CSSM_DL_DB_RECORD_X509_CERTIFICATE] = mDotMacRelation;

	// add to the attribute info database
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new StringValue ("CSSM_DL_DB_RECORD_X509_CERTIFICATE"));
	
	
}



void DotMacDLModule::InitializeRelations ()
{
	mRelationMap = new RelationMap;
	SetupSchemaRelationRelation ();
	SetupSchemaAttributeRelation ();
	SetupSchemaIndexRelation ();
	SetupSchemaParsingModuleRelation ();
	SetupDotMacRelation ();
}



DotMacDLModule::DotMacDLModule (pthread_mutex_t *globalLock, CSSM_SPI_ModuleEventHandler CssmNotifyCallback, void* CssmNotifyCallbackCtx) : 
	DataStorageLibrary (globalLock, CssmNotifyCallback, CssmNotifyCallbackCtx)
{
	if (mSchemaRelationRelation == NULL)
	{
		InitializeRelations ();
	}
}



DotMacDLModule::~DotMacDLModule ()
{
}



AttachedInstance* DotMacDLModule::MakeAttachedInstance ()
{
	return new DotMacAttachedInstance ();
}



Relation* DotMacDLModule::LookupRelation (CSSM_DB_RECORDTYPE recordType)
{
	RelationMap::iterator r = (*mRelationMap).find (recordType);
	if (r == (*mRelationMap).end ())
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_INVALID_RECORDTYPE);
	}
	
	return (*r).second;
}



Database* DotMacAttachedInstance::MakeDatabaseObject ()
{
	return new DotMacDatabase (this);
}



DotMacDatabase::DotMacDatabase (AttachedInstance *ai) : Database (ai), mNextHandle (0)
{
	secdebug("dotmacdl", "DotMacDatabase construct this %p", this);
}



DotMacDatabase::~DotMacDatabase ()
{
	secdebug("dotmacdl", "DotMacDatabase destruct this %p", this);
}



void DotMacDatabase::DbOpen (const char* DbName, const CSSM_NET_ADDRESS *dbLocation, const CSSM_DB_ACCESS_TYPE accessRequest,
						   const CSSM_ACCESS_CREDENTIALS *accessCredentials, const void* openParameters)
{
	secdebug("dotmacdl", "DotMacDatabase::DbOpen this %p", this);
	if (DbName == NULL)
	{
		CSSMError::ThrowCSSMError (CSSMERR_CSSM_INVALID_POINTER);
	}
	
	// this database can only be opened read only.  Any attempt to gain write permissions will be dealt with severely
	if (accessRequest != CSSM_DB_ACCESS_READ)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_INVALID_ACCESS_REQUEST);
	}
	
	mDatabaseName = DbName;
}



void DotMacDatabase::DbClose ()
{
	secdebug("dotmacdl", "DotMacDatabase::DbClose this %p", this);
	// we're about to disappear...
}



void DotMacDatabase::DbGetDbNameFromHandle (char** dbName)
{
	// return the name of our storage library
	
	secdebug("dotmacdl", "DotMacDatabase::DbGetDbNameFromHandle this %p", this);
	*dbName = (char*) mAttachedInstance->malloc (mDatabaseName.length () + 1);
	strcpy (*dbName, mDatabaseName.c_str ());
}



void DotMacDatabase::CopyAttributes (Relation *r, Tuple *t, CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes)
{
	secdebug("dotmacdl", "DotMacDatabase::CopyAttributes this %p", this);

	// fill out each attribute requested
	uint32 i;
	CSSM_DB_ATTRIBUTE_DATA* d = attributes->AttributeData;
	if (r) attributes->DataRecordType = r->GetRecordType ();
	else return;


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
				columnNumber = -1;
			}
		}
		
		if ( columnNumber == -1 ) {
			d->Value->Data = NULL;
			d->Value->Length = 0;
			d->NumberOfValues = 0;
		} else {
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
		}
		
		d += 1;
	}
}



const uint32 kMaximumSelectionPredicates = 1000;



void DotMacDatabase::GetDataFromTuple (Tuple *t, CSSM_DATA &data)
{
	// get the data from the tuple
	CSSM_DATA tmpData;
	t->GetData (tmpData);
	
	// clone it
	data.Data = (uint8 *)mAttachedInstance->malloc (tmpData.Length);
	data.Length = tmpData.Length;
	memmove(data.Data, tmpData.Data, data.Length);
}



CSSM_HANDLE DotMacDatabase::DbDataGetFirst (const CSSM_QUERY *query,
										  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
										  CSSM_DATA_PTR data,
										  CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	// since we really only track one record type, record type CSSM_DL_DB_RECORD_ANY is the same as
	// CSSM_DL_DB_RECORD_X509_CERTIFICATE
	
	//secdebug("dotmacdl", "DotMacDatabase::DbDataGetFirst this %p", this);
	CSSM_DB_RECORDTYPE recordType = query->RecordType;
	if (recordType == CSSM_DL_DB_RECORD_ANY )
	{
		recordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	}
		
	switch (recordType) {
		case CSSM_DL_DB_SCHEMA_INFO:
			break;
		case CSSM_DL_DB_SCHEMA_INDEXES:
			break;
		case CSSM_DL_DB_SCHEMA_ATTRIBUTES:
			break;
		case CSSM_DL_DB_SCHEMA_PARSING_MODULE:
			break;
		case CSSM_DL_DB_RECORD_X509_CERTIFICATE:
			break;
		default:
			CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
			break;
	}
	
	// do error checking on the attributes
	if ((attributes != NULL) && (attributes->SemanticInformation != 0))
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_INVALID_QUERY);
	}
	
	// set an arbitrary limit on the number of selection predicates -- mostly for range checking
	if (query->NumSelectionPredicates > kMaximumSelectionPredicates)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_NUM_SELECTION_PREDS);
	}
	
	// lookup our relation in the relation map
	Relation* r = DotMacDLModule::LookupRelation (recordType);
	
	// make a query for this request
	Query *q = r->MakeQuery (query);
	
	UniqueIdentifier *id;
	Tuple* t = q->GetNextTuple (id);
	
	if (t == NULL)
	{
		*uniqueID = NULL;
		delete q;
		CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	}
	else
	{
		if(attributes != NULL) 
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
		
	throw 0; // keep the compiler happy
}



void DotMacDatabase::ExportUniqueID (UniqueIdentifier *id, CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	secdebug("dotmacdl", "DotMacDatabase::ExportUniqueID this %p id %p", this, id);
	CSSM_DB_UNIQUE_RECORD *ur = new CSSM_DB_UNIQUE_RECORD;
	id->Export (*ur);
	ur->RecordIdentifier.Length = sizeof (id);
	ur->RecordIdentifier.Data = (uint8*) id;
	*uniqueID = ur;
}



bool DotMacDatabase::DbDataGetNext (CSSM_HANDLE resultsHandle,
								  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
								  CSSM_DATA_PTR data,
								  CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	secdebug("dotmacdl", "DotMacDatabase::DbDataGetNext this %p", this);
	QueryMap::iterator it = mQueryMap.find (resultsHandle);
	if (it == mQueryMap.end ())
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	}
	
	Query* q = (*it).second;
	
	UniqueIdentifier* id;
	Tuple *t = q->GetNextTuple (id);
	Relation *r = q->GetRelation ();

	if (t == NULL)
	{
		//delete q;
		mQueryMap.erase (resultsHandle);
		CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
		return false;
	}
	else
	{
		if(attributes != NULL) 
		{
			CopyAttributes (r, t, attributes);
		}
		if (data != NULL)
		{
			GetDataFromTuple (t, *data);
		}
		
		// make a new handle for this query
		ExportUniqueID (id, uniqueID);
		return true;
	}
}



void DotMacDatabase::DbDataAbortQuery (CSSM_HANDLE resultsHandle)
{
	secdebug("dotmacdl", "DotMacDatabase::DbDataAbortQueryÊthis %p", this);
	QueryMap::iterator it = mQueryMap.find (resultsHandle);
	if (it == mQueryMap.end ())
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_INVALID_DB_HANDLE);
	}
	
	Query* q = (*it).second;
	delete q;
	mQueryMap.erase (resultsHandle);
}



void DotMacDatabase::DbFreeUniqueRecord (CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord)
{
	secdebug("dotmacdl", "DotMacDatabase::DbFreeUniqueRecord this %p", this);
	//UniqueIdentifier* id;
	//if(uniqueRecord)
		//id = (UniqueIdentifier*) uniqueRecord->RecordIdentifier.Data;
	//if(id) delete id;
	// if(uniqueRecord) delete uniqueRecord;
}



void DotMacDatabase::DbDataGetFromUniqueRecordID (const CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord,
												CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
												CSSM_DATA_PTR data)
{

	// recover the identifier
	UniqueIdentifier* id = (UniqueIdentifier*) uniqueRecord->RecordIdentifier.Data;
	secdebug("dotmacdl", "DotMacDatabase::DbDataGetFromUniqueRecordID this %p id %p", this, id);
	CSSM_DB_RECORDTYPE recordType = id->GetRecordType ();
	Relation* r = DotMacDLModule::LookupRelation (recordType);
	Tuple* t = r->GetTupleFromUniqueIdentifier (id);
	if(attributes != NULL)
	{
		CopyAttributes (r, t, attributes);
	}
	if (data != NULL)
	{
		GetDataFromTuple (t, *data);
	}
}
