/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// signclient - client interface to CSSM sign/verify contexts
//
#include <security_cdsa_client/signclient.h>

using namespace CssmClient;


//
// Common features of signing and verify contexts
//
void SigningContext::activate()
{
	if (!mActive)
	{
		check(CSSM_CSP_CreateSignatureContext(attachment()->handle(), mAlgorithm,
			  cred(), mKey, &mHandle));
		mActive = true;
	}
}


//
// Signing
//
void Sign::sign(const CssmData *data, uint32 count, CssmData &signature)
{
	unstaged();
	check(CSSM_SignData(handle(), data, count, mSignOnly, &signature));
}

void Sign::init()
{
	check(CSSM_SignDataInit(handle()));
	mStaged = true;
}

void Sign::sign(const CssmData *data, uint32 count)
{
	staged();
	check(CSSM_SignDataUpdate(handle(), data, count));
}

void Sign::operator () (CssmData &signature)
{
	staged();
	check(CSSM_SignDataFinal(handle(), &signature));
	mStaged = false;
}


//
// Verifying
//
void Verify::verify(const CssmData *data, uint32 count, const CssmData &signature)
{
	unstaged();
	check(CSSM_VerifyData(handle(), data, count, mSignOnly, &signature));
}

void Verify::init()
{
	check(CSSM_VerifyDataInit(handle()));
	mStaged = true;
}

void Verify::verify(const CssmData *data, uint32 count)
{
	staged();
	check(CSSM_VerifyDataUpdate(handle(), data, count));
}

void Verify::operator () (const CssmData &signature)
{
	staged();
	check(CSSM_VerifyDataFinal(handle(), &signature));
	mStaged = false;
}
