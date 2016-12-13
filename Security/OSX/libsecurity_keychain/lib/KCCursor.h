/*
 * Copyright (c) 2000-2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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

    // Occasionally, you might end up with a keychain where finding a record
    // might return CSSMERR_DL_RECORD_NOT_FOUND. This is usually due to having a
    // existing SSGroup element whose matching SSGroup key has been deleted.
    //
    // You might also have invalid ACLs or records with bad MACs.
    //
    // If you set this to true, this KCCursor will silently suppress errors when
    // creating items, and try to delete these corrupt records.
    void setDeleteInvalidRecords(bool deleteRecord);

private:
	StorageManager::KeychainList mSearchList;
	StorageManager::KeychainList::iterator mCurrent;
	CssmClient::DbCursor mDbCursor;
	bool mAllFailed;
    bool mDeleteInvalidRecords;

    // Remembers if we've called newKeychain() on mCurrent.
    bool mIsNewKeychain;

protected:
	Mutex mMutex;

    // Call this every time we switch to a new keychain
    // Will:
    //  1. handle the read locks on the new keychain and the old one
    //  2. Try to upgrade the new keychain if needed and possible
    // Handles the end iterator.
    void newKeychain(StorageManager::KeychainList::iterator kcIter);

    // Try to delete a record. Silently swallow any RECORD_NOT_FOUND exceptions,
    // but throw others upward.
    void deleteInvalidRecord(DbUniqueRecord& uniqueId);
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
