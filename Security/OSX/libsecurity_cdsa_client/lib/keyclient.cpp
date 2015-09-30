/*
 * Copyright (c) 2000-2001,2011-2014 Apple Inc. All Rights Reserved.
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
// keyclient
//
#include <security_cdsa_client/keyclient.h>
#include <security_cdsa_utilities/cssmdata.h>


using namespace CssmClient;


KeyImpl::KeyImpl(const CSP &csp) : ObjectImpl(csp), CssmKey() 
{
	mActive=false;
}

KeyImpl::KeyImpl(const CSP &csp, const CSSM_KEY &key, bool copy) : ObjectImpl(csp), CssmKey(key)
{
	if (copy)
		keyData() = CssmAutoData(csp.allocator(), keyData()).release();
	mActive=true;
}

KeyImpl::KeyImpl(const CSP &csp, const CSSM_DATA &keyData) : ObjectImpl(csp),
CssmKey((uint32)keyData.Length, csp->allocator().alloc<uint8>((UInt32)keyData.Length))
{
	memcpy(KeyData.Data, keyData.Data, keyData.Length);
	mActive=true;
}

KeyImpl::~KeyImpl()
try
{
    deactivate();
}
catch (...)
{
}

void
KeyImpl::deleteKey(const CSSM_ACCESS_CREDENTIALS *cred)
{
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive=false;
		check(CSSM_FreeKey(csp()->handle(), cred, this, CSSM_TRUE));
	}
}

CssmKeySize
KeyImpl::sizeInBits() const
{
    CssmKeySize size;
    check(CSSM_QueryKeySizeInBits(csp()->handle(), CSSM_INVALID_HANDLE, this, &size));
    return size;
}

void
KeyImpl::getAcl(AutoAclEntryInfoList &aclInfos, const char *selectionTag) const
{
	aclInfos.allocator(allocator());
	check(CSSM_GetKeyAcl(csp()->handle(), this, reinterpret_cast<const CSSM_STRING *>(selectionTag), aclInfos, aclInfos));
}

void
KeyImpl::changeAcl(const CSSM_ACL_EDIT &aclEdit,
	const CSSM_ACCESS_CREDENTIALS *accessCred)
{
	check(CSSM_ChangeKeyAcl(csp()->handle(),
		AccessCredentials::needed(accessCred), &aclEdit, this));
}

void
KeyImpl::getOwner(AutoAclOwnerPrototype &owner) const
{
	owner.allocator(allocator());
	check(CSSM_GetKeyOwner(csp()->handle(), this, owner));
}

void
KeyImpl::changeOwner(const CSSM_ACL_OWNER_PROTOTYPE &newOwner,
	const CSSM_ACCESS_CREDENTIALS *accessCred)
{
	check(CSSM_ChangeKeyOwner(csp()->handle(),
		AccessCredentials::needed(accessCred), this, &newOwner));
}

void KeyImpl::activate()
{
    StLock<Mutex> _(mActivateMutex);
	mActive=true;
}

void KeyImpl::deactivate()
{
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive=false;
		check(CSSM_FreeKey(csp()->handle(), NULL, this, CSSM_FALSE));
	}
}
