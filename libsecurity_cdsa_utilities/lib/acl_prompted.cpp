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
// acl_prompted - password-based validation with out-of-band prompting.
//
#include <security_cdsa_utilities/acl_prompted.h>
#include <security_utilities/debugging.h>
#include <security_utilities/endian.h>
#include <algorithm>


//
// Construct PromptedAclSubjects from prompts and optional data
//
PromptedAclSubject::PromptedAclSubject(Allocator &alloc,
	const CssmData &prompt, const CssmData &password)
	: SecretAclSubject(alloc, CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD, password),
	  mPrompt(alloc, prompt) { }
PromptedAclSubject::PromptedAclSubject(Allocator &alloc,
	CssmManagedData &prompt, CssmManagedData &password)
	: SecretAclSubject(alloc, CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD, password),
	  mPrompt(alloc, prompt) { }
PromptedAclSubject::PromptedAclSubject(Allocator &alloc,
	const CssmData &prompt, bool cache)
	: SecretAclSubject(alloc, CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD, cache),
	  mPrompt(alloc, prompt) { }


//
// PromptedAclSubject will prompt for the secret
//
bool PromptedAclSubject::getSecret(const AclValidationContext &context,
	const TypedList &subject, CssmOwnedData &secret) const
{
	if (Environment *env = context.environment<Environment>()) {
		return env->getSecret(secret, mPrompt);
	} else {
		return false;
	}
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList PromptedAclSubject::toList(Allocator &alloc) const
{
    // the password itself is private and not exported to CSSM
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD,
		new(alloc) ListElement(alloc, mPrompt));
}


//
// Create a PromptedAclSubject
//
PromptedAclSubject *PromptedAclSubject::Maker::make(const TypedList &list) const
{
    Allocator &alloc = Allocator::standard(Allocator::sensitive);
	switch (list.length()) {
	case 2:
		{
			ListElement *elem[1];
			crack(list, 1, elem, CSSM_LIST_ELEMENT_DATUM);
			return new PromptedAclSubject(alloc, elem[0]->data(), true);
		}
	case 3:
		{
			ListElement *elem[2];
			crack(list, 2, elem, CSSM_LIST_ELEMENT_DATUM, CSSM_LIST_ELEMENT_DATUM);
			return new PromptedAclSubject(alloc, elem[0]->data(), elem[1]->data());
		}
	default:
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	}
}

PromptedAclSubject *PromptedAclSubject::Maker::make(Version, Reader &pub, Reader &priv) const
{
    Allocator &alloc = Allocator::standard(Allocator::sensitive);
    const void *data; size_t length; priv.countedData(data, length);
	return new PromptedAclSubject(alloc, CssmAutoData(alloc, data, length), true);
}


//
// Export the subject to a memory blob
//
void PromptedAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	pub.countedData(mPrompt);
}

void PromptedAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	pub.countedData(mPrompt);
}


#ifdef DEBUGDUMP

void PromptedAclSubject::debugDump() const
{
	Debug::dump("Prompted-Password");
	SecretAclSubject::debugDump();
}

#endif //DEBUGDUMP
