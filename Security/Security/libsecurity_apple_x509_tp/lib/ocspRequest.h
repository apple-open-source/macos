/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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

/*
 * ocspRequest.h - OCSP Request class
 */
 
#ifndef	_OCSP_REQUEST_H_
#define _OCSP_REQUEST_H_

#include "TPCertInfo.h"
#include <Security/SecAsn1Coder.h>
#include <Security/ocspTemplates.h>

class OCSPClientCertID;

class OCSPRequest
{
	NOCOPY(OCSPRequest)
public:
	/*
	 * The only constructor. Subject and issuer must remain valid for the 
	 * lifetime of this object (they are not refcounted). 
	 */
	OCSPRequest(
		TPCertInfo		&subject,
		TPCertInfo		&issuer,
		bool			genNonce);
		
	~OCSPRequest();
	
	/* 
	 * Obtain encoded OCSP request suitable for posting to responder.
	 * This object owns and maintains the memory.
	 */
	const CSSM_DATA *encode();

	/* 
	 * Obtain this request's nonce (which we randomly generate at encode() time),
	 * This object owns and maintains the memory. Result is NULL} if we
	 * didn't generate a nonce. 
	 */
	const CSSM_DATA *nonce();
			
	/* 
	 * Obtain this request's CertID. Used to look up matching SingleResponse
	 * in the OCSPResponse.
	 */
	OCSPClientCertID	*certID();
	
private:
	SecAsn1CoderRef		mCoder;
	TPCertInfo			&mSubject;
	TPCertInfo			&mIssuer;
	bool				mGenNonce;
	CSSM_DATA			mNonce;
	CSSM_DATA			mEncoded;	/* lazily evaluated */
	OCSPClientCertID	*mCertID;	/* calculated during encode() */
	
};

#endif	/* _OCSP_REQUEST_H_ */

