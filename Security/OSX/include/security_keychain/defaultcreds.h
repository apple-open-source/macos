/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
#ifndef _SECURITY_DEFAULTCREDS_H
#define _SECURITY_DEFAULTCREDS_H

#include "SecBase.h"
#include <security_cdsa_utilities/cssmcred.h>
#include <security_utilities/trackingallocator.h>
#include <security_cdsa_client/dlclient.h>
#include <security_cdsa_client/dl_standard.h>
#include <vector>
#include <set>


namespace Security {
namespace KeychainCore {


class Keychain;
class KeychainImpl;
class Item;


//
// DefaultCredentials is a self-constructing AccessCredentials variant
// that performs the magic "where are ways to unlock this keychain?" search.
//
class DefaultCredentials : public TrackingAllocator, public AutoCredentials {
public:
	DefaultCredentials(KeychainImpl *kcImpl, Allocator &alloc = Allocator::standard());
	
	bool operator () (CssmClient::Db database);
	
	void clear();
	
private:
	typedef vector<Keychain> KeychainList;

	void keyReferral(const CssmClient::UnlockReferralRecord &ref);
	bool unlockKey(const CssmClient::UnlockReferralRecord &ref, const KeychainList &list);
	
	KeychainList fallbackSearchList(const DLDbIdentifier &ident);
	
private:
	bool mMade;						// we did it already
	set<Item> mNeededItems;			// Items we need to keep around for unlock use
	KeychainImpl *mKeychainImpl;
};

            
} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_DEFAULTCREDS_H
