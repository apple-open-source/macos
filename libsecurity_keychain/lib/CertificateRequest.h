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
// CertificateRequest.h
//
#ifndef _SECURITY_CERTIFICATEREQUEST_H_
#define _SECURITY_CERTIFICATEREQUEST_H_

#include <Security/SecCertificateRequest.h>
#include <security_utilities/seccfobject.h>
#include "SecCFTypes.h"
#include <security_utilities/alloc.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_client/tpclient.h>
#include <security_cdsa_client/clclient.h>
#include <security_utilities/debugging.h>
#include <CoreFoundation/CoreFoundation.h>

#define certReqDbg(args...) secdebug("certReq", ## args)

namespace Security
{

namespace KeychainCore
{

class CertificateRequest : public SecCFObject
{
	NOCOPY(CertificateRequest)
public:
	SECCFFUNCTIONS(CertificateRequest, SecCertificateRequestRef, errSecInvalidItemRef, gTypes().CertificateRequest)

    CertificateRequest(const CSSM_OID &policy,	
		CSSM_CERT_TYPE certificateType,
		CSSM_TP_AUTHORITY_REQUEST_TYPE requestType,
		SecKeyRef privateKeyItemRef,		// optional 
		SecKeyRef publicKeyItemRef,			// optional
		const SecCertificateRequestAttributeList *attributeList,
		/* 
		 * true when called from SecCertificateRequestCreate, cooking up a new
		 *      request from scratch
		 * false when called from SecCertificateFindRequest, recomnstructing
		 *      a request in progress
		 */
		bool isNew = true);

    virtual ~CertificateRequest() throw();

	void submit(
		sint32 *estimatedTime);
	void getResult(
		sint32			*estimatedTime,		// optional
		CssmData		&certData);

	/* 
	 * Obtain policy/error specific return data blob. We own the data, it's
	 * not copied. 
	 */
	void getReturnData(
		CssmData		&rtnData);
		
	CSSM_CERT_TYPE					certType()	{ return mCertType; }
	CSSM_TP_AUTHORITY_REQUEST_TYPE	reqType()	{ return mReqType; }

private:
	void submitDotMac(
		sint32			*estimatedTime);
	void getResultDotMac(
		sint32			*estimatedTime,		// optional
		CssmData		&certData);
	void postPendingRequest();
	
	/* preferences support */
	CFStringRef createUserKey();
	CFStringRef createPolicyKey();
	CFDictionaryRef getPolicyDictionary(
		CFDictionaryRef			prefsDict);
	CFDictionaryRef getUserDictionary(
		CFDictionaryRef			policyDict);
		
	/* 
	 * Preferences storage and retrieval.
	 * Both assume valid mPolicy and mUserName. storeResults stores the 
	 * specified data; retrieveResults retrieves whatever is found in the 
	 * prefs dictionary and restores to mRefId or mCert as appropriate.
	 */
	OSStatus storeResults(
		const CSSM_DATA		*refId,			// optional, for queued requests
		const CSSM_DATA		*certDat);		// optional, for immediate completion
	void retrieveResults();
	void removeResults();

	typedef enum {
		CRS_New = 0,		// created via SecCertificateRequestCreate
		CRS_Reconstructed,	// created via SecCertificateFindRequest
		CRS_HaveCert,		// completed request one way or another, have a good cert
		CRS_HaveRefId,		// submitted request, have RefId for later retrieval
		CRS_HaveOtherData	// submitted request, have other data in mRefId
	} CertReqState;
	
	Allocator						&mAlloc;
	CssmClient::TP					mTP;
	CssmClient::CL					mCL;
	CssmAutoData					mPolicy;	/* i.e., "CssmAutoOid" */
	CSSM_CERT_TYPE					mCertType;
	CSSM_TP_AUTHORITY_REQUEST_TYPE	mReqType;
	SecKeyRef						mPrivKey;
	SecKeyRef						mPubKey;
	sint32							mEstTime;
	CssmAutoData					mRefId;		/* returned from SubmitCredRequest() */
	CertReqState					mCertState;
	CssmAutoData					mCertData;
	
	/* 
	 * The incoming SecCertificateRequestAttributeList oid/value pairs
	 * map to these:
	 */
	CssmAutoData					mUserName;
	CssmAutoData					mPassword;	/* optional (lookup doesn't use it) */
	CssmAutoData					mHostName;	/* optional */
	CssmAutoData					mDomain;	/* optional */
	bool							mDoRenew;
	bool							mIsAsync;	/* true means no persistent state
												 * stored in user prefs; default 
												 * is false */
	Mutex							mMutex;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_CERTIFICATEREQUEST_H_
