/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
#include <security_cdsa_utilities/acl_comment.h>
#include <security_cdsa_utilities/cssmwalkers.h>
#include <security_cdsa_utilities/cssmlist.h>
#include <algorithm>

using namespace DataWalkers;


//
// The COMMENT subject matches nothing, no matter how pretty.
//
bool CommentAclSubject::validate(const AclValidationContext &) const
{
	return false;
}


//
// The toList function simply returns a copy of the preserved list.
// The interface convention requires that we chunkCopy here.
//
CssmList CommentAclSubject::toList(Allocator &alloc) const
{
	CssmList result = CssmList::overlay(*mComment);
	ChunkCopyWalker w(alloc);
	walk(w, result);
	return result;
}


//
// Construct-from-list makes a unified copy of the list.
//
CommentAclSubject *CommentAclSubject::Maker::make(const TypedList &list) const
{
	const CSSM_LIST *baseList = &list;
	size_t commentSize = size(baseList);
	CSSM_LIST *comment = copy(baseList, Allocator::standard(), commentSize);
	return new CommentAclSubject(comment, commentSize);
}

CommentAclSubject *CommentAclSubject::Maker::make(Version, Reader &pub, Reader &) const
{
	CSSM_LIST *base; pub(base);	// get original pointer base
	const void *data; uint32 length; pub.countedData(data, length);	// data blob

	// copy the input blob into writable memory
	CSSM_LIST *list = Allocator::standard().malloc<CSSM_LIST>(length);
	memcpy(list, data, length);
	
	// relocate it based on the base pointer we stored
	relocate(list, base);
	
	// good
    return new CommentAclSubject(list, length);
}


//
// Export to blob form.
// Since we store the list in unified form, this isn't very hard. Do try to figure
// out how walkers work before messing with this code.
//
void CommentAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &)
{
	pub(mComment);	// yes, the pointer itself
	pub.countedData(mComment, mSize);
}

void CommentAclSubject::exportBlob(Writer &pub, Writer &)
{
	pub(mComment);
	pub.countedData(mComment, mSize);
}


#ifdef DEBUGDUMP

void CommentAclSubject::debugDump() const
{
	Debug::dump("Comment[never]");
}

#endif //DEBUGDUMP
