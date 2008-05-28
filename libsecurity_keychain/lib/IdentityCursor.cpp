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
 *
 * IdentityCursor.cpp -- Working with IdentityCursor
 */

#include <security_keychain/IdentityCursor.h>
#include <security_keychain/Identity.h>
#include <security_keychain/Trust.h>
#include <security_keychain/Item.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/KeyItem.h>
#include <security_keychain/Globals.h>
#include <security_cdsa_utilities/Schema.h>
#include <security_cdsa_utilities/KeySchema.h>
#include <Security/oidsalg.h>
#include <Security/SecKeychainItemPriv.h>
#include <security_utilities/simpleprefs.h>
#include <sys/param.h>

using namespace KeychainCore;

IdentityCursorPolicyAndID::IdentityCursorPolicyAndID(const StorageManager::KeychainList &searchList, CSSM_KEYUSE keyUsage, CFStringRef idString, SecPolicyRef policy, bool returnOnlyValidIdentities) :
	IdentityCursor(searchList, keyUsage),
	mPolicy(policy),
	mIDString(idString),
	mReturnOnlyValidIdentities(returnOnlyValidIdentities),
	mPreferredIdentityChecked(false),
	mPreferredIdentity(nil)
{
    if (mPolicy)
        CFRetain(mPolicy);

    if (mIDString)
        CFRetain(mIDString);
}

IdentityCursorPolicyAndID::~IdentityCursorPolicyAndID() throw()
{
    if (mPolicy)
        CFRelease(mPolicy);

    if (mIDString)
        CFRelease(mIDString);
}

void
IdentityCursorPolicyAndID::findPreferredIdentity()
{
	char idUTF8[MAXPATHLEN];
	if (!mIDString || !CFStringGetCString(mIDString, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
		idUTF8[0] = (char)'\0';
	uint32_t iprfValue = 'iprf'; // value is specified in host byte order, since kSecTypeItemAttr has type uint32 in the db schema
	SecKeychainAttribute sAttrs[] = {
		{ kSecTypeItemAttr, sizeof(uint32_t), &iprfValue },
		{ kSecServiceItemAttr, strlen(idUTF8), (char *)idUTF8 }
	};
	SecKeychainAttributeList sAttrList = { sizeof(sAttrs) / sizeof(sAttrs[0]), sAttrs };

//	StorageManager::KeychainList keychains;
//	globals().storageManager.optionalSearchList((CFTypeRef)nil, keychains);

	Item item;
	KCCursor cursor(mSearchList /*keychains*/, kSecGenericPasswordItemClass, &sAttrList);
	if (!cursor->next(item))
		return;

	// get persistent certificate reference
	SecKeychainAttribute itemAttrs[] = { { kSecGenericItemAttr, 0, NULL } };
	SecKeychainAttributeList itemAttrList = { sizeof(itemAttrs) / sizeof(itemAttrs[0]), itemAttrs };
	item->getContent(NULL, &itemAttrList, NULL, NULL);

	// find certificate, given persistent reference data
	CFDataRef pItemRef = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)itemAttrs[0].data, itemAttrs[0].length, kCFAllocatorNull);
	SecKeychainItemRef certItemRef = nil;
	OSStatus status = SecKeychainItemCopyFromPersistentReference(pItemRef, &certItemRef);
	if (pItemRef)
		CFRelease(pItemRef);
	item->freeContent(&itemAttrList, NULL);
	if (status || !certItemRef)
		return;

	// create identity reference, given certificate
	Item certItem = ItemImpl::required(SecKeychainItemRef(certItemRef));
	SecPointer<Certificate> certificate(static_cast<Certificate *>(certItem.get()));
	SecPointer<Identity> identity(new Identity(mSearchList /*keychains*/, certificate));

	mPreferredIdentity = identity;
	
	if (certItemRef)
		CFRelease(certItemRef);
}

bool
IdentityCursorPolicyAndID::next(SecPointer<Identity> &identity)
{
	SecPointer<Identity> currIdentity;
	Boolean identityOK = true;

	if (!mPreferredIdentityChecked)
	{
        try
        {
            findPreferredIdentity();
        }
        catch(...) {}
		mPreferredIdentityChecked = true;
		if (mPreferredIdentity)
		{
			identity = mPreferredIdentity;
			return true;
		}
	}

	for (;;)
	{
		bool result = IdentityCursor::next(currIdentity);   // base class finds the next identity by keyUsage
		if ( result )
		{
			if (mPreferredIdentity && (currIdentity == mPreferredIdentity))
			{
				identityOK = false;	// we already returned this one, move on to the next
				continue;
			}

			// If there was no policy specified, we're done.
			if ( !mPolicy )
			{
				identityOK = true; // return this identity
				break;
			}

			// To reduce the number of (potentially expensive) trust evaluations performed, we need
			// to do some pre-processing to filter out certs that don't match the search criteria.
			// Rather than try to duplicate the TP's policy logic here, we'll just call the TP with
			// a single-element certificate array, no anchors, and no keychains to search.

			SecPointer<Certificate> certificate = currIdentity->certificate();
			CFRef<SecCertificateRef> certRef(certificate->handle());
			CFRef<CFMutableArrayRef> anchorsArray(CFArrayCreateMutable(NULL, 1, NULL));
			CFRef<CFMutableArrayRef> certArray(CFArrayCreateMutable(NULL, 1, NULL));
			if ( !certArray || !anchorsArray )
			{
				identityOK = false; // skip this and move on to the next one
				continue;
			}
			CFArrayAppendValue(certArray, certRef);

			SecPointer<Trust> trustLite = new Trust(certArray, mPolicy);
			StorageManager::KeychainList emptyList;
			// Set the anchors and keychain search list to be empty
			trustLite->anchors(anchorsArray);
			trustLite->searchLibs(emptyList);
			trustLite->evaluate();
			SecTrustResultType trustResult = trustLite->result();

			if (trustResult == kSecTrustResultRecoverableTrustFailure ||
				trustResult == kSecTrustResultFatalTrustFailure)
			{
				CFArrayRef certChain = NULL;
				CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL, *evInfo = NULL;
				trustLite->buildEvidence(certChain, TPEvidenceInfo::overlayVar(statusChain));
				if (statusChain)
					evInfo = &statusChain[0];
				if (!evInfo || evInfo->NumStatusCodes > 0) // per-cert codes means we can't use this cert for this policy
					trustResult = kSecTrustResultInvalid; // handled below
				if (certChain)
					CFRelease(certChain);
			}
			if (trustResult == kSecTrustResultInvalid)
			{
				identityOK = false; // move on to the next one
				continue;
			}

			// If trust evaluation isn't requested, we're done.
			if ( !mReturnOnlyValidIdentities )
			{
				identityOK = true; // return this identity
				break;
			}

			// Perform a full trust evaluation on the certificate with the specified policy.
			SecPointer<Trust> trust = new Trust(certArray, mPolicy);
			trust->evaluate();
			trustResult = trust->result();

			if (trustResult == kSecTrustResultInvalid ||
				trustResult == kSecTrustResultRecoverableTrustFailure ||
				trustResult == kSecTrustResultFatalTrustFailure)
			{
				identityOK = false; // move on to the next one
				continue;
			}

			identityOK = true; // this one was OK; return it.
			break;
		}
		else
		{
			identityOK = false; // no more left.
			break;
		}
	}   // for(;;)
	
	if ( identityOK )
	{
		identity = currIdentity; // caller will release the identity
		return true;
	}
	else
	{
		return false;
	}
}


IdentityCursor::IdentityCursor(const StorageManager::KeychainList &searchList, CSSM_KEYUSE keyUsage) :
	mSearchList(searchList),
	mKeyCursor(mSearchList, CSSM_DL_DB_RECORD_PRIVATE_KEY, NULL)
{
	// If keyUsage is CSSM_KEYUSE_ANY then we need a key that can do everything
	if (keyUsage & CSSM_KEYUSE_ANY)
		keyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT
						   | CSSM_KEYUSE_DERIVE | CSSM_KEYUSE_SIGN
						   | CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_SIGN_RECOVER
						   | CSSM_KEYUSE_VERIFY_RECOVER | CSSM_KEYUSE_WRAP
						   | CSSM_KEYUSE_UNWRAP;

	if (keyUsage & CSSM_KEYUSE_ENCRYPT)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Encrypt, true);
	if (keyUsage & CSSM_KEYUSE_DECRYPT)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Decrypt, true);
	if (keyUsage & CSSM_KEYUSE_DERIVE)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Derive, true);
	if (keyUsage & CSSM_KEYUSE_SIGN)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Sign, true);
	if (keyUsage & CSSM_KEYUSE_VERIFY)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Verify, true);
	if (keyUsage & CSSM_KEYUSE_SIGN_RECOVER)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::SignRecover, true);
	if (keyUsage & CSSM_KEYUSE_VERIFY_RECOVER)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::VerifyRecover, true);
	if (keyUsage & CSSM_KEYUSE_WRAP)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Wrap, true);
	if (keyUsage & CSSM_KEYUSE_UNWRAP)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Unwrap, true);
}

IdentityCursor::~IdentityCursor() throw()
{
}

CFDataRef
IdentityCursor::pubKeyHashForSystemIdentity(CFStringRef domain)
{
    CFDataRef entryValue = nil;
    auto_ptr<Dictionary> identDict;
    try {
        identDict.reset(new Dictionary("com.apple.security.systemidentities", Dictionary::US_System));
        entryValue = identDict->getDataValue(domain);
        if (entryValue == nil) {
            /* try for default entry if we're not already looking for default */
            if(!CFEqual(domain, kSecIdentityDomainDefault)) {
                entryValue = identDict->getDataValue(kSecIdentityDomainDefault);
            }
        }
    }
    catch(...) {
    }
    if (entryValue) {
        CFRetain(entryValue);
    }
    return entryValue;
}

bool
IdentityCursor::next(SecPointer<Identity> &identity)
{
	for (;;)
	{
		if (!mCertificateCursor)
		{
			Item key;
			if (!mKeyCursor->next(key))
				return false;
	
			mCurrentKey = static_cast<KeyItem *>(key.get());

			CssmClient::DbUniqueRecord uniqueId = mCurrentKey->dbUniqueRecord();
			CssmClient::DbAttributes dbAttributes(uniqueId->database(), 1);
			dbAttributes.add(KeySchema::Label);
			uniqueId->get(&dbAttributes, NULL);
			const CssmData &keyHash = dbAttributes[0];
            
			mCertificateCursor = KCCursor(mSearchList, CSSM_DL_DB_RECORD_X509_CERTIFICATE, NULL);
			mCertificateCursor->add(CSSM_DB_EQUAL, Schema::kX509CertificatePublicKeyHash, keyHash);

            // if we have entries for the system identities, exclude their public key hashes in the search
            CFDataRef systemDefaultCertPubKeyHash = pubKeyHashForSystemIdentity(kSecIdentityDomainDefault);
            if (systemDefaultCertPubKeyHash) {
                CssmData pkHash((void *)CFDataGetBytePtr(systemDefaultCertPubKeyHash), CFDataGetLength(systemDefaultCertPubKeyHash));
                mCertificateCursor->add(CSSM_DB_NOT_EQUAL, Schema::kX509CertificatePublicKeyHash, pkHash);
                CFRelease(systemDefaultCertPubKeyHash);
            }
            CFDataRef kerbKDCCertPubKeyHash = pubKeyHashForSystemIdentity(kSecIdentityDomainKerberosKDC);
            if (kerbKDCCertPubKeyHash) {
                CssmData pkHash((void *)CFDataGetBytePtr(kerbKDCCertPubKeyHash), CFDataGetLength(kerbKDCCertPubKeyHash));
                mCertificateCursor->add(CSSM_DB_NOT_EQUAL, Schema::kX509CertificatePublicKeyHash, pkHash);
                CFRelease(kerbKDCCertPubKeyHash);
            }
		}
	
		Item cert;
		if (mCertificateCursor->next(cert))
		{
			SecPointer<Certificate> certificate(static_cast<Certificate *>(cert.get()));
			identity = new Identity(mCurrentKey, certificate);
			return true;
		}
		else
			mCertificateCursor = KCCursor();
	}
}
