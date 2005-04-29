/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
// tpclient - client interface to CSSM TPs and their operations
//
#include <security_cdsa_client/tpclient.h>

namespace Security {
namespace CssmClient {


//
// Manage TP attachments
//
TPImpl::TPImpl(const Guid &guid)
    : AttachmentImpl(guid, CSSM_SERVICE_TP), mUseCL(NULL), mUseCSP(NULL),
    mOwnCL(false), mOwnCSP(false)
{
}

TPImpl::TPImpl(const Module &module)
    : AttachmentImpl(module, CSSM_SERVICE_TP), mUseCL(NULL), mUseCSP(NULL),
    mOwnCL(false), mOwnCSP(false)
{
}

TPImpl::~TPImpl()
{
    if (mOwnCL)
        delete mUseCL;
    if (mOwnCSP)
        delete mUseCSP;
}


//
// Verify a CertGroup
//
void TPImpl::certGroupVerify(const CertGroup &certGroup,
    const TPVerifyContext &context,
    TPVerifyResult *result)
{
    setupCL();
    setupCSP();
    check(CSSM_TP_CertGroupVerify(handle(), (*mUseCL)->handle(), (*mUseCSP)->handle(),
        &certGroup, &context, result));
}


//
// Initialize auxiliary modules for operation
//
void TPImpl::setupCL()
{
    if (mUseCL == NULL) {
        secdebug("tpclient", "TP is auto-attaching supporting CL");
        mUseCL = new CL(gGuidAppleX509CL);
        mOwnCL = true;
    }
}

void TPImpl::setupCSP()
{
    if (mUseCSP == NULL) {
        secdebug("tpclient", "TP is auto-attaching supporting CSP");
        mUseCSP = new CSP(gGuidAppleCSP);
        mOwnCSP = true;
    }
}

void TPImpl::use(CL &cl)
{
    if (mOwnCL)
        delete mUseCL;
    mUseCL = &cl;
    mOwnCL = false;
}

void TPImpl::use(CSP &csp)
{
    if (mOwnCSP)
        delete mUseCSP;
    mUseCSP = &csp;
    mOwnCSP = false;
}

CL &TPImpl::usedCL()
{
    setupCL();
    return *mUseCL;
}

CSP &TPImpl::usedCSP()
{
    setupCSP();
    return *mUseCSP;
}


//
// A TPBuildVerifyContext
//
TPBuildVerifyContext::TPBuildVerifyContext(CSSM_TP_ACTION action, Allocator &alloc)
    : allocator(alloc)
{
    // clear out the PODs
    clearPod();
    mCallerAuth.clearPod();
	mDlDbList.clearPod();
    
    // set initial elements
    Action = action;
    callerAuthPtr(&mCallerAuth);
	mCallerAuth.dlDbList() = &mDlDbList;
}


}	// end namespace CssmClient
}	// end namespace Security

