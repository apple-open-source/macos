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
#ifdef __MWERKS__
#define _CPP_ACL_ANY
#endif

#include <Security/acl_any.h>
#include <algorithm>


//
// The ANY subject matches all credentials, including none at all.
//
bool AnyAclSubject::validate(const AclValidationContext &) const
{
	return true;
}


//
// The CSSM_LIST version is trivial. It has no private part to omit.
//
CssmList AnyAclSubject::toList(CssmAllocator &alloc) const
{
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_ANY);
}


//
// The subject form takes no arguments.
//
AnyAclSubject *AnyAclSubject::Maker::make(const TypedList &list) const
{
	crack(list, 0);	// no arguments in input list
	return new AnyAclSubject();
}

AnyAclSubject *AnyAclSubject::Maker::make(Version, Reader &, Reader &) const
{
    return new AnyAclSubject();
}

