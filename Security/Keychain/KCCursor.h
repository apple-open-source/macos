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


/*
	File:		KCCursor.h

	Contains:	The keychain class

	Copyright:	2000 by Apple Computer, Inc., all rights reserved.

	To Do:
*/

#ifndef _H_KCCURSOR_
#define _H_KCCURSOR_

#include <Security/StorageManager.h>

namespace Security
{

namespace KeychainCore
{

class KCCursor;

class KCCursorImpl : public ReferencedObject
{
    NOCOPY(KCCursorImpl)
    friend class KCCursor;
protected:
	KCCursorImpl(const CssmClient::DbCursor &dbCursor, SecItemClass itemClass, const SecKeychainAttributeList *attrList);
	KCCursorImpl(const CssmClient::DbCursor &dbCursor, const SecKeychainAttributeList *attrList);

public:
	virtual ~KCCursorImpl();
	bool next(Item &item);

private:
	CssmClient::DbCursor mDbCursor;
};


class KCCursor : public RefPointer<KCCursorImpl>
{
public:
    KCCursor() {}
    
    KCCursor(KCCursorImpl *impl) : RefPointer<KCCursorImpl>(impl) {}

    KCCursor(const CssmClient::DbCursor &dbCursor, const SecKeychainAttributeList *attrList)
	: RefPointer<KCCursorImpl>(new KCCursorImpl(dbCursor, attrList)) {}

    KCCursor(const CssmClient::DbCursor &dbCursor, SecItemClass itemClass, const SecKeychainAttributeList *attrList)
	: RefPointer<KCCursorImpl>(new KCCursorImpl(dbCursor, itemClass, attrList)) {}

	typedef KCCursorImpl Impl;
};


typedef Ref<KCCursor, KCCursorImpl, SecKeychainSearchRef, errSecInvalidSearchRef> KCCursorRef;

} // end namespace KeychainCore

} // end namespace Security

#endif /* _H_KCCURSOR_ */

