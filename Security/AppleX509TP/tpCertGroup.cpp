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
#include "tpPolicies.h"
#include "tpdebugging.h"
#include "rootCerts.h"
#include <Security/oidsalg.h>
#include <Security/cssmapple.h>

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
	TPCertGroup *tpCertGroup;
	CertGroupConstructPriv(clHand,
		cspHand,
		DBList,
		ConstructParams,
		CertGroupFrag,
		CSSM_FALSE,			// allowExpired
		NULL,				// cssmTimeStr
		tpCertGroup);
	CertGroup = tpCertGroup->buildCssmCertGroup();
	delete tpCertGroup;	
}


/* 
 * Private version of CertGroupConstruct, used by CertGroupConstruct and
 * CertGroupVerify. Returns a TP-style TPCertGroup for further processing.
 * This only throws CSSM-style exceptions in the following cases: 
 * 
 *  -- input parameter errors
 *  -- the first (leaf) cert is bad (doesn't parse, expired, not valid yet).
 *
 *  All other cert-related errors simply result in the bad cert being ignored.
 *  Other exceptions are gross system errors like malloc failure.
 */
void AppleTPSession::CertGroupConstructPriv(CSSM_CL_HANDLE clHand,
		CSSM_CSP_HANDLE cspHand,
		const CSSM_DL_DB_LIST &DBList,
		const void *ConstructParams,
		const CSSM_CERTGROUP &CertGroupFrag,
		CSSM_BOOL allowExpired,
		const char *cssmTimeStr,					// May be NULL
		TPCertGroup *&CertGroup)
{
	TPCertGroup			*inCertGroup;				// unordered input certs
	TPCertGroup			*outCertGroup;				// ordered, verified output certs

	/*
	 * subjectCert refers to the cert we're currently trying to verify. It's either
	 * an element in inCertGroup (if we're verifying a cert from the incoming
	 * CertGroupFrag) or dbSubject (if we're verifying a cert which came from a DB).
	 *
	 * Similarly, issuerCert, when non-NULL, points to a cert which has just
	 * been located as a verifiable issuer of subjectCert. It points to either
	 * an element in inCertGroup or to dbIssuer.
	 */
	TPCertInfo			*subjectCert;				// the one we're working on
	TPCertInfo			*issuerCert = NULL;			// verified as next one in chain
	TPCertInfo			*certInfo;					// working cert
	unsigned			certDex;					// index into certInfo
	CSSM_RETURN			crtn;
	CSSM_RETURN			outErr = CSSM_OK;
	
	/* verify input args */
	if(cspHand == CSSM_INVALID_HANDLE) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CSP_HANDLE);
	}
	if(clHand == CSSM_INVALID_HANDLE)	{
		CssmError::throwMe(CSSMERR_TP_INVALID_CL_HANDLE);
	}
	if( (CertGroupFrag.NumCerts == 0) ||				// list is empty
	    (CertGroupFrag.CertGroupType != CSSM_CERTGROUP_DATA) ||
	    (CertGroupFrag.GroupList.CertList[0].Data == NULL) ||	// first cert empty
	    (CertGroupFrag.GroupList.CertList[0].Length == 0)) {		// first cert empty
		CssmError::throwMe(CSSMERR_CL_INVALID_CERTGROUP_POINTER);
	}
	switch(CertGroupFrag.CertType) {
		case CSSM_CERT_X_509v1:
		case CSSM_CERT_X_509v2:
		case CSSM_CERT_X_509v3:
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
	}
	switch(CertGroupFrag.CertEncoding) {
		case CSSM_CERT_ENCODING_BER:
		case CSSM_CERT_ENCODING_DER:
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
	}
	
	/* 
	 * Set up incoming and outgoing TPCertGrorups. 
	 */
	inCertGroup  = new TPCertGroup(*this, CertGroupFrag.NumCerts - 1);
	outCertGroup = new TPCertGroup(*this, CertGroupFrag.NumCerts);
	
	/*
	 * Parse first (leaf) cert. Note that this cert is special: if it's bad we abort
	 * immediately; otherwise it goes directly into outCertGroup.
	 */
	try {
		certInfo = new TPCertInfo(
			&CertGroupFrag.GroupList.CertList[0],
			clHand,
			cssmTimeStr);
		certInfo->index(0);
	}
	catch(const CssmError &cerr) {
		outErr = CSSMERR_TP_INVALID_CERTIFICATE;
		goto abort;
	}
	catch(...) {
		/* everything else is way fatal */
		throw;
	}
	
	/* Add to outCertGroup even if it's not current */
	outCertGroup->appendCert(certInfo);
	
	#if 	TP_CERT_CURRENT_CHECK_INLINE
	/* verify this first one is current */
	outErr = certInfo->isCurrent(allowExpired);
	if(outErr) {
		goto abort;
	}
	#endif
	
	/* this'll be the first subject cert in the main loop */
	subjectCert = certInfo;
	
	/* 
	 * Add remaining input certs to inCertGroup. Note that this lets us 
	 * skip bad incoming certs right away.
	 */
	for(certDex=1; certDex<CertGroupFrag.NumCerts; certDex++) {
		try {
			certInfo = new TPCertInfo(&CertGroupFrag.GroupList.CertList[certDex],
				clHand,
				cssmTimeStr);
		}
		catch (...) {
			/* just ignore this cert */
			continue;
		}
		certInfo->index(certDex);
		inCertGroup->appendCert(certInfo);
	}
	
	/*** main loop ***
	 *
	 * On entry, we have two TPCertGroups. InCertGroup contains n-1 certs, where n 
	 * is the size of the CertGroupFrag passed to us by the caller. The certs in
	 * inCertGroup are unordered but are known to be parseable, CL-cacheable certs.
	 * OutGroupCert contains one cert, the incoming leaf cert.
	 *
	 * The job in this loop is to build an ordered, verified cert chain in  
	 * outCertGroup out of certs from inCertGroup and/or DBList. As good certs
	 * are found in inCertGroup, they're removed from that TPCertGroup. On exit
	 * we delete inCertGroup, which deletes all the remaining TPCertInfo's in it. 
	 * The constructed outCertGroup is returned to the caller. 
	 *
	 * Exit loop on: 
	 *   -- find a root cert in the chain
	 *   -- memory error
	 *   -- or no more certs to add to chain. 
	 */
	for(;;) {
		/* top of loop: subjectCert is the cert we're trying to verify. */
		
		/* is this a root cert?  */
		if(subjectCert->isSelfSigned()) {
			/*
			 * Verify this alleged root cert. We're at the end of the chain no 
			 * matter what happens here. 
			 * Note we already validated before/after when this was tested
			 * as issuer (or, if it's the leaf cert, before we entered this loop). 
			 */ 
			outErr = tp_VerifyCert(clHand,
				cspHand,
				subjectCert,
				subjectCert,
				CSSM_FALSE,		// checkIssuerCurrent
				CSSM_TRUE);		// allowExpired, don't care
			break;
		}
		
		/* Search unused incoming certs to find an issuer */
		for(certDex=0; certDex<inCertGroup->numCerts(); certDex++) {
			certInfo = inCertGroup->certAtIndex(certDex);
			
			/* potential issuer - names match? */
			if(tpIsSameName(subjectCert->issuerName(), certInfo->subjectName())) {
				/* yep, do a sig verify with "not before/after" check */
				crtn = tp_VerifyCert(clHand,
					cspHand,
					subjectCert,
					certInfo,
					CSSM_TRUE,
					allowExpired);			
				switch(crtn) {
					case CSSM_OK:
						/* YES! We'll add it to outCertGroup below...*/
						issuerCert = certInfo;
						inCertGroup->removeCertAtIndex(certDex);
						goto issuerLoopEnd;
					case CSSMERR_TP_CERT_NOT_VALID_YET:
					case CSSMERR_TP_CERT_EXPIRED:
						/* special case - abort immediateley (note the cert
						 * sig verify succeeded.) */
						/*** for now we include this in the evidence ***/
						outCertGroup->appendCert(subjectCert);
						outErr = crtn;
						goto abort;
					default:
						/* just skip this one and keep looking */
						break;
				}
			} 	/* names match */
		} 		/* searching inCertGroup for issuer */
		
issuerLoopEnd:

		#if	TP_DL_ENABLE
		if(issuerCert == NULL) {
			/* Issuer not in incoming cert group. Search DBList. */
			CSSM_BOOL subjectExpired = CSSM_FALSE;
			issuerCert = tpFindIssuer(*this,
				clHand,
				cspHand,
				subjectCert,
				subjectCert->issuerName(),
				&DBList,
				cssmTimeStr,
				&subjectExpired);
			if(subjectExpired) {
				/* special case - abort immediately */
				outErr = subjectExpired;
				goto abort;
			}
		}	/*  Issuer not in incoming cert group */
		#endif	/* TP_DL_ENABLE */
		
		if(issuerCert == NULL) {
			/* end of search, broken chain */
			break;
		}
		
		/*
		 * One way or the other, we've found a cert which verifies subjectCert.
		 * Add the issuer to outCertGroup and make it the new subjectCert for
		 * the next pass.
		 */
		outCertGroup->appendCert(issuerCert);
		subjectCert = issuerCert;
		issuerCert = NULL;
	}	/* main loop */
	
abort:
	delete inCertGroup;
	CertGroup = outCertGroup;
	if(outErr) {
		CssmError::throwMe(outErr);
	}
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
 *---------------------------------------------------------------------------*/

void AppleTPSession::CertGroupVerify(CSSM_CL_HANDLE clHand,
		CSSM_CSP_HANDLE cspHand,
		const CSSM_CERTGROUP &CertGroupToBeVerified,
		const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT_PTR VerifyContextResult)
{
	unsigned				i;
	TPCertInfo				*lastCert;
	CSSM_BOOL				verifiedToRoot = CSSM_FALSE;
	TPPolicy				policy;
	CSSM_RETURN				constructReturn = CSSM_OK;
	CSSM_RETURN				policyReturn = CSSM_OK;
	CSSM_RETURN				crtn;
	const CSSM_TP_CALLERAUTH_CONTEXT *cred;
	CSSM_OID_PTR 			oid = NULL;
	TPCertGroup 			*tpCertGroup = NULL;	// created by
													// CertGroupConstructPriv
	TPCertInfo 				*certInfo = NULL;
	CSSM_BOOL				allowExpired = CSSM_FALSE;
	CSSM_BOOL				allowExpiredRoot = CSSM_FALSE;
	/* declare volatile as compiler workaround to avoid caching in CR4 */
	const CSSM_APPLE_TP_ACTION_DATA * volatile actionData = NULL;
	const CSSM_APPLE_TP_SSL_OPTIONS *sslOpts = NULL;
	const CSSM_DATA 		*fieldVal;
	CSSM_TIMESTRING			cssmTimeStr;
	
	/* verify input args, skipping the ones checked by CertGroupConstruct */
	if((VerifyContext == NULL) || (VerifyContext->Cred == NULL)) {
		/* the spec says that this is optional but we require it */
			CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	cred = VerifyContext->Cred;
	
	/* Check out requested policies */
	switch(cred->Policy.NumberOfPolicyIds) {
		case 0:
			/* default */
			policy = kTPDefault;
			break;			
	    case 1:
	    	if(cred->Policy.PolicyIds == NULL) {
				CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
	    	}
			fieldVal = &cred->Policy.PolicyIds->FieldValue;
			oid      = &cred->Policy.PolicyIds->FieldOid;
	    	if(tpCompareOids(oid, &CSSMOID_APPLE_ISIGN)) {
				policy = kTPiSign;
				/* no options */
				if(fieldVal->Data != NULL) {
					CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
				}
	    	}
	    	else if(tpCompareOids(oid, &CSSMOID_APPLE_X509_BASIC)) {
				policy = kTPx509Basic;
				/* no options */
				if(fieldVal->Data != NULL) {
					CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
				}
	    	}
	    	else if(tpCompareOids(oid, &CSSMOID_APPLE_TP_SSL)) {
				policy = kTP_SSL;
				/* SSL-specific options */
				sslOpts = (CSSM_APPLE_TP_SSL_OPTIONS *)fieldVal->Data;
				if(sslOpts != NULL) {
					switch(sslOpts->Version) {
						case CSSM_APPLE_TP_SSL_OPTS_VERSION:
							if(fieldVal->Length != 
									sizeof(CSSM_APPLE_TP_SSL_OPTIONS)) {
								CssmError::throwMe(
									CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
							}
							break;
						/* handle backwards compatibility here if necessary */
						default:
							CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
					}
				}
	    	}
	    	else {
	    		/* unknown TP OID */
				CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
	    	}
	    	break;
		default:
			/* only zero or one allowed */
			CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
	} 
	
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
			/* handle backwards versions here if we ever go byond version 0 */
			default:
				CssmError::throwMe(CSSMERR_TP_INVALID_ACTION_DATA);
		}
		if(actionData->ActionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED) {
			allowExpired = CSSM_TRUE;
		}
		if(actionData->ActionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED_ROOT) {
			allowExpiredRoot = CSSM_TRUE;
		}
	}
	
	/* optional, may be NULL */
	cssmTimeStr = cred->VerifyTime;
	
	/* now the args we can't deal with */
	if(cred->CallerCredentials != NULL) {
			CssmError::throwMe(CSSMERR_TP_INVALID_CALLERAUTH_CONTEXT_POINTER);
	}
	/* FIXME - ANY OTHERS? */
	
	/* get verified (possibly partial) outCertGroup - error is fatal */
	/* BUT: we still return partial evidence if asked to...from now on. */
	try {
		CertGroupConstructPriv(
			clHand,
			cspHand,
			*cred->DBList, 		// not optional to Construct!
			NULL,
			CertGroupToBeVerified,
			allowExpired,
			cssmTimeStr,
			tpCertGroup);
	}
	catch(const CssmError &cerr) {
		constructReturn = cerr.cssmError();
		/* abort if no certs found */
		if((tpCertGroup == NULL) || (tpCertGroup->numCerts() == 0)) {
			CssmError::throwMe(constructReturn);
		}
		/* else press on, collecting as much info as we can */
	}
	/* others are way fatal */
	CASSERT(tpCertGroup != NULL);
	CASSERT(tpCertGroup->numCerts() >= 1);
	
	/* subsequent errors and returns to out: */

	/*
	 * Case 1: last cert in outCertGroup is a root cert. See if 
	 * the root cert is in AnchorCerts.
	 * Note that TP_CertGroupConstruct did the actual root 
	 * self-verify test.
	 */
	lastCert = tpCertGroup->lastCert();
	if(lastCert->isSelfSigned()) {
		verifiedToRoot = CSSM_TRUE;
		
		/* see if that root cert is identical to one of the anchor certs */
		for(i=0; i<cred->NumberOfAnchorCerts; i++) {
			if(tp_CompareCerts(lastCert->certData(), &cred->AnchorCerts[i])) {
				/* one fully successful return */
				goto out;
			}
		}
		
		/* verified to a root cert which is not an anchor */
		constructReturn = CSSMERR_TP_INVALID_ANCHOR_CERT;
		goto out;
	}

	/* try to validate lastCert with anchor certs */
	for(i=0; i<cred->NumberOfAnchorCerts; i++) {
		try {
			certInfo = new TPCertInfo(&cred->AnchorCerts[i],
				clHand,
				cssmTimeStr);
		}
		catch(...) {
			/* bad anchor cert - ignore it */
			continue;
		}
		if(!tpIsSameName(lastCert->issuerName(), certInfo->subjectName())) {
			/* not this anchor */
			delete certInfo;
			continue;
		}
		crtn = tp_VerifyCert(clHand, 
			cspHand, 
			lastCert, 
			certInfo, 
			CSSM_TRUE,				// check not/before of anchor
			allowExpired);
		switch(crtn) {
			case CSSM_OK:
				/*  The other normal fully successful return. */
				if(certInfo->isSelfSigned()) {
					verifiedToRoot = CSSM_TRUE;	
				}
				
				/*
				 * One more thing: add this anchor cert to the Evidence chain
				 */
				try {
					tpCertGroup->appendCert(certInfo);
					certInfo->isAnchor(true);
					certInfo->index(i);
				}
				catch(...) {
					/* shoot - must be memory error */
					verifiedToRoot = CSSM_FALSE;
					delete certInfo;
					constructReturn = CSSMERR_TP_MEMORY_ERROR;
				}
				goto out;
				
			#if 	TP_CERT_CURRENT_CHECK_INLINE
			case CSSMERR_TP_CERT_NOT_VALID_YET:
			case CSSMERR_TP_CERT_EXPIRED:
				/* special case - abort immediateley */
				delete certInfo;
				constructReturn = crtn;
				goto out;
			#endif	/* TP_CERT_CURRENT_CHECK_INLINE */
			
			default:
				/* continue to next anchor */
				delete certInfo;
				break;
		}
	}	/* for each anchor */
	
	/* partial chain, no root, not verifiable by anchor */
	constructReturn = CSSMERR_TP_NOT_TRUSTED;

	/* common exit - error or success */
out:
	/* 
	 * Do further policy verification if appropriate.
	 *
	 * SSL: CSSMERR_TP_NOT_TRUSTED and CSSMERR_TP_INVALID_ANCHOR_CERT
	 * are both special cases which can result in full success. 
	 */
	#if 	TP_ROOT_CERT_ENABLE
	if((policy == kTP_SSL) && 
	   (constructReturn == CSSMERR_TP_NOT_TRUSTED) &&
	   (actionData != NULL) &&
	   (actionData->ActionFlags & 0x80000000)) {// The secret "enable root cert check" flag
		/* see if last cert can be verified by an embedded SSL root */
		certInfo = tpCertGroup->lastCert();
		CSSM_BOOL brtn = tp_verifyWithSslRoots(clHand, 
			cspHand, 
			certInfo);
		if(brtn) {
			/* SSL success with no incoming root */
			/* note unknown incoming root (INVALID_ANCHOR_CERT) is handled
			 * below, after tp_policyVerify */
			constructReturn = CSSM_OK;
		}
	}
	#endif	/* TP_ROOT_CERT_ENABLE */
	if(tpCertGroup->numCerts() != 0) {
		/* policy check if we saw even one cert */
		policyReturn = tp_policyVerify(policy,
			*this,
			clHand,
			cspHand,
			tpCertGroup,
			verifiedToRoot,
			actionData,
			sslOpts,
			cred->Policy.PolicyControl);		// not currently used
		#if TP_ROOT_CERT_ENABLE
		if((policyReturn == CSSM_OK) &&
		   (constructReturn == CSSMERR_TP_INVALID_ANCHOR_CERT) && 
		   (policy == kTP_SSL) &&
		   (actionData != NULL) &&
		   (actionData->ActionFlags & 0x80000000)) {
			/* The secret "enable root cert check" flag... */
			/* SSL - found a good anchor, move to full success */
			constructReturn = CSSM_OK;
		}
		#endif
	}

	/* return evidence - i.e., current chain - if asked to */
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
			(CSSM_TP_APPLE_EVIDENCE_HEADER *)malloc(sizeof(CSSM_TP_APPLE_EVIDENCE_HEADER));
		hdr->Version = CSSM_TP_APPLE_EVIDENCE_VERSION;
		CSSM_EVIDENCE_PTR ev = &VerifyContextResult->Evidence[0];
		ev->EvidenceForm = CSSM_EVIDENCE_FORM_APPLE_HEADER;
		ev->Evidence = hdr;
		
		ev = &VerifyContextResult->Evidence[1];
		ev->EvidenceForm = CSSM_EVIDENCE_FORM_APPLE_CERTGROUP;
		ev->Evidence = tpCertGroup->buildCssmCertGroup();
		
		ev = &VerifyContextResult->Evidence[2];
		ev->EvidenceForm = CSSM_EVIDENCE_FORM_APPLE_CERT_INFO;
		ev->Evidence = tpCertGroup->buildCssmEvidenceInfo();

	}
	CSSM_RETURN outErr = tpCertGroup->getReturnCode(constructReturn,
		allowExpired, allowExpiredRoot, policyReturn);
		
	/* delete (internal use only) TPCertGroup */
	delete tpCertGroup;
	if(outErr) {
		CssmError::throwMe(outErr);
	}
}


