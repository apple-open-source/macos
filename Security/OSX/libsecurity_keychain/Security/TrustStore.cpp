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
// TrustStore.h - Abstract interface to permanent user trust assignments
//
#include <security_keychain/TrustStore.h>
#include <security_keychain/Globals.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/KCCursor.h>
#include <security_keychain/SecCFTypes.h>
#include <security_cdsa_utilities/Schema.h>
#include <Security/SecTrustSettingsPriv.h>

namespace Security {
namespace KeychainCore {


//
// Make and break: trivial
//
TrustStore::TrustStore(Allocator &alloc)
	: allocator(alloc), mRootsValid(false), mRootBytes(allocator), mMutex(Mutex::recursive)
{
}

TrustStore::~TrustStore()
{ }

//
// Retrieve the trust setting for a (certificate, policy) pair.
//
SecTrustUserSetting TrustStore::find(Certificate *cert, Policy *policy,
	StorageManager::KeychainList &keychainList)
{
	StLock<Mutex> _(mMutex);

	if (Item item = findItem(cert, policy, keychainList)) {
		// Make sure that the certificate is available in some keychain,
		// to provide a basis for editing the trust setting that we're returning.
		if (cert->keychain() == NULL) {
			if (cert->findInKeychain(keychainList) == NULL) {
				Keychain defaultKeychain = Keychain::optional(NULL);
				if (Keychain location = item->keychain()) {
					try {
						cert->copyTo(location);	// add cert to the trust item's keychain
					} catch (...) {
						secinfo("trusteval", "failed to add certificate %p to keychain \"%s\"",
							cert, location->name());
						try {
							if (&*location != &*defaultKeychain)
								cert->copyTo(defaultKeychain);	// try the default (if it's not the same)
						} catch (...) {
							// unable to add the certificate
							secinfo("trusteval", "failed to add certificate %p to keychain \"%s\"",
								cert, defaultKeychain->name());
						}
					}
				}
			}
		}
		CssmDataContainer data;
		item->getData(data);
		if (data.length() != sizeof(TrustData))
			MacOSError::throwMe(errSecInvalidTrustSetting);
		TrustData &trust = *data.interpretedAs<TrustData>();
		if (trust.version != UserTrustItem::currentVersion)
			MacOSError::throwMe(errSecInvalidTrustSetting);
		return trust.trust;
	} else {
		return kSecTrustResultUnspecified;
	}
}


//
// Set an individual trust element
//
void TrustStore::assign(Certificate *cert, Policy *policy, SecTrustUserSetting trust)
{
	StLock<Mutex> _(mMutex);

	TrustData trustData = { UserTrustItem::currentVersion, trust };
	Keychain defaultKeychain = Keychain::optional(NULL);
	Keychain trustLocation = defaultKeychain;	// default keychain, unless trust entry found
	StorageManager::KeychainList searchList;
	globals().storageManager.getSearchList(searchList);

	if (Item item = findItem(cert, policy, searchList)) {
		// user has a trust setting in a keychain - modify that
		trustLocation = item->keychain();
		if (trust == kSecTrustResultUnspecified)
			item->keychain()->deleteItem(item);
		else
			item->modifyContent(NULL, sizeof(trustData), &trustData);
	} else {
		// no trust entry: make one
		if (trust != kSecTrustResultUnspecified) {
			Item item = new UserTrustItem(cert, policy, trustData);
			if (Keychain location = cert->keychain()) {
				try {
					location->add(item);				// try the cert's keychain first
					trustLocation = location;
				} catch (...) {
					if (&*location != &*defaultKeychain)
						defaultKeychain->add(item);		// try the default (if it's not the same)
				}
			} else {
				defaultKeychain->add(item);				// raw cert - use default keychain
			}
		}
	}

	// Make sure that the certificate is available in some keychain,
	// to provide a basis for editing the trust setting that we're assigning.
	if (cert->keychain() == NULL) {
		if (cert->findInKeychain(searchList) == NULL) {
			try {
				cert->copyTo(trustLocation);	// add cert to the trust item's keychain
			} catch (...) {
				secinfo("trusteval", "failed to add certificate %p to keychain \"%s\"",
					cert, trustLocation->name());
				try {
					if (&*trustLocation != &*defaultKeychain)
						cert->copyTo(defaultKeychain);	// try the default (if it's not the same)
				} catch (...) {
					// unable to add the certificate
					secinfo("trusteval", "failed to add certificate %p to keychain \"%s\"",
						cert, defaultKeychain->name());
				}
			}
		}
	}
}


//
// Search the user's configured keychains for a trust setting.
// If found, return it (as a TrustItem). Otherwise, return NULL.
// Note that this function throws if a "real" error is encountered.
//
Item TrustStore::findItem(Certificate *cert, Policy *policy,
	StorageManager::KeychainList &keychainList)
{
	// As of OS X 10.5, user trust records are no longer stored in keychains.
	// SecTrustSetUserTrust was replaced with SecTrustSettingsSetTrustSettings,
	// which stores per-user trust in a separate root-owned file. This method,
	// however, would continue to find old trust records created prior to 10.5.
	// Since those are increasingly unlikely to exist (and cannot be edited),
	// we no longer need or want to look for them anymore.
	return ((ItemImpl*)NULL);

#if 0
	StLock<Mutex> _(mMutex);

	try {
		SecKeychainAttribute attrs[2];
		CssmAutoData certIndex(Allocator::standard());
		UserTrustItem::makeCertIndex(cert, certIndex);
		attrs[0].tag = kSecTrustCertAttr;
		attrs[0].length = (UInt32)certIndex.length();
		attrs[0].data = certIndex.data();
		const CssmOid &policyOid = policy->oid();
		attrs[1].tag = kSecTrustPolicyAttr;
		attrs[1].length = (UInt32)policyOid.length();
		attrs[1].data = policyOid.data();
		SecKeychainAttributeList attrList = { 2, attrs };
		KCCursor cursor(keychainList, CSSM_DL_DB_RECORD_USER_TRUST, &attrList);
		Item item;
		if (cursor->next(item))
			return item;
	}
	catch (const CommonError &error) {}

	return ((ItemImpl*)NULL);	// no trust schema, no records, no error
#endif
}

void TrustStore::getCssmRootCertificates(CertGroup &rootCerts)
{
	StLock<Mutex> _(mMutex);

	if (!mRootsValid)
		loadRootCertificates();
	rootCerts = CertGroup(CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER, CSSM_CERTGROUP_DATA);
	rootCerts.blobCerts() = &mRoots[0];
	rootCerts.count() = (uint32)mRoots.size();
}

//
// Load root (anchor) certificates from disk
//
void TrustStore::loadRootCertificates()
{
	StLock<Mutex> _(mMutex);

	CFRef<CFArrayRef> anchors;
	OSStatus ortn;

	/*
	 * Get the current set of all positively trusted anchors.
	 */
	ortn = SecTrustSettingsCopyUnrestrictedRoots(
		true, true, true,		/* all domains */
		anchors.take());
	if(ortn) {
		MacOSError::throwMe(ortn);
	}

	// how many data bytes do we need?
	size_t size = 0;
	CFIndex numCerts = CFArrayGetCount(anchors);
	CSSM_RETURN crtn;
	for(CFIndex dex=0; dex<numCerts; dex++) {
		SecCertificateRef certRef = (SecCertificateRef)CFArrayGetValueAtIndex(anchors, dex);
		CSSM_DATA certData;
		crtn = SecCertificateGetData(certRef, &certData);
		if(crtn) {
			CssmError::throwMe(crtn);
		}
		size += certData.Length;
	}
	mRootBytes.length(size);

	// fill CssmData vector while copying data bytes together
	mRoots.clear();
	uint8 *base = mRootBytes.data<uint8>();
	for(CFIndex dex=0; dex<numCerts; dex++) {
		SecCertificateRef certRef = (SecCertificateRef)CFArrayGetValueAtIndex(anchors, dex);
		CSSM_DATA certData;
		SecCertificateGetData(certRef, &certData);
		memcpy(base, certData.Data, certData.Length);
		mRoots.push_back(CssmData(base, certData.Length));
		base += certData.Length;
	}

	secinfo("anchors", "%ld anchors loaded", (long)numCerts);

	mRootsValid = true;			// ready to roll
}

} // end namespace KeychainCore
} // end namespace Security
