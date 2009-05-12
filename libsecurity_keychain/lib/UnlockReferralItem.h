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
// UnlockReferralItem.h - Abstract interface to permanent user trust assignments
//
#ifndef _SECURITY_UNLOCKREFERRAL_H_
#define _SECURITY_UNLOCKREFERRAL_H_

#include <security_keychain/Item.h>


namespace Security {
namespace KeychainCore {


//
// A trust item in a keychain.
// Currently, Item constructors do not explicitly generate this subclass.
// They don't need to, since our ownly user (TrustStore) can deal with
// the generic Item class just fine.
// If we ever need Item to produce UserTrustItem impls, we would need to
// add constructors from primary key (see Certificate for an example).
//
class UnlockReferralItem : public ItemImpl {
	NOCOPY(UnlockReferralItem)
public:	

public:
	// new item constructor
    UnlockReferralItem();
    virtual ~UnlockReferralItem();

protected:
	virtual PrimaryKey add(Keychain &keychain);

	void populateAttributes();

private:
};


} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_UNLOCKREFERRAL_H_
