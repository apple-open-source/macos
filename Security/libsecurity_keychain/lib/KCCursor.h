/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// KCCursor.h
//
#ifndef _SECURITY_KCCURSOR_H_
#define _SECURITY_KCCURSOR_H_

#include <security_keychain/StorageManager.h>

namespace Security
{

namespace KeychainCore
{

class KCCursorImpl : public SecCFObject, public CssmAutoQuery
{
    NOCOPY(KCCursorImpl)
public:
	SECCFFUNCTIONS(KCCursorImpl, SecKeychainSearchRef, errSecInvalidSearchRef, gTypes().KCCursorImpl)

    friend class KCCursor;
protected:
	KCCursorImpl(const StorageManager::KeychainList &searchList, SecItemClass itemClass, const SecKeychainAttributeList *attrList, CSSM_DB_CONJUNCTIVE dbConjunctive, CSSM_DB_OPERATOR dbOperator);
	KCCursorImpl(const StorageManager::KeychainList &searchList, const SecKeychainAttributeList *attrList);

public:
	virtual ~KCCursorImpl() throw();
	bool next(Item &item);
    bool mayDelete();
    
private:
	StorageManager::KeychainList mSearchList;
	StorageManager::KeychainList::iterator mCurrent;
	CssmClient::DbCursor mDbCursor;
	bool mAllFailed;

protected:
	Mutex mMutex;
};


class KCCursor : public SecPointer<KCCursorImpl>
{
public:
    KCCursor() {}
    
    KCCursor(KCCursorImpl *impl) : SecPointer<KCCursorImpl>(impl) {}

    KCCursor(const StorageManager::KeychainList &searchList, const SecKeychainAttributeList *attrList)
	: SecPointer<KCCursorImpl>(new KCCursorImpl(searchList, attrList)) {}

    KCCursor(const StorageManager::KeychainList &searchList, SecItemClass itemClass, const SecKeychainAttributeList *attrList, CSSM_DB_CONJUNCTIVE dbConjunctive=CSSM_DB_AND, CSSM_DB_OPERATOR dbOperator=CSSM_DB_EQUAL)
	: SecPointer<KCCursorImpl>(new KCCursorImpl(searchList, itemClass, attrList, dbConjunctive, dbOperator)) {}

	typedef KCCursorImpl Impl;
};


} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_KCCURSOR_H_
