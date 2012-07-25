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
// ACL.h - ACL control wrappers
//
#ifndef _SECURITY_ACL_H_
#define _SECURITY_ACL_H_

#include <Security/SecACL.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_client/aclclient.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_utilities/seccfobject.h>
#include "SecCFTypes.h"

#include <vector>

namespace Security {
namespace KeychainCore {

using CssmClient::AclBearer;

class Access;
class TrustedApplication;


//
// An ACL Entry for an Access object
//
class ACL : public SecCFObject {
	NOCOPY(ACL)
public:
	SECCFFUNCTIONS(ACL, SecACLRef, errSecInvalidItemRef, gTypes().ACL)

	// create from CSSM layer ACL entry
	ACL(Access &acc, const AclEntryInfo &info,
		Allocator &alloc = Allocator::standard());
	// create from CSSM layer owner prototype
	ACL(Access &acc, const AclOwnerPrototype &owner,
		Allocator &alloc = Allocator::standard());
	// create an "any" ACL
	ACL(Access &acc, Allocator &alloc = Allocator::standard());
	// create from "standard form" arguments (with empty application list)
	ACL(Access &acc, string description, const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR &promptSelector,
		Allocator &alloc = Allocator::standard());
    virtual ~ACL();
	
	Allocator &allocator;
	
	enum State {
		unchanged,					// unchanged from source
		inserted,					// new
		modified,					// was changed (replace)
		deleted						// was deleted (now invalid)
	};
	State state() const { return mState; }
	
	enum Form {
		invalidForm,				// invalid
		customForm,					// not a recognized format (but valid)
		allowAllForm,				// indiscriminate
		appListForm					// list of apps + prompt confirm
	};
	Form form() const { return mForm; }
	void form(Form f) { mForm = f; }
	
	Access &access;					// we belong to this Access
	
public:
	AclAuthorizationSet &authorizations()	{ return mAuthorizations; }
	bool authorizes(AclAuthorization right);
	void setAuthorization(CSSM_ACL_AUTHORIZATION_TAG auth)
	{ mAuthorizations.clear(); mAuthorizations.insert(auth); }
	
	typedef vector< SecPointer<TrustedApplication> > ApplicationList;
	ApplicationList &applications()
	{ assert(form() == appListForm); return mAppList; }
	void addApplication(TrustedApplication *app);
	
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR &promptSelector()	{ return mPromptSelector; }
	string &promptDescription()							{ return mPromptDescription; }
	
	CSSM_ACL_HANDLE entryHandle() const	{ return mCssmHandle; }
	
	static const CSSM_ACL_HANDLE ownerHandle = 0xff0e2743;	// pseudo-handle for owner ACL
	bool isOwner() const			{ return mCssmHandle == ownerHandle; }
	void makeOwner()				{ mCssmHandle = ownerHandle; }
	
	void modify();					// mark modified (update on commit)
	void remove();					// mark removed (delete on commit)
	
	// produce chunk copies of CSSM forms; caller takes ownership
	void copyAclEntry(AclEntryPrototype &proto, Allocator &alloc = Allocator::standard());
	void copyAclOwner(AclOwnerPrototype &proto, Allocator &alloc = Allocator::standard());
	
public:
	void setAccess(AclBearer &target, bool update = false,
		const AccessCredentials *cred = NULL);

public:
	struct ParseError { };
	
public:
	static const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR defaultSelector;
	
private:
	void parse(const TypedList &subject);
	void parsePrompt(const TypedList &subject);
	void makeSubject();
	void clearSubjects(Form newForm);

private:
	State mState;					// change state
	Form mForm;						// format type

	// AclEntryPrototype fields (minus subject, which is virtually constructed)
	CSSM_ACL_HANDLE mCssmHandle;	// CSSM entry handle (for updates)
	string mEntryTag;				// CSSM entry tag (64 bytes or so, they say)
	bool mDelegate;					// CSSM delegate flag
	AclAuthorizationSet mAuthorizations; // rights for this ACL entry
	
	// composite AclEntryPrototype (constructed when needed)
	TypedList *mSubjectForm;
	
	// following values valid only if form() == appListForm
	ApplicationList mAppList;		// list of trusted applications
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR mPromptSelector; // selector field of PROMPT subject
	string mPromptDescription;		// description field of PROMPT subject
	Mutex mMutex;
};


} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_ACL_H_
