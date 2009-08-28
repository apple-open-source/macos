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
#include "DotMacRelation.h"
#include "CommonCrypto/CommonDigest.h"
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/debugging.h>
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>

/*
	int mNumberOfValues;
	Value* mValues;
	BlobValue *mData;
*/

DotMacTuple::DotMacTuple (int numberOfValues) : mNumberOfValues (numberOfValues), mValues (NULL), mData (NULL)
{
	mValues = new Value*[numberOfValues];
	int i;
	for (i = 0; i < numberOfValues; ++i)
	{
		mValues[i] = NULL;
	}
}



DotMacTuple::~DotMacTuple ()
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



void DotMacTuple::SetValue (int i, Value* v)
{
	mValues[i] = v;
}



Value* DotMacTuple::GetValue (int i)
{
	return mValues[i];
}



int DotMacTuple::GetNumberOfValues ()
{
	return mNumberOfValues;
}



void DotMacTuple::GetData (CSSM_DATA &data)
{
	size_t t;
	const uint8* d = mData->GetRawValue (t);
	data.Data = (uint8*) d;
	data.Length = t;
}



void DotMacTuple::SetData (BlobValue *value)
{
	mData = value;
}



DotMacUniqueIdentifier::DotMacUniqueIdentifier (DotMacTuple *t) :
	UniqueIdentifier (CSSM_DL_DB_RECORD_X509_CERTIFICATE), mTuple (t)
{
}



DotMacUniqueIdentifier::~DotMacUniqueIdentifier ()
{
	delete mTuple;
}



void DotMacUniqueIdentifier::Export (CSSM_DB_UNIQUE_RECORD &record)
{
	// memset the whole thing to 0
	memset (&record, 0, sizeof (CSSM_DB_UNIQUE_RECORD));
	record.RecordIdentifier.Data = (uint8*) this;
}



DotMacTuple* DotMacUniqueIdentifier::GetTuple ()
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
	if (result != 0)
	{
		CSSMError::ThrowCSSMError (result);
	}
}



void DotMacRelation::InitializeCertLibrary ()
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



DotMacRelation::DotMacRelation () : PartialRelation (CSSM_DL_DB_RECORD_X509_CERTIFICATE, kNumberOfX509Attributes), mCertificateLibrary (0)
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



DotMacRelation::~DotMacRelation ()
{
	if (mCertificateLibrary != 0)
	{
		CSSM_ModuleDetach (mCertificateLibrary);
	}
}



Query* DotMacRelation::MakeQuery (const CSSM_QUERY* query)
{
	return new DotMacQuery (this, query);
}



Tuple* DotMacRelation::GetTupleFromUniqueIdentifier (UniqueIdentifier* uniqueID)
{
	DotMacUniqueIdentifier *id = (DotMacUniqueIdentifier*) uniqueID;
	return id->GetTuple ();
}



UniqueIdentifier* DotMacRelation::ImportUniqueIdentifier (CSSM_DB_UNIQUE_RECORD *uniqueRecord)
{
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_QUERY);
}



CSSM_CL_HANDLE DotMacRelation::GetCLHandle ()
{
	InitializeCertLibrary ();
	return mCertificateLibrary;
}

/* SPI to specify timeout on CFReadStream */
#define _kCFStreamPropertyReadTimeout   CFSTR("_kCFStreamPropertyReadTimeout")

/* the timeout we set */
#define READ_STREAM_TIMEOUT		15

const int kResponseIncrement = 4096;

char* DotMacQuery::ReadStream (CFURLRef url, size_t &responseLength)
{
	SInt32 ito;
	CFNumberRef cfnTo = NULL;
	CFDictionaryRef proxyDict = NULL;
	
	// make a connection to the provided URL
	CFHTTPMessageRef httpRequestRef = CFHTTPMessageCreateRequest (kCFAllocatorDefault, CFSTR("GET"), url, kCFHTTPVersion1_1);
	if (httpRequestRef == NULL)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_RECORD_NOT_FOUND);
	}
	
	// open the stream
	CFReadStreamRef httpStreamRef = CFReadStreamCreateForHTTPRequest (kCFAllocatorDefault, httpRequestRef);
	if (httpStreamRef == NULL)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_RECORD_NOT_FOUND);
	}

	// set a reasonable timeout
	ito = READ_STREAM_TIMEOUT;
	cfnTo = CFNumberCreate(NULL, kCFNumberSInt32Type, &ito);
    if(!CFReadStreamSetProperty(httpStreamRef, _kCFStreamPropertyReadTimeout, cfnTo)) {
		// oh well - keep going 
	}
	
	/* set up possible proxy info */
	proxyDict = SCDynamicStoreCopyProxies(NULL);
	if(proxyDict) {
		CFReadStreamSetProperty(httpStreamRef, kCFStreamPropertyHTTPProxy, proxyDict);
	}

	if (CFReadStreamOpen (httpStreamRef) == false)
	{
		CFRelease(httpRequestRef);
		CFRelease(httpStreamRef);
		CFRelease(cfnTo);
		if(proxyDict) {
			CFRelease(proxyDict);
		}
		CSSMError::ThrowCSSMError (CSSMERR_DL_RECORD_NOT_FOUND);
	}
	
	char* response = (char*) malloc (kResponseIncrement);
	size_t responseBufferLength = kResponseIncrement;
	responseLength = 0;

	// read data from the stream
	CFIndex bytesRead = CFReadStreamRead (httpStreamRef, (UInt8*) response + responseLength, kResponseIncrement);
	while (bytesRead > 0)
	{
		responseLength += bytesRead;
		responseBufferLength = responseLength + kResponseIncrement;
		response = (char*) realloc (response, responseBufferLength);
		
		bytesRead = CFReadStreamRead (httpStreamRef, (UInt8*) response + responseLength, kResponseIncrement);
	}
	
	CFRelease (httpRequestRef);
	CFRelease (httpStreamRef);
	CFRelease(cfnTo);
	if(proxyDict) {
		CFRelease(proxyDict);
	}

	// check for error
	if (bytesRead < 0)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_RECORD_NOT_FOUND);
	}
	
	return response;
}



std::string DotMacQuery::ReadLine ()
{
	// extract one line from the buffer
	char* lineStart = mBufferPos;
	while (mBufferPos < mTarget && *mBufferPos != '\n')
	{
		mBufferPos += 1;
	}
	
	if (mBufferPos >= mTarget)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_DATABASE_CORRUPT);
	}
	
	// calculate the end of line
	char* end = mBufferPos;
	if (*(end - 1) == '\r')
	{
		end -= 1;
	}
	
	// tweek if we have to
	if (end < lineStart)
	{
		end = lineStart;
	}
	
	// move our cursor to the next line for the next time through the loop
	mBufferPos += 1;
	
	// the line of text is now delimited by (lineStart, end)
	size_t length = end - lineStart;
	return std::string (lineStart, length);
}



static u_int8_t gBase64Array[256] =
{
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62,  0,  0,  0, 63, 
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0, 
	 0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0, 
	 0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static void Base64ToBin (const std::string &s, u_int8_t* data, size_t &length)
{
	int16_t accum = 0;
	int bitsInAccum = 0;
	u_int8_t* finger = data;

	int numEquals = 0;
	
	unsigned i;
	for (i = 0; i < s.length (); ++i)
	{
		int index = s[i];
		int b64;
		
		if (index == '=')
		{
			b64 = 0;
			numEquals += 1;
		}
		else
		{
			b64 = gBase64Array[index];
		}
		
		accum = (accum << 6) | b64;
		bitsInAccum += 6;
		
		if (bitsInAccum >= 8)
		{
			bitsInAccum -= 8;
			*finger++ = (accum >> bitsInAccum) & 0xFF;
		}
	}
	
	// adjust for padding
	finger -= numEquals;
	length = finger - data;
}



void DotMacQuery::ReadCertificatesFromURL (CFURLRef url)
{
	// get our data
	mBuffer = ReadStream (url, mBufferSize);
	mBufferPos = mBuffer;
	mTarget = mBuffer + mBufferSize;
	
	secdebug ("dotmacdl", "Read %lu bytes", (unsigned long)mBufferSize);
	
	std::string userName;
	while (mBufferPos < mTarget)
	{
		userName = ReadLine ();
		if (userName.length () == 0)
		{
			// skip blank lines
			continue;
		}
		else
		{
			break;
		}
	}
	
	if (mBufferPos >= mTarget) // out of data so soon?
	{
		secdebug ("dotmacdl", "unexpected end of data");
		CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	}

	// parse the data
	while (mBufferPos < mTarget)
	{
		// skip everything until PEM header marker
		std::string line = ReadLine ();
		if (mBufferPos >= mTarget)
		{
			// we are done
			goto Exit;
		}
		if (line.length () == 0) 
		{
			continue;
		}
		if (line.find ("BEGIN CERTIFICATE", 0) == std::string::npos)
		{
			continue;
		}
		
		// what follows is the certificate data, make a big string that concatenates it together
		void* certData = NULL;
		size_t certLen = 0;
		
		line = ReadLine ();
		while (mBufferPos < mTarget && line.find ("END CERTIFICATE", 0) == std::string::npos)
		{
			u_int8_t dataBuffer [line.length ()]; // without question big enough to hold the data
			size_t length = 0;
			Base64ToBin (line, dataBuffer, length);
			certData = realloc (certData, certLen + length);
			memmove (((char*) certData) + certLen, dataBuffer, length);
			certLen += length;
			
			line = ReadLine ();
		}
		
		if (mBufferPos >= mTarget)
		{
			secdebug ("dotmacdl", "no END");
			CSSMError::ThrowCSSMError (CSSMERR_DL_DATABASE_CORRUPT);
		}
		
		secdebug ("dotmacdl", "found cert length %lu", (unsigned long)certLen);
		CSSM_DATA cert;
		cert.Data = (uint8*) certData;
		cert.Length = certLen;

		// save off the cert
		mCertList.push_back (cert);
	}
	
Exit:
	mCertIterator = mCertList.begin ();
}



const char* kEmailName = "Alias";
const char* kPrintName = "PrintName";
const char* kMacDotCom = "@mac.com";
const char* kMeDotCom = "@me.com";

bool DotMacQuery::ValidateQueryString(CSSM_DATA mailAddr)
{
	std::string nameAsString ((char*) mailAddr.Data, mailAddr.Length);
	size_t atPos = nameAsString.find_first_of("@");
	if(atPos == string::npos)
		return validQuery = false;
	queryUserName = nameAsString.substr(0, atPos);
	queryDomainName = nameAsString.substr(atPos); 	
	if (!(queryDomainName.compare(kMacDotCom) == 0) && !(queryDomainName.compare(kMeDotCom) == 0)) 
		return validQuery = false;
	return validQuery = true;
}

static bool StringEndsWith (const std::string &s, const std::string &suffix)
{
	size_t atPos = s.rfind(suffix);
	if(atPos == string::npos) return false;
	if(s.substr(atPos).compare(suffix) == 0) return true;
	return false;
}



DotMacQuery::DotMacQuery (DotMacRelation* relation, const CSSM_QUERY *queryBase) : Query (relation, queryBase)
{
	uint32 i;
	
	bool found = false;
	
	CSSM_DATA name = {0, NULL};

	// look for a selection predicate we'e comfortable with
	for (i = 0; i < mNumSelectionPredicates; ++i)
	{
		// the name has to be "Alias" or "PrintName", specified in string format 
		if (mSelectionPredicates[i].GetAttributeNameFormat () != CSSM_DB_ATTRIBUTE_NAME_AS_STRING)
		{
			continue;
		}
		char *attrName = mSelectionPredicates[i].GetAttributeName();
		if(!strcmp(attrName, kPrintName) && !strcmp(attrName, kEmailName))
		{
			continue;
		}
		
		if (found)
		{
			// oops, we can only have one "Alias" or "PrintName" predicate in the query
			CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
		}
		
		// the operator has to be CSSM_DB_EQUAL or CSSM_DB_CONTAINS. We
		// treat these identically.
		switch(mSelectionPredicates[i].GetOperator ()) {
			case CSSM_DB_EQUAL:
			case CSSM_DB_CONTAINS:
				break;
			default:
				CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_QUERY);
		}
		
		// we have at found a predicate of the proper form.
		name = mSelectionPredicates[i].GetValue (0);
		found = true;
	}
	if (!found)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
	}

	// Parse the query string into an e-mail address.  We're looking for something of the form username@me.com or something
	// in particular the resulting domain name needs to match one of the MobileMe server names.

	if(! ValidateQueryString(name) )
		CSSMError::ThrowCSSMError(CSSMERR_DL_ENDOFDATA);

	CFStringRef userName = CFStringCreateWithCString (kCFAllocatorDefault, queryUserName.c_str(), kCFStringEncodingMacRoman);

	// now that we've policed the query, make the query URL
	CFMutableStringRef queryString = CFStringCreateMutable (kCFAllocatorDefault, 0);
	
	// set to the beginning of the query
	CFStringAppendCString (queryString, "http://certinfo.mac.com/locate?", kCFStringEncodingMacRoman);
	
	// append the user name
	CFStringAppend (queryString, userName);
	CFRelease (userName);

	// make the URL object that corresponds to our string
	CFURLRef url = CFURLCreateWithString (kCFAllocatorDefault, queryString, NULL);
	CFRelease (queryString);
	
	secdebug ("dotmacdl", "reading certs for %s", queryUserName.c_str ());
	ReadCertificatesFromURL (url);
	CFRelease (url);
}



DotMacQuery::~DotMacQuery ()
{
	CertList::iterator it = mCertList.begin ();
	while (it != mCertList.end ())
	{
		free (it++->Data);
	}
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
	
	CSSMError::ThrowCSSMError(CSSMERR_CSSM_INVALID_ATTRIBUTE);
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



Tuple* DotMacQuery::GetNextTuple (UniqueIdentifier *&id)
{
	CSSM_CL_HANDLE clHandle = ((DotMacRelation*) mRelation)->GetCLHandle ();

	while (mCertIterator != mCertList.end ()) // no more certs for this query?
	{
		// we now need to parse the cert
		CSSM_DATA cert = *mCertIterator++;
		
		CSSM_FIELD *fields;
		uint32 numberOfFields;
		
		CSSM_RETURN result = CSSM_CL_CertGetAllFields (clHandle, &cert, &numberOfFields, &fields);
		CheckResult (result);
		
		// get the version
		CSSM_DATA data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1Version);

		// make a tuple
		DotMacTuple* t = new DotMacTuple (kNumberOfX509Attributes);
		
		// set the data types
		uint32 certType = data.Data[0];
		t->SetValue (kCertTypeID, new UInt32Value (certType));
		t->SetValue (kCertEncodingID, new UInt32Value (CSSM_CERT_ENCODING_DER));
		
		// we need the print name, so start with the subject name
		data = GetValueFromFields (fields, numberOfFields, CSSMOID_X509V1SubjectNameCStruct);
		
		// from there, get the attribute
		CSSM_X509_NAME* namePtr = (CSSM_X509_NAME*) data.Data;
		CSSM_DATA *dp;
		dp = GetAttributeFromX509Name (namePtr, CSSMOID_CommonName);
		std::string commonName = std::string ((char*) dp->Data, dp->Length);
		secdebug ("dotmacdl", "Common name=%s", commonName.c_str ());

		// does the common name end in queryDomainName?  If not, add it
		if (!StringEndsWith (commonName, queryDomainName))
		{
			commonName += queryDomainName;
		}
	
		t->SetValue (kCertPrintName, dp == NULL ? NULL : new BlobValue (dp->Data, dp->Length));
		
		// also get the email address to act as the alias
		dp = GetAttributeFromX509Name (namePtr, CSSMOID_EmailAddress);
		if (dp == NULL)
		{
			secdebug ("dotmacdl", "CSSMOID_EmailAddress is NULL; using commonName");
			t->SetValue (kCertAlias, new BlobValue ((UInt8*) commonName.c_str (), commonName.length ()));
		}
		else
		{
			std::string s = std::string ((char*) dp->Data, dp->Length);
			if (!StringEndsWith (s, queryDomainName))
			{
				s += queryDomainName;
			}
			
			t->SetValue (kCertAlias, dp == NULL ? NULL : new BlobValue (dp->Data, dp->Length));
		}
		

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
		
		// get the data that we will ultimately return
		t->SetData (new BlobValue (cert.Data, cert.Length));
		
		// release the cert data
		CSSM_CL_FreeFields (clHandle, numberOfFields, &fields);

		if (EvaluateTuple (t))
		{
			id = new DotMacUniqueIdentifier (t);
			return t;
		}
		else
		{
			delete t;
		}
	}
	
	// out of data
	CSSMError::ThrowCSSMError (CSSMERR_DL_ENDOFDATA);
}
