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
// acl_protectedpw - protected-path password-based ACL subject types.
//
// This implements "protected path" password-based subject types as per CSSM standard.
// The actual protected path is not implemented in this class; it's up to the user to provide it.
//
#ifndef _ACL_PROTECTED_PASSWORD
#define _ACL_PROTECTED_PASSWORD

#include <Security/cssmdata.h>
#include <Security/cssmacl.h>
#include <string>


namespace Security {

class ProtectedPasswordAclSubject : public SimpleAclSubject {
public:
    bool validate(const AclValidationContext &baseCtx, const TypedList &sample) const;
    CssmList toList(CssmAllocator &alloc) const;
    
    ProtectedPasswordAclSubject(CssmAllocator &alloc, const CssmData &password);
    ProtectedPasswordAclSubject(CssmAllocator &alloc, CssmManagedData &password);
    
    CssmAllocator &allocator;
    
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
