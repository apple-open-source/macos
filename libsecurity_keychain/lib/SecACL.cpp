/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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

#include <Security/SecACL.h>
#include <security_keychain/ACL.h>
#include <security_keychain/Access.h>

#include "SecBridge.h"


//
// Local functions
//
static void setApplications(ACL *acl, CFArrayRef applicationList);


CFTypeID
SecACLGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().ACL.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


/*!
 */
OSStatus SecACLCreateFromSimpleContents(SecAccessRef accessRef,
	CFArrayRef applicationList,
	CFStringRef description, const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR *promptSelector,
	SecACLRef *newAcl)
{
	BEGIN_SECAPI
	SecPointer<Access> access = Access::required(accessRef);
	SecPointer<ACL> acl = new ACL(*access, cfString(description), *promptSelector);
	if (applicationList) {
		// application-list + prompt
		acl->form(ACL::appListForm);
		setApplications(acl, applicationList);
	} else {
		// allow-any
		acl->form(ACL::allowAllForm);
	}
	access->add(acl.get());
	Required(newAcl) = acl->handle();
	END_SECAPI
}


/*!
 */
OSStatus SecACLRemove(SecACLRef aclRef)
{
	BEGIN_SECAPI
	ACL::required(aclRef)->remove();
	END_SECAPI
}


static SecTrustedApplicationRef
convert(const SecPointer<TrustedApplication> &trustedApplication)
{
	return *trustedApplication;
}

/*!
 */
OSStatus SecACLCopySimpleContents(SecACLRef aclRef,
	CFArrayRef *applicationList,
	CFStringRef *promptDescription, CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR *promptSelector)
{
	BEGIN_SECAPI
	SecPointer<ACL> acl = ACL::required(aclRef);
	switch (acl->form()) {
	case ACL::allowAllForm:
		Required(applicationList) = NULL;
		Required(promptDescription) =
			acl->promptDescription().empty() ? NULL
				: makeCFString(acl->promptDescription());
		Required(promptSelector) = acl->promptSelector();
		break;
	case ACL::appListForm:
		Required(applicationList) =
			makeCFArray(convert, acl->applications());
		Required(promptDescription) = makeCFString(acl->promptDescription());
		Required(promptSelector) = acl->promptSelector();
		break;
	default:
		return errSecACLNotSimple;		// custom or unknown
	}
	END_SECAPI
}

OSStatus SecACLSetSimpleContents(SecACLRef aclRef,
	CFArrayRef applicationList,
	CFStringRef description, const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR *promptSelector)
{
	BEGIN_SECAPI
	SecPointer<ACL> acl = ACL::required(aclRef);
	acl->promptDescription() = description ? cfString(description) : "";
	acl->promptSelector() = promptSelector ? *promptSelector : ACL::defaultSelector;
	if (applicationList) {
		// application-list + prompt
		acl->form(ACL::appListForm);
		setApplications(acl, applicationList);
	} else {
		// allow-any
		acl->form(ACL::allowAllForm);
	}
	acl->modify();
	END_SECAPI
}


//
// Stuff a CFArray-of-SecTrustedApplications into an ACL object
//
static void setApplications(ACL *acl, CFArrayRef applicationList)
{
	ACL::ApplicationList &appList = acl->applications();
	appList.clear();
	//@@@ should really use STL iterator overlay on CFArray. By hand...
	CFIndex count = CFArrayGetCount(applicationList);
	for (CFIndex n = 0; n < count; n++)
		appList.push_back(TrustedApplication::required(
			SecTrustedApplicationRef(CFArrayGetValueAtIndex(applicationList, n))));
}


//
// Set and get the authorization tags of an ACL entry
//
OSStatus SecACLGetAuthorizations(SecACLRef acl,
	CSSM_ACL_AUTHORIZATION_TAG *tags, uint32 *tagCount)
{
	BEGIN_SECAPI
	AclAuthorizationSet auths = ACL::required(acl)->authorizations();
	if (Required(tagCount) < auths.size()) {	// overflow
		*tagCount = auths.size();				// report size required
		CssmError::throwMe(paramErr);
	}
	*tagCount = auths.size();
	copy(auths.begin(), auths.end(), tags);
	END_SECAPI
}

OSStatus SecACLSetAuthorizations(SecACLRef aclRef,
	CSSM_ACL_AUTHORIZATION_TAG *tags, uint32 tagCount)
{
	BEGIN_SECAPI
	SecPointer<ACL> acl = ACL::required(aclRef);
	if (acl->isOwner())		// can't change rights of the owner ACL
		MacOSError::throwMe(errSecInvalidOwnerEdit);
	AclAuthorizationSet &auths = acl->authorizations();
	auths.clear();
	copy(tags, tags + tagCount, insert_iterator<AclAuthorizationSet>(auths, auths.begin()));
	acl->modify();
	END_SECAPI
}
