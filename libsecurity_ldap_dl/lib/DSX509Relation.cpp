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

#include <string.h>
#include "CommonCode.h"
#include "DSX509Relation.h"
#include "DirectoryServices.h"
#include "CommonCrypto/CommonDigest.h"

/*
	int mNumberOfValues;
	Value* mValues;
	BlobValue *mData;
*/

DSX509Tuple::DSX509Tuple (int numberOfValues) : mNumberOfValues (numberOfValues), mValues (NULL), mData (NULL)
{
	mValues = new Value*[numberOfValues];
	int i;
	for (i = 0; i < numberOfValues; ++i)
	{
		mValues[i] = NULL;
	}
}



DSX509Tuple::~DSX509Tuple ()
{
	// walk the value array and delete each value
	int i;
	for (i = 0; i < mNumberOfValues; ++i)
	{
		if (mValues[i] != NULL)
		{
			delete mValues[i];
		}
	}
	
	delete [] mValues;
	
	if (mData != NULL)
	{
		delete mData;
	}
}



void DSX509Tuple::SetValue (int i, Value* v)
{
	mValues[i] = v;
}



Value* DSX509Tuple::GetValue (int i)
{
	return mValues[i];
}



int DSX509Tuple::GetNumberOfValues ()
{
	return mNumberOfValues;
}



void DSX509Tuple::GetData (CSSM_DATA &data)
{
	size_t t;
	const uint8* d = mData->GetRawValue (t);
	data.Data = (uint8*) d;
	data.Length = t;
}



void DSX509Tuple::SetData (BlobValue *value)
{
	mData = value;
}



DSX509UniqueIdentifier::DSX509UniqueIdentifier (DSX509Tuple *t) :
	UniqueIdentifier (CSSM_DL_DB_RECORD_X509_CERTIFICATE), mTuple (t)
{
}



DSX509UniqueIdentifier::~DSX509UniqueIdentifier ()
{
	delete mTuple;
}



void DSX509UniqueIdentifier::Export (CSSM_DB_UNIQUE_RECORD &record)
{
	// memset the whole thing to 0, we don't care
	memset (&record, 0, sizeof (CSSM_DB_UNIQUE_RECORD));
}



DSX509Tuple* DSX509UniqueIdentifier::GetTuple ()
{
	return mTuple;
}



static void * appMalloc (uint32 size, void *allocRef) {
	return (malloc (size));
}



static void appFree (void *mem_ptr, void *allocRef) {
	free (mem_ptr);
 	return;
}



static void * appRealloc (void *ptr, uint32 size, void *allocRef) {
	return (realloc (ptr, size));
}



static void * appCalloc (uint32 num, uint32 size, void *allocRef) {
	return (calloc (num, size));
}



static CSSM_API_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };



static void CheckResult (CSSM_RETURN result)
{
	if (result != 0)
	{
		throw CSSMError (result);
	}
}



void DSX509Relation::InitializeCertLibrary ()
{
	if (mCertificateLibrary != 0)
	{
		return;
	}
	
	// figure out which GUID to attach to
	const CSSM_GUID* attachGuid = &gGuidAppleX509CL;
	
	// Initialize CDSA
	CSSM_VERSION version = {2, 0};

	// load the CL
	CSSM_RETURN result = CSSM_ModuleLoad (attachGuid, CSSM_KEY_HIERARCHY_NONE, NULL, NULL);
	CheckResult (result);

	result = CSSM_ModuleAttach (attachGuid,
								&version,
								&memFuncs,
								0,					// no subservice ID
								CSSM_SERVICE_CL,	
								0,
								CSSM_KEY_HIERARCHY_NONE,
								NULL,
								0,
								NULL,
								&mCertificateLibrary);
	CheckResult (result);
}



DSX509Relation::DSX509Relation () : PartialRelation (CSSM_DL_DB_RECORD_X509_CERTIFICATE, kNumberOfX509Attributes), mCertificateLibrary (0)
{
	SetColumnNames ("CertType", "CertEncoding", "PrintName",
					"Alias", "Subject", "Issuer",
					"SerialNumber", "SubjectKeyIdentifier", "PublicKeyHash");
	SetColumnIDs ('ctyp', 'cenc', 'labl',
				  'alis', 'subj', 'issu',
				  'snbr', 'skid', 'hpky');
	SetColumnFormats (CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_BLOB,
					  CSSM_DB_ATTRIBUTE_FORMAT_BLOB, CSSM_DB_ATTRIBUTE_FORMAT_BLOB, CSSM_DB_ATTRIBUTE_FORMAT_BLOB,
					  CSSM_DB_ATTRIBUTE_FORMAT_BLOB, CSSM_DB_ATTRIBUTE_FORMAT_BLOB, CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
}



DSX509Relation::~DSX509Relation ()
{
	if (mCertificateLibrary != 0)
	{
		CSSM_ModuleDetach (mCertificateLibrary);
	}
}



Query* DSX509Relation::MakeQuery (const CSSM_QUERY* query)
{
	return new DSX509Query (this, query);
}



Tuple* DSX509Relation::GetTupleFromUniqueIdentifier (UniqueIdentifier* uniqueID)
{
	DSX509UniqueIdentifier *id = (DSX509UniqueIdentifier*) uniqueID;
	return id->GetTuple ();
}



UniqueIdentifier* DSX509Relation::ImportUniqueIdentifier (CSSM_DB_UNIQUE_RECORD *uniqueRecord)
{
	throw CSSMERR_DL_UNSUPPORTED_QUERY;
}



CSSM_CL_HANDLE DSX509Relation::GetCLHandle ()
{
	InitializeCertLibrary ();
	return mCertificateLibrary;
}



static const char* kEmailName = "Alias";
const char* kPrintName = "PrintName";

static tDirPatternMatch LookupPatternMatch (CSSM_DB_OPERATOR op)
{
	switch (op)
	{
		case CSSM_DB_EQUAL:
			return eDSiExact;
		
		case CSSM_DB_NOT_EQUAL:
			throw CSSMERR_DL_UNSUPPORTED_QUERY;
		
		case CSSM_DB_LESS_THAN:
			return eDSiLessThan;
		
		case CSSM_DB_GREATER_THAN:
			return eDSGreaterThan;
		
		case CSSM_DB_CONTAINS:
			return eDSiContains;
		
		case CSSM_DB_CONTAINS_INITIAL_SUBSTRING: // sheesh, could we use a couple of more words here?
			return eDSiStartsWith; // much better
		
		case CSSM_DB_CONTAINS_FINAL_SUBSTRING:
			return eDSiEndsWith;
		
		default:
			throw CSSMERR_DL_UNSUPPORTED_QUERY;
	}
}

	
	
DSX509Query::DSX509Query (DSX509Relation* relation, const CSSM_QUERY *queryBase) :
	Query (relation, queryBase), mCurrentNodeIndex (0), mRecordCount (0), mCurrentItem (1), mCurrentNode (NULL),
	mPatternToMatch (NULL), mRecordList (NULL), mNumberOfTuples (0), mNextTuple (0)
{
	// the parent class has already parsed the query, but we need either need a "blank" query or the query
	// must query the "alias" or "printname" attribute.  Otherwise, no go.
	
	unsigned long emailAttribute = 0;

	if (mNumSelectionPredicates > 0)
	{
		for (emailAttribute = 0; emailAttribute < mNumSelectionPredicates; ++emailAttribute)
		{
			if (mSelectionPredicates[emailAttribute].GetAttributeNameFormat () != CSSM_DB_ATTRIBUTE_NAME_AS_STRING) // string format?
			{
				continue;
			}
			
			if (strcmp (mSelectionPredicates[emailAttribute].GetAttributeName (), kEmailName) == 0 ||
				strcmp (mSelectionPredicates[emailAttribute].GetAttributeName (), kPrintName) == 0)
			{
				break;
			}
		}
		
		if (emailAttribute >= mNumSelectionPredicates) // we can't do this query, sorry
		{
			throw CSSMError (CSSMERR_DL_UNSUPPORTED_QUERY);
		}
	}
	
	// attach to open directory
	mDirectoryService = new DirectoryService ("/LDAPv3/");
	mNodeList = &mDirectoryService->GetNodeList ();
	
	// now that we have a directory service object, pre-flight our base query
	if (mNumSelectionPredicates > 0)
	{
		mPatternMatch = LookupPatternMatch (mSelectionPredicates[emailAttribute].GetOperator ());
		CSSM_DATA& valueAsData = mSelectionPredicates[emailAttribute].GetValue (0);
	
		std::string attributeValue ((char*) valueAsData.Data, valueAsData.Length); 
		mPatternToMatch = new DSDataNode (*mDirectoryService, attributeValue.c_str ());
	}
	
	// make a continue data
	mDSContext = new DSContext (*mDirectoryService);
}



DSX509Query::~DSX509Query ()
{
	delete mDSContext;
	delete mDirectoryService;
	if (mPatternToMatch) delete mPatternToMatch;
	delete mRecordList;
}



void DSX509Query::ConnectToNextNode ()
{
	bool found = false;
	while (!found)
	{
		try
		{
			if (mCurrentNodeIndex >= mNodeList->size ())
			{
				throw CSSMError (CSSMERR_DL_ENDOFDATA);
			}
		
			mCurrentNode = &((*mNodeList)[mCurrentNodeIndex++]);
			mCurrentNode->Connect ();
			mRecordCount = 0;
			found = true;
		}
		catch (DirectoryServiceException e)
		{
			continue;
		}
	}
}



void DSX509Query::SetupNextSearch ()
{
	if (mRecordList != NULL)
	{
		delete mRecordList;
	}
	
	mRecordList = new DSRecordList (*mDirectoryService, *mCurrentNode, 100 * 1024);

	DSDataList recordNames (*mDirectoryService);
	recordNames.BuildFromStrings (kDSRecordsAll, NULL);

	DSDataList recordTypes (*mDirectoryService);
	recordTypes.BuildFromStrings (kDSStdRecordTypeUsers, kDSStdRecordTypePeople, NULL);

	// this only works if we are getting all records, but it's not hurting much
	DSDataList attributeTypes (*mDirectoryService);
	attributeTypes.BuildFromStrings (kDSAttributesAll, NULL);

	DSDataNode attributeName (*mDirectoryService, kDSNAttrEMailAddress);

	// due to some open directory stupidity, a listing of all records has to be handled differently than a "search"
	if (mNumSelectionPredicates == 0)
	{
		mCurrentNode->GetRecordList (*mRecordList, recordNames, eDSExact, recordTypes, attributeTypes, false, mRecordCount, *mDSContext);
	}
	else
	{
		mCurrentNode->Search (*mRecordList, recordTypes, attributeName, mPatternMatch, *mPatternToMatch, mRecordCount, *mDSContext);
	}
	
	mCurrentItem = 1;
}



static bool CompareOIDs (const CSSM_OID &a, const CSSM_OID &b)
{
	if (a.Length != b.Length)
	{
		return false;
	}
	
	return memcmp (a.Data, b.Data, a.Length) == 0;
}



static CSSM_DATA GetValueFromFields (CSSM_FIELD *fields, uint32 numFields, const CSSM_OID& oid)
{
	uint32 i;
	for (i = 0; i < numFields; ++i)
	{
		if (CompareOIDs (fields[i].FieldOid, oid))
		{
			return fields[i].FieldValue;
		}
	}
	
	throw CSSMERR_CSSM_INVALID_ATTRIBUTE;
}



static CSSM_DATA* GetAttributeFromX509Name (CSSM_X509_NAME *name, const CSSM_OID& oid)
{
	uint32 i;
	for (i = 0; i < name->numberOfRDNs; ++i)
	{
		CSSM_X509_RDN &rdn = name->RelativeDistinguishedName[i];
		uint32 j;
		for (j = 0; j < rdn.numberOfPairs; ++j)
		{
			CSSM_X509_TYPE_VALUE_PAIR &pair = rdn.AttributeTypeAndValue[j];
			if (CompareOIDs (pair.type, oid))
			{
				return &pair.value;
			}
		}
	}
	
	return NULL;
}



Tuple* DSX509Query::GetNextTuple (UniqueIdentifier *&id)
{
	DSX509Tuple* t;

	while (true)
	{
		if (mNextTuple < mNumberOfTuples)
		{
			t = mTupleList[mNextTuple++];
		}
		else
		{
			// check to see if we are past the end of the current data list
			while (mCurrentItem > mRecordCount)
			{
				if (mDSContext->Empty ()) // move to the next node?
				{
					if (mCurrentNode != NULL) // do we have a node open?
					{
						mCurrentNode->Disconnect ();
					}
					
					ConnectToNextNode (); // this will exit the loop through an exception
				}
				
				SetupNextSearch ();
			}
			
			// get the current item from the list
			DSX509Record record ((DSX509Relation*) mRelation);
			mRecordList->GetRecordEntry (mCurrentItem++, record);
			
			mNumberOfTuples = record.GetTuple (mTupleList, kMaxTuples);
			if (mNumberOfTuples == 0) // can this record be coerced to fit?
			{
				continue; // nope
			}
			else
			{
				t = mTupleList[mNextTuple++];
			}
		}
		
		if (EvaluateTuple (t))
		{
			id = new DSX509UniqueIdentifier (t);
			return t;
		}
		else
		{
			delete t;
		}
	}
}



static const char* kCertificateList[] = {kDS1AttrUserCertificate, kDS1AttrUserSMIMECertificate, 
										 "dsAttrTypeNative:userSMIMECertificate", NULL};



int DSX509Record::GetTuple (DSX509Tuple *tupleList[], int maxTuples)
{
	DSX509Tuple** tupleFinger = tupleList;
	int tupleCount = 0;

	CSSM_CL_HANDLE clHandle = mRelation->GetCLHandle ();
	
	// go through the record and see if we have a certificate
	DSAttributeEntry attributeEntry (mInternalRecord->GetDirectoryService ());

	uint32 i;
	unsigned long numAttributes = GetAttributeCount ();
	
	const char** finger = kCertificateList;
	const char* attr;
	
	while ((attr = *finger++) != NULL)
	{
		for (i = 1; i <= numAttributes; ++i)
		{
			GetAttributeEntry (i, attributeEntry);
			const char* ae = attributeEntry.GetSignature ().c_str ();
			
			// look for a matching prefix
			if (strcmp (attr, ae) == 0)
			{
				break;
			}
		}
		
		if (i > numAttributes)
		{
			continue;
		}
		
		// get the value
		DSAttributeValueEntry attributeValueEntry (mInternalRecord->GetDirectoryService ());
		attributeEntry.GetAttributeValue (1, attributeValueEntry);
		
		// we now need to parse the cert
		CSSM_DATA cert;
		cert.Data = (uint8*) attributeValueEntry.GetData ();
		cert.Length = attributeValueEntry.GetLength ();
		
		CSSM_FIELD *fields;
		uint32 numberOfFields;
		
		CSSM_RETURN result = CSSM_CL_CertGetAllFields (clHandle, &cert, &numberOfFields, &fields);
		CheckResult (result);
		
		CSSM_DATA data;
		
		// get the version
		data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1Version);

		// make a tuple
		DSX509Tuple* t = new DSX509Tuple (kNumberOfX509Attributes);
		
		// set the data types
		t->SetValue (kCertTypeID, new UInt32Value (*(uint32*) data.Data));
		t->SetValue (kCertEncodingID, new UInt32Value (CSSM_CERT_ENCODING_DER));
		
		// we need the print name, so start with the subject name
		data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1SubjectNameCStruct);
		
		// from there, get the attribute
		CSSM_X509_NAME* namePtr = (CSSM_X509_NAME*) data.Data;
		CSSM_DATA *dp;
		dp = GetAttributeFromX509Name (namePtr, CSSMOID_CommonName);
		t->SetValue (kCertPrintName, dp == NULL ? NULL : new BlobValue (dp->Data, dp->Length));
		
		// also get the email address to act as the alias
		dp = GetAttributeFromX509Name (namePtr, CSSMOID_EmailAddress);
		t->SetValue (kCertAlias, dp == NULL ? NULL : new BlobValue (dp->Data, dp->Length));

		// get the subject
		data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1SubjectName);
		t->SetValue (kCertSubject, new BlobValue (data.Data, data.Length));
		
		// get the issuer
		data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1IssuerName);
		t->SetValue (kCertIssuer, new BlobValue (data.Data, data.Length));
		
		// get the serial number
		data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1SerialNumber);
		t->SetValue (kCertSerialNumber, new BlobValue (data.Data, data.Length));
		
		// handle the subject key identifier
		t->SetValue (kCertSubjectKeyIdentifier, NULL);
		
		// make hash of the public key.
		data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1SubjectPublicKeyCStruct);
		CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *publicKeyInfo = (CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *) data.Data;
		
		CC_SHA1_CTX context;
		CC_SHA1_Init (&context);
		CC_SHA1_Update (&context, publicKeyInfo->subjectPublicKey.Data, publicKeyInfo->subjectPublicKey.Length);
		
		uint8 sha1Digest [20];
		CC_SHA1_Final (sha1Digest, &context);
		
		t->SetValue (kCertPublicKeyHash, new BlobValue (sha1Digest, 20));
		
		// release the cert data
		CSSM_CL_FreeFields (clHandle, numberOfFields, &fields);
		
		// get the data that we will ultimately return
		t->SetData (new BlobValue (cert.Data, cert.Length));
		
		// save off the tuple
		*tupleFinger++ = t;
		tupleCount += 1;
		
		// don't overflow the tuple buffer
		if (--maxTuples == 0)
		{
			break;
		}
	}
	
	return tupleCount;
}
