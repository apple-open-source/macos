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
// Identity.cpp - Working with Identities
//
#include <security_keychain/Identity.h>

#include <security_cdsa_utilities/KeySchema.h>
#include <security_keychain/KCCursor.h>
#include <string.h>

using namespace KeychainCore;

Identity::Identity(const SecPointer<KeyItem> &privateKey,
		const SecPointer<Certificate> &certificate) :
	mPrivateKey(privateKey),
	mCertificate(certificate)
{
}

Identity::Identity(const StorageManager::KeychainList &keychains, const SecPointer<Certificate> &certificate) :
	mCertificate(certificate)
{
	// Find a key whose label matches the publicKeyHash of the public key in the certificate.
	KCCursor keyCursor(keychains, CSSM_DL_DB_RECORD_PRIVATE_KEY, NULL);
	keyCursor->add(CSSM_DB_EQUAL, KeySchema::Label, certificate->publicKeyHash());

	Item key;
	if (!keyCursor->next(key))
		MacOSError::throwMe(errSecItemNotFound);

	SecPointer<KeyItem> keyItem(static_cast<KeyItem *>(&*key));
	mPrivateKey = keyItem;
}

Identity::~Identity() throw()
{
}

SecPointer<KeyItem>
Identity::privateKey() const
{
	return mPrivateKey;
}

SecPointer<Certificate>
Identity::certificate() const
{
	return mCertificate;
}

bool
Identity::operator < (const Identity &other) const
{
	// Certificates in different keychains are considered equal if data is equal
	return (mCertificate < other.mCertificate);
}

bool
Identity::operator == (const Identity &other) const
{
	// Certificates in different keychains are considered equal if data is equal;
	// however, if their keys are in different keychains, the identities should
	// not be considered equal (according to mb)
	return (mCertificate == other.mCertificate && mPrivateKey == other.mPrivateKey);
}

bool Identity::equal(SecCFObject &other)
{
	CFHashCode this_hash = hash();
	CFHashCode other_hash = other.hash();
	return (this_hash == other_hash);
}

CFHashCode Identity::hash()
{
	CFHashCode result = SecCFObject::hash();
	
	   
    struct keyAndCertHash
    {
        CFHashCode keyHash;
        CFHashCode certHash;
    };
    
    struct keyAndCertHash hashes;
    memset(&hashes, 0, sizeof(struct keyAndCertHash));
	
	KeyItem* pKeyItem = mPrivateKey.get();
	if (NULL != pKeyItem)
	{
		hashes.keyHash = pKeyItem->hash();
	}
	
	Certificate* pCert = mCertificate.get();
	if (NULL != pCert)
	{
		hashes.certHash = pCert->hash();
	}
	
	if (hashes.keyHash != 0 || hashes.certHash != 0)
	{
        
		CFDataRef temp_data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)&hashes, sizeof(struct keyAndCertHash), kCFAllocatorNull);
		if (NULL != temp_data)
		{
			result = CFHash(temp_data);	
			CFRelease(temp_data);
		}
	}

	return result;
}

