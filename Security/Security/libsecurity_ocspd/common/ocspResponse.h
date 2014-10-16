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
 * ocspResponse.h - OCSP Response class
 */
 
#ifndef	_OCSP_RESPONSE_H_
#define _OCSP_RESPONSE_H_

#include <security_ocspd/ocspExtensions.h>
#include <Security/ocspTemplates.h>
#include <Security/certextensions.h>
#include <Security/SecAsn1Coder.h>
#include <CoreFoundation/CoreFoundation.h>

/* used to indicate "I don't know the CRLReason" */
#define CrlReason_NONE		((CE_CrlReason)-1)

/*
 * CertIDs can be represented differently by two peers even though they refer to 
 * the same cert. Client can use SHA1 hash and server can use MD5, for example. 
 * So all of our code which creates a CertID based on known, existing subject and
 * issuer certs uses one of these "smart" certIDs which can encode itself and also 
 * compare against any form of existing SecAsn1OCSPCertID.
 */
class OCSPClientCertID
{
	NOCOPY(OCSPClientCertID);
public:
	/*
	 * Basic constructor given issuer's public key and name, and subject's
	 * serial number.
	 */
	OCSPClientCertID(
		const CSSM_DATA			&issuerName,
		const CSSM_DATA			&issuerPubKey,
		const CSSM_DATA			&subjectSerial);
		
	~OCSPClientCertID();
	
	/*
	 * DER encode.
	 */
	const CSSM_DATA *encode();
		
	/*
	 * Does this object refer to the same cert as specified SecAsn1OCSPCertID?
	 * This is the main purpose of this class's existence; this function works 
	 * even if specified SecAsn1OCSPCertID uses a different hash algorithm
	 * than we do, since we keep copies of our basic components. 
	 *
	 * Returns true if compare successful.
	 */
	bool compareToExist(
		const SecAsn1OCSPCertID	&exist);
		
	/* 
	 * Convenience function, like compareToExist, with a raw encoded CertID.
	 */
	bool compareToExist( 
		const CSSM_DATA	&exist);
		
private:
	CSSM_DATA mIssuerName;
	CSSM_DATA mIssuerPubKey;
	CSSM_DATA mSubjectSerial;
	CSSM_DATA mEncoded;
};

/*
 * Object representing one SecAsn1OCSPSingleResponse, i.e., the portion of 
 * an OCSP response associated with a single CertID. These are created and
 * vended solely by an OCSPResponse object. The client which gets them from
 * an OCSPResponse (via singleResponse()) must delete the object when finished
 * with it. 
 */
class OCSPSingleResponse
{
	NOCOPY(OCSPSingleResponse);
public:
	/* only OCSPResponse creates these */
	~OCSPSingleResponse();
	friend class OCSPResponse;
protected:
	
	OCSPSingleResponse(
		SecAsn1OCSPSingleResponse	*resp);
public:
		SecAsn1OCSPCertStatusTag	certStatus()	{ return mCertStatus; }
		CFAbsoluteTime				thisUpdate()	{ return mThisUpdate; }
		CFAbsoluteTime				nextUpdate()	{ return mNextUpdate; }
		CFAbsoluteTime				revokedTime()	{ return mRevokedTime; }
		CE_CrlReason				crlReason()		{ return mCrlReason; }
		
		/* Extension accessors - all are optional */
		
		/* CRL Reference */
		const CSSM_DATA				*crlUrl();
		const CSSM_DATA				*crlNum();
		CFAbsoluteTime				crlTime();			/* may be NULL_TIME */
		
		/* archive cutoff */
		CFAbsoluteTime				archiveCutoff();
		
		/* service locator not implemented yet */
private:
		SecAsn1CoderRef				mCoder;
		SecAsn1OCSPCertStatusTag	mCertStatus;
		CFAbsoluteTime				mThisUpdate;
		CFAbsoluteTime				mNextUpdate;		/* may be NULL_TIME */
		CFAbsoluteTime				mRevokedTime;		/* != NULL_TIME for CS_Revoked */
		CE_CrlReason				mCrlReason;		
		OCSPExtensions				*mExtensions;
};

/*
 * OCSPResponse maintains its own temporal validity status based on the values of 
 * all of the enclosed SingleResponses' thisUpdate and (optional) nextUpdate
 * fields, in addition to a default time-to-live (TTL) value passed to
 * OCSPResponse's constructor.
 *
 * First, all of the thisUpdate fields are checked during OCSPResponse's constructor. 
 * if any of these are later than the current time, the entire response is considered
 * invalid and the constructor throws a CssmError(CSSMERR_APPLETP_OCSP_BAD_RESPONSE). 
 * Subsequent to construction, all thisUpdate fields are ignored. 
 *
 * The NextUpdate times are handled as follows. 
 *
 * 1. An OCSPResponse's latestNextUpdate is defined as the latest of all of the 
 *    nextUpdate fields in its SingleResponses. This is evaluated during construction. 
 *
 * 2. An OCSPResponse's latestNextUpdate is NULL_TIME if none of its SingleResponses 
 *    contain any nextUpdate (this field is in fact optional). 
 *
 * 3. The caller of OCSPResponse's constructor passes in a default time-to-live 
 *    (TTL) in seconds; call this defaultTTL. Call the time at which the 
 *    constructor is called, PLUS defaultTTL, "defaultExpire".
 * 
 * -- If the OCSPResponse's latestNextUpdate is NULL_TIME then expireTime() returns
 *    defaultExpire.
 *
 * -- Otherwise, expireTime() returns the lesser of (latestNextUpdate, 
 *    defaultExpire).
 *
 * Note that this mechanism is used by both the TP's in-core cache and ocspd's
 * on-disk cache; the two have different default TTLs values but the mechanism
 * for calcuating expireTime() is identical. 
 */
class OCSPResponse
{
	NOCOPY(OCSPResponse)
public:
	/* only constructor, from DER encoded data */
	OCSPResponse(
		const CSSM_DATA &resp,
		CFTimeInterval defaultTTL);		// default time-to-live in seconds
		
	~OCSPResponse();
	
	/* 
	 * Info obtained during decode (which is done immediately during constructor) 
	 */
	SecAsn1OCSPResponseStatus	responseStatus();
	const CSSM_DATA				*nonce();			/* NULL means not present */
	CFAbsoluteTime				producedAt();		/* should always work */
	CSSM_RETURN					sigStatus();
	uint32						numSignerCerts();
	const CSSM_DATA				*signerCert(uint32 dex);
	
	/* 
	 * Obtain a OCSPSingleResponse for a given CertID. 
	 */
	OCSPSingleResponse			*singleResponseFor(OCSPClientCertID &certID);
	OCSPSingleResponse			*singleResponseFor(const CSSM_DATA &matchCertID);
	
	CFAbsoluteTime				expireTime()		{ return mExpireTime; }

	/*
	 * Access to decoded data.
	 */
	const SecAsn1OCSPResponseData	&responseData()		{ return mResponseData; }
	const SecAsn1OCSPBasicResponse	&basicResponse()	{ return mBasicResponse; }
	const SecAsn1OCSPResponderID	&responderID()		{ return mResponderId; }
	SecAsn1OCSPResponderIDTag		responderIDTag()	{ return mResponderIdTag; }

	const CSSM_DATA					*encResponderName();
	
private:
	bool						calculateValidity(CFTimeInterval defaultTTL);
	
	SecAsn1CoderRef				mCoder;
	CFAbsoluteTime				mLatestNextUpdate;
	CFAbsoluteTime				mExpireTime;
	CSSM_DATA					mEncResponderName;	// encoded ResponderId.byName, 
													// if responder is in that format,
													// lazily evaluated
	/* 
	 * Fields we decode - all in mCoder's memory space 
	 */
	SecAsn1OCSPResponse			mTopResp;
	SecAsn1OCSPBasicResponse	mBasicResponse;
	SecAsn1OCSPResponseData		mResponseData;
	SecAsn1OCSPResponderID		mResponderId;		// we have to decode
	SecAsn1OCSPResponderIDTag	mResponderIdTag;	// IDs previous field
	OCSPExtensions				*mExtensions;
};	
#endif	/* _OCSP_RESPONSE_H_ */

