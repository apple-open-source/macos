/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// TrustStore.h - Abstract interface to permanent user trust assignments
//
#ifndef _SECURITY_TRUSTITEM_H_
#define _SECURITY_TRUSTITEM_H_

#include <Security/utilities.h>
#include <Security/Certificate.h>
#include <Security/Policies.h>
#include <Security/SecTrust.h>


// unique keychain item attributes for user trust records
enum {
    kSecTrustCertAttr 				 = 'tcrt',
    kSecTrustPolicyAttr				 = 'tpol'
};


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
		uint32 version;					// version mark
		SecTrustUserSetting trust;		// user's trust choice
	};
	static const uint32 currentVersion = 0x101;

public:
	// new item constructor
    UserTrustItem(Certificate *cert, Policy *policy, const TrustData &trust);
    virtual ~UserTrustItem();

	TrustData trust();

protected:
	virtual PrimaryKey add(Keychain &keychain);

	void populateAttributes();

private:
	RefPointer<Certificate> mCertificate;
	RefPointer<Policy> mPolicy;
};


} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_TRUSTITEM_H_
