/*
 * Copyright (c) 2002-2004,2012 Apple Inc. All Rights Reserved.
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
// PolicyCursor.cpp
//
#include <security_keychain/PolicyCursor.h>
#include <security_keychain/Policies.h>
#include <Security/oidsalg.h>
#include <security_cdsa_client/tpclient.h>

using namespace KeychainCore;
using namespace CssmClient;


//
// This preliminary implementation bypasses MDS and uses
// a fixed set of policies known to exist in the one known TP.
//
struct TheOneTP : public TP {
	TheOneTP() : TP(gGuidAppleX509TP) { }
};

static ModuleNexus<TheOneTP> theOneTP;
static const CssmOid *theOidList[] = {
	static_cast<const CssmOid *>(&CSSMOID_APPLE_ISIGN),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_X509_BASIC),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_SSL),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_SMIME),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_EAP),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_SW_UPDATE_SIGNING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_IP_SEC),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_ICHAT),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_RESOURCE_SIGN),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_PKINIT_CLIENT),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_PKINIT_SERVER),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_CODE_SIGNING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_PACKAGE_SIGNING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_REVOCATION_CRL),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_REVOCATION_OCSP),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_APPLEID_SHARING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_TIMESTAMPING),
	NULL	// sentinel
};


//
// Canonical Construction
//
PolicyCursor::PolicyCursor(const CSSM_OID* oid, const CSSM_DATA* value)
    : mOid(Allocator::standard()), mOidGiven(false), mMutex(Mutex::recursive)
{
    if (oid) {
        mOid = CssmOid::required(oid);
        mOidGiven = true;
    }
    mSearchPos = 0;
}


//
// Destroy
//
PolicyCursor::~PolicyCursor() throw()
{
}


//
// Crank the iterator
//
bool PolicyCursor::next(SecPointer<Policy> &policy)
{
	StLock<Mutex>_(mMutex);

    while (theOidList[mSearchPos]) {
        if (mOidGiven && mOid != *theOidList[mSearchPos]) {
            mSearchPos++;
            continue;	// no oid match
        }
        // ignoring mValue - not used by current TP
        policy = new Policy(theOneTP(), *theOidList[mSearchPos]);
        mSearchPos++;	// advance cursor
        return true;	// return next match
    }
    return false;	// end of table, no more matches
}

//
// Return a new policy instance for an OID, outside of cursor iteration
//
void PolicyCursor::policy(const CSSM_OID* oid, SecPointer<Policy> &policy)
{
	const CssmOid *policyOid = static_cast<const CssmOid *>(oid);
	policy = new Policy(theOneTP(), *policyOid);
}

