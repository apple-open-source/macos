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
// acl_keychain - a subject type for the protected-path
//				  keychain prompt interaction model.
//
// Arguments in list form:
//	list[1] = CssmData: Descriptive String
//
#ifdef __MWERKS__
#define _CPP_ACL_KEYCHAIN
#endif

#include <Security/acl_keychain.h>
#include <algorithm>


//
// Validate a credential set against this subject
//
bool KeychainPromptAclSubject::validate(const AclValidationContext &,
    const TypedList &sample) const
{
    return interface.validate(description);
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList KeychainPromptAclSubject::toList(CssmAllocator &alloc) const
{
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
        new(alloc) ListElement(alloc, description));
}


//
// Create a PasswordAclSubject
//
KeychainPromptAclSubject *KeychainPromptAclSubject::Maker::make(const TypedList &list) const
{
    ListElement *params[1];
	crack(list, 1, params, CSSM_LIST_ELEMENT_DATUM);
	return new KeychainPromptAclSubject(interface, *params[0]);
}

KeychainPromptAclSubject *KeychainPromptAclSubject::Maker::make(Reader &pub, Reader &) const
{
    char *description; pub(description);
	return new KeychainPromptAclSubject(interface, description);
}

KeychainPromptAclSubject::KeychainPromptAclSubject(KeychainPromptInterface &ifc,
    string descr)
: SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT),
  interface(ifc), description(descr)
{
}


//
// Export the subject to a memory blob
//
void KeychainPromptAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
    pub(description.size() + 1);
}

void KeychainPromptAclSubject::exportBlob(Writer &pub, Writer &priv)
{
    pub(description.c_str());
}

