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

//
// defaultcreds - default computations for keychain open credentials
//
#include "Keychains.h"
#include "defaultcreds.h"
#include "StorageManager.h"
#include "Globals.h"
#include "KCCursor.h"
#include <security_cdsa_utilities/Schema.h>
#include <algorithm>


namespace Security {
namespace KeychainCore {

using namespace CssmClient;


DefaultCredentials::DefaultCredentials(KeychainImpl* kcImpl, Allocator &alloc)
	: TrackingAllocator(alloc), AutoCredentials(static_cast<TrackingAllocator&>(*this)),
	  mMade(false), mKeychainImpl (kcImpl)
{
}


void DefaultCredentials::clear()
{
	TrackingAllocator::reset();
	mNeededItems.clear();
	mMade = false;
}


//
// The main driver.
// This scans a database for referral records and forms corresponding
// credentials to trigger unlocks.
// Returns true if any valid unlock credentials were found; false otherwise.
// Only throws if the database is messed up.
//
bool DefaultCredentials::operator () (Db database)
{
	if (!mMade) {
		try {
			// before we do anything else, see if we have a relation in the database of the appropriate type
			KeychainSchema keychainSchema = mKeychainImpl->keychainSchema();
			if (keychainSchema->hasRecordType(UnlockReferralRecord::recordType))
			{
				clear();			
				Table<UnlockReferralRecord> referrals(database);
				for (Table<UnlockReferralRecord>::iterator it = referrals.begin(); it != referrals.end(); it++) {
					switch ((*it)->type()) {
					case CSSM_APPLE_UNLOCK_TYPE_KEY_DIRECT:
					case CSSM_APPLE_UNLOCK_TYPE_WRAPPED_PRIVATE:
						keyReferral(**it);
						break;
					default:
						secdebug("kcreferral", "referral type %lu (to %s) not supported",
							(unsigned long)(*it)->type(), (*it)->dbName().c_str());
						break;
					}
				}
			}
			secdebug("kcreferral", "%lu samples generated", (unsigned long)size());
		} catch (...) {
			secdebug("kcreferral", "exception setting default credentials for %s; using standard value", database->name());
		}
		mMade = true;
	}
	
	return size() > 0;	// got credentials?
}


//
// Process a single referral record. This will handle all known types
// of referrals.
//
void DefaultCredentials::keyReferral(const UnlockReferralRecord &ref)
{
	secdebug("kcreferral", "processing type %ld referral to %s",
		(long)ref.type(), ref.dbName().c_str());
	DLDbIdentifier identifier(ref.dbName().c_str(), ref.dbGuid(), ref.dbSSID(), ref.dbSSType());

	// first, try the keychain indicated
	try {
		KeychainList list;
		list.push_back(globals().storageManager.keychain(identifier));
		if (unlockKey(ref, list))	// try just this database...
			return;						// ... bingo!
	} catch (...) { }
	
	// try the entire search list (just in case)
	try {
		secdebug("kcreferral", "no joy with %s; trying the entire keychain list for guid %s",
			ref.dbName().c_str(), ref.dbGuid().toString().c_str());
		unlockKey(ref, fallbackSearchList(identifier));
		return;
	} catch (...) { }
	secdebug("kcreferral", "no luck at all; we'll skip this record");
}


bool DefaultCredentials::unlockKey(const UnlockReferralRecord &ref, const KeychainList &list)
{
	bool foundSome = false;
	try {
		// form the query
		SecKeychainAttribute attributes[1] = {
			{ kSecKeyLabel, ref.keyLabel().length(), ref.keyLabel().data() }
		};
		SecKeychainAttributeList search = { 1, attributes };
		CSSM_DB_RECORDTYPE recordType =
			(ref.type() == CSSM_APPLE_UNLOCK_TYPE_KEY_DIRECT) ?
				CSSM_DL_DB_RECORD_SYMMETRIC_KEY : CSSM_DL_DB_RECORD_PRIVATE_KEY;
		KCCursor cursor(list, recordType, &search);
		
		Item keyItem;
		while (cursor->next(keyItem)) {
			secdebug("kcreferral", "located source key in %s", keyItem->keychain()->name());
			
			// get a reference to the key in the provider keychain
			CssmClient::Key key = dynamic_cast<KeyItem &>(*keyItem).key();
			const CssmKey &masterKey = key;
			
			// get the CSP handle FOR THE UNLOCKING KEY'S KEYCHAIN
			CSSM_CSP_HANDLE cspHandle = key->csp()->handle();

			// (a)symmetric-key form: KCLOCK, (A)SYMMETRIC_KEY, cspHandle, masterKey
			// Note that the last list element ("ref") is doing an implicit cast to a
			// CssmData, which passes the data portion of the UnlockReferralRecord
			append(TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
				new(allocator) ListElement((recordType==CSSM_DL_DB_RECORD_SYMMETRIC_KEY)?
					CSSM_WORDID_SYMMETRIC_KEY:CSSM_WORDID_ASYMMETRIC_KEY),
				new(allocator) ListElement(allocator, CssmData::wrap(cspHandle)),
				new(allocator) ListElement(allocator, CssmData::wrap(masterKey)),
				new(allocator) ListElement(allocator, ref.get())		
				));

			// let's make sure everything we need stays around
			mNeededItems.insert(keyItem);
			foundSome = true;
		}
	} catch (...) {
		// (ignore it)
	}
	return foundSome;
}


//
// Take the official keychain search list, and return those keychains whose
// module Guid matches the one given. Essentially, this focuses the search list
// to a particular type of keychain.
//
struct NotGuid {
	NotGuid(const Guid &g) : guid(g) { }
	const Guid &guid;
	bool operator () (Keychain kc) { return kc->database()->dl()->guid() != guid; }
};

DefaultCredentials::KeychainList DefaultCredentials::fallbackSearchList(const DLDbIdentifier &ident)
{
	KeychainList list;
	globals().storageManager.getSearchList(list);
	list.erase(remove_if(list.begin(), list.end(), NotGuid(ident.ssuid().guid())), list.end());
	return list;
}


}	// namespace KeychainCore
}	// namespace Security
