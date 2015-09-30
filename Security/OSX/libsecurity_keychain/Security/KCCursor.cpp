/*
 * Copyright (c) 2000-2004,2011-2014 Apple Inc. All Rights Reserved.
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


//
// KCCursor.cpp
//

#include "KCCursor.h"

#include "Item.h"
#include <security_cdsa_utilities/Schema.h>
#include <security_cdsa_utilities/KeySchema.h>
#include "cssmdatetime.h"
#include "Globals.h"
#include "StorageManager.h"
#include <Security/SecKeychainItemPriv.h>
#include <SecBase.h>

using namespace KeychainCore;
using namespace CssmClient;
using namespace CSSMDateTimeUtils;

using namespace KeySchema;

// define a table of our attributes for easy lookup
static const CSSM_DB_ATTRIBUTE_INFO* gKeyAttributeLookupTable[] =
{
	&KeyClass, &PrintName, &Alias, &Permanent, &Private, &Modifiable, &Label, &ApplicationTag, &KeyCreator,
	&KeyType, &KeySizeInBits, &EffectiveKeySize, &StartDate, &EndDate, &Sensitive, &AlwaysSensitive, &Extractable,
	&NeverExtractable, &Encrypt, &Decrypt, &Derive, &Sign, &Verify, &SignRecover, &VerifyRecover, &Wrap, &Unwrap
};

//
// KCCursorImpl
//
KCCursorImpl::KCCursorImpl(const StorageManager::KeychainList &searchList, SecItemClass itemClass, const SecKeychainAttributeList *attrList, CSSM_DB_CONJUNCTIVE dbConjunctive, CSSM_DB_OPERATOR dbOperator) :
	mSearchList(searchList),
	mCurrent(mSearchList.begin()),
	mAllFailed(true),
	mMutex(Mutex::recursive)
{
    recordType(Schema::recordTypeFor(itemClass));

	if (!attrList) // No additional selectionPredicates: we are done
		return;

	conjunctive(dbConjunctive);
	const SecKeychainAttribute *end=&attrList->attr[attrList->count];
	// Add all the attrs in attrs list to the cursor.
	for (const SecKeychainAttribute *attr=attrList->attr; attr != end; ++attr)
	{
		const CSSM_DB_ATTRIBUTE_INFO *temp;
		
		if (attr->tag <'    ') // ok, is this a key schema?  Handle differently, just because we can...
		{
			temp = gKeyAttributeLookupTable[attr->tag];
		}
		else
		{
			temp = &Schema::attributeInfo(attr->tag);
		}
        const CssmDbAttributeInfo &info = *temp;
        void *buf = attr->data;
        UInt32 length = attr->length;
        uint8 timeString[16];
    
        // XXX This code is duplicated in NewItemImpl::setAttribute()
        // Convert a 4 or 8 byte TIME_DATE to a CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE
        // style attribute value.
        if (info.format() == CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE)
        {
            if (length == sizeof(UInt32))
            {
                MacSecondsToTimeString(*reinterpret_cast<const UInt32 *>(buf),
                                        16, &timeString);
                buf = &timeString;
                length = 16;
            }
            else if (length == sizeof(SInt64))
            {
                MacLongDateTimeToTimeString(*reinterpret_cast<const SInt64 *>(buf),
                                            16, &timeString);
                buf = &timeString;
                length = 16;
            }
        }
        add(dbOperator ,info, CssmData(buf,length));
	}
}

KCCursorImpl::KCCursorImpl(const StorageManager::KeychainList &searchList, const SecKeychainAttributeList *attrList) :
	mSearchList(searchList),
	mCurrent(mSearchList.begin()),
	mAllFailed(true),
	mMutex(Mutex::recursive)
{
	if (!attrList) // No additional selectionPredicates: we are done
		return;

	conjunctive(CSSM_DB_AND);
	bool foundClassAttribute=false;
	const SecKeychainAttribute *end=&attrList->attr[attrList->count];
	// Add all the attrs in attrs list to the cursor.
	for (const SecKeychainAttribute *attr=attrList->attr; attr != end; ++attr)
	{
		if (attr->tag!=kSecClassItemAttr)	// a regular attribute
		{
            const CssmDbAttributeInfo &info = Schema::attributeInfo(attr->tag);
            void *buf = attr->data;
            UInt32 length = attr->length;
            uint8 timeString[16];
        
            // XXX This code is duplicated in NewItemImpl::setAttribute()
            // Convert a 4 or 8 byte TIME_DATE to a CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE
            // style attribute value.
            if (info.format() == CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE)
            {
                if (length == sizeof(UInt32))
                {
                    MacSecondsToTimeString(*reinterpret_cast<const UInt32 *>(buf),
                                           16, &timeString);
                    buf = &timeString;
                    length = 16;
                }
                else if (length == sizeof(SInt64))
                {
                    MacLongDateTimeToTimeString(*reinterpret_cast<const SInt64 *>(buf),
                                                16, &timeString);
                    buf = &timeString;
                    length = 16;
                }
            }
			add(CSSM_DB_EQUAL,info, CssmData(buf,length));

			continue;
		}
		
		// the class attribute
		if (foundClassAttribute || attr->length != sizeof(SecItemClass))
			MacOSError::throwMe(errSecParam); // We have 2 different 'clas' attributes

		recordType(Schema::recordTypeFor(*reinterpret_cast<SecItemClass *>(attr->data)));
		foundClassAttribute=true;
	}
}

KCCursorImpl::~KCCursorImpl() throw()
{
}

//static ModuleNexus<Mutex> gActivationMutex;

bool
KCCursorImpl::next(Item &item)
{
	StLock<Mutex>_(mMutex);
	DbAttributes dbAttributes;
	DbUniqueRecord uniqueId;
	OSStatus status = 0;

	for (;;)
	{
		while (!mDbCursor)
		{
			if (mCurrent == mSearchList.end())
			{
				// If we got always failed when calling mDbCursor->next return the error from
				// the last call to mDbCursor->next now
				if (mAllFailed && status)
					CssmError::throwMe(status);
				
				// No more keychains to search so we are done.
				return false;
			}
			
			try
			{
                // StLock<Mutex> _(gActivationMutex()); // force serialization of cursor creation
                Keychain &kc = *mCurrent;
                Mutex* mutex = kc->getKeychainMutex();
                StLock<Mutex> _(*mutex);
				(*mCurrent)->database()->activate();
				mDbCursor = DbCursor((*mCurrent)->database(), *this);
			}
			catch(const CommonError &err)
			{
				++mCurrent;
			}
		}

        Keychain &kc = *mCurrent;
        Mutex* mutex = kc->getKeychainMutex();
        StLock<Mutex> _(*mutex);
        
		bool gotRecord;
		try
		{
			// Clear out existing attributes first!
			// (the previous iteration may have left attributes from a different schema)
			dbAttributes.clear();

			gotRecord = mDbCursor->next(&dbAttributes, NULL, uniqueId);
			mAllFailed = false;
		}
		catch(const CommonError &err)
		{
			// Catch the last error we get and move on to the next keychain
			// This error will be returned when we reach the end of our keychain list
			// iff all calls to KCCursorImpl::next failed
			status = err.osStatus();
			gotRecord = false;
            dbAttributes.invalidate();
		}
		catch(...)
		{
			// Catch all other errors
			status = errSecItemNotFound;
			gotRecord = false;
		}

		// If we did not get a record from the current keychain or the current
		// keychain did not exist skip to the next keychain in the list.
		if (!gotRecord)
		{
			++mCurrent;
			mDbCursor = DbCursor();
			continue;
		}

        // If doing a search for all records, skip the db blob added by the CSPDL
        if (dbAttributes.recordType() == CSSM_DL_DB_RECORD_METADATA &&
            mDbCursor->recordType() == CSSM_DL_DB_RECORD_ANY)
                continue;
        
        // Filter out group keys at this layer
        if (dbAttributes.recordType() == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
        {
			bool groupKey = false;
			try
			{
				// fetch the key label attribute, if it exists
				dbAttributes.add(KeySchema::Label);
				Db db((*mCurrent)->database());
				CSSM_RETURN getattr_result = CSSM_DL_DataGetFromUniqueRecordId(db->handle(), uniqueId, &dbAttributes, NULL);
				if (getattr_result == CSSM_OK)
				{
					CssmDbAttributeData *label = dbAttributes.find(KeySchema::Label);
					CssmData attrData;
					if (label)
						attrData = *label;
					if (attrData.length() > 4 && !memcmp(attrData.data(), "ssgp", 4))
						groupKey = true;
				}
                else
                {
                    dbAttributes.invalidate();
                }
			}
			catch (...) {}

			if (groupKey)
				continue;
        }

		break;
	}

	// Go though Keychain since item might already exist.
    Keychain &kc = *mCurrent;
    StLock<Mutex> _mutexLocker(*kc->getKeychainMutex());
	item = (*mCurrent)->item(dbAttributes.recordType(), uniqueId);
	return true;
}



bool KCCursorImpl::mayDelete()
{
    if (mDbCursor.get() != NULL)
    {
        return mDbCursor->isIdle();
    }
    else
    {
        return true;
    }
}
