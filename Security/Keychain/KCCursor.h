/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
// KCCursor.h
//
#ifndef _SECURITY_KCCURSOR_H_
#define _SECURITY_KCCURSOR_H_

#include <Security/StorageManager.h>

namespace Security
{

namespace KeychainCore
{

class KCCursorImpl : public SecCFObject, public CssmAutoQuery
{
    NOCOPY(KCCursorImpl)
public:
	SECCFFUNCTIONS(KCCursorImpl, SecKeychainSearchRef, errSecInvalidSearchRef)

    friend class KCCursor;
protected:
	KCCursorImpl(const StorageManager::KeychainList &searchList, SecItemClass itemClass, const SecKeychainAttributeList *attrList);
	KCCursorImpl(const StorageManager::KeychainList &searchList, const SecKeychainAttributeList *attrList);

public:
	virtual ~KCCursorImpl() throw();
	bool next(Item &item);

private:
	StorageManager::KeychainList mSearchList;
	StorageManager::KeychainList::iterator mCurrent;
	CssmClient::DbCursor mDbCursor;
	bool mAllFailed;
};


class KCCursor : public SecPointer<KCCursorImpl>
{
public:
    KCCursor() {}
    
    KCCursor(KCCursorImpl *impl) : SecPointer<KCCursorImpl>(impl) {}

    KCCursor(const StorageManager::KeychainList &searchList, const SecKeychainAttributeList *attrList)
	: SecPointer<KCCursorImpl>(new KCCursorImpl(searchList, attrList)) {}

    KCCursor(const StorageManager::KeychainList &searchList, SecItemClass itemClass, const SecKeychainAttributeList *attrList)
	: SecPointer<KCCursorImpl>(new KCCursorImpl(searchList, itemClass, attrList)) {}

	typedef KCCursorImpl Impl;
};


} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_KCCURSOR_H_
