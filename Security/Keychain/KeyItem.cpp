/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// KeyItem.cpp
//
#include <Security/KeyItem.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

using namespace KeychainCore;

KeyItem::KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId) :
	ItemImpl(keychain, primaryKey, uniqueId),
	mKey(NULL)
{
}

KeyItem::KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey)  :
	ItemImpl(keychain, primaryKey),
	mKey(NULL)
{
}

KeyItem::KeyItem(KeyItem &keyItem) :
	ItemImpl(keyItem),
	mKey(NULL)
{
}

KeyItem::~KeyItem()
{
	if (mKey)
	{
		CssmClient::SSDbUniqueRecord uniqueId(ssDbUniqueRecord());
		uniqueId->database()->csp()->freeKey(*mKey);
		uniqueId->allocator().free(mKey);
	}
}

void
KeyItem::update()
{
	MacOSError::throwMe(unimpErr);
}

Item
KeyItem::copyTo(const Keychain &keychain)
{
	MacOSError::throwMe(unimpErr);
}

void
KeyItem::didModify()
{
}

PrimaryKey
KeyItem::add(Keychain &keychain)
{
	MacOSError::throwMe(unimpErr);
}

CssmClient::SSDbUniqueRecord
KeyItem::ssDbUniqueRecord()
{
	DbUniqueRecordImpl *impl = &*dbUniqueRecord();
	return CssmClient::SSDbUniqueRecord(safe_cast<Security::CssmClient::SSDbUniqueRecordImpl *>(impl));
}

const CssmKey &
KeyItem::cssmKey()
{
	if (!mKey)
	{
		CssmClient::SSDbUniqueRecord uniqueId(ssDbUniqueRecord());
		CssmDataContainer dataBlob(uniqueId->allocator());
		uniqueId->get(NULL, &dataBlob);
		mKey = reinterpret_cast<CssmKey *>(dataBlob.Data);
		dataBlob.Data = NULL;
		dataBlob.Length = 0;
	}

	return *mKey;
}
