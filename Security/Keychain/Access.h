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
// Access.h - Access control wrappers
//
#ifndef _SECURITY_ACCESS_H_
#define _SECURITY_ACCESS_H_

#include <Security/SecRuntime.h>
#include <Security/ACL.h>
#include <Security/trackingallocator.h>
#include <Security/cssmaclpod.h>
#include <Security/cssmacl.h>
#include <Security/aclclient.h>
#include <Security/TrustedApplication.h>
#include <map>

namespace Security {
namespace KeychainCore {

using CssmClient::AclBearer;


class Access : public SecCFObject {
	NOCOPY(Access)
public:
	class Maker {
		NOCOPY(Maker)
		static const size_t keySize = 16;	// number of (random) bytes
		friend class Access;
	public:
		Maker(CssmAllocator &alloc = CssmAllocator::standard());
		
		void initialOwner(ResourceControlContext &ctx, const AccessCredentials *creds = NULL);
		const AccessCredentials *cred();
		
		TrackingAllocator allocator;
		
		static const char creationEntryTag[];

	private:
		CssmAutoData mKey;
		AclEntryInput mInput;
		AutoCredentials mCreds;
	};

public:
    Access(const string &description);
    Access(const string &description, const ACL::ApplicationList &trusted);
	Access(AclBearer &source);
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
	
	void addApplicationToRight(AclAuthorization right, TrustedApplication *app);
	
protected:
    void makeStandard(const string &description, const ACL::ApplicationList &trusted);
    void compile(const CSSM_ACL_OWNER_PROTOTYPE &owner,
        uint32 aclCount, const CSSM_ACL_ENTRY_INFO *acls);
	
	void editAccess(AclBearer &target, bool update, const AccessCredentials *cred);

private:
	static const CSSM_ACL_HANDLE ownerHandle = ACL::ownerHandle;
	typedef map<CSSM_ACL_HANDLE, RefPointer<ACL> > Map;

	Map mAcls;			// set of ACL entries
};


} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_ACCESS_H_
