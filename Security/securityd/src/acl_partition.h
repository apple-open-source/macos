/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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
// acl_partition - "ignore" ACL subject type
//
// A pseudo-ACL that stores partition identifier data.
//
// A PartitionAclSubject always fails to verify.
//
#ifndef _ACL_PARTITION
#define _ACL_PARTITION

#include <security_cdsa_utilities/cssmacl.h>
#include <security_utilities/cfutilities.h>


namespace Security
{

//
// The ANY subject simply matches everything. No sweat.
//
class PartitionAclSubject : public AclSubject {
public:
	PartitionAclSubject()
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_PARTITION), payload(Allocator::standard()) { }
	PartitionAclSubject(Allocator& alloc, const CssmData &data)
	: AclSubject(CSSM_ACL_SUBJECT_TYPE_PARTITION), payload(alloc, data) { }
	
public:
	CssmAutoData payload;
	CFDictionaryRef createDictionaryPayload() const;
	void setDictionaryPayload(Allocator& alloc, CFDictionaryRef dict);
	
public:
	bool validates(const AclValidationContext &ctx) const;
	CssmList toList(Allocator &alloc) const;

    void exportBlob(Writer::Counter &pub, Writer::Counter &priv);
    void exportBlob(Writer &pub, Writer &priv);

	class Maker : public AclSubject::Maker {
	public:
		Maker() : AclSubject::Maker(CSSM_ACL_SUBJECT_TYPE_PARTITION) { }
		PartitionAclSubject *make(const TypedList &list) const;
    	PartitionAclSubject *make(Version, Reader &pub, Reader &priv) const;
	};
};

} // end namespace Security


#endif //_ACL_PARTITION
