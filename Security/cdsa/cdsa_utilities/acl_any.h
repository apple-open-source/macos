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
// acl_any - "anyone" ACL subject type.
//
// This subject will categorically match everything and anything, even no
// credentials at all (a NULL AccessCredentials pointer).
//
#ifndef _ACL_ANY
#define _ACL_ANY

#include <Security/cssmacl.h>
#include <string>

#ifdef _CPP_ACL_ANY
#pragma export on
#endif

namespace Security
{

//
// The ANY subject simply matches everything. No sweat.
//
class AnyAclSubject : public AclSubject {
public:
    AnyAclSubject() : AclSubject(CSSM_ACL_SUBJECT_TYPE_ANY) { }
	bool validate(const AclValidationContext &ctx) const;
	CssmList toList(CssmAllocator &alloc) const;

	class Maker : public AclSubject::Maker {
	public:
		Maker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_ANY) { }
		AnyAclSubject *make(const TypedList &list) const;
    	AnyAclSubject *make(Version, Reader &pub, Reader &priv) const;
	};
};

} // end namespace Security

#ifdef _CPP_ACL_ANY
#pragma export off
#endif


#endif //_ACL_ANY
