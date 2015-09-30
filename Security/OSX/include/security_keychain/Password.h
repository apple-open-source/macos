/*
 * Copyright (c) 2002-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// Password.h - Password acquiring wrapper
//
#ifndef _SECURITY_PASSWORD_H_
#define _SECURITY_PASSWORD_H_

#include <security_keychain/Item.h>
// included by item #include <security_keychain/Keychains.h>
#include <security_keychain/Access.h>


namespace Security {
namespace KeychainCore {

class PasswordImpl : public SecCFObject {
public:
    SECCFFUNCTIONS(PasswordImpl, SecPasswordRef, errSecInvalidPasswordRef, gTypes().PasswordImpl)

public:
    // make default forms
    PasswordImpl(SecItemClass itemClass, SecKeychainAttributeList *searchAttrList, SecKeychainAttributeList *itemAttrList);
	PasswordImpl(PasswordImpl& existing);

    virtual ~PasswordImpl() throw();

    bool getData(UInt32 *length, const void **data);
    void setData(UInt32 length,const void *data);
    void save();
    bool useKeychain() const { return mUseKeychain; }
    bool rememberInKeychain() const { return mRememberInKeychain; }
    void setRememberInKeychain(bool remember) { mRememberInKeychain = remember; }
    void setAccess(Access *access);

private:
    // keychain item cached?
    Keychain mKeychain;
    Item mItem;
    bool mUseKeychain;
    bool mFoundInKeychain;
    bool mRememberInKeychain;
	Mutex mMutex;
};

class Password : public SecPointer<PasswordImpl>
{
public:
    Password(SecItemClass itemClass, SecKeychainAttributeList *searchAttrList, SecKeychainAttributeList *itemAttrList);
    Password(PasswordImpl *impl);
	Password(PasswordImpl &impl);
};



            
} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_PASSWORD_H_
