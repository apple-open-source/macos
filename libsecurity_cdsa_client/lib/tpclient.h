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
#ifndef _H_CDSA_CLIENT_TPCLIENT
#define _H_CDSA_CLIENT_TPCLIENT  1

#include <security_cdsa_client/cssmclient.h>
#include <security_cdsa_client/clclient.h>
#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_utilities/cssmtrust.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_cdsa_utilities/cssmdata.h>


namespace Security {
namespace CssmClient {


//
// A TP attachment
//
class TPImpl : public AttachmentImpl
{
public:
	TPImpl(const Guid &guid);
	TPImpl(const Module &module);
	virtual ~TPImpl();
    
public:
    // the CL and CSP used with many TP operations is usually
    // pretty stable. The system may even figure them out
    // automatically in the future.
    void use(CL &cl);
    void use(CSP &csp);
    CL &usedCL();
    CSP &usedCSP();

public:
    void certGroupVerify(const CertGroup &certGroup, const TPVerifyContext &context,
        TPVerifyResult *result);

private:
    void setupCL();				// setup mUseCL
    void setupCSP();			// setup mUseCSP

private:
    CL *mUseCL;				// use this CL for TP operation
    CSP *mUseCSP;			// use this CSP for TP operation
    bool mOwnCL, mOwnCSP;	// whether we've made our own
};


class TP : public Attachment
{
public:
	typedef TPImpl Impl;

	explicit TP(Impl *impl) : Attachment(impl) {}
	TP(const Guid &guid) : Attachment(new Impl(guid)) {}
	TP(const Module &module) : Attachment(new Impl(module)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// A self-building TPVerifyContext.
// This is a TPVerifyContext, but it's NOT A PODWRAPPER (it's larger).
//
// NOTE: This is not a client-side object.
//
class TPBuildVerifyContext : public TPVerifyContext {
public:
    TPBuildVerifyContext(CSSM_TP_ACTION action = CSSM_TP_ACTION_DEFAULT,
        Allocator &alloc = Allocator::standard());
    
    Allocator &allocator;
    
private:
    TPCallerAuth mCallerAuth;
    PolicyInfo mPolicyInfo;
	CssmDlDbList mDlDbList;
};


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_CLCLIENT
