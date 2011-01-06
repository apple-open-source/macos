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
 * tpCertGroup.cpp - Cert group functions (construct, verify) 
 *
 * Created 10/5/2000 by Doug Mitchell.
 */
 
#include "AppleTPSession.h"
#include "certGroupUtils.h"
#include "TPCertInfo.h"
#include "TPCrlInfo.h"
#include "tpPolicies.h"
#include "tpdebugging.h"
#include "tpCrlVerify.h"
#include <Security/oidsalg.h>
#include <Security/cssmapple.h>

/*
 * This is a temporary hack to allow verification of PKINIT server certs
 * which are self-signed and not in the system anchors list. If the self-
 * signed cert is in a magic keychain (whose location is not published),
 * we'll allow it as if it were indeed a full-fledged anchor cert. 
 */
#define TP_PKINIT_SERVER_HACK	1
#if		TP_PKINIT_SERVER_HACK

#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecCertificate.h>
#include <Security/oidscert.h>
#include <sys/types.h>
#include <pwd.h>

#define CFRELEASE(cf)	if(cf) { CFRelease(cf); }

/* 
 * Returns true if we are to allow/trust the specified
 * cert as a PKINIT-only anchor.
 */
static bool tpCheckPkinitServerCert(
	TPCertGroup &certGroup)
{
	/* 
	 * Basic requirement: exactly one cert, self-signed.
	 * The numCerts == 1 requirement might change...
	 */
	unsigned numCerts = certGroup.numCerts();
	if(numCerts != 1) {
		tpDebug("tpCheckPkinitServerCert: too many certs");
		return false;
	}
	/* end of chain... */
	TPCertInfo *theCert = certGroup.certAtIndex(numCerts - 1);
	if(!theCert->isSelfSigned()) {
		tpDebug("tpCheckPkinitServerCert: 1 cert, not self-signed");
		return false;
	}
	const CSSM_DATA *subjectName = theCert->subjectName();
	
	/* 
	 * Open the magic keychain.
	 * We're going up and over the Sec layer here, not generally 
	 * kosher, but this is a temp hack.
	 */
	OSStatus ortn;
	SecKeychainRef kcRef = NULL;
	string fullPathName;
	const char *homeDir = getenv("HOME");
	if (homeDir == NULL)
	{
		// If $HOME is unset get the current user's home directory
		// from the passwd file.
		uid_t uid = geteuid();
		if (!uid) uid = getuid();
		struct passwd *pw = getpwuid(uid);
		if (!pw) {
			return false;
		}
		homeDir = pw->pw_dir;
	}
	fullPathName = homeDir;
	fullPathName += "/Library/Application Support/PKINIT/TrustedServers.keychain";
	ortn = SecKeychainOpen(fullPathName.c_str(), &kcRef);
	if(ortn) {
		tpDebug("tpCheckPkinitServerCert: keychain not found (1)");
		return false;
	}
	/* subsequent errors to errOut: */
	
	bool ourRtn = false;
	SecKeychainStatus kcStatus;
	CSSM_DATA_PTR subjSerial = NULL;
	CSSM_RETURN crtn;
	SecKeychainSearchRef		srchRef = NULL;
	SecKeychainAttributeList	attrList;
	SecKeychainAttribute		attrs[2];
	SecKeychainItemRef			foundItem = NULL;
	
	ortn = SecKeychainGetStatus(kcRef, &kcStatus);
	if(ortn) {
		tpDebug("tpCheckPkinitServerCert: keychain not found (2)");
		goto errOut;
	}
	
	/*
	 * We already have this cert's normalized name; get its
	 * serial number.
	 */
	crtn = theCert->fetchField(&CSSMOID_X509V1SerialNumber, &subjSerial);
	if(crtn) {
		/* should never happen */
		tpDebug("tpCheckPkinitServerCert: error fetching serial number");
		goto errOut;
	}
	
	attrs[0].tag    = kSecSubjectItemAttr;
	attrs[0].length = subjectName->Length;
	attrs[0].data   = subjectName->Data;
	attrs[1].tag    = kSecSerialNumberItemAttr;
	attrs[1].length = subjSerial->Length;
	attrs[1].data   = subjSerial->Data;
	attrList.count  = 2;
	attrList.attr   = attrs;
	
	ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		kSecCertificateItemClass,
		&attrList,
		&srchRef);
	if(ortn) {
		tpDebug("tpCheckPkinitServerCert: search failure");
		goto errOut;
	}
	for(;;) {
		ortn = SecKeychainSearchCopyNext(srchRef, &foundItem);
		if(ortn) {
			tpDebug("tpCheckPkinitServerCert: end search");
			break;
		}
		
		/* found a matching cert; do byte-for-byte compare */
		CSSM_DATA certData;
		ortn = SecCertificateGetData((SecCertificateRef)foundItem, &certData);
		if(ortn) {
			tpDebug("tpCheckPkinitServerCert: SecCertificateGetData failure");
			continue;
		}
		if(tpCompareCssmData(&certData, theCert->itemData())){
			tpDebug("tpCheckPkinitServerCert: FOUND CERT");
			ourRtn = true;
			break;
		}
		tpDebug("tpCheckPkinitServerCert: skipping matching cert");
		CFRelease(foundItem);
		foundItem = NULL;
	}
errOut:
	CFRELEASE(kcRef);
	CFRELEASE(srchRef);
	CFRELEASE(foundItem);
	if(subjSerial != NULL) {
		theCert->freeField(&CSSMOID_X509V1SerialNumber, subjSerial);
	}
	return ourRtn;
}
#endif	/* TP_PKINIT_SERVER_HACK */


/*-----------------------------------------------------------------------------
 * CertGroupConstruct
 *
 * Description:
 *   This function returns a pointer to a mallocd CSSM_CERTGROUP which
 *   refers to a mallocd list of raw ordered X.509 certs which verify back as
 *   far as the TP is able to go. The first cert of the returned list is the
 *   subject cert. The TP will attempt to search thru the DBs passed in
 *   DBList in order to complete the chain. The chain is completed when a
 *   self-signed (root) cert is found in the chain. The root cert may be
 *   present in the input CertGroupFrag, or it may have been obtained from
 *   one of the DBs passed in DBList. It is not an error if no root cert is
 *   found. 
 *   
 *   The error conditions are: 
 *   -- The first cert of CertGroupFrag is an invalid cert. NULL is returned,
 *		err = CSSM_TP_INVALID_CERTIFICATE.
 *   -- The root cert (if found) fails to verify. Valid certgroup is returned,
 *      err = CSSMERR_TP_VERIFICATION_FAILURE.
 *   -- Any cert in the (possibly partially) constructed chain has expired or
 *      isn't valid yet, err = CSSMERR_TP_CERT_EXPIRED or 
 *      CSSMERR_TP_CERT_NOT_VALID_YET. A CertGroup is returned. 
 *   -- CSSMERR_TP_CERT_EXPIRED and CSSMERR_TP_CERT_NOT_VALID_YET. If one of these
 *		conditions obtains for the first (leaf) cert, the function throws this
 *		error immediately and the outgoing cert group is empty. For subsequent certs,
 *		the temporal validity of a cert is only tested AFTER a cert successfully
 *		meets the cert chaining criteria (subject/issuer match and signature
 *		verify). A cert in a chain with this error is not added to the outgoing
 *		cert group. 
 *   -- the usual errors like bad handle or memory failure. 
 *
 * Parameters:
 *   Two handles - to an open CL and CSP. The CSP must be capable of
 *   dealing with the signature algorithms used by the certs. The CL must be
 *   an X.509-savvy CL.  
 *   
 *   CertGroupFrag, an unordered array of raw X.509 certs in the form of a
 *   CSSM_CERTGROUP_PTR. The first cert of this list is the subject cert
 *   which is eventually to be verified. The other certs can be in any order 
 *   and may not even have any relevance to the cert chain being constructed. 
 *   They may also be invalid certs. 
 *   
 *   DBList, a list of DB/DL handles which may contain certs necessary to
 *   complete the desired cert chain. (Not currently implemented.)
 *
 *---------------------------------------------------------------------------*/
 
/* public version */
void AppleTPSession::CertGroupConstruct(CSSM_CL_HANDLE clHand,
		CSSM_CSP_HANDLE cspHand,
		const CSSM_DL_DB_LIST &DBList,
		const void *ConstructParams,
		const CSSM_CERTGROUP &CertGroupFrag,
		CSSM_CERTGROUP_PTR &CertGroup)
{
	TPCertGroup outCertGroup(*this, TGO_Caller);
	TPCertGroup inCertGroup(CertGroupFrag, 
		clHand, 
		cspHand, 
		*this, 
		NULL,		// cssmTimeStr
		true, 		// firstCertMustBeValid
		TGO_Group);
		
	/* set up for disposal of TPCertInfos created by CertGroupConstructPriv */
	TPCertGroup			gatheredCerts(*this, TGO_Group);
	
	CSSM_RETURN constructReturn = CSSM_OK;
	CSSM_APPLE_TP_ACTION_FLAGS	actionFlags = 0;
	CSSM_BOOL verifiedToRoot;		// not used
	CSSM_BOOL verifiedToAnchor;		// not used
	CSSM_BOOL verifiedViaTrustSetting;	// not used
	
	try {
		CertGroupConstructPriv(clHand,
			cspHand,
			inCertGroup,
			&DBList,
			NULL,				// cssmTimeStr
			/* no anchors */
			0, NULL,
			actionFlags,
			/* no user trust */
			NULL, NULL, 0, 0,
			gatheredCerts,
			verifiedToRoot,
			verifiedToAnchor,
			verifiedViaTrustSetting,
			outCertGroup);
	}
	catch(const CssmError &cerr) {
		constructReturn = cerr.error;
		/* abort if no certs found */
		if(outCertGroup.numCerts() == 0) {
			CssmError::throwMe(constructReturn);
		}
	}
	CertGroup = outCertGroup.buildCssmCertGroup();
	/* caller of this function never gets evidence... */
	outCertGroup.freeDbRecords();
	
	if(constructReturn) {
		CssmError::throwMe(constructReturn);
	}
}


/* 
 * Private version of CertGroupConstruct, used by CertGroupConstruct and
 * CertGroupVerify. Populates a TP-style TPCertGroup for further processing.
 * This only throws CSSM-style exceptions in the following cases: 
 * 
 *  -- input parameter errors
 *  -- the first (leaf) cert is bad (doesn't parse, expired, not valid yet).
 *  -- root found but it doesn't self-verify 
 *
 *  All other cert-related errors simply result in the bad cert being ignored.
 *  Other exceptions are gross system errors like malloc failure.
 */
void AppleTPSession::CertGroupConstructPriv(CSSM_CL_HANDLE clHand,
		CSSM_CSP_HANDLE 		cspHand,
		TPCertGroup 			&inCertGroup,
		const CSSM_DL_DB_LIST 	*DBList,			// optional here
		const char 				*cssmTimeStr,		// optional
		
		/* trusted anchors, optional */
		/* FIXME - maybe this should be a TPCertGroup */
		uint32 					numAnchorCerts,
		const CSSM_DATA			*anchorCerts,
		
		/* CSSM_TP_ACTION_FETCH_CERT_FROM_NET, CSSM_TP_ACTION_TRUST_SETTINGS */
		CSSM_APPLE_TP_ACTION_FLAGS	actionFlags,

		/* optional user trust parameters */
		const CSSM_OID			*policyOid,
		const char				*policyStr,
		uint32					policyStrLen,
		SecTrustSettingsKeyUsage	keyUse,
		
		/* 
		 * Certs to be freed by caller (i.e., TPCertInfo which we allocate
		 * as a result of using a cert from anchorCerts or dbList) are added
		 * to this group.
		 */
		TPCertGroup				&certsToBeFreed,

		/* returned */
		CSSM_BOOL				&verifiedToRoot,		// end of chain self-verifies
		CSSM_BOOL				&verifiedToAnchor,		// end of chain in anchors
		CSSM_BOOL				&verifiedViaTrustSetting,	// chain ends per User Trust setting
		TPCertGroup 			&outCertGroup)			// RETURNED
{
	TPCertInfo			*subjectCert;				// the one we're working on
	CSSM_RETURN			outErr = CSSM_OK;
	
	/* this'll be the first subject cert in the main loop */
	subjectCert = inCertGroup.certAtIndex(0);

	/* Append leaf cert to outCertGroup */
	outCertGroup.appendCert(subjectCert);
	subjectCert->isLeaf(true);
	subjectCert->isFromInputCerts(true);
	outCertGroup.setAllUnused();
	subjectCert->used(true);
	
	outErr = outCertGroup.buildCertGroup(
		*subjectCert,	
		&inCertGroup,
		DBList,
		clHand,
		cspHand,
		cssmTimeStr,	
		numAnchorCerts,
		anchorCerts,
		certsToBeFreed,
		&certsToBeFreed,	// gatheredCerts to accumulate net/DB fetches
		CSSM_TRUE,			// subjectIsInGroup - enables root check on
							//    subject cert
		actionFlags,
		policyOid,
		policyStr,
		policyStrLen,
		keyUse,
		
		verifiedToRoot,	
		verifiedToAnchor,
		verifiedViaTrustSetting);
	if(outErr) {
		CssmError::throwMe(outErr);
	}
}

/*
 * Map a policy OID to one of the standard (non-revocation) policies.
 * Returns true if it's a standard policy.
 */
static bool checkPolicyOid(
	const CSSM_OID	&oid,
	TPPolicy		&tpPolicy)		/* RETURNED */
{
	if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_SSL)) {
		tpPolicy = kTP_SSL;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_X509_BASIC)) {
		tpPolicy = kTPx509Basic;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_SMIME)) {
		tpPolicy = kTP_SMIME;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_EAP)) {
		tpPolicy = kTP_EAP;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_SW_UPDATE_SIGNING)) {
		/* note: this was CSSMOID_APPLE_TP_CODE_SIGN until 8/15/06 */
		tpPolicy = kTP_SWUpdateSign;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_RESOURCE_SIGN)) {
		tpPolicy = kTP_ResourceSign;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_IP_SEC)) {
		tpPolicy = kTP_IPSec;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_ICHAT)) {
		tpPolicy = kTP_iChat;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_ISIGN)) {
		tpPolicy = kTPiSign;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_PKINIT_CLIENT)) {
		tpPolicy = kTP_PKINIT_Client;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_PKINIT_SERVER)) {
		tpPolicy = kTP_PKINIT_Server;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_CODE_SIGNING)) {
		tpPolicy = kTP_CodeSigning;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_PACKAGE_SIGNING)) {
		tpPolicy = kTP_PackageSigning;
		return true;
	}
	else if(tpCompareOids(&oid, &CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT)) {
		tpPolicy = kTP_MacAppStoreRec;
		return true;
	}
	return false;
}

/*-----------------------------------------------------------------------------
 * CertGroupVerify
 *
 * Description:
 *   -- Construct a cert chain using TP_CertGroupConstruct. 
 *   -- Attempt to verify that cert chain against one of the known 
 *      good certs passed in AnchorCerts. 
 *   -- Optionally enforces additional policies (TBD) when verifying the cert chain.
 *   -- Optionally returns the entire cert chain constructed in 
 *      TP_CertGroupConstruct and here, all the way to an anchor cert or as 
 *      far as we were able to go, in *Evidence. 
 *
 * Parameters:
 *   Two handles - to an open CL and CSP. The CSP must be capable of
 *   dealing with the signature algorithms used by the certs. The CL must be
 *   an X.509-savvy CL.  
 *   
 *   RawCerts, an unordered array of raw certs in the form of a
 *   CSSM_CERTGROUP_PTR. The first cert of this list is the subject cert
 *   which is eventually to be verified. The other certs can be in any order
 *   and may not even have any relevance to the cert chain being constructed.
 *   They may also be invalid certs. 
 *   
 *   DBList, a list of DB/DL handles which may contain certs necessary to
 *   complete the desired cert chain. (Currently not implemented.)
 *   
 *   AnchorCerts, a list of known trusted certs. 
 *   NumberOfAnchorCerts, size of AnchorCerts array. 
 *   
 *   PolicyIdentifiers, Optional policy OID. NULL indicates default
 *		X.509 trust policy.
 *
 *	 Supported Policies:
 *			CSSMOID_APPLE_ISIGN
 *			CSSMOID_APPLE_X509_BASIC
 *		
 *			For both of these, the associated FieldValue must be {0, NULL},
 *
 *   NumberOfPolicyIdentifiers, size of PolicyIdentifiers array, must be 
 *      zero or one. 
 * 
 *   All other arguments must be zero/NULL.
 *
 *   Returns:
 *      CSSM_OK : cert chain verified all the way back to an AnchorCert.
 *      CSSMERR_TP_INVALID_ANCHOR_CERT : In this case, the cert chain
 *   		was validated back to a self-signed (root) cert found in either
 *   		CertToBeVerified or in one of the DBs in DBList, but that root cert
 *   		was *NOT* found in the AnchorCert list. 
 *		CSSMERR_TP_NOT_TRUSTED: no root cert was found and no AnchorCert
 *   		verified the end of the constructed cert chain.
 *		CSSMERR_TP_VERIFICATION_FAILURE: a root cert was found which does
 *   		not self-verify. 
 *   	CSSMERR_TP_VERIFY_ACTION_FAILED: indicates a failure of the requested 
 *			policy action. 
 *   	CSSMERR_TP_INVALID_CERTIFICATE: indicates a bad leaf cert. 
 *		CSSMERR_TP_INVALID_REQUEST_INPUTS : no incoming VerifyContext.
 *		CSSMERR_TP_CERT_EXPIRED and CSSMERR_TP_CERT_NOT_VALID_YET: see comments
 *			for CertGroupConstruct. 
 *		CSSMERR_TP_CERTIFICATE_CANT_OPERATE : issuer cert was found with a partial
 *			public key, rendering full verification impossible. 
 *   	CSSMERR_TP_INVALID_CERT_AUTHORITY : issuer cert was found with a partial 
 *			public key and which failed to perform subsequent signature
 *			verification.
 *---------------------------------------------------------------------------*/

void AppleTPSession::CertGroupVerify(CSSM_CL_HANDLE clHand,
		CSSM_CSP_HANDLE cspHand,
		const CSSM_CERTGROUP &CertGroupToBeVerified,
		const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT_PTR VerifyContextResult)
{
	CSSM_BOOL				verifiedToRoot = CSSM_FALSE;
	CSSM_BOOL				verifiedToAnchor = CSSM_FALSE;
	CSSM_BOOL				verifiedViaTrustSetting = CSSM_FALSE;
	CSSM_RETURN				constructReturn = CSSM_OK;
	CSSM_RETURN				policyReturn = CSSM_OK;
	const CSSM_TP_CALLERAUTH_CONTEXT *cred;
	/* declare volatile as compiler workaround to avoid caching in CR4 */
	const CSSM_APPLE_TP_ACTION_DATA * volatile actionData = NULL;
	CSSM_TIMESTRING			cssmTimeStr;
	CSSM_APPLE_TP_ACTION_FLAGS	actionFlags = 0;
	CSSM_TP_STOP_ON 		tpStopOn = 0;
	
	/* keep track of whether we did policy checking; if not, we do defaults */
	bool					didCertPolicy = false;
	bool					didRevokePolicy = false;
	
	/* user trust parameters */
	CSSM_OID				utNullPolicy = {0, NULL};
	const CSSM_OID			*utPolicyOid = NULL;
	const char				*utPolicyStr = NULL;
	uint32					utPolicyStrLen = 0;
	SecTrustSettingsKeyUsage	utKeyUse = 0;
	bool					utTrustSettingEnabled = false;
	
	if(VerifyContextResult) {
		memset(VerifyContextResult, 0, sizeof(*VerifyContextResult));
	}

	/* verify input args, skipping the ones checked by CertGroupConstruct */
	if((VerifyContext == NULL) || (VerifyContext->Cred == NULL)) {
		/* the spec says that this is optional but we require it */
			CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	cred = VerifyContext->Cred;
	
	/* Optional ActionData affecting all policies */
	actionData = (CSSM_APPLE_TP_ACTION_DATA * volatile)VerifyContext->ActionData.Data;
	if(actionData != NULL) {
		switch(actionData->Version) {
			case CSSM_APPLE_TP_ACTION_VERSION:
				if(VerifyContext->ActionData.Length !=
						sizeof(CSSM_APPLE_TP_ACTION_DATA)) {
					CssmError::throwMe(CSSMERR_TP_INVALID_ACTION_DATA);
				}
				break;
			/* handle backwards versions here if we ever go beyond version 0 */
			default:
				CssmError::throwMe(CSSMERR_TP_INVALID_ACTION_DATA);
		}
		actionFlags = actionData->ActionFlags;
		if(actionFlags & CSSM_TP_ACTION_TRUST_SETTINGS) {
			utTrustSettingEnabled = true;
		}
	}
	
	/* optional, may be NULL */
	cssmTimeStr = cred->VerifyTime;
	
	tpStopOn = cred->VerificationAbortOn;
	switch(tpStopOn) {
		/* the only two we support */
		case CSSM_TP_STOP_ON_NONE:	
		case CSSM_TP_STOP_ON_FIRST_FAIL:
			break;
		/* default maps to stop on first fail */
		case CSSM_TP_STOP_ON_POLICY:
			tpStopOn = CSSM_TP_STOP_ON_FIRST_FAIL;
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_INVALID_STOP_ON_POLICY);
	}
	
	/* now the args we can't deal with */
	if(cred->CallerCredentials != NULL) {
			CssmError::throwMe(CSSMERR_TP_INVALID_CALLERAUTH_CONTEXT_POINTER);
	}
	/* ...any others? */
	
	/* set up for optional user trust evaluation */
	if(utTrustSettingEnabled) {
		const CSSM_TP_POLICYINFO *pinfo = &cred->Policy;
		TPPolicy utPolicy = kTPx509Basic;
		
		/* default policy OID in case caller hasn't specified one */
		utPolicyOid = &utNullPolicy;
		if(pinfo->NumberOfPolicyIds == 0) {
			tpTrustSettingsDbg("CertGroupVerify: User trust enabled but no policies (1)");
			/* keep going, I guess - no policy-specific info - use kTPx509Basic */
		}
		else {
			CSSM_FIELD_PTR utPolicyField = &pinfo->PolicyIds[0];
			utPolicyOid = &utPolicyField->FieldOid;
			bool foundPolicy = checkPolicyOid(*utPolicyOid, utPolicy);
			if(!foundPolicy) {
				tpTrustSettingsDbg("CertGroupVerify: User trust enabled but no policies");
				/* keep going, I guess - no policy-specific info - use kTPx509Basic */
			}
			else {
				/* get policy-specific info */
				tp_policyTrustSettingParams(utPolicy, &utPolicyField->FieldValue,
					&utPolicyStr, &utPolicyStrLen, &utKeyUse);
			}
		}	
	}
	
	/* get verified (possibly partial) outCertGroup - error is fatal */
	/* BUT: we still return partial evidence if asked to...from now on. */
	TPCertGroup outCertGroup(*this, 
		TGO_Caller);		// certs are owned by inCertGroup
	TPCertGroup inCertGroup(CertGroupToBeVerified, clHand, cspHand, *this, 
		cssmTimeStr, 		// optional 'this' time
		true, 				// firstCertMustBeValid
		TGO_Group);	
		
	/* set up for disposal of TPCertInfos created by CertGroupConstructPriv */
	TPCertGroup	gatheredCerts(*this, TGO_Group);
	
	try {
		CertGroupConstructPriv(
			clHand,
			cspHand,
			inCertGroup,
			cred->DBList, 
			cssmTimeStr,
			cred->NumberOfAnchorCerts,
			cred->AnchorCerts,
			actionFlags,
			utPolicyOid,
			utPolicyStr,
			utPolicyStrLen,
			utKeyUse,
			gatheredCerts,
			verifiedToRoot, 
			verifiedToAnchor,
			verifiedViaTrustSetting,
			outCertGroup);
	}
	catch(const CssmError &cerr) {
		constructReturn = cerr.error;
		/* abort if no certs found */
		if(outCertGroup.numCerts() == 0) {
			CssmError::throwMe(constructReturn);
		}
		/* else press on, collecting as much info as we can */
	}
	/* others are way fatal */
	assert(outCertGroup.numCerts() >= 1);
	
	/* Infer interim status from return values */
	switch(constructReturn) {
		/* these values do not get overridden */
		case CSSMERR_TP_CERTIFICATE_CANT_OPERATE:
		case CSSMERR_TP_INVALID_CERT_AUTHORITY:
		case CSSMERR_APPLETP_TRUST_SETTING_DENY:
		case errSecInvalidTrustSettings:
			break;
		default:
			/* infer status from these values... */
			if(verifiedToAnchor || verifiedViaTrustSetting) {
				/* full success; anchor doesn't have to be root */
				constructReturn = CSSM_OK;
			}
			else if(verifiedToRoot) {
				if(actionFlags & CSSM_TP_ACTION_IMPLICIT_ANCHORS) {
					constructReturn = CSSM_OK;
				}
				else {
					/* verified to root which is not an anchor */
					constructReturn = CSSMERR_TP_INVALID_ANCHOR_CERT;
				}
			}
			else {
				/* partial chain, no root, not verifiable by anchor */
				constructReturn = CSSMERR_TP_NOT_TRUSTED;
			}

			/* 
 			 * Those errors can be allowed, cert-chain-wide, per individual
			 * certs' allowedErrors
			 */
			if((constructReturn != CSSM_OK) && 
			    outCertGroup.isAllowedError(constructReturn)) {
				constructReturn = CSSM_OK;
			}
			break;
	}
	
	/*
	 * Parameters passed to tp_policyVerify() and which vary per policy
	 * in the loop below 
	 */
	TPPolicy tpPolicy;
	const CSSM_APPLE_TP_SSL_OPTIONS	*sslOpts;
	CSSM_RETURN thisPolicyRtn = CSSM_OK;	// returned from tp_policyVerify()
	
	/* common CRL verify parameters */
	TPCrlGroup *crlGroup = NULL;
	try {
		crlGroup = new TPCrlGroup(&VerifyContext->Crls,
			clHand, cspHand, 
			*this,				// alloc
			NULL, 				// cssmTimeStr - we want CRLs that are valid 'now'
			TGO_Group);
	}
	catch(const CssmError &cerr) {
		CSSM_RETURN cr = cerr.error;
		/* I don't see a straightforward way to report this error,
		 * other than adding it to the leaf cert's status... */
		outCertGroup.certAtIndex(0)->addStatusCode(cr);
		tpDebug("CertGroupVerify: error constructing CrlGroup; continuing\n");
	}
	/* others are way fatal */

	TPVerifyContext revokeVfyContext(*this,
		clHand,
		cspHand,
		cssmTimeStr,
		cred->NumberOfAnchorCerts,
		cred->AnchorCerts,
		&inCertGroup,
		crlGroup,
		/*
		 * This may consist of certs gathered from the net (which is the purpose
		 * of this argument) and from DLDBs (a side-effect optimization).
		 */
		gatheredCerts,
		cred->DBList,
		kRevokeNone,		// policy
		actionFlags,
		NULL,				// CRL options
		NULL,				// OCSP options
		utPolicyOid,
		utPolicyStr,
		utPolicyStrLen,
		utKeyUse);
		
	/* true if we're to execute tp_policyVerify at end of loop */
	bool doPolicyVerify;
	/* true if we're to execute a revocation policy at end of loop */
	bool doRevocationPolicy;
	
	/* grind thru each policy */
	for(uint32 polDex=0; polDex<cred->Policy.NumberOfPolicyIds; polDex++) {
		if(cred->Policy.PolicyIds == NULL) {
			policyReturn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
			break;
		}
		CSSM_FIELD_PTR policyId = &cred->Policy.PolicyIds[polDex];
		const CSSM_DATA *fieldVal = &policyId->FieldValue;
		const CSSM_OID	*oid = &policyId->FieldOid;
		thisPolicyRtn = CSSM_OK;
		doPolicyVerify = false;
		doRevocationPolicy = false;
		sslOpts = NULL;
		
		/* first the basic cert policies */
		doPolicyVerify = checkPolicyOid(*oid, tpPolicy);
		if(doPolicyVerify) {
			/* some basic checks... */
			bool policyAbort = false;
			switch(tpPolicy) {
				case kTPx509Basic:
				case kTPiSign:
				case kTP_PKINIT_Client:
				case kTP_PKINIT_Server:
					if(fieldVal->Data != NULL) {
						policyReturn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
						policyAbort = true;
						break;
					}
					break;
				default:
					break;
			}
			if(policyAbort) {
				break;
			}
			#if		TP_PKINIT_SERVER_HACK
			if(tpPolicy == kTP_PKINIT_Server) {
				/* possible override of "root not in anchors" */
				if(constructReturn == CSSMERR_TP_INVALID_ANCHOR_CERT) {
					if(tpCheckPkinitServerCert(outCertGroup)) {
						constructReturn = CSSM_OK;
					}
				}
			}
			#endif	/* TP_PKINIT_SERVER_HACK */
		}
		
		/* 
		 * Now revocation policies. Note some fields in revokeVfyContext can 
		 * accumulate across multiple policy calls, e.g., signerCerts. 
		 */
		else if(tpCompareOids(oid, &CSSMOID_APPLE_TP_REVOCATION_CRL)) {
			/* CRL-specific options */
			const CSSM_APPLE_TP_CRL_OPTIONS *crlOpts;
			crlOpts = (CSSM_APPLE_TP_CRL_OPTIONS *)fieldVal->Data;
			thisPolicyRtn = CSSM_OK;
			if(crlOpts != NULL) {
				switch(crlOpts->Version) {
					case CSSM_APPLE_TP_CRL_OPTS_VERSION:
						if(fieldVal->Length != 
								sizeof(CSSM_APPLE_TP_CRL_OPTIONS)) {
							thisPolicyRtn = 
								CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
							break;
						}
						break;
					/* handle backwards compatibility here if necessary */
					default:
						thisPolicyRtn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
						break;
				}
				if(thisPolicyRtn != CSSM_OK) {
					policyReturn = thisPolicyRtn;
					break;
				}
			}
			revokeVfyContext.policy = kRevokeCrlBasic;
			revokeVfyContext.crlOpts = crlOpts;
			doRevocationPolicy = true;
		}
		else if(tpCompareOids(oid, &CSSMOID_APPLE_TP_REVOCATION_OCSP)) {
			/* OCSP-specific options */
			const CSSM_APPLE_TP_OCSP_OPTIONS *ocspOpts;
			ocspOpts = (CSSM_APPLE_TP_OCSP_OPTIONS *)fieldVal->Data;
			thisPolicyRtn = CSSM_OK;
			if(ocspOpts != NULL) {
				switch(ocspOpts->Version) {
					case CSSM_APPLE_TP_OCSP_OPTS_VERSION:
						if(fieldVal->Length != 
								sizeof(CSSM_APPLE_TP_OCSP_OPTIONS)) {
							thisPolicyRtn = 
								CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
							break;
						}
						break;
					/* handle backwards compatibility here if necessary */
					default:
						thisPolicyRtn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
						break;
				}
				if(thisPolicyRtn != CSSM_OK) {
					policyReturn = thisPolicyRtn;
					break;
				}
			}
			revokeVfyContext.policy = kRevokeOcsp;
			revokeVfyContext.ocspOpts = ocspOpts;
			doRevocationPolicy = true;
		}
		/* etc. - add more policies here */
		else {
			/* unknown TP policy OID */
			policyReturn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
			break;
		}
		
		/* common cert policy call */
		if(doPolicyVerify) {
			assert(!doRevocationPolicy);	// one at a time
			thisPolicyRtn = tp_policyVerify(tpPolicy,
				*this,
				clHand,
				cspHand,
				&outCertGroup,
				verifiedToRoot,
				verifiedViaTrustSetting,
				actionFlags,
				fieldVal,
				cred->Policy.PolicyControl);	// not currently used
			didCertPolicy = true;
		}
		/* common revocation policy call */
		if(doRevocationPolicy) {
			assert(!doPolicyVerify);	// one at a time
			thisPolicyRtn = tpRevocationPolicyVerify(revokeVfyContext, outCertGroup);
			didRevokePolicy = true;
		}
		/* See if possible error is allowed, cert-chain-wide. */
		if((thisPolicyRtn != CSSM_OK) &&
		    outCertGroup.isAllowedError(thisPolicyRtn)) {
			thisPolicyRtn = CSSM_OK;
		}
		if(thisPolicyRtn) {
			/* Now remember the error if it's the first policy
			 * error we've seen. */
			if(policyReturn == CSSM_OK) {
				policyReturn = thisPolicyRtn;
			}
			/* Keep going? */
			if(tpStopOn == CSSM_TP_STOP_ON_FIRST_FAIL) {
				/* Nope; we're done with policy evaluation */
				break;
			}
		}
	}	/* for each policy */
	
	/*
	 * Upon completion of the above loop, perform default policy ops if
	 * appropriate.
	 */
	if((policyReturn == CSSM_OK) || (tpStopOn == CSSM_TP_STOP_ON_NONE)) {
		if(!didCertPolicy) {
			policyReturn = tp_policyVerify(kTPDefault,
				*this,
				clHand,
				cspHand,
				&outCertGroup,
				verifiedToRoot,
				verifiedViaTrustSetting,
				actionFlags,
				NULL,							// policyFieldData
				cred->Policy.PolicyControl);	// not currently used
			/* See if error is allowed, cert-chain-wide. */
			if((policyReturn != CSSM_OK) &&
				outCertGroup.isAllowedError(policyReturn)) {
				policyReturn = CSSM_OK;
			}
		}
		if( !didRevokePolicy &&							// no revoke policy yet
			( (policyReturn == CSSM_OK || 				// default cert policy OK
		      (tpStopOn == CSSM_TP_STOP_ON_NONE)) 		// keep going anyway
			)
		  ) {
			revokeVfyContext.policy = TP_CRL_POLICY_DEFAULT;
			CSSM_RETURN thisPolicyRtn = tpRevocationPolicyVerify(revokeVfyContext, 
				outCertGroup);
			if((thisPolicyRtn != CSSM_OK) &&
				outCertGroup.isAllowedError(thisPolicyRtn)) {
				thisPolicyRtn = CSSM_OK;
			}
			if((thisPolicyRtn != CSSM_OK) && (policyReturn == CSSM_OK)) {
				policyReturn = thisPolicyRtn;
			}
		
		}
	}	/* default policy opts */
	
	delete crlGroup;
	
	/* return evidence - i.e., constructed chain - if asked to */
	if(VerifyContextResult != NULL) {
		/*
		 * VerifyContextResult->Evidence[0] : CSSM_TP_APPLE_EVIDENCE_HEADER
		 * VerifyContextResult->Evidence[1] : CSSM_CERTGROUP
		 * VerifyContextResult->Evidence[2] : CSSM_TP_APPLE_EVIDENCE_INFO
		 */
		VerifyContextResult->NumberOfEvidences = 3;
		VerifyContextResult->Evidence = 
			(CSSM_EVIDENCE_PTR)calloc(3, sizeof(CSSM_EVIDENCE));

		CSSM_TP_APPLE_EVIDENCE_HEADER *hdr = 
			(CSSM_TP_APPLE_EVIDENCE_HEADER *)malloc(
				sizeof(CSSM_TP_APPLE_EVIDENCE_HEADER));
		hdr->Version = CSSM_TP_APPLE_EVIDENCE_VERSION;
		CSSM_EVIDENCE_PTR ev = &VerifyContextResult->Evidence[0];
		ev->EvidenceForm = CSSM_EVIDENCE_FORM_APPLE_HEADER;
		ev->Evidence = hdr;
		
		ev = &VerifyContextResult->Evidence[1];
		ev->EvidenceForm = CSSM_EVIDENCE_FORM_APPLE_CERTGROUP;
		ev->Evidence = outCertGroup.buildCssmCertGroup();
		
		ev = &VerifyContextResult->Evidence[2];
		ev->EvidenceForm = CSSM_EVIDENCE_FORM_APPLE_CERT_INFO;
		ev->Evidence = outCertGroup.buildCssmEvidenceInfo();
	}
	else {
		/* caller responsible for freeing these if they are for evidence.... */
		outCertGroup.freeDbRecords();
	}
	CSSM_RETURN outErr = outCertGroup.getReturnCode(constructReturn, policyReturn,
		actionFlags);
	
	if(outErr) {
		CssmError::throwMe(outErr);
	}
}


