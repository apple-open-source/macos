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

#ifndef __DotMacRELATION__
#define __DotMacRELATION__

#include "PartialRelation.h"
#include <list>

/*
	These classes define the relationship between CDSA and DotMac
*/

// relation column numbers
enum {kCertTypeID = 0, kCertEncodingID, kCertPrintName, kCertAlias, kCertSubject, kCertIssuer, kCertSerialNumber,
	  kCertSubjectKeyIdentifier, kCertPublicKeyHash};

const int kNumberOfX509Attributes = kCertPublicKeyHash - kCertTypeID + 1;

// the "tuple" we return
class DotMacTuple : public Tuple
{
protected:
	int mNumberOfValues;							// number of attributes
	Value** mValues;								// the attributes themselves
	BlobValue *mData;								// the data for this tuple

public:
	DotMacTuple (int numberOfValues);
	virtual ~DotMacTuple ();

	void SetValue (int i, Value* v);				// set an attribute by column number
	
	Value* GetValue (int i);						// get an attribute
	
	int GetNumberOfValues ();						// number of attributes
	
	void GetData (CSSM_DATA &data);					// get the data
	void SetData (BlobValue *value);				// set the data
};



class DotMacRelation;

// a class representing a unique identifier for a record (in the CDSA sense)
class DotMacUniqueIdentifier : public UniqueIdentifier
{
protected:
	DotMacTuple *mTuple;

public:
	DotMacUniqueIdentifier (DotMacTuple *t);
	virtual ~DotMacUniqueIdentifier ();
	virtual void Export (CSSM_DB_UNIQUE_RECORD &record);
	DotMacTuple* GetTuple ();
};



// a class which converts between a CDSA query and an open directory lookup
class DotMacQuery : public Query
{
protected:
	char* mBuffer;
	size_t mBufferSize;
	char* mBufferPos;
	char* mTarget;
	std::string queryDomainName;
	std::string queryUserName;
	bool validQuery;

	typedef std::list<CSSM_DATA> CertList;
	CertList mCertList;
	CertList::iterator mCertIterator;

	std::string ReadLine ();

	char* ReadStream (CFURLRef url, size_t &responseLength);
	void ReadCertificatesFromURL (CFURLRef url);
	bool ValidateQueryString(CSSM_DATA mailAddr);
public:
	DotMacQuery (DotMacRelation* relation, const CSSM_QUERY *queryBase);
	virtual ~DotMacQuery ();
	
	virtual Tuple* GetNextTuple (UniqueIdentifier *&id);				// get a tuple and return an ID that identifies it
};



class DotMacRelation : public PartialRelation
{
protected:
	CSSM_CL_HANDLE mCertificateLibrary;
	
	void InitializeCertLibrary ();										// load the CL

public:
	DotMacRelation ();
	virtual ~DotMacRelation ();

	Query* MakeQuery (const CSSM_QUERY* query);							// convert a CSSM_QUERY object to an internal form
	Tuple* GetTupleFromUniqueIdentifier (UniqueIdentifier* uniqueID);	// get tuple by unique ID
	UniqueIdentifier* ImportUniqueIdentifier (CSSM_DB_UNIQUE_RECORD *uniqueRecord);	// make a unique ID from an external form
	CSSM_CL_HANDLE GetCLHandle ();										// get the CL handle -- initialize if necessary
};



#endif
