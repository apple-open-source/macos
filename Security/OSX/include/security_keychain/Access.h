/*
 * Copyright (c) 2002-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// Access.h - Access control wrappers
//
#ifndef _SECURITY_ACCESS_H_
#define _SECURITY_ACCESS_H_

#include <security_keychain/ACL.h>
#include <security_utilities/trackingallocator.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_client/aclclient.h>
#include <security_keychain/TrustedApplication.h>
#include <map>

namespace Security {
namespace KeychainCore {

using CssmClient::AclBearer;


class Access : public SecCFObject {
	NOCOPY(Access)
public:
	SECCFFUNCTIONS(Access, SecAccessRef, errSecInvalidItemRef, gTypes().Access)

	class Maker {
		NOCOPY(Maker)
		static const size_t keySize = 16;	// number of (random) bytes
		friend class Access;
	public:
		enum MakerType {kStandardMakerType, kAnyMakerType};
	
		Maker(Allocator &alloc = Allocator::standard(), MakerType makerType = kStandardMakerType);
		
		void initialOwner(ResourceControlContext &ctx, const AccessCredentials *creds = NULL);
		const AccessCredentials *cred();
		
		TrackingAllocator allocator;
		
		static const char creationEntryTag[];

		MakerType makerType() {return mMakerType;}
		
	private:
		CssmAutoData mKey;
		AclEntryInput mInput;
		AutoCredentials mCreds;
		MakerType mMakerType;
	};

public:
	// make default forms
    Access(const string &description);
    Access(const string &description, const ACL::ApplicationList &trusted);
    Access(const string &description, const ACL::ApplicationList &trusted,
		const AclAuthorizationSet &limitedRights, const AclAuthorizationSet &freeRights);
	
	// make a completely open Access (anyone can do anything)
	Access();
	
	// retrieve from an existing AclBearer
	Access(AclBearer &source);
	
	// make from CSSM layer information (presumably retrieved by caller)
	Access(const CSSM_ACL_OWNER_PROTOTYPE &owner,
		uint32 aclCount, const CSSM_ACL_ENTRY_INFO *acls);
    virtual ~Access();

public:
	CFArrayRef copySecACLs() const;
	CFArrayRef copySecACLs(CSSM_ACL_AUTHORIZATION_TAG action) const;
	
	void add(ACL *newAcl);
	void addOwner(ACL *newOwnerAcl);
	
	void setAccess(AclBearer &target, bool update = false);
	void setAccess(AclBearer &target, Maker &maker);

	template <class Container>
	void findAclsForRight(AclAuthorization right, Container &cont)
	{
		cont.clear();
		for (Map::const_iterator it = mAcls.begin(); it != mAcls.end(); it++)
			if (it->second->authorizes(right))
				cont.push_back(it->second);
	}
	
	std::string promptDescription() const;	// from any one of the ACLs contained
	
	void addApplicationToRight(AclAuthorization right, TrustedApplication *app);
	
	void copyOwnerAndAcl(CSSM_ACL_OWNER_PROTOTYPE * &owner,
		uint32 &aclCount, CSSM_ACL_ENTRY_INFO * &acls);
	
protected:
    void makeStandard(const string &description, const ACL::ApplicationList &trusted,
		const AclAuthorizationSet &limitedRights = AclAuthorizationSet(),
		const AclAuthorizationSet &freeRights = AclAuthorizationSet());
    void compile(const CSSM_ACL_OWNER_PROTOTYPE &owner,
        uint32 aclCount, const CSSM_ACL_ENTRY_INFO *acls);
	
	void editAccess(AclBearer &target, bool update, const AccessCredentials *cred);

private:
	static const CSSM_ACL_HANDLE ownerHandle = ACL::ownerHandle;
	typedef map<CSSM_ACL_HANDLE, SecPointer<ACL> > Map;

	Map mAcls;			// set of ACL entries
	Mutex mMutex;
};


} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_ACCESS_H_
