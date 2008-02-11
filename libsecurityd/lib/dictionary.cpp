/*
 * Copyright (c) 2003-2006 Apple Computer, Inc. All Rights Reserved.
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


#include "dictionary.h"
#include <ctype.h>

namespace Security {

CssmData NameValuePair::CloneData (const CssmData &value)
{
	void* clonedData = (void*) new unsigned char [value.length ()];
	if (clonedData != NULL)
	{
		memcpy (clonedData, value.data (), value.length ());
		return CssmData (clonedData, value.length ());
	}
	else
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
}



NameValuePair::NameValuePair (uint32 name, const CssmData &value) : mName (name), mValue (CloneData (value))
{
}



NameValuePair::NameValuePair (const CssmData &data)
{
	// the first four bytes are the name
	unsigned char* finger = (unsigned char*) data.data ();
	mName = 0;
	
	unsigned int i;
	for (i = 0; i < sizeof (uint32); ++i)
	{
		mName = (mName << 8) | *finger++;
	}
	
	// the next four bytes are the length
	uint32 length = 0;
	for (i = 0; i < sizeof (uint32); ++i)
	{
		length = (length << 8) | *finger++;
	}
	
	// what's left is the data
	mValue = CloneData (CssmData (finger, length));
}



NameValuePair::~NameValuePair ()
{
	delete (unsigned char*) mValue.data ();
}



void NameValuePair::Export (CssmData &data) const
{
	// export the data in the format name length data
	uint32 outSize = 2 * sizeof (uint32) + mValue.length ();
	unsigned char* d = new unsigned char [outSize];
	unsigned char* finger = d;
	
	// export the name
	uint32 intBuffer = mName;

	int i;
	for (i = sizeof (uint32) - 1; i >= 0; --i)
	{
		finger[i] = intBuffer & 0xFF;
		intBuffer >>= 8;
	}
	
	// export the length
	finger += sizeof (uint32);
	intBuffer = mValue.length ();
	for (i = sizeof (uint32) - 1; i >= 0; --i)
	{
		finger[i] = intBuffer & 0xFF;
		intBuffer >>= 8;
	}

	// export the data
	finger += sizeof (uint32);
	memcpy (finger, mValue.data (), mValue.length ());
	
	data = CssmData (d, outSize);
}



NameValueDictionary::NameValueDictionary ()
{
}



NameValueDictionary::~NameValueDictionary ()
{
	// to prevent leaks, delete all members of the vector
	int i = mVector.size ();
	while (i > 0)
	{
		delete mVector[--i];
		
		mVector.erase (mVector.begin () + i);
	}
}



NameValueDictionary::NameValueDictionary (const CssmData &data)
{
	// reconstruct a name value dictionary from a series of exported NameValuePair blobs
	unsigned char* finger = (unsigned char*) data.data ();
	unsigned char* target = finger + data.length ();

	do
	{
		// compute the length of data blob
		unsigned int i;
		uint32 length = 0;
		for (i = sizeof (uint32); i < 2 * sizeof (uint32); ++i)
		{
			length = (length << 8) | finger[i];
		}
		
		if (length > data.length())
		{
			break;
		}
		
		// add the length of the "header"
		length += 2 * sizeof (uint32);
		Insert (new NameValuePair (CssmData (finger, length)));
		
		// skip to the next data
		finger += length;
	} while (finger < target);
}
	


void NameValueDictionary::Insert (NameValuePair* pair)
{
	mVector.push_back (pair);
}



void NameValueDictionary::RemoveByName (uint32 name)
{
	int which = FindPositionByName (name);
	if (which != -1)
	{
		NameValuePair* nvp = mVector[which];
		mVector.erase (mVector.begin () + which);
		delete nvp;
	}
}



int NameValueDictionary::FindPositionByName (uint32 name) const
{
	int target = CountElements ();
	int i;
	
	for (i = 0; i < target; ++i)
	{
		if (mVector[i]->Name () == name)
		{
			return i;
		}
	}
	
	return -1;
}



const NameValuePair* NameValueDictionary::FindByName (uint32 name) const
{
	int which = FindPositionByName (name);
	return which == -1 ? NULL : mVector[which];
}




int NameValueDictionary::CountElements () const
{
	return mVector.size ();
}



const NameValuePair* NameValueDictionary::GetElement (int which)
{
	return mVector[which];
}



void NameValueDictionary::Export (CssmData &outData)
{
	// get each element in the dictionary, and add it to the data blob
	int i;
	uint32 length = 0;
	unsigned char* data = 0;

	for (i = 0; i < CountElements (); ++i)
	{
		CssmData exportedData;
		const NameValuePair *nvp = GetElement (i);
		nvp->Export (exportedData);
		
		uint32 oldLength = length;
		length += exportedData.length ();
		data = (unsigned char*) realloc (data, length);
		
		memcpy (data + oldLength, exportedData.data (), exportedData.length ());
		
		delete (unsigned char*) exportedData.data ();
	}
	
	outData = CssmData (data, length);
}



void NameValueDictionary::MakeNameValueDictionaryFromDLDbIdentifier (const DLDbIdentifier &identifier, NameValueDictionary &nvd)
{
	// get the subserviceID
	DLDbIdentifier d = identifier;
	
	const CssmSubserviceUid &ssuid = identifier.ssuid ();
	CSSM_SUBSERVICE_UID baseID = ssuid;
	baseID.Version.Major = h2n (baseID.Version.Major);
	baseID.Version.Minor = h2n (baseID.Version.Minor);
	baseID.SubserviceId = h2n (baseID.SubserviceId);
	baseID.SubserviceType = h2n (baseID.SubserviceType);
	
	nvd.Insert (new NameValuePair (SSUID_KEY, CssmData::wrap(baseID)));
	
	// get the name
	const char* dbName = identifier.dbName ();
	if (dbName != NULL)
	{
		nvd.Insert (new NameValuePair (DB_NAME, CssmData::wrap (dbName, strlen (dbName) + 1)));
	}
	
	// get the net address
	const CSSM_NET_ADDRESS* add = identifier.dbLocation ();
	if (add != NULL)
	{
		nvd.Insert (new NameValuePair (DB_LOCATION, CssmData::wrap (add)));
	}
}



DLDbIdentifier NameValueDictionary::MakeDLDbIdentifierFromNameValueDictionary (const NameValueDictionary &nvd)
{
	CSSM_SUBSERVICE_UID* uid = (CSSM_SUBSERVICE_UID*) nvd.FindByName (SSUID_KEY)->Value ().data ();
	if (uid == NULL)
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
	
	CSSM_SUBSERVICE_UID baseID = *uid;
	
	baseID.Version.Major = n2h (baseID.Version.Major);
	baseID.Version.Minor = n2h (baseID.Version.Minor);
	baseID.SubserviceId = n2h (baseID.SubserviceId);
	baseID.SubserviceType = n2h (baseID.SubserviceType);
	
	char* name = (char*) nvd.FindByName (DB_NAME)->Value ().data ();
	
	const NameValuePair* nvp = nvd.FindByName (DB_LOCATION);
	CSSM_NET_ADDRESS* address = nvp ? (CSSM_NET_ADDRESS*) nvp->Value ().data () : NULL;
	
	return DLDbIdentifier (baseID, name, address);
}

}; // end Security namespace
