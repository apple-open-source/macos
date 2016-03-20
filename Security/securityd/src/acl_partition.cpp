/*
 * Copyright (c) 2000-2006,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// acl_partition - partition identifier store
//
// This ACL subject stores keychain partition data.
// When evaluated, it always fails. Securityd explicitly
//
#include "acl_partition.h"
#include <security_cdsa_utilities/cssmwalkers.h>
#include <security_cdsa_utilities/cssmlist.h>
#include <algorithm>

using namespace DataWalkers;


//
// The dictionaryPayload is the payload blob interpreted as an XML dictionary, or NULL if that didn't work.
//
CFDictionaryRef PartitionAclSubject::createDictionaryPayload() const
{
	return makeCFDictionaryFrom(CFTempData(this->payload));
}

void PartitionAclSubject::setDictionaryPayload(Allocator& alloc, CFDictionaryRef dict)
{
	CFRef<CFDataRef> xmlData = makeCFData(dict);
	this->payload = CssmAutoData(alloc, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
}


//
// The partition subject matches nothing, no matter how pretty.
//
bool PartitionAclSubject::validates(const AclValidationContext &) const
{
	return false;
}


//
// The list form has a simple CssmData payload.
//
CssmList PartitionAclSubject::toList(Allocator &alloc) const
{
	return TypedList(Allocator::standard(), CSSM_ACL_SUBJECT_TYPE_PARTITION,
					 new(alloc) ListElement(alloc, this->payload));
}


//
// Set payload from list input.
//
PartitionAclSubject *PartitionAclSubject::Maker::make(const TypedList &list) const
{
	Allocator &alloc = Allocator::standard();
	if (list.length() != 2)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	ListElement *payloadItem;
	crack(list, 1, &payloadItem, CSSM_LIST_ELEMENT_DATUM);
	return new PartitionAclSubject(alloc, payloadItem->data());
}


//
// A PartitionAclSubject is a "null" subject that contains out of band data
// for further security evaluation. When evaluated as an ACL subject, it always fails.
//
PartitionAclSubject *PartitionAclSubject::Maker::make(Version, Reader &pub, Reader &) const
{
	Allocator& alloc = Allocator::standard();
	const void* data; size_t length;
	pub.countedData(data, length);
	CssmAutoData payloadData(alloc, data, length);
	return new PartitionAclSubject(alloc, payloadData);
}


//
// Export to blob form.
// This simply writes the smallest form consistent with the heuristic above.
//
void PartitionAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &)
{
	pub.countedData(this->payload);
}

void PartitionAclSubject::exportBlob(Writer &pub, Writer &)
{
	pub.countedData(this->payload);
}
