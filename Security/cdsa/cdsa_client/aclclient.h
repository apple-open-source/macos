/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// keyclient 
//
#ifndef _H_CDSA_CLIENT_ACLCLIENT
#define _H_CDSA_CLIENT_ACLCLIENT  1

#include <Security/cssmaclpod.h>
#include <Security/cssmacl.h>
#include <Security/cssmcred.h>
#include <Security/refcount.h>
#include <Security/globalizer.h>

namespace Security {
namespace CssmClient {

class CSP;


//
// Any client-side object that has CSSM-layer ACLs shall be
// derived from AclBearer and implement its methods accordingly.
// Note the (shared/virtual) RefCount - you should handle AclBearer
// references via RefPointers.
//
class AclBearer : public virtual RefCount {
public:
	virtual ~AclBearer();

	// Acl manipulation
	virtual void getAcl(AutoAclEntryInfoList &aclInfos,
		const char *selectionTag = NULL) const = 0;
	virtual void changeAcl(const CSSM_ACL_EDIT &aclEdit,
		const CSSM_ACCESS_CREDENTIALS *cred = NULL) = 0;
	
	void addAcl(const AclEntryInput &input, const CSSM_ACCESS_CREDENTIALS *cred = NULL);
	void changeAcl(CSSM_ACL_HANDLE handle, const AclEntryInput &input,
		const CSSM_ACCESS_CREDENTIALS *cred = NULL);
	void deleteAcl(CSSM_ACL_HANDLE handle, const CSSM_ACCESS_CREDENTIALS *cred = NULL);
	void deleteAcl(const char *tag = NULL, const CSSM_ACCESS_CREDENTIALS *cred = NULL);

	// Acl owner manipulation
	virtual void getOwner(AutoAclOwnerPrototype &owner) const = 0;
	virtual void changeOwner(const CSSM_ACL_OWNER_PROTOTYPE &newOwner,
		const CSSM_ACCESS_CREDENTIALS *cred = NULL) = 0;
};


//
// An AclFactory helps create and maintain CSSM-layer AccessCredentials
// and matching samples. There is state in an AclFactory, though simple
// uses may not care about it.
//
class AclFactory {
public:
	AclFactory();
	virtual ~AclFactory();
	
	// these values are owned by the AclFactory and persist
	// until it is destroyed. You don't own the memory.
	const AccessCredentials *nullCred() const;
	const AccessCredentials *promptCred() const;
	const AccessCredentials *unlockCred() const;

protected:
	class KeychainCredentials {
	public:
		KeychainCredentials(CssmAllocator &alloc)
			: allocator(alloc), mCredentials(new AutoCredentials(alloc)) { }
		virtual ~KeychainCredentials();

		CssmAllocator &allocator;

        operator const AccessCredentials* () { return mCredentials; }
	
    protected:
		AutoCredentials *mCredentials;
	};
    
public:
    // create a self-managed AccessCredentials to explicitly provide a keychain passphrase
    class PassphraseUnlockCredentials : public KeychainCredentials {
    public:
        PassphraseUnlockCredentials (const CssmData& password, CssmAllocator& allocator);
    };
        
	// create a self-managed AccessCredentials to change a keychain passphrase
    class PasswordChangeCredentials : public KeychainCredentials {
    public:
        PasswordChangeCredentials (const CssmData& password, CssmAllocator& allocator);
    };
	
public:
	class AnyResourceContext : public ResourceControlContext {
	public:
		AnyResourceContext(const CSSM_ACCESS_CREDENTIALS *cred = NULL);
		
	private:
		ListElement mAny;
		CSSM_ACL_AUTHORIZATION_TAG mTag;
	};
};


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_ACLCLIENT
