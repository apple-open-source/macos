/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All Rights Reserved.
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
// acl_comment - "ignore" ACL subject type
//
// This ACL subject is a historical mistake. It has no use in present applications,
// and remains only to support existing keychains with their already-baked item ACLs.
// Do not use this for new applications of ANY kind.
//
// A CommentAclSubject always fails to verify.
// See further (mis-)usage comments in the .cpp.
//
#ifndef _ACL_COMMENT
#define _ACL_COMMENT

#include <security_cdsa_utilities/cssmacl.h>


namespace Security
{

//
// The ANY subject simply matches everything. No sweat.
//
class CommentAclSubject : public AclSubject {
public:
	CommentAclSubject()
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_COMMENT) { }
	
	bool validate(const AclValidationContext &ctx) const;
	CssmList toList(Allocator &alloc) const;

    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);

	class Maker : public AclSubject::Maker {
	public:
		Maker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_COMMENT) { }
		CommentAclSubject *make(const TypedList &list) const;
    	CommentAclSubject *make(Version, Reader &pub, Reader &priv) const;
	};
	
	IFDUMP(void debugDump() const);
};

} // end namespace Security


#endif //_ACL_COMMENT
