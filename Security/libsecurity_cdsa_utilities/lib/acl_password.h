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
// acl_password - password-based ACL subject types.
//
// This implements simple password-based subject types as per CSSM standard.
//
#ifndef _ACL_PASSWORD
#define _ACL_PASSWORD

#include <security_cdsa_utilities/acl_secret.h>


namespace Security {


//
// A PasswordAclSubject simply contains its secret.
// The environment is never consulted; we just compare against our known secret.
//
class PasswordAclSubject : public SecretAclSubject {
public:
    CssmList toList(Allocator &alloc) const;
    
    PasswordAclSubject(Allocator &alloc, const CssmData &password)
		: SecretAclSubject(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD, password) { }
    PasswordAclSubject(Allocator &alloc, CssmManagedData &password)
		: SecretAclSubject(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD, password) { }
	PasswordAclSubject(Allocator &alloc, bool cache)
		: SecretAclSubject(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD, cache) { }
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);

public:
    class Maker : public SecretAclSubject::Maker {
    public:
    	Maker() : SecretAclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_PASSWORD) { }
    	PasswordAclSubject *make(const TypedList &list) const;
    	PasswordAclSubject *make(Version, Reader &pub, Reader &priv) const;
    };
	
protected:
	bool getSecret(const AclValidationContext &context,
		const TypedList &subject, CssmOwnedData &secret) const;
};

} // end namespace Security


#endif //_ACL_PASSWORD
