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
// keyclient
//
#include <Security/keyclient.h>

using namespace CssmClient;


KeyImpl::KeyImpl(const CSP &csp) : ObjectImpl(csp), CssmKey() 
{
	mActive=false;
}

KeyImpl::KeyImpl(const CSP &csp, CSSM_KEY &key) : ObjectImpl(csp), CssmKey(key) 
{
	mActive=true;
}

KeyImpl::KeyImpl(const CSP &csp, const CSSM_DATA &keyData) : ObjectImpl(csp),
CssmKey(keyData.Length, csp->allocator().alloc<uint8>(keyData.Length)) 
{
	memcpy(KeyData.Data, keyData.Data, keyData.Length);
	mActive=true;
}

KeyImpl::~KeyImpl()
{
	try
	{
		deactivate();
	}
	catch(...) {}
}

void
KeyImpl::deleteKey(const CSSM_ACCESS_CREDENTIALS *cred)
{
	if (mActive)
	{
		mActive=false;
		check(CSSM_FreeKey(csp()->handle(), cred, this, CSSM_TRUE));
	}
}

void
KeyImpl::getAcl(const char *selectionTag, AutoAclEntryInfoList &aclInfos) const
{
	aclInfos.allocator(allocator());
	check(CSSM_GetKeyAcl(csp()->handle(), this, reinterpret_cast<const CSSM_STRING *>(selectionTag), aclInfos, aclInfos));
}

void
KeyImpl::changeAcl(const CSSM_ACCESS_CREDENTIALS *accessCred,
				   const CSSM_ACL_EDIT &aclEdit)
{
	check(CSSM_ChangeKeyAcl(csp()->handle(), accessCred, &aclEdit, this));
}

void
KeyImpl::getOwner(AutoAclOwnerPrototype &owner) const
{
	owner.allocator(allocator());
	check(CSSM_GetKeyOwner(csp()->handle(), this, owner));
}

void
KeyImpl::changeOwner(const CSSM_ACCESS_CREDENTIALS *accessCred,
					 const CSSM_ACL_OWNER_PROTOTYPE &newOwner)
{
	check(CSSM_ChangeKeyOwner(csp()->handle(), accessCred, this, &newOwner));
}

void KeyImpl::activate()
{
	mActive=true;
}

void KeyImpl::deactivate()
{
	if (mActive)
	{
		mActive=false;
		check(CSSM_FreeKey(csp()->handle(), NULL, this, CSSM_FALSE));
	}
}
