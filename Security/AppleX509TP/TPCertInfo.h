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


/*
 * TPCertInfo.h - TP's private certificate info and cert group classes
 *
 * Written 10/23/2000 by Doug Mitchell.
 */
 
#ifndef	_TP_CERT_INFO_H_
#define _TP_CERT_INFO_H_

#include <Security/cssmtype.h>
#include <Security/utilities.h>
#include <Security/cssmalloc.h>

/*
 * Class representing one certificate. The raw cert data usually comes from
 * a client (via incoming cert groups in CertGroupConstruct() and CertGroupVerify());
 * In this case, we don't own the raw data and don't copy or free it. Caller can 
 * optionally specify that we copy (and own and eventnually free) the raw cert data. 
 * The constructor throws on any error (bad cert data); subsequent to successful 
 * construction, no CSSM errors are thrown and it's guaranteed that the cert is
 * basically good and successfully cached in the CL, and that we have a locally
 * cached subject and issuer name (in normalized encoded format). 
 */ 
class TPCertInfo
{
public:
	/* 
	 * No default constructor - this is the only way.
	 * This caches the cert and fetches subjectName and issuerName
	 * to ensure the incoming certData is well-constructed.
	 */
	TPCertInfo(
		const CSSM_DATA		*certData,
		CSSM_CL_HANDLE		clHand,
		bool				copyCertData = false);	// true: we copy, we free
													// false - caller owns
		
	/* frees mSubjectName, mIssuerName, mCacheHand via mClHand */
	~TPCertInfo();
	
	/* 
	 * Fetch arbitrary field from cached cert.
	 * Only should be used when caller is sure there is either zero or one
	 * of the requested fields present in the cert.
	 */
	CSSM_RETURN fetchField(
		const CSSM_OID	*fieldOid,
		CSSM_DATA_PTR	*fieldData);			// mallocd by CL and RETURNED

	/* free arbitrary field obtained from fetchField() */
	CSSM_RETURN freeField( 
		const CSSM_OID	*fieldOid,
		CSSM_DATA_PTR	fieldData);
		
	/* accessors */
	CSSM_CL_HANDLE	clHand();
	CSSM_HANDLE		cacheHand();
	const CSSM_DATA *certData();
	const CSSM_DATA *subjectName();
	const CSSM_DATA *issuerName();				

	bool isSelfSigned();						// i.e., subject == issuer
	
	/* 
	 * Verify validity (not before/after). Returns 
	 * 		CSSMERR_TP_CERT_NOT_VALID_YET
	 *		CSSMERR_TP_CERT_EXPIRED
	 *		CSSM_OK
	 *		CSSMERR_TP_INVALID_CERT_POINTER, other "bogus cert" errors
	 */
	CSSM_RETURN isCurrent(
		CSSM_BOOL		allowExpired = CSSM_FALSE);
		
private:
	CSSM_DATA				*mCertData;			// always valid
	bool					mWeOwnTheData;		// if true, we have to free mCertData
	CSSM_CL_HANDLE			mClHand;			// always valid
	CSSM_HANDLE				mCacheHand;			// always valid
	CSSM_DATA_PTR			mSubjectName;		// always valid
	CSSM_DATA_PTR			mIssuerName;		// always valid
	
	void releaseResources();
	
	/* other field accessors here */
};

/*
 * TP's private Cert Group class. Provides a list of TPCertInfo pointers, to which 
 * caller can append additional elements, access an element at an arbitrary position, 
 * and remover an element at an arbitrrary position. 
 */
class TPCertGroup
{
public:
	/*
	 * No default constructor - use this to cook up an instance with 
	 * space for numCerts TPCertInfos. 
	 */
	TPCertGroup(
		CssmAllocator		&alloc,
		unsigned			numCerts);
	
	/*
	 * Deletes all TPCertInfo's.
	 */
	~TPCertGroup();
	
	/* add/remove/access TPTCertInfo's. */
	void appendCert(
		TPCertInfo			*certInfo);			// appends to end of mCertInfo
	TPCertInfo *certAtIndex(
		unsigned			index);
	TPCertInfo *removeCertAtIndex(
		unsigned			index);				// doesn't delete the cert, just 
												// removes it from our list
	unsigned numCerts();						// how many do we have? 
	
	/* 
	 * Convenience accessors for first and last cert, only valid when we have
	 * at least one cert.
	 */
	TPCertInfo 
		*firstCert();
	TPCertInfo
		*lastCert();
		
	/* build a CSSM_CERTGROUP corresponding with our mCertInfo */
	CSSM_CERTGROUP_PTR		
		buildCssmCertGroup();
	
private:
	CssmAllocator			&mAlloc;
	TPCertInfo				**mCertInfo;		// just an array of pointers
	unsigned				mNumCerts;			// valid certs in certInfo
	unsigned				mSizeofCertInfo;	// mallocd space in certInfo
};
#endif	/* _TP_CERT_INFO_H_ */
