/*
 * Copyright (c) 2000-2001,2007 Apple Inc. All Rights Reserved.
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
// aclclient 
//
#ifndef _H_CDSA_CLIENT_ACLCLIENT
#define _H_CDSA_CLIENT_ACLCLIENT  1

#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <security_utilities/refcount.h>
#include <security_utilities/globalizer.h>

namespace Security {
namespace CssmClient {

class CSP;


//
// Any client-side object that has CSSM-layer ACLs shall be
// derived from AclBearer and implement its methods accordingly.
// Note the (shared/virtual) RefCount - you should handle AclBearer
// references via RefPointers.
// All the non-pure methods are implemented (in AclBearer) in terms of
// the pure virtual methods; they just restate the problem in various ways.
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
// An AclBearer applied to a raw CSSM key
//
class KeyAclBearer : public AclBearer {
public:
	KeyAclBearer(CSSM_CSP_HANDLE cspH, CSSM_KEY &theKey, Allocator &alloc)
		: csp(cspH), key(theKey), allocator(alloc) { }
	
	const CSSM_CSP_HANDLE csp;
	CSSM_KEY &key;
	Allocator &allocator;
	
protected:
	void getAcl(AutoAclEntryInfoList &aclInfos,
		const char *selectionTag = NULL) const;
	void changeAcl(const CSSM_ACL_EDIT &aclEdit,
		const CSSM_ACCESS_CREDENTIALS *cred = NULL);
	void getOwner(AutoAclOwnerPrototype &owner) const;
	void changeOwner(const CSSM_ACL_OWNER_PROTOTYPE &newOwner,
		const CSSM_ACCESS_CREDENTIALS *cred = NULL);
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
	const AccessCredentials *nullCred() const;			// conforming empty
	const AccessCredentials *promptCred() const;		// enable interactive prompting
	const AccessCredentials *unlockCred() const;
	const AccessCredentials *cancelCred() const;
	const AccessCredentials *promptedPINCred() const;
	const AccessCredentials *promptedPINItemCred() const;

	const AclOwnerPrototype &anyOwner() const;			// wide-open owner
	const AclEntryInfo &anyAcl() const;					// wide-open ACL entry (authorizes anything)

protected:
	class KeychainCredentials {
	public:
		KeychainCredentials(Allocator &alloc)
			: allocator(alloc), mCredentials(new AutoCredentials(alloc)) { }
		virtual ~KeychainCredentials();

		Allocator &allocator;

        operator const AccessCredentials* () const { return mCredentials; }
	
    protected:
		AutoCredentials *mCredentials;
	};
    
public:
    // create a self-managed AccessCredentials to explicitly provide a keychain passphrase
    class PassphraseUnlockCredentials : public KeychainCredentials {
    public:
        PassphraseUnlockCredentials (const CssmData& password, Allocator& allocator);
    };
        
	// create a self-managed AccessCredentials to change a keychain passphrase
    class PasswordChangeCredentials : public KeychainCredentials {
    public:
        PasswordChangeCredentials (const CssmData& password, Allocator& allocator);
    };

public:
	class AnyResourceContext : public ResourceControlContext {
	public:
		AnyResourceContext(const CSSM_ACCESS_CREDENTIALS *cred = NULL);
		
	private:
		ListElement mAny;
		CSSM_ACL_AUTHORIZATION_TAG mTag;
	};

public:
	//
	// Subject makers. Contents are chunk-allocated with the Allocator given
	//
	struct Subject : public TypedList {
		Subject(Allocator &alloc, CSSM_ACL_SUBJECT_TYPE type);
	};
	
	// an ANY subject, allocated dynamically for you
	struct AnySubject : public Subject {
		AnySubject(Allocator &alloc) : Subject(alloc, CSSM_ACL_SUBJECT_TYPE_ANY) { }
	};
	
	// a "nobody" subject (something guaranteed never to match)
	struct NobodySubject : public Subject {
		NobodySubject(Allocator &alloc) : Subject(alloc, CSSM_ACL_SUBJECT_TYPE_COMMENT) { }
	};

	// password subjects
	struct PWSubject : public Subject {
		PWSubject(Allocator &alloc);							// no secret
		PWSubject(Allocator &alloc, const CssmData &secret);	// this secret
	};

	struct PromptPWSubject : public Subject {
		PromptPWSubject(Allocator &alloc, const CssmData &prompt);
		PromptPWSubject(Allocator &alloc, const CssmData &prompt, const CssmData &secret);
	};

	struct ProtectedPWSubject : public Subject {
		ProtectedPWSubject(Allocator &alloc);
	};
	
	// PIN (pre-auth) reference, origin side
	struct PinSubject : public Subject {
		PinSubject(Allocator &alloc, uint32 slot);
	};
	
	// PIN (pre-auth) source site
	struct PinSourceSubject : public Subject {
		PinSourceSubject(Allocator &alloc, const TypedList &form);
	};
};


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_ACLCLIENT
