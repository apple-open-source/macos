/*
 * Copyright (c) 2002-2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// Password.cpp
//
#include "Password.h"
#include <Security/SecBase.h>
#include "SecBridge.h"

#include "KCCursor.h"

using namespace KeychainCore;
using namespace CssmClient;

PasswordImpl::PasswordImpl(SecItemClass itemClass, SecKeychainAttributeList *searchAttrList, SecKeychainAttributeList *itemAttrList) :
    mItem(itemClass, itemAttrList, 0, NULL), mUseKeychain(false), mFoundInKeychain(false), mRememberInKeychain(false), mMutex(Mutex::recursive)
{
    if (searchAttrList && itemAttrList)
    {
        mUseKeychain = true;
        mKeychain = Keychain::optional(NULL);
		mRememberInKeychain = true;

        // initialize mFoundInKeychain to true if mItem is found
        
        StorageManager::KeychainList keychains;
        globals().storageManager.optionalSearchList(NULL, keychains);
        KCCursor cursor(keychains, itemClass, searchAttrList);

        if (cursor->next(mItem))
            mFoundInKeychain = true;
    }
}

PasswordImpl::PasswordImpl(PasswordImpl& existing)
{
	mKeychain = existing.mKeychain;
	mItem = existing.mItem;
    mUseKeychain = existing.mUseKeychain;
    mFoundInKeychain = existing.mFoundInKeychain;
    mRememberInKeychain = existing.mRememberInKeychain;
}



PasswordImpl::~PasswordImpl() throw()
{
}

void
PasswordImpl::setAccess(Access *access)
{
    // changing an existing ACL is more work than this SPI wants to do
    if (!mFoundInKeychain)
        mItem->setAccess(access);
}

void
PasswordImpl::setData(UInt32 length, const void *data)
{
    assert(mUseKeychain);
    
    // do different things based on mFoundInKeychain?
    mItem->setData(length,data);
}

bool
PasswordImpl::getData(UInt32 *length, const void **data)
{
    if (mItem->isPersistent())
    {
        // try to retrieve it
        CssmDataContainer outData;
        try
        {
            mItem->getData(outData);
            if (length && data)
            {
                *length=(uint32)outData.length();
                outData.Length=0;
                *data=outData.data();
                outData.Data=NULL;
            }
            return true;
        }
        catch (...)
        {
            // cancel unlock: CSP_USER_CANCELED
            // deny rogue app CSP_OPERATION_AUTH_DENIED
            return false;
        }
    }
    else
        return false;
}

void
PasswordImpl::save()
{
    assert(mUseKeychain);
    
    if (mFoundInKeychain)
    {
        mItem->update();
    }
    else
    {
        mKeychain->add(mItem);

        // reinitialize mItem now it's on mKeychain
        mFoundInKeychain = true; // should be set by member that resets mItem
    }
}

Password::Password(SecItemClass itemClass, SecKeychainAttributeList *searchAttrList, SecKeychainAttributeList *itemAttrList) : 
    SecPointer<PasswordImpl>(new PasswordImpl(itemClass, searchAttrList, itemAttrList))
{
}

Password::Password(PasswordImpl *impl) : SecPointer<PasswordImpl>(impl)
{
}

Password::Password(PasswordImpl &impl) : SecPointer<PasswordImpl>(new PasswordImpl(impl))
{
}
