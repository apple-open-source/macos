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
// TrustStore.h - Abstract interface to permanent user trust assignments
//
#include <Security/TrustStore.h>
#include <Security/Globals.h>
#include <Security/Certificate.h>
#include <Security/SecCFTypes.h>
#include <Security/schema.h>


namespace Security {
namespace KeychainCore {


//
// Make and break: trivial
//
TrustStore::TrustStore(CssmAllocator &alloc)
	: allocator(alloc), mRootsValid(false), mRootBytes(allocator)
{
}

TrustStore::~TrustStore()
{ }


//
// Retrieve the trust setting for a (certificate, policy) pair.
//
SecTrustUserSetting TrustStore::find(Certificate *cert, Policy *policy)
{
	if (Item item = findItem(cert, policy)) {
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
	TrustData trustData = { UserTrustItem::currentVersion, trust };
	if (Item item = findItem(cert, policy)) {
		// user has a trust setting in a keychain - modify that
		item->modifyContent(NULL, sizeof(trustData), &trustData);
	} else {
		// no trust entry: make one
		Item item = new UserTrustItem(cert, policy, trustData);
		if (Keychain location = cert->keychain())
			location->add(item);					// in the cert's keychain
		else
			Keychain::optional(NULL)->add(item);	// in the default keychain
	}
}


//
// Search the user's configured keychains for a trust setting.
// If found, return it (as a TrustItem). Otherwise, return NULL.
// Note that this function throws if a "real" error is encountered.
//
Item TrustStore::findItem(Certificate *cert, Policy *policy)
{
	try {
		SecKeychainAttribute attrs[2];
		const CssmData &data = cert->data();
		attrs[0].tag = kSecTrustCertAttr;
		attrs[0].length = data.length();
		attrs[0].data = data.data();
		const CssmOid &policyOid = policy->oid();
		attrs[1].tag = kSecTrustPolicyAttr;
		attrs[1].length = policyOid.length();
		attrs[1].data = policyOid.data();
		SecKeychainAttributeList attrList = { 2, attrs };
		KCCursor cursor = globals().storageManager.createCursor(CSSM_DL_DB_RECORD_USER_TRUST, &attrList);
		Item item;
		if (cursor->next(item))
			return item;
		else
			return NULL;
	} catch (const CssmError &error) {
		if (error.cssmError() == CSSMERR_DL_INVALID_RECORDTYPE)
			return NULL;	// no trust schema, no records, no error
		throw;
	}
}


//
// Return the root certificate list.
// This list is cached.
//
CFArrayRef TrustStore::copyRootCertificates()
{
	if (!mRootsValid) {
		loadRootCertificates();
		mCFRoots = NULL;
	}
	if (!mCFRoots) {
		uint32 count = mRoots.size();
		debug("anchors", "building %ld CF-style anchor certificates", count);
		vector<SecCertificateRef> roots(count);
        for (uint32 n = 0; n < count; n++) {
            RefPointer<Certificate> cert = new Certificate(mRoots[n],
                CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER);
            roots[n] = gTypes().certificate.handle(*cert);
        }
        mCFRoots = CFArrayCreate(NULL, (const void **)&roots[0], count,
            &kCFTypeArrayCallBacks);
        for (uint32 n = 0; n < count; n++)
            CFRelease(roots[n]);	// undo CFArray's retain
	}
    CFRetain(mCFRoots);
    return mCFRoots;
}

void TrustStore::getCssmRootCertificates(CertGroup &rootCerts)
{
	if (!mRootsValid)
		loadRootCertificates();
	rootCerts = CertGroup(CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER, CSSM_CERTGROUP_DATA);
	rootCerts.blobCerts() = &mRoots[0];
	rootCerts.count() = mRoots.size();
}

void TrustStore::refreshRootCertificates()
{
	if (mRootsValid) {
		debug("anchors", "clearing %ld cached anchor certificates", mRoots.size());
		
		// throw out the CF version
		if (mCFRoots) {
			CFRelease(mCFRoots);
			mCFRoots = NULL;
		}
		
		// release cert memory
		mRootBytes.reset();
		mRoots.clear();
		
		// all pristine again
		mRootsValid = false;
	}
}


//
// Load root (anchor) certificates from disk
//
void TrustStore::loadRootCertificates()
{
	using namespace CssmClient;
	using namespace KeychainCore::Schema;
	
	// release previous cached data (if any)
	refreshRootCertificates();
	
	static const char anchorLibrary[] = "/System/Library/Keychains/X509Anchors";

	// open anchor database and formulate query (x509v3 certs)
	debug("anchors", "Loading anchors from %s", anchorLibrary);
	DL dl(gGuidAppleFileDL);
	Db db(dl, anchorLibrary);
	DbCursor search(db);
	search->recordType(CSSM_DL_DB_RECORD_X509_CERTIFICATE);
	search->conjunctive(CSSM_DB_OR);
#if 0	// if we ever need to support v1/v2 certificates...
	search->add(CSSM_DB_EQUAL, kX509CertificateCertType, UInt32(CSSM_CERT_X_509v1));
	search->add(CSSM_DB_EQUAL, kX509CertificateCertType, UInt32(CSSM_CERT_X_509v2));
	search->add(CSSM_DB_EQUAL, kX509CertificateCertType, UInt32(CSSM_CERT_X_509v3));
#endif

	// collect certificate data
	typedef list<CssmDataContainer> ContainerList;
	ContainerList::iterator last;
	ContainerList certs;
	for (;;) {
		DbUniqueRecord id;
		last = certs.insert(certs.end());
		if (!search->next(NULL, &*last, id))
			break;
	}

	// how many data bytes do we need?
	size_t size = 0;
	for (ContainerList::const_iterator it = certs.begin(); it != last; it++)
		size += it->length();
	mRootBytes.length(size);

	// fill CssmData vector while copying data bytes together
	mRoots.clear();
	uint8 *base = mRootBytes.data<uint8>();
	for (ContainerList::const_iterator it = certs.begin(); it != last; it++) {
		memcpy(base, it->data(), it->length());
		mRoots.push_back(CssmData(base, it->length()));
		base += it->length();
	}
	debug("anchors", "%ld anchors loaded", mRoots.size());

	mRootsValid = true;			// ready to roll
}


} // end namespace KeychainCore
} // end namespace Security
