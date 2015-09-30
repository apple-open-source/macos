/*
 * Copyright (c) 2003-2004,2006,2011-2014 Apple Inc. All Rights Reserved.
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


#ifndef _DICTIONARY_H__
#define _DICTIONARY_H__


#include <vector>
#include <security_cdsa_utilities/cssmdb.h>

namespace Security {



#define PID_KEY				'pidk'
#define ITEM_KEY			'item'
#define SSUID_KEY			'ssui'
#define IDENTITY_KEY		'idnt'
#define DB_NAME				'dbnm'
#define DB_LOCATION			'dblc'



class NameValuePair
{
protected:
	uint32 mName;
	CssmData mValue;

	CssmData CloneData (const CssmData &value);

public:
	NameValuePair (uint32 name, const CssmData &value);
	NameValuePair (const CssmData &data);
	~NameValuePair ();

	const uint32 Name () {return mName;}
	const CssmData& Value () const {return mValue;}
	void Export (CssmData &data) const;
};



typedef std::vector<NameValuePair*> NameValuePairVector;



class NameValueDictionary
{
protected:
	NameValuePairVector mVector;

	int FindPositionByName (uint32 name) const;

	void MakeFromData(const CssmData &data);

public:
	NameValueDictionary ();
	~NameValueDictionary ();
	NameValueDictionary (const CssmData &data);

	void Insert (NameValuePair* pair);
	void RemoveByName (uint32 name);
	const NameValuePair* FindByName (uint32 name) const;

	int CountElements () const;
	const NameValuePair* GetElement (int which);
	void Export (CssmData &data);

	// utility functions
	static void MakeNameValueDictionaryFromDLDbIdentifier (const DLDbIdentifier &identifier, NameValueDictionary &nvd);
	static DLDbIdentifier MakeDLDbIdentifierFromNameValueDictionary (const NameValueDictionary &nvd);
};


};

#endif
