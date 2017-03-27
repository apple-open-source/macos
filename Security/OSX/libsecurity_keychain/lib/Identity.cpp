/*
 * Copyright (c) 2002-2004,2011-2014 Apple Inc. All Rights Reserved.
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

#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeychain.h>

using namespace KeychainCore;

Identity::Identity(const SecPointer<KeyItem> &privateKey,
                   const SecPointer<Certificate> &certificate) :
    mPrivateKey(privateKey->handle()),
    mCertificate(certificate)
{
}

Identity::Identity(SecKeyRef privateKey,
		const SecPointer<Certificate> &certificate) :
	mPrivateKey((SecKeyRef)CFRetain(privateKey)),
	mCertificate(certificate)
{
}

Identity::Identity(const StorageManager::KeychainList &keychains, const SecPointer<Certificate> &certificate) :
	mPrivateKey(NULL), mCertificate(certificate)
{
    // Find a key whose label matches the publicKeyHash of the public key in the certificate.
    CssmData publicKeyHash = certificate->publicKeyHash();
    CFRef<CFDataRef> keyHash = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                           (const UInt8 *)publicKeyHash.data(),
                                                           publicKeyHash.length(),
                                                           kCFAllocatorNull);
    // First, try the new iOS keychain.
    {
        const void *keys[] = { kSecClass, kSecAttrKeyClass, kSecAttrApplicationLabel, kSecReturnRef, kSecAttrNoLegacy };
        const void *values[] = { kSecClassKey, kSecAttrKeyClassPrivate, keyHash, kCFBooleanTrue, kCFBooleanTrue };
        CFRef<CFDictionaryRef> query = CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                                                          sizeof(keys) / sizeof(*keys),
                                                          &kCFTypeDictionaryKeyCallBacks,
                                                          &kCFTypeDictionaryValueCallBacks);
        OSStatus status = SecItemCopyMatching(query, (CFTypeRef *)&mPrivateKey);
        if (status == errSecSuccess) {
            return;
        }
    }
    // Second, try the legacy OS X keychain(s).
    {
        mPrivateKey = NULL;
        CFRef<CFArrayRef> dynamicKeychains;
        SecKeychainCopyDomainSearchList(kSecPreferencesDomainDynamic, dynamicKeychains.take());
        CFRef<CFMutableArrayRef> dynamicSearchList  = CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)keychains.size(), &kCFTypeArrayCallBacks);
        CFRef<CFMutableArrayRef> searchList = CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)keychains.size(), &kCFTypeArrayCallBacks);
        for (StorageManager::KeychainList::const_iterator it = keychains.begin(), end = keychains.end(); it != end; ++it) {
            if (dynamicKeychains && CFArrayGetCount(dynamicKeychains) && CFArrayContainsValue(dynamicKeychains, CFRangeMake(0, CFArrayGetCount(dynamicKeychains)), **it)) {
                CFArrayAppendValue(dynamicSearchList, **it);
            }
            CFArrayAppendValue(searchList, **it);
        }
        const void *keys[] = { kSecClass, kSecAttrKeyClass, kSecAttrApplicationLabel, kSecReturnRef, kSecMatchSearchList };
        const void *values[] = { kSecClassKey, kSecAttrKeyClassPrivate, keyHash, kCFBooleanTrue, searchList };
        CFRef<CFDictionaryRef> query = CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                                                          sizeof(keys) / sizeof(*keys),
                                                          &kCFTypeDictionaryKeyCallBacks,
                                                          &kCFTypeDictionaryValueCallBacks);
        OSStatus status = SecItemCopyMatching(query, (CFTypeRef *)&mPrivateKey);
        if (status != errSecSuccess) {
            if (CFArrayGetCount(dynamicSearchList)) {
                // Legacy way is used for dynamic keychains because SmartCards keychain does not support strict CSSM queries which are generated in SecItemCopyMatching
                // Find a key whose label matches the publicKeyHash of the public key in the certificate.
                KCCursor keyCursor(keychains, (SecItemClass) CSSM_DL_DB_RECORD_PRIVATE_KEY, NULL);
                keyCursor->add(CSSM_DB_EQUAL, KeySchema::Label, certificate->publicKeyHash());

                Item key;
                if (!keyCursor->next(key))
                    MacOSError::throwMe(errSecItemNotFound);

                SecPointer<KeyItem> keyItem(static_cast<KeyItem *>(&*key));
                mPrivateKey = keyItem->handle();
            }
            else {
                MacOSError::throwMe(errSecItemNotFound);
            }
        }
    }
}

Identity::~Identity() throw()
{
    if (mPrivateKey)
        CFRelease(mPrivateKey);
}

SecPointer<KeyItem>
Identity::privateKey() const
{
	return SecPointer<KeyItem>(KeyItem::required(mPrivateKey));
}

SecPointer<Certificate>
Identity::certificate() const
{
	return mCertificate;
}

SecKeyRef
Identity::privateKeyRef() const
{
    return mPrivateKey;
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
    // Compare certificates first.
    if (Identity *otherIdentity = dynamic_cast<Identity *>(&other)) {
        Certificate *pCert = mCertificate.get(), *pOtherCert = otherIdentity->mCertificate.get();
        if (pCert == NULL || pOtherCert == NULL) {
            return pCert == pOtherCert;
        }

        if (pCert->equal(*pOtherCert)) {
            // Compare private keys.
            if (mPrivateKey == NULL || otherIdentity->mPrivateKey == NULL) {
                return mPrivateKey == otherIdentity->mPrivateKey;
            }
            return CFEqual(mPrivateKey, otherIdentity->mPrivateKey);
        }
    }
    return false;
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
	
    hashes.keyHash = CFHash(mPrivateKey);

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

