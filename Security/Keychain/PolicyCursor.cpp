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
// PolicyCursor.cpp
//
#include <Security/PolicyCursor.h>
#include <Security/Policies.h>
#include <Security/oidsalg.h>
#include <Security/tpclient.h>

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
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_REVOCATION_CRL),
    NULL	// sentinel
};


//
// Canonical Construction
//
PolicyCursor::PolicyCursor(const CSSM_OID* oid, const CSSM_DATA* value)
    : mOid(CssmAllocator::standard()), mOidGiven(false)
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
