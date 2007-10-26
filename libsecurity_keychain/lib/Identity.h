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

//
// Identity.h - Working with Identities
//
#ifndef _SECURITY_IDENTITY_H_
#define _SECURITY_IDENTITY_H_

#include <security_keychain/Certificate.h>
#include <security_keychain/KeyItem.h>

namespace Security
{

namespace KeychainCore
{

class Identity : public SecCFObject
{
    NOCOPY(Identity)
public:
	SECCFFUNCTIONS(Identity, SecIdentityRef, errSecInvalidItemRef, gTypes().Identity)

    Identity(const SecPointer<KeyItem> &privateKey,
		const SecPointer<Certificate> &certificate);
    Identity(const StorageManager::KeychainList &keychains, const SecPointer<Certificate> &certificate);
    virtual ~Identity() throw();

	SecPointer<KeyItem> privateKey() const;
	SecPointer<Certificate> certificate() const;

	bool operator < (const Identity &other) const;
	bool operator == (const Identity &other) const;

	bool equal(SecCFObject &other);

private:
	SecPointer<KeyItem> mPrivateKey;
	SecPointer<Certificate> mCertificate;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_IDENTITY_H_
