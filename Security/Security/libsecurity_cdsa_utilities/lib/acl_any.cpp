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
#include <security_cdsa_utilities/acl_any.h>
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
CssmList AnyAclSubject::toList(Allocator &alloc) const
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

