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
	
	static columnInfoLoader ciLoader[] = {
		{ 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 0, "RelationName", CSSM_DB_ATTRIBUTE_FORMAT_STRING },
	};
	
	mSchemaRelationRelation = new TableRelation (CSSM_DL_DB_SCHEMA_INFO, (sizeof(ciLoader) / sizeof(columnInfoLoader)), ciLoader);	
	
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INFO), new StringValue ("CSSM_DL_DB_SCHEMA_INFO"));
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_ATTRIBUTES), new StringValue ("CSSM_DL_DB_SCHEMA_ATTRIBUTES"));
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_INDEXES), new StringValue ("CSSM_DL_DB_SCHEMA_INDEXES"));
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_SCHEMA_PARSING_MODULE), new StringValue ("CSSM_DL_DB_SCHEMA_PARSING_MODULE"));
	mSchemaRelationRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new StringValue ("CSSM_DL_DB_RECORD_X509_CERTIFICATE"));
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INFO] = mSchemaRelationRelation;
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INDEXES] = mSchemaRelationRelation;
}


struct attributeLoader {
	UInt32 relationID;
	UInt32 attrID;
	const char * attrName;
	UInt32 attrFormat;
};

void LDAPDLModule::SetupSchemaAttributeRelation ()
{
	static columnInfoLoader ciLoader[] = {
		{ 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 1, "AttributeID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 2, "AttributeNameFormat", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 3, "AttributeName", CSSM_DB_ATTRIBUTE_FORMAT_STRING },
		{ 4, "AttributeNameID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 5, "AttributeFormat", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
	};
	mSchemaAttributeRelation = new TableRelation (CSSM_DL_DB_SCHEMA_ATTRIBUTES, (sizeof(ciLoader) / sizeof(columnInfoLoader)), ciLoader);	
	
	static struct attributeLoader loader[] = {	
		// setup the index
		{  CSSM_DL_DB_SCHEMA_INFO, 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_INFO, 1, "RelationName", CSSM_DB_ATTRIBUTE_FORMAT_STRING },
		
		// setup the attribute table
		{  CSSM_DL_DB_SCHEMA_ATTRIBUTES, 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_ATTRIBUTES, 1, "AttributeID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_ATTRIBUTES, 2, "AttributeNameFormat", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_ATTRIBUTES, 3, "AttributeName", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_ATTRIBUTES, 4, "AttributeName", CSSM_DB_ATTRIBUTE_FORMAT_STRING },
		{  CSSM_DL_DB_SCHEMA_ATTRIBUTES, 5, "AttributeNameID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },

		// setup the index table
		{  CSSM_DL_DB_SCHEMA_INDEXES, 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_INDEXES, 1, "IndexID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_INDEXES, 2, "AttributeID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_INDEXES, 3, "IndexType", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_INDEXES, 4, "IndexedDataLocation", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },

		// setup the schema parsing module
		{  CSSM_DL_DB_SCHEMA_PARSING_MODULE, 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_PARSING_MODULE, 1, "AttributeID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_PARSING_MODULE, 2, "ModuleID", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{  CSSM_DL_DB_SCHEMA_PARSING_MODULE, 3, "AddInVersion", CSSM_DB_ATTRIBUTE_FORMAT_STRING },
		{  CSSM_DL_DB_SCHEMA_PARSING_MODULE, 4, "SSID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{  CSSM_DL_DB_SCHEMA_PARSING_MODULE, 5, "SubserviceType", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },

	};
	
	int nrows = sizeof(loader) / sizeof(struct attributeLoader);
	for(int i=0; i < nrows; i++) 	
		mSchemaAttributeRelation->AddTuple (new UInt32Value (loader[i].relationID),new UInt32Value (loader[i].attrID),new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING),new StringValue (loader[i].attrName), NULL, new UInt32Value (loader[i].attrFormat));

	(*mRelationMap)[CSSM_DL_DB_SCHEMA_ATTRIBUTES] = mSchemaAttributeRelation;
}



struct indexLoader {
	UInt32 relationID;
	UInt32 indexID;
	UInt32 attributeID;
	UInt32 indexType;
	UInt32 indexedDataLocation;
};

void LDAPDLModule::SetupSchemaIndexRelation ()
{
	static columnInfoLoader ciLoader[] = {
		{ 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 1, "IndexID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 2, "AttributeID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 3, "IndexType", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 4, "IndexedDataLocation", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
	};
	mSchemaIndexRelation = new TableRelation (CSSM_DL_DB_SCHEMA_INDEXES, (sizeof(ciLoader) / sizeof(columnInfoLoader)), ciLoader);

	static struct indexLoader loader[] = {	
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 0, 'ctyp', 0, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 1, 'issu', 0, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 2, 'snbr', 0, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 3, 'alis', 1, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 4, 'subj', 1, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 5, 'issu', 1, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 6, 'snbr', 1, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 7, 'skid', 1, 1 },
		{ CSSM_DL_DB_RECORD_X509_CERTIFICATE, 8, 'hpky', 1, 1 },
	};
	
	int nrows = sizeof(loader) / sizeof(struct indexLoader);
	for(int i=0; i < nrows; i++) 	
		mSchemaIndexRelation->AddTuple (new UInt32Value (loader[i].relationID),new UInt32Value (loader[i].indexID), new UInt32Value (loader[i].attributeID), new UInt32Value (loader[i].indexType), new UInt32Value (loader[i].indexedDataLocation));

	(*mRelationMap)[CSSM_DL_DB_SCHEMA_INDEXES] = mSchemaIndexRelation;
}



void LDAPDLModule::SetupSchemaParsingModuleRelation ()
{
	static columnInfoLoader ciLoader[] = {
		{ 0, "RelationID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 1, "AttributeID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 2, "ModuleID", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{ 3, "AddinVersion", CSSM_DB_ATTRIBUTE_FORMAT_STRING },
		{ 4, "SSID", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 5, "SubserviceType", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
	};
	mSchemaParsingModuleRelation = new TableRelation (CSSM_DL_DB_SCHEMA_PARSING_MODULE, (sizeof(ciLoader) / sizeof(columnInfoLoader)), ciLoader);
	(*mRelationMap)[CSSM_DL_DB_SCHEMA_PARSING_MODULE] = mSchemaParsingModuleRelation;
}



void LDAPDLModule::SetupX509Relation ()
{
	static columnInfoLoader ciLoader[] = {
		{ 'ctyp', "CertType", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 'cenc', "CertEncoding", CSSM_DB_ATTRIBUTE_FORMAT_UINT32 },
		{ 'labl', "PrintName", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{ 'alis', "Alias", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{ 'subj', "Subject", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{ 'issu', "Issuer", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{ 'snbr', "SerialNumber", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{ 'skid', "SubjectKeyIdentifier", CSSM_DB_ATTRIBUTE_FORMAT_BLOB },
		{ 'hpky', "PublicKeyHash", CSSM_DB_ATTRIBUTE_FORMAT_BLOB }
	};
	mX509Relation = new DSX509Relation (CSSM_DL_DB_RECORD_X509_CERTIFICATE, (sizeof(ciLoader) / sizeof(columnInfoLoader)), ciLoader);
	
	// add the relation to the attributes table
	int max = mX509Relation->GetNumberOfColumns ();

	for (int i = 0; i < max; ++i)
		mSchemaAttributeRelation->AddTuple (new UInt32Value (CSSM_DL_DB_RECORD_X509_CERTIFICATE), new UInt32Value (mX509Relation->GetColumnIDs (i)), new UInt32Value (CSSM_DB_ATTRIBUTE_NAME_AS_STRING), 
											mX509Relation->GetColumnName (i), NULL, new UInt32Value (mX509Relation->GetColumnFormat (i)));
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
		InitializeRelations ();
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
		CSSMError::ThrowCSSMError (CSSMERR_DL_INVALID_RECORDTYPE);
	
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
		CSSMError::ThrowCSSMError (CSSMERR_CSSM_INVALID_POINTER);
	
	// this database can only be opened read only.  Any attempt to gain write permissions will be dealt with severely
	if (accessRequest != CSSM_DB_ACCESS_READ)
		CSSMError::ThrowCSSMError (CSSMERR_DL_INVALID_ACCESS_REQUEST);
	
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
	if(t == NULL || r == NULL || attributes == NULL) return;
	
	uint32 i;
	CSSM_DB_ATTRIBUTE_DATA* d = attributes->AttributeData;
	attributes->DataRecordType = r->GetRecordType ();
	Value *v;
	
	for (i = 0; i < attributes->NumberOfAttributes; ++i, d++) {
		int columnNumber;
		switch (d->Info.AttributeNameFormat) {
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
			columnNumber = r->GetColumnNumber (d->Info.Label.AttributeName);
			break;			
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
			columnNumber = r->GetColumnNumber (d->Info.Label.AttributeID);
			break;			
		default:
			columnNumber = r->GetColumnNumber (d->Info.Label.AttributeID);
			break;
		}
		
		if ( columnNumber != -1 && (v = t->GetValue (columnNumber)) != NULL) {
			d->Value = (CSSM_DATA_PTR) mAttachedInstance->malloc (sizeof (CSSM_DATA));
			d->Info.AttributeFormat = v->GetValueType();
			uint32 numItems, length;
			
			d->Value->Data = v->CloneContents (mAttachedInstance, numItems, length);
			d->Value->Length = length;
			d->NumberOfValues = numItems;
		} else {
			d->Value = NULL;
			d->NumberOfValues = 0;
		}
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


void LDAPDatabase::processNext(CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes, CSSM_DATA_PTR data,CSSM_DB_UNIQUE_RECORD_PTR *uniqueID, Query *q, Relation *r)
{
	UniqueIdentifier *id;
	Tuple* t = q->GetNextTuple (id);
	
	if (t == NULL)  CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	if(attributes != NULL)  CopyAttributes (r, t, attributes);
	if (data != NULL) GetDataFromTuple (t, *data);
	
	// new record unique ID
	ExportUniqueID (id, uniqueID);
}

CSSM_HANDLE LDAPDatabase::DbDataGetFirst (const CSSM_QUERY *query,
										  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
										  CSSM_DATA_PTR data,
										  CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	// since we really only track one record type, record type CSSM_DL_DB_RECORD_ANY is the same as
	// CSSM_DL_DB_RECORD_X509_CERTIFICATE

	*uniqueID = NULL;

	if(!query) CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);

	CSSM_DB_RECORDTYPE recordType = query->RecordType;

	switch (recordType) {
		case CSSM_DL_DB_RECORD_ANY:
			recordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
			break;
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
	if ((attributes != NULL) && (attributes->SemanticInformation != 0)) CSSMError::ThrowCSSMError (CSSMERR_DL_INVALID_QUERY);
	
	// set an arbitrary limit on the number of selection predicates -- mostly for range checking
	if (query->NumSelectionPredicates > kMaximumSelectionPredicates) CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_NUM_SELECTION_PREDS);
	
	// lookup our relation in the relation map
	Relation* r = LDAPDLModule::LookupRelation (recordType);
	
	// make a query for this request
	Query *q = r->MakeQuery (query);
	
	if(!q) CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	
	processNext(attributes, data, uniqueID, q, r);
	
	// make a new handle for this query
	CSSM_HANDLE h = mNextHandle++;
	mQueryMap[h] = q;
	return h;
}


bool LDAPDatabase::DbDataGetNext (CSSM_HANDLE resultsHandle,
								  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
								  CSSM_DATA_PTR data,
								  CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	*uniqueID = NULL;
	
	QueryMap::iterator it = mQueryMap.find (resultsHandle);
	if (it == mQueryMap.end ())  CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	
	Query* q = (*it).second;
	
	if(!q) CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	Relation *r = q->GetRelation ();

	processNext(attributes, data, uniqueID, q, r);
	
	return true;
}


void LDAPDatabase::ExportUniqueID (UniqueIdentifier *id, CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	CSSM_DB_UNIQUE_RECORD *ur = new CSSM_DB_UNIQUE_RECORD;
	// id->Export (*ur);
	ur->RecordIdentifier.Length = sizeof (id);
	ur->RecordIdentifier.Data = (uint8*) id;
	*uniqueID = ur;
}


void LDAPDatabase::DbDataAbortQuery (CSSM_HANDLE resultsHandle)
{
	QueryMap::iterator it = mQueryMap.find (resultsHandle);
	if (it == mQueryMap.end ()) return;
	
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
	if(uniqueRecord == NULL) return;
	
	UniqueIdentifier* id = (UniqueIdentifier*) uniqueRecord->RecordIdentifier.Data;
	if(id==NULL) return;
	
	CSSM_DB_RECORDTYPE recordType = id->GetRecordType ();
	Relation* r = LDAPDLModule::LookupRelation (recordType);
	if(r == NULL)return;
	
	Tuple* t = r->GetTupleFromUniqueIdentifier (id);
	if(t == NULL) return;
	
	if(attributes != NULL)  CopyAttributes (r, t, attributes);
	if (data != NULL) GetDataFromTuple (t, *data);
}
