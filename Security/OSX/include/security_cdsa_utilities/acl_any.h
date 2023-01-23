/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
// acl_any - "anyone" ACL subject type.
//
// This subject will categorically match everything and anything, even no
// credentials at all (a NULL AccessCredentials pointer).
//
#ifndef _ACL_ANY
#define _ACL_ANY

#include <security_cdsa_utilities/cssmacl.h>
#include <string>

namespace Security {


//
// The ANY subject simply matches everything. No sweat.
//
class AnyAclSubject : public AclSubject {
public:
    AnyAclSubject() : AclSubject(CSSM_ACL_SUBJECT_TYPE_ANY) { }
	bool validates(const AclValidationContext &ctx) const;
	CssmList toList(Allocator &alloc) const;

	class Maker : public AclSubject::Maker {
	public:
		Maker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_ANY) { }
		AnyAclSubject *make(const TypedList &list) const;
    	AnyAclSubject *make(Version, Reader &pub, Reader &priv) const;
	};
};

} // end namespace Security


#endif //_ACL_ANY
