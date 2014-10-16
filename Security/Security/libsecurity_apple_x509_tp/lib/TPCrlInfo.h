/*
 * Copyright (c) 2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * TPCrlInfo.h - TP's private CRL and CRL group classes
 *
 */
 
#ifndef	_TP_CRL_INFO_H_
#define _TP_CRL_INFO_H_

#include <Security/cssmtype.h>
#include <security_utilities/alloc.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include "TPCertInfo.h"
#include "tpCrlVerify.h"

/*
 * Verification state of a TPCrlInfo. Verification refers to the process
 * of cert chain validation from the CRL to a trusted root. Since this
 * is a rather heavyweight operation, this is done on demand, when a given
 * CRL is "believed to be" the appropriate one for a given cert. It
 * is separate from not before/after verification, which is performed
 * on the fly as needed.
 */
typedef enum {
	CVS_Unknown,		// initial default state
	CVS_Good,			// known good
	CVS_Bad				// known bad
} TPCrlVerifyState;

/*
 * Indicates where a particular CRL came from. Currently only used
 * in the tpCrlVerify module.
 */
typedef enum {
	CFW_Nowhere,		// default, never returned
	CFW_InGroup,		// from incoming TPCrlGroup
	CFW_DlDb,			// verifyContext.dbList
	CFW_LocalCache,		// tpGlobalCrlCache
	CFW_Net,			// tpFetchCrlFromNet
	/* probably others */
} TPCrlFromWhere;


/*
 * Class representing one CRL. The raw CRL data usually comes from
 * a client (via incoming CSSM_TP_VERIFY_CONTEXT.Crls); in this case, we 
 * don't own the raw data and don't copy or free it. Caller can 
 * optionally specify that we copy (and own and eventually free) the raw cert data. 
 * Currently this is only done when we find a CRL in a DlDb. The constructor throws 
 * on any error (bad CRL data); subsequent to successful construction, no CSSM 
 * errors are thrown and it's guaranteed that the CRL is basically readable and 
 * successfully cached in the CL, and that we have a locally cached 
 * CSSM_X509_SIGNED_CRL and issuer name (in normalized encoded format). 
 */ 
class TPCrlInfo : public TPClItemInfo
{
	NOCOPY(TPCrlInfo)
public:
	/* 
	 * No default constructor - this is the only way.
	 */
	TPCrlInfo(
		CSSM_CL_HANDLE		clHand,
		CSSM_CSP_HANDLE		cspHand,
		const CSSM_DATA		*crlData,
		TPItemCopy			copyCrlData,	
		const char 			*verifyTime);	// NULL ==> time = right now
		
	/* frees mIssuerName, mCacheHand, mX509Crl via mClHand */
	~TPCrlInfo();
	
	/* 
	 * The heavyweight "perform full verification" op.
	 * If doCrlVerify is true, we'll do an eventually recursive
	 * CRL verification test on the cert group we construct
	 * here to verify the CRL in question. This recursive
	 * verify is also done if the CRL is an indirect CRL.
	 * Currently, the doCrlVerifyFlag will be set false in the
	 * normal case of verifying a cert chain; in that case the 
	 * various certs needed to verify the CRL are assumed to 
	 * be a subset of the cert chain being verified, and CRL
	 * verification of that cert chain is being performed 
	 * elsewhere. The caller would set doCrlVerify true when 
	 * the top-level op is simply a CRL verify.
	 */
	CSSM_RETURN verifyWithContext(
		TPVerifyContext		&tpVerifyContext,
		TPCertInfo			*forCert,			// optional
		bool				doCrlVerify = false);	
	
	/*
 	 * Wrapper for verifyWithContext for use when evaluating a CRL
  	 * "now" instead of at the time in TPVerifyContext.verifyTime.
	 */
	CSSM_RETURN verifyWithContextNow(
		TPVerifyContext		&tpVerifyContext,
		TPCertInfo			*forCert,			// optional
		bool				doCrlVerify = false);	
	
	/*
	 * Do I have the same issuer as the specified subject cert? 
	 * Returns true if so.
	 */
	bool hasSameIssuer(
		const TPCertInfo		&subject);
		
	/*
	 * Determine if specified cert has been revoked as of the
	 * provided time; a NULL timestring indicates "now".
	 * Assumes that the current CRL has been fully verified.
	 */
	CSSM_RETURN isCertRevoked(
		TPCertInfo 				&subjectCert,
		CSSM_TIMESTRING 		verifyTime);
		
	/* accessors */
	const CSSM_X509_SIGNED_CRL *x509Crl()		{ return mX509Crl; }
	TPCrlVerifyState			verifyState() 	{ return mVerifyState; }
	
	const CSSM_DATA				*uri()			{ return &mUri; }
	void 						uri(const CSSM_DATA &uri); 
	
	/* 
	 * Ref count info maintained by caller (currently only in 
	 * tpCrlVfy.cpp's global cache module).
	 */
	int 					mRefCount;
	
	/* used only by tpCrlVerify */
	TPCrlFromWhere			mFromWhere;
	
	
private:
	CSSM_X509_SIGNED_CRL	*mX509Crl;
	CSSM_DATA_PTR			mCrlFieldToFree;
	TPCrlVerifyState		mVerifyState;
	CSSM_RETURN				mVerifyError;		// only if mVerifyState = CVS_Bad
	CSSM_DATA				mUri;				// if fetched from net
	
	void releaseResources();
	CSSM_RETURN parseExtensions(
		TPVerifyContext				&tpVerifyContext,
		bool						isPerEntry,
		uint32						entryIndex,		// if isPerEntry
		const CSSM_X509_EXTENSIONS	&extens,
		TPCertInfo					*forCert,		// optional
		bool						&isIndirectCrl);// RETURNED
	
};

/*
 * TP's private CRL Group class.  
 */
class TPCrlGroup
{
	NOCOPY(TPCrlGroup)
public:
	/* construct empty CRL group */
	TPCrlGroup(
		Allocator				&alloc,
		TPGroupOwner			whoOwns);		// if TGO_Group, we delete
	
	/*
	 * Construct from unordered, untrusted CSSM_CRLGROUP. Resulting
	 * TPCrlInfos are more or less in the same order as the incoming
	 * CRLs, though incoming CRLs are discarded if they don't parse.
	 * No verification of any sort is performed. 
	 */
	TPCrlGroup(
		const CSSM_CRLGROUP 	*cssmCrlGroup,		// optional
		CSSM_CL_HANDLE 			clHand,
		CSSM_CSP_HANDLE 		cspHand,
		Allocator				&alloc,
		const char				*cssmTimeStr,		// may be NULL
		TPGroupOwner			whoOwns);	
	
	/*
	 * Deletes all TPCrlInfo's.
	 */
	~TPCrlGroup();
	
	/* add/remove/access TPCrlInfo's. */
	void appendCrl(
		TPCrlInfo			&crlInfo);			// appends to end of mCertInfo
	TPCrlInfo *crlAtIndex(
		unsigned			index);
	TPCrlInfo &removeCrlAtIndex(
		unsigned			index);				// doesn't delete the cert, just 
												// removes it from our list
	void removeCrl(
		TPCrlInfo			&crlInfo);			// ditto
	
	/* 
	 * Convenience accessors for first and last CRL, only valid when we have
	 * at least one cert.
	 */
	TPCrlInfo *firstCrl();
	TPCrlInfo *lastCrl();
		
	/* 
	 * Find a CRL whose issuer matches specified subject cert.
	 * Returned CRL has not necessarily been verified.
	 */
	TPCrlInfo *findCrlForCert(
		TPCertInfo			&subject);
		
	Allocator &alloc() 							{ return mAlloc; }
	unsigned numCrls()								{ return mNumCrls; }
	
private:
	Allocator				&mAlloc;
	TPCrlInfo				**mCrlInfo;			// just an array of pointers
	unsigned				mNumCrls;			// valid certs in certInfo
	unsigned				mSizeofCrlInfo;		// mallocd space in certInfo
	TPGroupOwner			mWhoOwns;			// if TGO_Group, we delete CRLs 
												//    upon destruction
};
#endif	/* _TP_CRL_INFO_H_ */

