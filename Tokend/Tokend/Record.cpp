/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  Record.cpp
 *  TokendMuscle
 */

#include "Record.h"

#include <security_cdsa_client/aclclient.h>

namespace Tokend
{

AutoAclOwnerPrototype Record::gNobodyAclOwner;
AutoAclEntryInfoList Record::gAnyReadAclEntries;

Record::Record()
{
}

Record::~Record()
{
	for_each_delete(mAttributes.begin(), mAttributes.end());
}

bool
Record::hasAttributeAtIndex(uint32 attributeIndex) const
{
	if (attributeIndex < mAttributes.size())
		return mAttributes[attributeIndex] != NULL;

	return false;
}

const Attribute &
Record::attributeAtIndex(uint32 attributeIndex) const
{
	if (attributeIndex < mAttributes.size())
	{
		Attribute *attribute = mAttributes[attributeIndex];
		if (attribute)
			return *attribute;
	}

	CssmError::throwMe(CSSMERR_DL_INTERNAL_ERROR);
}

void Record::attributeAtIndex(uint32 attributeIndex, Attribute *attribute)
{
	auto_ptr<Attribute> _(attribute);
	if (attributeIndex >= mAttributes.size())
		mAttributes.resize(attributeIndex + 1);

	if (mAttributes[attributeIndex] != NULL)
		CssmError::throwMe(CSSMERR_DL_INTERNAL_ERROR);

	mAttributes[attributeIndex] = _.release();
}

void Record::getOwner(AclOwnerPrototype &owner)
{
	// Normally nobody can change the acl of an object on a smartcard.
	if (!gNobodyAclOwner)
	{
		Allocator &alloc = Allocator::standard();
		gNobodyAclOwner.allocator(alloc);
		gNobodyAclOwner = CssmClient::AclFactory::NobodySubject(alloc);
	}
	owner = gNobodyAclOwner;
}

void Record::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	// Normally anyone can read an object on a smartcard (subclasses might
	// override this).
	if (!gAnyReadAclEntries) {
		gAnyReadAclEntries.allocator(Allocator::standard());
		gAnyReadAclEntries.add(CssmClient::AclFactory::AnySubject(
			gAnyReadAclEntries.allocator()),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
	}
	count = gAnyReadAclEntries.size();
	acls = gAnyReadAclEntries.entries();
}

void Record::changeOwner(const AclOwnerPrototype &owner)
{
	// Default changeOwner on a record always fails.
	CssmError::throwMe(CSSM_ERRCODE_OBJECT_MANIP_AUTH_DENIED);
}

void Record::changeAcl(const AccessCredentials &cred, const AclEdit &edit)
{
	// Default changeAcl on a record always fails.
	CssmError::throwMe(CSSM_ERRCODE_OBJECT_MANIP_AUTH_DENIED);
}

const char *Record::description()
{
	CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);
}

Attribute *Record::getDataAttribute(TokenContext *tokenContext)
{
	CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);
}


} // end namespace Tokend

