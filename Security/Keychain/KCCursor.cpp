/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// KCCursor.cpp
//

#include "KCCursor.h"

#include "Item.h"
#include "Schema.h"
#include "cssmdatetime.h"
#include "Globals.h"
#include "StorageManager.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

using namespace KeychainCore;
using namespace CssmClient;
using namespace CSSMDateTimeUtils;

//
// KCCursorImpl
//
KCCursorImpl::KCCursorImpl(const DbCursor &dbCursor, SecItemClass itemClass, const SecKeychainAttributeList *attrList)
: mDbCursor(dbCursor)
{
	if (!attrList) // No additional selectionPredicates: we are done
		return;

		
    mDbCursor->recordType(Schema::recordTypeFor(itemClass));

	mDbCursor->conjunctive(CSSM_DB_AND);
	const SecKeychainAttribute *end=&attrList->attr[attrList->count];
	// Add all the attrs in attrs list to the cursor.
	for (const SecKeychainAttribute *attr=attrList->attr; attr != end; ++attr)
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
        mDbCursor->add(CSSM_DB_EQUAL,info, CssmData(buf,length));
	}
}

KCCursorImpl::KCCursorImpl(const DbCursor &dbCursor, const SecKeychainAttributeList *attrList)
: mDbCursor(dbCursor)
{
	if (!attrList) // No additional selectionPredicates: we are done
		return;

	mDbCursor->conjunctive(CSSM_DB_AND);
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
			mDbCursor->add(CSSM_DB_EQUAL,info, CssmData(buf,length));

			continue;
		}
		
		// the class attribute
		if (foundClassAttribute || attr->length != sizeof(SecItemClass))
			MacOSError::throwMe(paramErr); // We have 2 different 'clas' attributes

		mDbCursor->recordType(Schema
            ::recordTypeFor(*reinterpret_cast<SecItemClass *>(attr->data)));
		foundClassAttribute=true;
	}
}

KCCursorImpl::~KCCursorImpl()
{
}

bool
KCCursorImpl::next(Item &item)
{
	DbAttributes dbAttributes;
	DbUniqueRecord uniqueId;
	if (!mDbCursor)
		MacOSError::throwMe(errSecInvalidSearchRef);

	for (;;)
	{
		if (!mDbCursor->next(&dbAttributes, NULL, uniqueId))
		{
			// Forget my resources.
			mDbCursor = DbCursor();
			return false;
		}

		// Skip records that we don't have a matching itemClass for,
		// since we can't do anything with them.
		if (Schema::itemClassFor(dbAttributes.recordType()))
			break;
	}

	Keychain keychain = globals().storageManager.keychain(uniqueId->database()->dlDbIdentifier());
	// Go though Keychain since item might already exist.
	item = keychain->item(dbAttributes.recordType(), uniqueId);
	return true;
}
