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
// acl_protectedpw - protected-path password-based ACL subject types.
//
#include <security_cdsa_utilities/acl_protectedpw.h>
#include <security_utilities/debugging.h>
#include <algorithm>


//
// Construct a password ACL subject
//
ProtectedPasswordAclSubject::ProtectedPasswordAclSubject(Allocator &alloc, const CssmData &password)
    : SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_PROTECTED_PASSWORD),
    allocator(alloc), mPassword(alloc, password)
{ }

ProtectedPasswordAclSubject::ProtectedPasswordAclSubject(Allocator &alloc, CssmManagedData &password)
    : SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_PROTECTED_PASSWORD),
    allocator(alloc), mPassword(alloc, password)
{ }


//
// Validate a credential set against this subject
//
bool ProtectedPasswordAclSubject::validate(const AclValidationContext &context,
    const TypedList &sample) const
{
    if (sample.length() == 1) {
        return true;	//@@@ validate against PP
    } else if (sample.length() == 2 && sample[1].type() == CSSM_LIST_ELEMENT_DATUM) {
        const CssmData &password = sample[1];
        return password == mPassword;
    } else
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList ProtectedPasswordAclSubject::toList(Allocator &alloc) const
{
    // the password itself is private and not exported to CSSM
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PROTECTED_PASSWORD);
}


//
// Create a ProtectedPasswordAclSubject
//
ProtectedPasswordAclSubject *ProtectedPasswordAclSubject::Maker::make(const TypedList &list) const
{
    CssmAutoData password(Allocator::standard(Allocator::sensitive));
    if (list.length() == 1) {
        char pass[] = "secret";
        CssmData password = CssmData::wrap(pass, 6);	        //@@@ get password from PP
        return new ProtectedPasswordAclSubject(Allocator::standard(Allocator::sensitive), password);
    } else {
        ListElement *password;
        crack(list, 1, &password, CSSM_LIST_ELEMENT_DATUM);
        return new ProtectedPasswordAclSubject(Allocator::standard(Allocator::sensitive), *password);
    }
}

ProtectedPasswordAclSubject *ProtectedPasswordAclSubject::Maker::make(Version,
	Reader &pub, Reader &priv) const
{
    Allocator &alloc = Allocator::standard(Allocator::sensitive);
	const void *data; size_t length; priv.countedData(data, length);
	return new ProtectedPasswordAclSubject(alloc, CssmAutoData(alloc, data, length));
}


//
// Export the subject to a memory blob
//
void ProtectedPasswordAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	priv.countedData(mPassword);
}

void ProtectedPasswordAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	priv.countedData(mPassword);
}


#ifdef DEBUGDUMP

void ProtectedPasswordAclSubject::debugDump() const
{
	Debug::dump("Protected Password ");
	Debug::dumpData(mPassword.data(), mPassword.length());
}

#endif //DEBUGDUMP
