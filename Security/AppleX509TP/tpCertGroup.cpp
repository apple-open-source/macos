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
#include "rootCerts.h"
#include "tpCrlVerify.h"
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
	TPCertGroup outCertGroup(*this, TGO_Caller);
	TPCertGroup inCertGroup(CertGroupFrag, 
		clHand, 
		cspHand, 
		*this, 
		NULL,		// cssmTimeStr
		true, 		// firstCertMustBeValid
		TGO_Group);
		
	/* set up for disposal of TPCertInfos created by CertGroupConstructPriv */
	TPCertGroup			certsToBeFreed(*this, TGO_Group);
	
	CSSM_RETURN constructReturn = CSSM_OK;
	CSSM_BOOL verifiedToRoot;		// not used
	CSSM_BOOL verifiedToAnchor;		// not used
		
	try {
		CertGroupConstructPriv(clHand,
			cspHand,
			inCertGroup,
			&DBList,
			NULL,				// cssmTimeStr
			/* no anchors */
			0, NULL,
			0,					// actionFlags
			certsToBeFreed,
			verifiedToRoot,
			verifiedToAnchor,
			outCertGroup);
	}
	catch(const CssmError &cerr) {
		constructReturn = cerr.cssmError();
		/* abort if no certs found */
		if(outCertGroup.numCerts() == 0) {
			CssmError::throwMe(constructReturn);
		}
	}
	CertGroup = outCertGroup.buildCssmCertGroup();
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
		
		/* currently, only CSSM_TP_ACTION_FETCH_CERT_FROM_NET is 
		 * interesting */
		CSSM_APPLE_TP_ACTION_FLAGS	actionFlags,
		/* 
		 * Certs to be freed by caller (i.e., TPCertInfo which we allocate
		 * as a result of using a cert from anchorCerts of dbList) are added
		 * to this group.
		 */
		TPCertGroup				&certsToBeFreed,

		/* returned */
		CSSM_BOOL				&verifiedToRoot,	// end of chain self-verifies
		CSSM_BOOL				&verifiedToAnchor,	// end of chain in anchors
		TPCertGroup 			&outCertGroup)		// RETURNED
{
	TPCertInfo			*subjectCert;				// the one we're working on
	CSSM_RETURN			outErr = CSSM_OK;
	
	/* this'll be the first subject cert in the main loop */
	subjectCert = inCertGroup.certAtIndex(0);

	/* Append leaf cert to outCertGroup */
	outCertGroup.appendCert(subjectCert);
	subjectCert->isLeaf(true);
	outCertGroup.setAllUnused();
	
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
		NULL,				// gatheredCerts - none here
		CSSM_TRUE,			// subjectIsInGroup - enables root check on
							//    subject cert
		actionFlags,
		verifiedToRoot,	
		verifiedToAnchor);
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
	CSSM_RETURN				constructReturn = CSSM_OK;
	CSSM_RETURN				policyReturn = CSSM_OK;
	const CSSM_TP_CALLERAUTH_CONTEXT *cred;
	CSSM_BOOL				allowExpired = CSSM_FALSE;
	CSSM_BOOL				allowExpiredRoot = CSSM_FALSE;
	/* declare volatile as compiler workaround to avoid caching in CR4 */
	const CSSM_APPLE_TP_ACTION_DATA * volatile actionData = NULL;
	CSSM_TIMESTRING			cssmTimeStr;
	CSSM_APPLE_TP_ACTION_FLAGS	actionFlags = 0;
	CSSM_TP_STOP_ON 		tpStopOn = 0;
	
	/* keep track of whether we did policy checking; if not, we do defaults */
	bool					didCertPolicy = false;
	bool					didRevokePolicy = false;
	
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
		if(actionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED) {
			allowExpired = CSSM_TRUE;
		}
		if(actionData->ActionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED_ROOT) {
			allowExpiredRoot = CSSM_TRUE;
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
	
	/* get verified (possibly partial) outCertGroup - error is fatal */
	/* BUT: we still return partial evidence if asked to...from now on. */
	TPCertGroup outCertGroup(*this, 
		TGO_Caller);		// certs are owned by inCertGroup
	TPCertGroup inCertGroup(CertGroupToBeVerified, clHand, cspHand, *this, 
		cssmTimeStr, 		// optional 'this' time
		true, 				// firstCertMustBeValid
		TGO_Group);	
		
	/* set up for disposal of TPCertInfos created by CertGroupConstructPriv */
	TPCertGroup	certsToBeFreed(*this, TGO_Group);
	
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
			certsToBeFreed,
			verifiedToRoot, 
			verifiedToAnchor,
			outCertGroup);
	}
	catch(const CssmError &cerr) {
		constructReturn = cerr.cssmError();
		/* abort if no certs found */
		if(outCertGroup.numCerts() == 0) {
			CssmError::throwMe(constructReturn);
		}
		/* else press on, collecting as much info as we can */
	}
	/* others are way fatal */
	assert(outCertGroup.numCerts() >= 1);
	
	/* Infer interim status from return values */
	if((constructReturn != CSSMERR_TP_CERTIFICATE_CANT_OPERATE) && 
	   (constructReturn != CSSMERR_TP_INVALID_CERT_AUTHORITY)) {
		/* these returns do not get overridden */
		if(verifiedToAnchor) {
			/* full success; anchor doesn't have to be root */
			constructReturn = CSSM_OK;
		}
		else if(verifiedToRoot) {
			/* verified to root which is not an anchor */
			constructReturn = CSSMERR_TP_INVALID_ANCHOR_CERT;
		}
		else {
			/* partial chain, no root, not verifiable by anchor */
			constructReturn = CSSMERR_TP_NOT_TRUSTED;
		}
	}
	
	/* 
	 * CSSMERR_TP_NOT_TRUSTED and CSSMERR_TP_INVALID_ANCHOR_CERT
	 * are both special cases which can result in full success
	 * when CSSM_TP_USE_INTERNAL_ROOT_CERTS is enabled. 
	 */
	#if 	TP_ROOT_CERT_ENABLE
	if(actionFlags & CSSM_TP_USE_INTERNAL_ROOT_CERTS) {
	   // The secret "enable root cert check" flag
	   
		TPCertInfo *lastCert = outCertGroup.lastCert();
		if(constructReturn == CSSMERR_TP_NOT_TRUSTED) {
			/* 
			 * See if last (non-root) cert can be verified by 
			 * an embedded root */
			assert(lastCert != NULL);
			CSSM_BOOL brtn = tp_verifyWithKnownRoots(clHand, 
				cspHand, 
				lastCert);
			if(brtn) {
				/* success with no incoming root, actually common (successful) case */
				constructReturn = CSSM_OK;
			}
		}
		else if(constructReturn == CSSMERR_TP_INVALID_ANCHOR_CERT) {
			/* is the end cert the same as one of our trusted roots? */
			assert(lastCert != NULL);
			bool brtn = tp_isKnownRootCert(lastCert, clHand);
			if(brtn) {
				constructReturn = CSSM_OK;
			}
		}
	}
	#endif	/* TP_ROOT_CERT_ENABLE */
	
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
			cssmTimeStr,
			TGO_Group);
	}
	catch(const CssmError &cerr) {
		CSSM_RETURN cr = cerr.cssmError();
		/* I don't see a straightforward way to report this error,
		 * other than adding it to the leaf cert's status... */
		outCertGroup.certAtIndex(0)->addStatusCode(cr);
		tpDebug("CertGroupVerify: error constructing CrlGroup; continuing\n");
	}
	/* others are way fatal */

	TPCrlVerifyContext crlVfyContext(*this,
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
		&certsToBeFreed,
		cred->DBList,
		kCrlNone,			// policy, varies per policy
		actionFlags,
		0);					// crlOptFlags, varies per policy

	/* true if we're to execute tp_policyVerify at end of loop */
	bool doPolicyVerify;
	
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
		sslOpts = NULL;
		
		/* first the basic cert policies */
		if(tpCompareOids(oid, &CSSMOID_APPLE_TP_SSL)) {
			tpPolicy = kTP_SSL;
			doPolicyVerify = true;
			/* and do the tp_policyVerify() call below */
		}

		else if(tpCompareOids(oid, &CSSMOID_APPLE_X509_BASIC)) {
			/* no options */
			if(fieldVal->Data != NULL) {
				policyReturn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
				break;
			}
			tpPolicy = kTPx509Basic;
			doPolicyVerify = true;
		}

		else if(tpCompareOids(oid, &CSSMOID_APPLE_TP_SMIME)) {
			tpPolicy = kTP_SMIME;
			doPolicyVerify = true;
		}

		else if(tpCompareOids(oid, &CSSMOID_APPLE_TP_EAP)) {
			/* treated here exactly the same as SSL */
			tpPolicy = kTP_SSL;
			doPolicyVerify = true;
		}
		
		else if(tpCompareOids(oid, &CSSMOID_APPLE_ISIGN)) {
			/* no options */
			if(fieldVal->Data != NULL) {
				policyReturn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
				break;
			}
			tpPolicy = kTPiSign;
			doPolicyVerify = true;
		}
		
		/* now revocation policies */
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
			crlVfyContext.policy = kCrlBasic;
			crlVfyContext.crlOpts = crlOpts;

			thisPolicyRtn = tpVerifyCertGroupWithCrls(outCertGroup,
				crlVfyContext);
			didRevokePolicy = true;
		}
		/* etc. - add more policies here */
		else {
			/* unknown TP policy OID */
			policyReturn = CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
			break;
		}
		
		/* common tp_policyVerify call */
		if(doPolicyVerify) {
			thisPolicyRtn = tp_policyVerify(tpPolicy,
				*this,
				clHand,
				cspHand,
				&outCertGroup,
				verifiedToRoot,
				actionFlags,
				fieldVal,
				cred->Policy.PolicyControl);	// not currently used
			didCertPolicy = true;
		}
		
		if(thisPolicyRtn) {
			/* Policy error. First remember the error if it's the first policy
			 * error we'veÊseen. */
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
				actionFlags,
				NULL,							// policyFieldData
				cred->Policy.PolicyControl);	// not currently used
		}
		if( !didRevokePolicy &&							// no revoke policy yet
			( (policyReturn == CSSM_OK || 				// default cert policy OK
		      (tpStopOn == CSSM_TP_STOP_ON_NONE)) 		// keep going anyway
			)
		  ) {

			crlVfyContext.policy = TP_CRL_POLICY_DEFAULT;
			crlVfyContext.crlOpts = NULL;
			CSSM_RETURN thisPolicyRtn = tpVerifyCertGroupWithCrls(outCertGroup,
				crlVfyContext);
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
	CSSM_RETURN outErr = outCertGroup.getReturnCode(constructReturn,
		allowExpired, allowExpiredRoot, policyReturn);
		
	if(outErr) {
		CssmError::throwMe(outErr);
	}
}


