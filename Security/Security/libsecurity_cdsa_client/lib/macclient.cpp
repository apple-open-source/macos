/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// macclient - client interface to CSSM sign/verify mac contexts
//
#include <security_cdsa_client/macclient.h>

using namespace CssmClient;


//
// Common features of signing and verify mac contexts
//
void MacContext::activate()
{
    {
        StLock<Mutex> _(mActivateMutex);
        if (!mActive) 
        {
            check(CSSM_CSP_CreateMacContext(attachment()->handle(), mAlgorithm,
                  mKey, &mHandle));
            mActive = true;
        }
    }

    if (cred())
        cred(cred());		// install explicitly
}


//
// Signing
//
void GenerateMac::sign(const CssmData *data, uint32 count, CssmData &mac)
{
	unstaged();
	check(CSSM_GenerateMac(handle(), data, count, &mac));
}

void GenerateMac::init()
{
	check(CSSM_GenerateMacInit(handle()));
	mStaged = true;
}

void GenerateMac::sign(const CssmData *data, uint32 count)
{
	staged();
	check(CSSM_GenerateMacUpdate(handle(), data, count));
}

void GenerateMac::operator () (CssmData &mac)
{
	staged();
	check(CSSM_GenerateMacFinal(handle(), &mac));
	mStaged = false;
}


//
// Verifying
//
void VerifyMac::verify(const CssmData *data, uint32 count, const CssmData &mac)
{
	unstaged();
	check(CSSM_VerifyMac(handle(), data, count, &mac));
}

void VerifyMac::init()
{
	check(CSSM_VerifyMacInit(handle()));
	mStaged = true;
}

void VerifyMac::verify(const CssmData *data, uint32 count)
{
	staged();
	check(CSSM_VerifyMacUpdate(handle(), data, count));
}

void VerifyMac::operator () (const CssmData &mac)
{
	staged();
	check(CSSM_VerifyMacFinal(handle(), &mac));
	mStaged = false;
}
