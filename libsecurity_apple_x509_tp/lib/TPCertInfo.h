/*
 * Copyright (c) 2000-2011 Apple Inc. All Rights Reserved.
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
 */
 
#ifndef	_TP_CERT_INFO_H_
#define _TP_CERT_INFO_H_

#include <Security/cssm.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettings.h>
#include <security_utilities/alloc.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <CoreFoundation/CFDate.h>

/* protects TP-wide access to time() and gmtime() */
extern ModuleNexus<Mutex> tpTimeLock;

/* 
 * Prototypes for functions which are isomorphic between certs and CRLs at the
 * CL API.
 */
typedef CSSM_RETURN (*clGetFirstFieldFcn)(
	CSSM_CL_HANDLE CLHandle,
	CSSM_HANDLE ItemHandle,			// cached cert or CRL
	const CSSM_OID *ItemField,
	CSSM_HANDLE_PTR ResultsHandle,
	uint32 *NumberOfMatchedFields,
	CSSM_DATA_PTR *Value);
typedef CSSM_RETURN (*clAbortQueryFcn)(
	CSSM_CL_HANDLE CLHandle,
	CSSM_HANDLE ResultsHandle);		// from clGetFirstFieldFcn
typedef CSSM_RETURN (*clCacheItemFcn)(
	CSSM_CL_HANDLE CLHandle,
	const CSSM_DATA *Item,			// raw cert or CRL
	CSSM_HANDLE_PTR CertHandle);
typedef CSSM_RETURN (*clAbortCacheFcn)(
	CSSM_CL_HANDLE CLHandle,
	CSSM_HANDLE ItemHandle);		// from clCacheItemFcn
typedef CSSM_RETURN (*clItemVfyFcn)(
	CSSM_CL_HANDLE CLHandle,
	CSSM_CC_HANDLE CCHandle,
	const CSSM_DATA *CrlOrCertToBeVerified,
	const CSSM_DATA *SignerCert,
	const CSSM_FIELD *VerifyScope,
	uint32 ScopeSize);

typedef struct {
	/* CL/cert-specific functions */
	clGetFirstFieldFcn	getField;
	clAbortQueryFcn		abortQuery;
	clCacheItemFcn		cacheItem;
	clAbortCacheFcn		abortCache;
	clItemVfyFcn		itemVerify;
	/* CL/cert-specific OIDs */
	const CSSM_OID		*notBeforeOid;
	const CSSM_OID		*notAfterOid;
	/* CL/cert specific errors */
	CSSM_RETURN			invalidItemRtn;	// CSSMERR_TP_INVALID_{CERT,CRL}_POINTER
	CSSM_RETURN			expiredRtn;		
	CSSM_RETURN			notValidYetRtn;
} TPClItemCalls;

class TPCertInfo;

/*
 * On construction of a TPClItemInfo, specifies whether or not to 
 * copy the incoming item data (in which we free it upon destruction)
 * or to use caller's data as is (in which case the caller maintains 
 * the data).
 */
typedef enum {
	TIC_None = 0,		// never used
	TIC_NoCopy,			// caller maintains
	TIC_CopyData		// we copy and free
} TPItemCopy;
 
/*
 * State of a cert's mIsRoot flag. We do signature self-verify on demand.
 */
typedef enum {
	TRS_Unknown,		// initial state
	TRS_NamesMatch,		// subject == issuer, but no sig verify yet
	TRS_NotRoot,		// subject != issuer, OR sig verify failed
	TRS_IsRoot			// it's a root
} TPRootState;

/*
 * Base class for TPCertInfo and TPCrlInfo. Encapsulates caching of 
 * an entity within the CL, field lookup/free, and signature verify,
 * all of which use similar functions at the CL API.
 */
class TPClItemInfo
{
	NOCOPY(TPClItemInfo)
public:
	TPClItemInfo(
		CSSM_CL_HANDLE		clHand,
		CSSM_CSP_HANDLE		cspHand,
		const TPClItemCalls	&clCalls,
		const CSSM_DATA		*itemData,
		TPItemCopy			copyItemData,		
		const char			*verifyTime);		// may be NULL
				
	~TPClItemInfo();
	void releaseResources();
	
	/* 
	 * Fetch arbitrary field from cached item.
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

	/* 
	 * Verify with an issuer cert - works on certs and CRLs.
	 * Issuer/subject name match already performed by caller.
	 * May return CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE without
	 * performing a signature op, in which case it is the caller's 
	 * resposibility to complete this operation later when 
	 * sufficient information is available.
	 *
	 * Optional paramCert is used to provide parameters when issuer
	 * has a partial public key.
	 */
	CSSM_RETURN verifyWithIssuer(
		TPCertInfo		*issuerCert,
		TPCertInfo		*paramCert = NULL) const;

	/* accessors */
	CSSM_CL_HANDLE	clHand()	  	const { return mClHand; }
	CSSM_CSP_HANDLE	cspHand()	  	const { return mCspHand; }
	CSSM_HANDLE		cacheHand()	  	const { return mCacheHand; }
	const CSSM_DATA *itemData()	  	const { return mItemData; }
	const CSSM_DATA *issuerName() 	const { return mIssuerName; };				
	unsigned 		index()			const { return mIndex; }	
	void 			index(unsigned dex)	  { mIndex = dex; }
	bool			isExpired()			  { return mIsExpired; }
	bool			isNotValidYet()		  { return mIsNotValidYet; }
	
	/* 
	 * Calculate validity (not before/after). Returns 
	 * 		CSSMERR_{TP_CERT,APPLETP_CRL}_NOT_VALID_YET
	 *		CSSMERR_xxx_T_EXPIRED
	 *		CSSM_OK
	 *		CSSMERR_xxx_INVALID_CERT_POINTER, other "bogus cert" errors
	 */
	CSSM_RETURN calculateCurrent(
		const char 			*verifyString = NULL);
		
private:

	/* Tell CL to parse and cache the item */
	CSSM_RETURN cacheItem(
		const CSSM_DATA		*itemData,
		TPItemCopy			copyItemData);			
													
		
	/* fetch not before/after fields */
	void fetchNotBeforeAfter();
		
	CSSM_CL_HANDLE			mClHand;				// always valid
	CSSM_CSP_HANDLE			mCspHand;				// always valid
	const TPClItemCalls		&mClCalls;
	bool					mWeOwnTheData;			// if true, we have to free 
													//    mCertData
	/* following four valid subsequent to cacheItem(), generally
	 * called by subclass's constructor */
	CSSM_HANDLE				mCacheHand;	
	CSSM_DATA_PTR			mIssuerName;
	CSSM_DATA				*mItemData;	
	CSSM_ALGORITHMS			mSigAlg;

	/* calculated implicitly at construction */
	CFDateRef				mNotBefore;
	CFDateRef				mNotAfter;
	
	/* also calculated at construction, but can be recalculated at will */
	bool					mIsExpired;
	bool					mIsNotValidYet;
	
	unsigned				mIndex;
};

/*
 * Class representing one certificate. The raw cert data usually comes from
 * a client (via incoming cert groups in CertGroupConstruct() and 
 * CertGroupVerify()); in this case, we don't own the raw data and 
 * don't copy or free it. Caller can optionally specify that we copy 
 * (and own and eventually free) the raw cert data. Currently this is 
 * done when we find a cert in a DlDb or from the net. The constructor throws 
 * on any error (bad cert data); subsequent to successful construction, no CSSM 
 * errors are thrown and it's guaranteed that the cert is basically good and 
 * successfully cached in the CL, and that we have a locally cached subject 
 * and issuer name (in normalized encoded format). 
 */ 
class TPCertInfo : public TPClItemInfo
{
	NOCOPY(TPCertInfo)
public:
	/* 
	 * No default constructor - this is the only way.
	 * This caches the cert and fetches subjectName and issuerName
	 * to ensure the incoming certData is well-constructed.
	 */
	TPCertInfo(
		CSSM_CL_HANDLE		clHand,
		CSSM_CSP_HANDLE		cspHand,
		const CSSM_DATA		*certData,
		TPItemCopy			copyCertData,	
											
		const char			*verifyTime);		// may be NULL
		
	/* frees mSubjectName, mIssuerName, mCacheHand via mClHand */
	~TPCertInfo();
	
	/* accessors */
	const CSSM_DATA *subjectName();

	bool 		isSelfSigned(bool avoidVerify = false);

	bool		isAnchor()				{ return mIsAnchor; }
	void		isAnchor(bool a)		{ mIsAnchor = a; }
	bool		isFromNet()				{ return mIsFromNet; }
	void		isFromNet(bool n)		{ mIsFromNet = n; };
	bool		isFromInputCerts()		{ return mIsFromInputCerts; }
	void		isFromInputCerts(bool i) { mIsFromInputCerts = i; }
	unsigned	numStatusCodes()		{ return mNumStatusCodes; }
	CSSM_RETURN	*statusCodes()			{ return mStatusCodes; }
	CSSM_DL_DB_HANDLE dlDbHandle()		{ return mDlDbHandle; }
	void dlDbHandle(CSSM_DL_DB_HANDLE hand)
										{ mDlDbHandle = hand; }
	CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord()
										{ return mUniqueRecord; }
	void uniqueRecord(CSSM_DB_UNIQUE_RECORD_PTR rec)
										{ mUniqueRecord = rec; }
	CSSM_KEY_PTR pubKey()				{ return mPublicKey; }
	bool		used()					{ return mUsed; }
	void 		used(bool u)			{ mUsed = u; }
	bool		isLeaf()				{ return mIsLeaf; }
	void		isLeaf(bool l)			{ mIsLeaf = l; }

	SecTrustSettingsDomain 	trustSettingsDomain() { return mTrustSettingsDomain; }
	SecTrustSettingsResult trustSettingsResult() { return mTrustSettingsResult; }
	bool ignoredError()					{ return mIgnoredError; }
	
	/* true means "verification terminated due to user trust setting" */
	bool		trustSettingsFound();
	/*
	 * Am I the issuer of the specified subject item? Returns true if so.
	 * Works for subject certs as well as CRLs. 
	 */
	bool isIssuerOf(
		const TPClItemInfo	&subject);
		
	/* 
	 * Add error status to mStatusCodes[]. Check to see if the
	 * added status is allowed per mAllowedErrs; if not return true.
	 * Returns false of the status *is* an allowed error.
	 */
	bool addStatusCode(
		CSSM_RETURN 		code);

	/* 
	 * See if the specified error status is allowed (return false) or
	 * fatal (return true) per mAllowedErrs[].
	 */
	bool isStatusFatal(
		CSSM_RETURN			code);
		
	/* 
	 * Indicate whether this cert's public key is a CSSM_KEYATTR_PARTIAL
	 * key.
	 */
	bool 					hasPartialKey();
	 
	/* Indicate whether this cert should be explicitly rejected.
	 */
	bool					shouldReject();

	/*
	 * Flag to indicate that at least one revocation policy has successfully
	 * achieved a positive verification of the cert. 
	 */
	bool				revokeCheckGood()			{ return mRevCheckGood; }
	void				revokeCheckGood(bool b)		{ mRevCheckGood = b; }
	
	/*
	 * Flag to indicate "I have successfully been checked for revocation
	 * status and the per-policy action data indicates that I need not be 
	 * checked again by any other revocation policy". E.g., 
	 * CSSM_TP_ACTION_CRL_SUFFICIENT is set and CRL revocation checking
	 * was successful for this cert. 
	 */
	bool				revokeCheckComplete()		{ return mRevCheckComplete; }
	void				revokeCheckComplete(bool b)	{ mRevCheckComplete = b; }
	
	/*
	 * Evaluate user trust; returns true if positive match found - i.e.,
	 * cert chain construction is done. 
	 * The foundEntry return value indicates that *some* entry was found for
	 * the cert, regardless of the trust setting evaluation. 
	 */
	OSStatus evaluateTrustSettings(
				const CSSM_OID			&policyOid,
				const char				*policyString,		// optional
				uint32					policyStringLen,
				SecTrustSettingsKeyUsage keyUse,				// optional
				bool 					*foundMatchingEntry,
				bool					*foundEntry);		// RETURNED
	
	bool 				hasEmptySubjectName();
	
	/* Free mUniqueRecord if it exists */
	void	freeUniqueRecord();
	
private:
	/* obtained from CL at construction */
	CSSM_DATA_PTR			mSubjectName;		// always valid
	CSSM_DATA_PTR			mPublicKeyData;		// mPublicKey obtained from this field
	CSSM_KEY_PTR			mPublicKey;
	
	/* maintained by caller, default at constructor 0/false */
	bool					mIsAnchor;
	bool					mIsFromInputCerts;
	bool					mIsFromNet;
	unsigned				mNumStatusCodes;
	CSSM_RETURN				*mStatusCodes;
	CSSM_DL_DB_HANDLE		mDlDbHandle;
	CSSM_DB_UNIQUE_RECORD_PTR mUniqueRecord;
	bool					mUsed;			// e.g., used in current loop 
	bool					mIsLeaf;		// first in chain
	TPRootState				mIsRoot;		// subject == issuer
	bool					mRevCheckGood;		// >= 1 revoke check good
	bool					mRevCheckComplete;	// no more revoke checking needed
	
	/* 
	 * When true, we've already called SecTrustSettingsEvaluateCert,
	 * and the cached results are in following member vars.
	 */
	bool					mTrustSettingsEvaluated;

	/* result of trust settings evaluation */
	SecTrustSettingsDomain	mTrustSettingsDomain;
	SecTrustSettingsResult	mTrustSettingsResult;
	bool					mTrustSettingsFoundAnyEntry;
	bool					mTrustSettingsFoundMatchingEntry;
	
	/* allowed errors obtained from SecTrustSettingsEvaluateCert() */
	CSSM_RETURN				*mAllowedErrs;
	uint32					mNumAllowedErrs;
	
	/* we actually ignored one of mAllowedErrors[] */
	bool					mIgnoredError;

	/* key usage for which mTrustSettingsResult was evaluated */
	SecTrustSettingsKeyUsage mTrustSettingsKeyUsage;
	
	/* for SecTrustSettingsEvaluateCert() */
	CFStringRef				mCertHashStr;		
	
	void 					releaseResources();
};

/* Describe who owns the items in a TP{Cert,Crl}Group */
typedef enum {
	TGO_None = 0,		// not used
	TGO_Group,			// TP{Cert,Crl}Group owns the items
	TGO_Caller			// caller owns the items
} TPGroupOwner;

/*
 * TP's private Cert Group class. Provides a list of TPCertInfo pointers,
 * to which caller can append additional elements, access an element at
 * an arbitrary position, and remove an element at an arbitrary position.
 */
class TPCertGroup
{
	NOCOPY(TPCertGroup)
public:
	/*
	 * No default constructor.
	 * This one creates an empty TPCertGroup.
	 */
	TPCertGroup(
		Allocator			&alloc,
		TPGroupOwner		whoOwns);		// if TGO_Group, we delete
	
	/*
	 * Construct from unordered, untrusted CSSM_CERTGROUP. Resulting
	 * TPCertInfos are more or less in the same order as the incoming
	 * certs, though incoming certs are discarded if they don't parse.
	 * No verification of any sort is performed. 
	 */
	TPCertGroup(
		const CSSM_CERTGROUP 	&CertGroupFrag,
		CSSM_CL_HANDLE 			clHand,
		CSSM_CSP_HANDLE 		cspHand,
		Allocator				&alloc,
		const char				*verifyString,			// may be NULL
		bool					firstCertMustBeValid,
		TPGroupOwner			whoOwns);	
		
	/*
	 * Deletes all TPCertInfo's.
	 */
	~TPCertGroup();
	
	/*
	 * Construct ordered, verified cert chain from a variety of inputs. 
	 * Time validity is ignored and needs to be checked by caller (it's
	 * stored in each TPCertInfo we add to ourself during construction).
	 * The only error returned is CSSMERR_APPLETP_INVALID_ROOT, meaning 
	 * we verified back to a supposed root cert which did not in fact
	 * self-verify. Other interesting status is returned via the
	 * verifiedToRoot and verifiedToAnchor flags. 
	 *
	 * NOTE: is it the caller's responsibility to call setAllUnused() 
	 * for both incoming cert groups (inCertGroup and gatheredCerts). 
	 * We don't do that here because we may call ourself recursively. 
	 *
	 * subjectItem may or may not be in the cert group (currently, it
	 * is in the group if it's a cert and it's not if it's a CRL, but 
	 * we don't rely on that). 
	 */
	CSSM_RETURN buildCertGroup(
		const TPClItemInfo		&subjectItem,	// Cert or CRL
		TPCertGroup				*inCertGroup,	// optional
		const CSSM_DL_DB_LIST 	*dbList,		// optional
		CSSM_CL_HANDLE 			clHand,
		CSSM_CSP_HANDLE 		cspHand,
		const char 				*verifyString,	// optional, for establishing
												//   validity of new TPCertInfos
		/* trusted anchors, optional */
		/* FIXME - maybe this should be a TPCertGroup */
		uint32 					numAnchorCerts,
		const CSSM_DATA			*anchorCerts,
		
		/* 
		 * Certs to be freed by caller (i.e., TPCertInfo which we allocate
		 * as a result of using a cert from anchorCerts or dbList) are added
		 * to this group.
		 */
		TPCertGroup				&certsToBeFreed,
		
		/*
		* Other certificates gathered during the course of this operation,
		* currently consisting of certs fetched from DBs and from the net.
		* This is not used when called by AppleTPSession::CertGroupConstructPriv;
		* it's an optimization for the case when we're building a cert group
		* for TPCrlInfo::verifyWithContext - we avoid re-fetching certs from
		* the net which are needed to verify both the subject cert and a CRL.
		*/
		TPCertGroup				*gatheredCerts,
		
		/*
		* Indicates that subjectItem is a cert in this cert group.
		* If true, that cert will be tested for "root-ness", including 
		*   -- subject/issuer compare
		*   -- signature self-verify
		*   -- anchor compare
		*/
		CSSM_BOOL				subjectIsInGroup,
		
		/* currently, only CSSM_TP_ACTION_FETCH_CERT_FROM_NET and 
		 * CSSM_TP_ACTION_TRUST_SETTINGS are interesting */
		CSSM_APPLE_TP_ACTION_FLAGS	actionFlags,
		
		/* CSSM_TP_ACTION_TRUST_SETTINGS parameters */
		const CSSM_OID			*policyOid,
		const char				*policyStr,
		uint32					policyStrLen,
		SecTrustSettingsKeyUsage	leafKeyUse,

		/* returned */
		CSSM_BOOL				&verifiedToRoot,		// end of chain self-verifies
		CSSM_BOOL				&verifiedToAnchor,		// end of chain in anchors
		CSSM_BOOL				&verifiedViaTrustSettings);	// chain ends per User Trust setting
		
	/* add/remove/access TPTCertInfo's. */
	void appendCert(
		TPCertInfo			*certInfo);			// appends to end of mCertInfo
	TPCertInfo *certAtIndex(
		unsigned			index);
	TPCertInfo *removeCertAtIndex(
		unsigned			index);				// doesn't delete the cert, just 
												// removes it from our list
	unsigned numCerts() const 					// how many do we have? 
		{ return mNumCerts; }
		
	/* 
	 * Convenience accessors for first and last cert, only valid when we have
	 * at least one cert.
	 */
	TPCertInfo *firstCert();
	TPCertInfo *lastCert();
		
	/* build a CSSM_CERTGROUP corresponding with our mCertInfo */
	CSSM_CERTGROUP_PTR buildCssmCertGroup();

	/* build a CSSM_TP_APPLE_EVIDENCE_INFO array corresponding with our
	 * mCertInfo */
	CSSM_TP_APPLE_EVIDENCE_INFO *buildCssmEvidenceInfo();
		
	/* Given a status for basic construction of a cert group and a status
	 * of (optional) policy verification, plus the implicit notBefore/notAfter
	 * status in the certs, calculate a global return code. This just 
	 * encapsulates a policy for CertGroupConstruct and CertGroupVerify.
	 */
	CSSM_RETURN getReturnCode(
		CSSM_RETURN					constructStatus,
		CSSM_RETURN					policyStatus,
		CSSM_APPLE_TP_ACTION_FLAGS	actionFlags);
	 
	Allocator
		&alloc() {return mAlloc; }
	
	/* set all TPCertInfo.mUsed flags false */
	void					setAllUnused();

	/* free records obtained from DBs */
	void					freeDbRecords();

	/* 
	 * See if the specified error status is allowed (return true) or
	 * fatal (return false) per each cert's mAllowedErrs[]. Returns
	 * true if any cert returns false for its isStatusFatal() call. 
	 * The list of errors which can apply to cert-chain-wide allowedErrors
	 * is right here; if the incoming error is not in that list, we
	 * return false. If the incoming error code is CSSM_OK we return
	 * true as a convenience for our callers. 
	 */
	bool isAllowedError(
		CSSM_RETURN	code);
	
	/* 
	 * Determine if we already have the specified cert in this group.
	 */
	bool isInGroup(TPCertInfo &certInfo);

	/*
	 * Given a constructed cert group, encode all the issuers
	 * (i.e. chain minus the leaf, unless numCerts() is 1) as a PEM data blob.
	 * Caller is responsible for freeing the data.
	 */
	void encodeIssuers(CSSM_DATA &issuers);
	
private:
	
	/* 
	 * Search unused incoming certs to find an issuer of specified 
	 * cert or CRL.
	 * WARNING this assumes a valied "used" state for all certs 
	 * in this group.
	 * If partialIssuerKey is true on return, caller must re-verify signature
     * of subject later when sufficient info is available. 
	 */ 
	TPCertInfo *findIssuerForCertOrCrl(
		const TPClItemInfo 	&subject,
		bool				&partialIssuerKey);

	/* 
	 * Called from buildCertGroup as final processing of a constructed
	 * group when CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE has been
	 * detected. Perform partial public key processing.
	 * Returns:
	 *   	CSSMERR_TP_CERTIFICATE_CANT_OPERATE - can't complete partial key
	 *		CSSMERR_TP_INVALID_CERT_AUTHORITY - sig verify failed with 
	 *			(supposedly) completed partial key
	 */
	CSSM_RETURN				verifyWithPartialKeys(
		const TPClItemInfo	&subjectItem);		// Cert or CRL
	
	Allocator				&mAlloc;
	TPCertInfo				**mCertInfo;		// just an array of pointers
	unsigned				mNumCerts;			// valid certs in certInfo
	unsigned				mSizeofCertInfo;	// mallocd space in certInfo
	TPGroupOwner			mWhoOwns;			// if TGO_Group, we delete certs 
												//    upon destruction
};
#endif	/* _TP_CERT_INFO_H_ */
