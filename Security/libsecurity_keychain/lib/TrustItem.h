/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
// TrustStore.h - Abstract interface to permanent user trust assignments
//
#ifndef _SECURITY_TRUSTITEM_H_
#define _SECURITY_TRUSTITEM_H_

#include <security_keychain/Item.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/Policies.h>
#include <Security/SecTrustPriv.h>


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
class UserTrustItem : public ItemImpl {
	NOCOPY(UserTrustItem)
public:	
	struct TrustData {
		Endian<uint32> version;					// version mark
		Endian<SecTrustUserSetting> trust;		// user's trust choice
	};
	static const uint32 currentVersion = 0x101;

public:
	// new item constructor
    UserTrustItem(Certificate *cert, Policy *policy, const TrustData &trust);
    virtual ~UserTrustItem();

	TrustData trust();
	
public:
	static void makeCertIndex(Certificate *cert, CssmOwnedData &index);

protected:
	virtual PrimaryKey add(Keychain &keychain);

	void populateAttributes();

private:
	SecPointer<Certificate> mCertificate;
	SecPointer<Policy> mPolicy;
};


} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_TRUSTITEM_H_
