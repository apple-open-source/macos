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
// acl_protectedpw - protected-path password-based ACL subject types.
//
// This implements "protected path" password-based subject types as per CSSM standard.
// A "protected path" is something that is outside the scope of the computer proper,
// like e.g. a PINpad directly attached to a smartcard token.
// Note: A password prompted through securityd/SecurityAgent is a "prompted password",
// not a "protected password". See acl_prompted.h.
//
// @@@ Warning: This is not quite implemented.
//
#ifndef _ACL_PROTECTED_PASSWORD
#define _ACL_PROTECTED_PASSWORD

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <string>


namespace Security {

class ProtectedPasswordAclSubject : public SimpleAclSubject {
public:
    bool validate(const AclValidationContext &baseCtx, const TypedList &sample) const;
    CssmList toList(Allocator &alloc) const;
    
    ProtectedPasswordAclSubject(Allocator &alloc, const CssmData &password);
    ProtectedPasswordAclSubject(Allocator &alloc, CssmManagedData &password);
    
    Allocator &allocator;
    
    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);
	
	IFDUMP(void debugDump() const);
    
    class Maker : public AclSubject::Maker {
    public:
    	Maker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_PROTECTED_PASSWORD) { }
    	ProtectedPasswordAclSubject *make(const TypedList &list) const;
    	ProtectedPasswordAclSubject *make(Version, Reader &pub, Reader &priv) const;
    };
    
private:
    CssmAutoData mPassword;
};

} // end namespace Security


#endif //_ACL_PROTECTED_PASSWORD
