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
#include <security_utilities/debugging.h>
#include "CommonCrypto/CommonDigest.h"

/*
	int mNumberOfValues;
	Value* mValues;
	BlobValue *mData;
*/

DSX509Tuple::DSX509Tuple (int numberOfValues) : mNumberOfValues (numberOfValues), mValues (NULL), mData (NULL)
{
	mValues = new Value*[numberOfValues];
	for (int i = 0; i < numberOfValues; ++i)
		mValues[i] = NULL;
}



DSX509Tuple::~DSX509Tuple ()
{
	// walk the value array and delete each value
	for (int i = 0; i < mNumberOfValues; ++i)
		if (mValues[i] != NULL) delete mValues[i];	
	delete [] mValues;
	
	if (mData != NULL) delete mData;
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



static void * appMalloc (CSSM_SIZE size, void *allocRef) {
	return (malloc (size));
}



static void appFree (void *mem_ptr, void *allocRef) {
	free (mem_ptr);
 	return;
}



static void * appRealloc (void *ptr, CSSM_SIZE size, void *allocRef) {
	return (realloc (ptr, size));
}



static void * appCalloc (uint32 num, CSSM_SIZE size, void *allocRef) {
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
	if (result != 0) throw CSSMError (result);
}



void DSX509Relation::InitializeCertLibrary ()
{
	if (mCertificateLibrary != 0) return;
	
	// figure out which GUID to attach to
	const CSSM_GUID* attachGuid = &gGuidAppleX509CL;
	
	// Initialize CDSA
	CSSM_VERSION version = {2, 0};

	// load the CL
	CSSM_RETURN result = CSSM_ModuleLoad (attachGuid, CSSM_KEY_HIERARCHY_NONE, NULL, NULL);
	CheckResult (result);

	result = CSSM_ModuleAttach (attachGuid, &version, &memFuncs, 0,	 CSSM_SERVICE_CL, 0, CSSM_KEY_HIERARCHY_NONE, NULL, 0, NULL, &mCertificateLibrary);
	CheckResult (result);
}



DSX509Relation::DSX509Relation (CSSM_DB_RECORDTYPE recordType, int numberOfColumns, columnInfoLoader *theColumnInfo) : PartialRelation (CSSM_DL_DB_RECORD_X509_CERTIFICATE, kNumberOfX509Attributes,theColumnInfo), mCertificateLibrary (0)
{
	mDirectoryService = new DirectoryService();
}



DSX509Relation::~DSX509Relation ()
{
	if (mCertificateLibrary != 0)
		CSSM_ModuleDetach (mCertificateLibrary);
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



	
DSX509Query::DSX509Query (DSX509Relation* relation, const CSSM_QUERY *queryBase) :
	Query (relation, queryBase), mRecordCount (0), mCurrentItem (1), 
	mRecordList (NULL), mNumberOfTuples (0), mNextTuple (0)
{
	CSSM_RETURN error;
	// attach to open directory
	mRecordList = relation->mDirectoryService->translate_cssm_query_to_OD_query(queryBase, &error);
	if(!mRecordList) throw CSSMERR_DL_ENDOFDATA;
}



DSX509Query::~DSX509Query ()
{
	// delete mRecordList;	need to release the results handle
}







static bool CompareOIDs (const CSSM_OID &a, const CSSM_OID &b)
{
	if (a.Length != b.Length) return false;
	return memcmp (a.Data, b.Data, a.Length) == 0;
}



static CSSM_DATA GetValueFromFields (CSSM_FIELD *fields, uint32 numFields, const CSSM_OID& oid)
{
	uint32 i;
	for (i = 0; i < numFields; ++i)
		if (CompareOIDs (fields[i].FieldOid, oid)) return fields[i].FieldValue;
	
	throw CSSMERR_CSSM_INVALID_ATTRIBUTE;
}



static CSSM_DATA* GetAttributeFromX509Name (CSSM_X509_NAME *name, const CSSM_OID& oid)
{
	uint32 i;
	for (i = 0; i < name->numberOfRDNs; ++i) {
		CSSM_X509_RDN &rdn = name->RelativeDistinguishedName[i];
		uint32 j;
		for (j = 0; j < rdn.numberOfPairs; ++j) {
			CSSM_X509_TYPE_VALUE_PAIR &pair = rdn.AttributeTypeAndValue[j];
			if (CompareOIDs (pair.type, oid))
				return &pair.value;
		}
	}
	return NULL;
}



Tuple* DSX509Query::GetNextTuple (UniqueIdentifier *&id)
{
	DSX509Tuple* t;
	CFStringRef original_search;
	
	original_search = this->mRecordList->searchString;
	
	// get the current item from the list
	CFDataRef	certData = this->mDirectoryService->getNextCertFromResults(this->mRecordList);
	if(!certData) return NULL;
	DSX509Record record ((DSX509Relation*) mRelation);
	
	mNumberOfTuples = record.GetTuple (certData, original_search, mTupleList, kMaxTuples);
	if (mNumberOfTuples != 0) // can this record be coerced to fit?
		t = mTupleList[mNextTuple++];
	
	if (EvaluateTuple (t)) {
		id = new DSX509UniqueIdentifier (t);
		return t;
	}
	delete t;
	return NULL;
}




int DSX509Record::GetTuple (CFDataRef certData, CFStringRef original_search, DSX509Tuple *tupleList[], int maxTuples)
{
	DSX509Tuple** tupleFinger = tupleList;
	CSSM_CL_HANDLE clHandle = mRelation->GetCLHandle ();
	// we now need to parse the cert
	CSSM_DATA cert;
	cert.Data = (uint8 *) CFDataGetBytePtr(certData);
	cert.Length = CFDataGetLength(certData);		
	
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
	t->SetValue (kCertPrintName, dp == NULL ? new BlobValue (original_search)  : new BlobValue (*dp));
	
	//  set the email address to the original search string
	t->SetValue (kCertAlias, new BlobValue (original_search));

	// get the subject
	data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1SubjectName);
	t->SetValue (kCertSubject, new BlobValue (data));
	
	// get the issuer
	data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1IssuerName);
	t->SetValue (kCertIssuer, new BlobValue (data));
	
	// get the serial number
	data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1SerialNumber);
	t->SetValue (kCertSerialNumber, new BlobValue (data));
	
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
	t->SetData (new BlobValue (cert));
	
	// save off the tuple
	*tupleFinger++ = t;
	--maxTuples;
	return 1;
}
