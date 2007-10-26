/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// acl_secret - secret-validation password ACLs framework.
//
#ifndef _ACL_SECRET
#define _ACL_SECRET

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <string>


namespace Security {


//
// SecretAclSubject implements AclSubjects that perform their validation by
// passing their secret through some deterministic validation mechanism.
// As a limiting case, the subject can contain the secret itself and validate
// by comparing for equality.
//
// This is not a fully functional ACL subject. You must subclass it.
//
// There are three elements to consider here:
// (1) How to OBTAIN the secret. This is the job of your subclass; SecretAclSubject
//     is agnostic (and abstract) in this respect.
// (2) How to VALIDATE the secret. This is delegated to an environment method,
//     which gets this very subject passed as an argument for maximum flexibility.
// (3) Whether to use a locally stored copy of the secret for validation (by equality)
//     or hand it off to the environment validator. This is fully implemented here.
// This implementation assumes that the secret, whatever it may be, can be stored
// as a (flat) data blob and can be compared for bit-wise equality. No other
// interpretation is required at this level.
//
class SecretAclSubject : public SimpleAclSubject {
public:
    bool validate(const AclValidationContext &ctx, const TypedList &sample) const;
    
    SecretAclSubject(Allocator &alloc, CSSM_ACL_SUBJECT_TYPE type, const CssmData &secret);
    SecretAclSubject(Allocator &alloc, CSSM_ACL_SUBJECT_TYPE type, CssmManagedData &secret);
	SecretAclSubject(Allocator &alloc, CSSM_ACL_SUBJECT_TYPE type, bool doCache);

	bool haveSecret() const		{ return mSecretValid; }
	bool cacheSecret() const	{ return mCacheSecret; }
	
    void secret(const CssmData &secret) const;
    void secret(CssmManagedData &secret) const;
    
    Allocator &allocator;
	
	IFDUMP(void debugDump() const);

public:
	class Environment : virtual public AclValidationEnvironment {
	public:
		virtual bool validateSecret(const SecretAclSubject *me,
			const AccessCredentials *secret) = 0;
	};

protected:
	// implement this to get your secret (somehow)
	virtual bool getSecret(const AclValidationContext &context,
		const TypedList &sample, CssmOwnedData &secret) const = 0;
	
	const CssmData &secret() const { assert(mSecretValid); return mSecret; }
    
private:
    mutable CssmAutoData mSecret; // locally known secret
	mutable bool mSecretValid;	// mSecret is valid
	bool mCacheSecret;			// cache secret locally and validate from cache
};

} // end namespace Security


#endif //_ACL_SECRET
