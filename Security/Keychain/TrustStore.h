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
#ifndef _SECURITY_TRUSTSTORE_H_
#define _SECURITY_TRUSTSTORE_H_

#include <Security/utilities.h>
#include <Security/Certificate.h>
#include <Security/Policies.h>
#include <Security/SecTrust.h>
#include <Security/TrustItem.h>


namespace Security {
namespace KeychainCore {


//
// A TrustStore object mediates access to "user trust" information stored
// for a user (usually in her keychains).
// For lack of a better home, access to the default anchor certificate
// list is also provided here.
//
class TrustStore {
	NOCOPY(TrustStore)
public:
    TrustStore(CssmAllocator &alloc = CssmAllocator::standard());
    virtual ~TrustStore();
	
	CssmAllocator &allocator;

	// set/get user trust for a certificate and policy
    SecTrustUserSetting find(Certificate *cert, Policy *policy);
    void assign(Certificate *cert, Policy *policy, SecTrustUserSetting assignment);
    
	// get access to the default root anchor certificates for X509
    CFArrayRef copyRootCertificates();
	void getCssmRootCertificates(CertGroup &roots);
	void refreshRootCertificates();
	
	typedef UserTrustItem::TrustData TrustData;
	
protected:
	Item findItem(Certificate *cert, Policy *policy);
	void loadRootCertificates();

private:
	bool mRootsValid;			// roots have been loaded from disk
	vector<CssmData> mRoots;	// array of CssmDatas to certificate datas
	CssmAutoData mRootBytes;	// certificate data blobs (bunched up)
    CFRef<CFArrayRef> mCFRoots;	// mRoots as CFArray<SecCertificate>
};

} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_TRUSTSTORE_H_
