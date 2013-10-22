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
#include <security_keychain/SecAccessPriv.h>

#include "SecBridge.h"

// Forward reference
/*!
	@function GetACLAuthorizationTagFromString
	@abstract Get the CSSM ACL item from the CFString
    @param aclStr The String name of the ACL
	@result The CSSM ACL value
*/
sint32 GetACLAuthorizationTagFromString(CFStringRef aclStr);

CFStringRef GetAuthStringFromACLAuthorizationTag(sint32 tag);

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

OSStatus SecACLCreateWithSimpleContents(SecAccessRef access,
										CFArrayRef applicationList,
										CFStringRef description, 
										SecKeychainPromptSelector promptSelector,
										SecACLRef *newAcl)
{
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR cdsaPromptSelector;
	cdsaPromptSelector.version = CSSM_ACL_KEYCHAIN_PROMPT_CURRENT_VERSION;
	cdsaPromptSelector.flags = promptSelector;
	return SecACLCreateFromSimpleContents(access, applicationList, description, &cdsaPromptSelector, newAcl);
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

OSStatus SecACLCopyContents(SecACLRef acl,
							CFArrayRef *applicationList,
							CFStringRef *description, 
							SecKeychainPromptSelector *promptSelector)
{
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR cdsaPromptSelector;
	memset(&cdsaPromptSelector, 0, sizeof(cdsaPromptSelector));
	OSStatus err = errSecSuccess;
	
	err = SecACLCopySimpleContents(acl, applicationList, description, &cdsaPromptSelector);
	*promptSelector = cdsaPromptSelector.flags;
	return err;	
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

OSStatus SecACLSetContents(SecACLRef acl,
						   CFArrayRef applicationList,
						   CFStringRef description, 
						   SecKeychainPromptSelector promptSelector)
{
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR cdsaPromptSelector;
	cdsaPromptSelector.version = CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION;
	cdsaPromptSelector.flags = promptSelector;
	return SecACLSetSimpleContents(acl, applicationList, description, &cdsaPromptSelector);	
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
		*tagCount = (uint32)auths.size();				// report size required
		CssmError::throwMe(errSecParam);
	}
	*tagCount = (uint32)auths.size();
	copy(auths.begin(), auths.end(), tags);
	END_SECAPI
}

CFArrayRef SecACLCopyAuthorizations(SecACLRef acl)
{
	CFArrayRef result = NULL;
	if (NULL == acl)
	{
		return result;
	}
	
	AclAuthorizationSet auths = ACL::required(acl)->authorizations();
	uint32 numAuths = (uint32)auths.size();				
	
    CSSM_ACL_AUTHORIZATION_TAG* tags = new CSSM_ACL_AUTHORIZATION_TAG[numAuths];
    int i;
    for (i = 0; i < numAuths; ++i)
    {
        tags[i] = NULL;
    }
	
	OSStatus err = SecACLGetAuthorizations(acl, tags, &numAuths);
	if (errSecSuccess != err)
	{
		
		return result;
	}
	
	CFTypeRef* strings = new CFTypeRef[numAuths];
    for (i = 0; i < numAuths; ++i)
    {
        strings[i] = NULL;
    }
    
	for (size_t iCnt = 0; iCnt < numAuths; iCnt++)
	{
		strings[iCnt] = (CFTypeRef)GetAuthStringFromACLAuthorizationTag(tags[iCnt]);
	}

	result = CFArrayCreate(kCFAllocatorDefault, (const void **)strings, numAuths, NULL);

	delete[] strings;
    delete[] tags;

	return result;
	
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

OSStatus SecACLUpdateAuthorizations(SecACLRef acl, CFArrayRef authorizations)
{
	if (NULL == acl || NULL == authorizations)
	{
		return errSecParam;
	}
	uint32 tagCount = (uint32)CFArrayGetCount(authorizations);
	
	size_t tagSize = (tagCount * sizeof(CSSM_ACL_AUTHORIZATION_TAG));
	
	CSSM_ACL_AUTHORIZATION_TAG* tags = (CSSM_ACL_AUTHORIZATION_TAG*)malloc(tagSize);
	memset(tags, 0, tagSize);
	for (uint32 iCnt = 0; iCnt < tagCount; iCnt++)
	{
		tags[iCnt] = GetACLAuthorizationTagFromString((CFStringRef)CFArrayGetValueAtIndex(authorizations, iCnt));
	}
	
	OSStatus result = SecACLSetAuthorizations(acl, tags, tagCount);
	free(tags);
	return result;
}
