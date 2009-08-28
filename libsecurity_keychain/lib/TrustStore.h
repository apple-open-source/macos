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
// TrustStore.h - Abstract interface to permanent user trust assignments
//
#ifndef _SECURITY_TRUSTSTORE_H_
#define _SECURITY_TRUSTSTORE_H_

#include <security_keychain/Certificate.h>
#include <security_keychain/Policies.h>
#include <Security/SecTrust.h>
#include <security_keychain/TrustItem.h>


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
    TrustStore(Allocator &alloc = Allocator::standard());
    virtual ~TrustStore();
	
	Allocator &allocator;

	// set/get user trust for a certificate and policy
    SecTrustUserSetting find(Certificate *cert, Policy *policy, 
		StorageManager::KeychainList &keychainList);
    void assign(Certificate *cert, Policy *policy, SecTrustUserSetting assignment);
    
	void getCssmRootCertificates(CertGroup &roots);
	
	typedef UserTrustItem::TrustData TrustData;
	
protected:
	Item findItem(Certificate *cert, Policy *policy, 
		StorageManager::KeychainList &keychainList);
	void loadRootCertificates();

private:
	bool mRootsValid;			// roots have been loaded from disk
	vector<CssmData> mRoots;	// array of CssmDatas to certificate datas
	CssmAutoData mRootBytes;	// certificate data blobs (bunched up)
    CFRef<CFArrayRef> mCFRoots;	// mRoots as CFArray<SecCertificate>
	Mutex mMutex;
};

} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_TRUSTSTORE_H_
