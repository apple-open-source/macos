/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// Identity.h - Working with Identities
//
#ifndef _SECURITY_IDENTITY_H_
#define _SECURITY_IDENTITY_H_

#include <Security/SecRuntime.h>
#include <Security/Certificate.h>
#include <Security/KeyItem.h>

namespace Security
{

namespace KeychainCore
{

class Identity : public SecCFObject
{
    NOCOPY(Identity)
public:
	SECCFFUNCTIONS(Identity, SecIdentityRef, errSecInvalidItemRef)

    Identity(const SecPointer<KeyItem> &privateKey,
		const SecPointer<Certificate> &certificate);
    Identity(const StorageManager::KeychainList &keychains, const SecPointer<Certificate> &certificate);
    virtual ~Identity() throw();

    SecPointer<KeyItem> privateKey() const;
	SecPointer<Certificate> certificate() const;

private:
    SecPointer<KeyItem> mPrivateKey;
	SecPointer<Certificate> mCertificate;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_IDENTITY_H_
