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
// acl_password - password-based ACL subject types
//
#ifdef __MWERKS__
#define _CPP_ACL_PASSWORD
#endif

#include <Security/acl_password.h>
#include <Security/debugging.h>
#include <algorithm>


//
// Construct a password ACL subject
//
PasswordAclSubject::PasswordAclSubject(CssmAllocator &alloc, const CssmData &password)
    : SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_PASSWORD, CSSM_SAMPLE_TYPE_PASSWORD),
    allocator(alloc), mPassword(alloc, password)
{ }

PasswordAclSubject::PasswordAclSubject(CssmAllocator &alloc, CssmManagedData &password)
    : SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_PASSWORD, CSSM_SAMPLE_TYPE_PASSWORD),
    allocator(alloc), mPassword(alloc, password)
{ }


//
// Validate a credential set against this subject
//
bool PasswordAclSubject::validate(const AclValidationContext &context,
    const TypedList &sample) const
{
	if (sample[1].type() != CSSM_LIST_ELEMENT_DATUM)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
    const CssmData &password = sample[1];
    return password == mPassword;
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList PasswordAclSubject::toList(CssmAllocator &alloc) const
{
    // the password itself is private and not exported to CSSM
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD);
}


//
// Create a PasswordAclSubject
//
PasswordAclSubject *PasswordAclSubject::Maker::make(const TypedList &list) const
{
	ListElement *password;
	crack(list, 1, &password, CSSM_LIST_ELEMENT_DATUM);
	return new PasswordAclSubject(CssmAllocator::standard(CssmAllocator::sensitive), *password);
}

PasswordAclSubject *PasswordAclSubject::Maker::make(Reader &pub, Reader &priv) const
{
    CssmAllocator &alloc = CssmAllocator::standard(CssmAllocator::sensitive);
	const void *data; uint32 length; priv.countedData(data, length);
	return new PasswordAclSubject(alloc, CssmAutoData(alloc, data, length));
}


//
// Export the subject to a memory blob
//
void PasswordAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	priv.countedData(mPassword);
}

void PasswordAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	priv.countedData(mPassword);
}


#ifdef DEBUGDUMP

void PasswordAclSubject::debugDump() const
{
	Debug::dump("Password ");
	Debug::dumpData(mPassword.data(), mPassword.length());
}

#endif //DEBUGDUMP
