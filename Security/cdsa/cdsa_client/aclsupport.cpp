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
// aclsupport.h - support for special Keychain style acls
//

#include <Security/osxsigning.h>
#include <Security/osxsigner.h>
#include "aclsupport.h"
#include "keychainacl.h"
#include <memory>

using namespace CssmClient;


// ---------------------------------------------------------------------------
//	TrustedApplicationImpl
// ---------------------------------------------------------------------------

TrustedApplicationImpl::TrustedApplicationImpl(const CssmData &signature, const CssmData &comment, bool enabled) :
	mSignature(CssmAllocator::standard(), signature),
	mComment(CssmAllocator::standard(), comment),
	mEnabled(enabled)
{
}

TrustedApplicationImpl::TrustedApplicationImpl(const char *path, const CssmData &comment, bool enabled) :	mSignature(CssmAllocator::standard(), calcSignature(path)),
	mComment(CssmAllocator::standard(), comment),
	mEnabled(enabled)
{
}


const CssmData & TrustedApplicationImpl::signature() const
{

	return mSignature;
}

const CssmData & TrustedApplicationImpl::comment() const
{
	return mComment;
}

bool TrustedApplicationImpl::enabled() const
{
	return mEnabled;
}

void TrustedApplicationImpl::enabled(bool enabled)
{
	mEnabled = enabled;
}

bool TrustedApplicationImpl::sameSignature(const char *path)
{
	// return true if object at given path has same signature
	return (mSignature.get() == calcSignature(path).get());
}

CssmAutoData TrustedApplicationImpl::calcSignature(const char *path)
{
	// generate a signature for the given object
	auto_ptr<CodeSigning::OSXCode> objToVerify(CodeSigning::OSXCode::at(path));
	CodeSigning::OSXSigner signer;
	auto_ptr<CodeSigning::OSXSigner::OSXSignature> signature(signer.sign(*objToVerify));

	return CssmAutoData(CssmAllocator::standard(), signature->data(), signature->length());
}

// ---------------------------------------------------------------------------
//	TrustedApplication
// ---------------------------------------------------------------------------

TrustedApplication::TrustedApplication()
{
}

TrustedApplication::TrustedApplication(
	const char *path, const CssmData &comment, bool enabled) :
RefPointer<TrustedApplicationImpl>(new TrustedApplicationImpl(path, comment, enabled))
{
}

TrustedApplication::TrustedApplication(
	const CssmData &signature, const CssmData &comment, bool enabled) :
RefPointer<TrustedApplicationImpl>(new TrustedApplicationImpl(signature, comment, enabled))
{
}

// ---------------------------------------------------------------------------
//	KeychainACL
// ---------------------------------------------------------------------------

KeychainACL::KeychainACL(const Key &key) :
    mLabel(CssmAllocator::standard())
{
    mKey = key;
	initialize();
}

void KeychainACL::initialize()
{
	mAnyAllow=false;
	mAlwaysAskUser=false;

	AutoAclEntryInfoList aclInfos;
	mKey->getAcl(NULL, aclInfos);
	mHandle = CSSM_INVALID_HANDLE;
	const AclEntryInfo *theInfo = NULL;
	for(uint32 entry=0; entry<aclInfos.size(); entry++)
	{
		const AclEntryInfo &info = aclInfos[entry];
		const AuthorizationGroup &authorizationGroup=info.proto().authorization();
		for(uint32 auth=0; auth<authorizationGroup.count(); auth++)
		{
			if(authorizationGroup[auth]==CSSM_ACL_AUTHORIZATION_DECRYPT || authorizationGroup[auth]==CSSM_ACL_AUTHORIZATION_ANY)
			{
				if (mHandle != CSSM_INVALID_HANDLE && mHandle != info.handle())
				{
					mIsCustomACL=true;
					return;
				}

				mHandle = info.handle();
				theInfo = &info;
			}
		}
	}
	if (!theInfo)
	{
		mIsCustomACL=true;
		return;
	}

	TypedList subject=theInfo->proto().subject();
	assert(subject.isProper());
	const ListElement *element = subject.first();

	switch(*element)
	{
		case CSSM_ACL_SUBJECT_TYPE_ANY:
			assert(element->next() == NULL);
			mAnyAllow=true;
			return;

		case CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT:
			mAlwaysAskUser=true;
			element = element->next();
			assert(element && element->type() == CSSM_LIST_ELEMENT_DATUM && element->next() == NULL);
			mLabel = element->data();
			return;
		
		case CSSM_ACL_SUBJECT_TYPE_THRESHOLD:
			break;

		default:
			mIsCustomACL = true;
			return;
	}

	// OK, it's a threshold acl
	element = element->next();
	assert(element && element->type() == CSSM_LIST_ELEMENT_WORDID);
	if (*element != 1) {
		mIsCustomACL = true;
		return;
	}
	element = element->next();
	assert(element && element->type() == CSSM_LIST_ELEMENT_WORDID);
	uint32 n = *element;
	assert(n > 0);

	int isEnabled=1;
	for (uint32 ix = 0; ix < n; ++ix)
	{
		element = element->next();
		assert(element && element->type() == CSSM_LIST_ELEMENT_SUBLIST);
		const TypedList &subList = *element;
		assert(subList.isProper());
		const ListElement *subElement = subList.first();

		switch(*subElement)
		{
		case CSSM_ACL_SUBJECT_TYPE_ANY:
			// Must be first subList in list.
			assert(ix == 0 && subElement->next() == NULL);
			mAnyAllow=true;
			break;

		case CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT:
			// Must be last subList in list.
			assert(ix == n - 1);
			mAlwaysAskUser=true;
			subElement = subElement->next();
			assert(subElement && subElement->type() == CSSM_LIST_ELEMENT_DATUM && subElement->next() == NULL);
			mLabel = subElement->data();
			break;


		case CSSM_ACL_SUBJECT_TYPE_COMMENT:
		case CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE:
		{
			// when the app is disabled it is commented out.
			if(*subElement==CSSM_ACL_SUBJECT_TYPE_COMMENT)
			{
				isEnabled=0;
				subElement = subElement->next();
			}
			subElement = subElement->next();
     		assert(subElement && subElement->type() == CSSM_LIST_ELEMENT_WORDID);
			uint32 sigType = *subElement;
    		subElement = subElement->next();
			assert(subElement && subElement->type() == CSSM_LIST_ELEMENT_DATUM);
			const CssmData &sig = subElement->data();
			subElement = subElement->next();
			assert(subElement && subElement->type() == CSSM_LIST_ELEMENT_DATUM && subElement->next() == NULL);
			const CssmData &comment = subElement->data();
			// Only if sigType is CSSM_ACL_CODE_SIGNATURE_OSX this element is enabled.
			// @@@ Otherwsie it should be CSSM_ACL_CODE_SIGNATURE_NONE (which is not defined yet).
			// additionally the enabled flag must be respected.
			push_back(TrustedApplication(sig, comment, (sigType == CSSM_ACL_CODE_SIGNATURE_OSX) && isEnabled));
			break;
		}

		default:
			mIsCustomACL = true;
		return;
		}
	}

	// Since we looked at N values we should be done.
	assert(element->next() == NULL);
}

void KeychainACL::commit()
{
	TrackingAllocator allocator(CssmAllocator::standard());

	KeychainAclFactory aclFactory(allocator);

	CssmList &list = *new(allocator) CssmList();

	list.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_THRESHOLD));   
	list.append(new(allocator) ListElement(1));
	list.append(new(allocator) ListElement(size()+mAnyAllow+mAlwaysAskUser));
	
	if(mAnyAllow)
	{
		CssmList &sublist = *new(allocator) CssmList();
		sublist.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_ANY));
		list.append(new(allocator) ListElement(sublist));
	}

		
	for (uint32 ix = 0; ix < size(); ++ix)
	{
		TrustedApplication app = at(ix);
		CssmList &sublist = *new(allocator) CssmList();
		if(!app->enabled()) sublist.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_COMMENT));
		sublist.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE));
		sublist.append(new(allocator) ListElement(CSSM_ACL_CODE_SIGNATURE_OSX));
		sublist.append(new(allocator) ListElement(app->signature()));
		sublist.append(new(allocator) ListElement(app->comment()));
		list.append(new(allocator) ListElement(sublist));
	}

	if(mAlwaysAskUser)
	{
		CssmList &sublist = *new(allocator) CssmList();
		sublist.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT));
		sublist.append(new(allocator) ListElement(mLabel.get()));
		list.append(new(allocator) ListElement(sublist));	
	}

	AclEntryPrototype aclEntry(list);
	// @@@ @@@ Force "decrypt" authorization for now -- should take this from input!! @@@
	AuthorizationGroup &anyDecryptAuthGroup = aclEntry.authorization();
	CSSM_ACL_AUTHORIZATION_TAG decryptTag = CSSM_ACL_AUTHORIZATION_DECRYPT;
	anyDecryptAuthGroup.NumberOfAuthTags = 1;
	anyDecryptAuthGroup.AuthTags = &decryptTag;
	const AccessCredentials *promptCred = aclFactory.keychainPromptCredentials();
	AclEdit edit(mHandle, aclEntry);
	mKey->changeAcl(promptCred, edit);
}

void KeychainACL::anyAllow(bool allow)
{
	mAnyAllow=allow;
}

bool KeychainACL::anyAllow() const
{
	return mAnyAllow;
}

void KeychainACL::alwaysAskUser(bool ask)
{
	mAlwaysAskUser=ask;
}

bool KeychainACL::alwaysAskUser() const
{
	return mAlwaysAskUser;
}

bool KeychainACL::isCustomACL() const
{
	return mIsCustomACL;
}

void KeychainACL::label(const CssmData &label)
{
    mLabel = label;
}
