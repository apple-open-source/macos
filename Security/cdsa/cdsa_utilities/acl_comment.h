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
// acl_comment - "ignore" ACL subject type
//
// This subject will never match anything - its presence is effectively ignored.
// Its usefulness lies in the fact that COMMENT type ACL subjects are valid ACL
// subjects that preserve their contents as uninterpreted data blobs. This allows
// you to keep information in an ACL that can be retrieved later. In particular,
// you can "prefix" any external ACL subject representation with an ACL_COMMENT
// header, which turns it into an inactive comment until you remove the prefix.
//
// Notes: 
// (1) All contents of a comment ACL are public.
// (2) While there is a COMMENT sample type, it is in no way related to this subject
//     type. Validation of a COMMENT acl subject never examines any samples.
//
#ifndef _ACL_COMMENT
#define _ACL_COMMENT

#include <Security/cssmacl.h>


namespace Security
{

//
// The ANY subject simply matches everything. No sweat.
//
class CommentAclSubject : public AclSubject {
public:
	CommentAclSubject::CommentAclSubject(CSSM_LIST *list, uint32 size)
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_COMMENT), mComment(list), mSize(size) { }
	~CommentAclSubject() { CssmAllocator::standard().free(mComment); }
	
	bool validate(const AclValidationContext &ctx) const;
	CssmList toList(CssmAllocator &alloc) const;

    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);

	class Maker : public AclSubject::Maker {
	public:
		Maker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_COMMENT) { }
		CommentAclSubject *make(const TypedList &list) const;
    	CommentAclSubject *make(Reader &pub, Reader &priv) const;
	};
	
private:
	CSSM_LIST *mComment;		// list form preserved
	uint32 mSize;				// size of mComment blob
};

} // end namespace Security


#endif //_ACL_COMMENT
