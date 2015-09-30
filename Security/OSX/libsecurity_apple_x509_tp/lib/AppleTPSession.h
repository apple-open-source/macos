/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
 * AppleTPSession.h - TP session functions.
 * 
 */
 
#ifndef _H_APPLE_TP_SESSION
#define _H_APPLE_TP_SESSION

#include <security_cdsa_plugin/TPsession.h>
#include "TPCertInfo.h"

#define REALLOC_WORKAROUND	0
#if 	REALLOC_WORKAROUND
#include <string.h>
#endif

class AppleTPSession : public TPPluginSession {

public:

	AppleTPSession(
		CSSM_MODULE_HANDLE theHandle,
		CssmPlugin &plug,
		const CSSM_VERSION &version,
		uint32 subserviceId,
		CSSM_SERVICE_TYPE subserviceType,
		CSSM_ATTACH_FLAGS attachFlags,
		const CSSM_UPCALLS &upcalls);

	~AppleTPSession();
	
	#if		REALLOC_WORKAROUND
	void *realloc(void *oldp, size_t size) {
		void *newp = malloc(size);
		memmove(newp, oldp, size);
		free(oldp);
		return newp;
	}
	#endif	/* REALLOC_WORKAROUND */

	/* methods declared in TPabstractSession.h */
	void CertCreateTemplate(CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD CertFields[],
         CssmData &CertTemplate);
	void CrlVerify(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ENCODED_CRL &CrlToBeVerified,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT *RevokerVerifyResult);
	void CertReclaimKey(const CSSM_CERTGROUP &CertGroup,
         uint32 CertIndex,
         CSSM_LONG_HANDLE KeyCacheHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry);
	void CertGroupVerify(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_CERTGROUP &CertGroupToBeVerified,
         const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR VerifyContextResult);
	void CertGroupConstruct(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_DL_DB_LIST &DBList,
         const void *ConstructParams,
         const CSSM_CERTGROUP &CertGroupFrag,
         CSSM_CERTGROUP_PTR &CertGroup);
	void CertSign(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CssmData &CertTemplateToBeSigned,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *SignerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT *SignerVerifyResult,
         CssmData &SignedCert);
	void TupleGroupToCertGroup(CSSM_CL_HANDLE CLHandle,
         const CSSM_TUPLEGROUP &TupleGroup,
         CSSM_CERTGROUP_PTR &CertTemplates);
	void ReceiveConfirmation(const CssmData &ReferenceIdentifier,
         CSSM_TP_CONFIRM_RESPONSE_PTR &Responses,
         sint32 &ElapsedTime);
	void PassThrough(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DL_DB_LIST *DBList,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams);
	void CertRemoveFromCrlTemplate(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CssmData *OldCrlTemplate,
         const CSSM_CERTGROUP &CertGroupToBeRemoved,
         const CSSM_CERTGROUP &RevokerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT &RevokerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT &RevokerVerifyResult,
         CssmData &NewCrlTemplate);
	void CertRevoke(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CssmData *OldCrlTemplate,
         const CSSM_CERTGROUP &CertGroupToBeRevoked,
         const CSSM_CERTGROUP &RevokerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT &RevokerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT &RevokerVerifyResult,
         CSSM_TP_CERTCHANGE_REASON Reason,
         CssmData &NewCrlTemplate);
	void CertReclaimAbort(CSSM_LONG_HANDLE KeyCacheHandle);
	void CrlCreateTemplate(CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD CrlFields[],
         CssmData &NewCrlTemplate);
	void CertGroupToTupleGroup(CSSM_CL_HANDLE CLHandle,
         const CSSM_CERTGROUP &CertGroup,
         CSSM_TUPLEGROUP_PTR &TupleGroup);
	void SubmitCredRequest(const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
         CSSM_TP_AUTHORITY_REQUEST_TYPE RequestType,
         const CSSM_TP_REQUEST_SET &RequestInput,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
         sint32 &EstimatedTime,
         CssmData &ReferenceIdentifier);
	void FormRequest(const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
         CSSM_TP_FORM_TYPE FormType,
         CssmData &BlankForm);
	void CrlSign(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_ENCODED_CRL &CrlToBeSigned,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *SignerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT *SignerVerifyResult,
         CssmData &SignedCrl);
	void CertGroupPrune(CSSM_CL_HANDLE CLHandle,
         const CSSM_DL_DB_LIST &DBList,
         const CSSM_CERTGROUP &OrderedCertGroup,
         CSSM_CERTGROUP_PTR &PrunedCertGroup);
	void ApplyCrlToDb(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ENCODED_CRL &CrlToBeApplied,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *ApplyCrlVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT &ApplyCrlVerifyResult);
	void CertGetAllTemplateFields(CSSM_CL_HANDLE CLHandle,
         const CssmData &CertTemplate,
         uint32 &NumberOfFields,
         CSSM_FIELD_PTR &CertFields);
	void ConfirmCredResult(const CssmData &ReferenceIdentifier,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
         const CSSM_TP_CONFIRM_RESPONSE &Responses,
         const CSSM_TP_AUTHORITY_ID *PreferredAuthority);
	void FormSubmit(CSSM_TP_FORM_TYPE FormType,
         const CssmData &Form,
         const CSSM_TP_AUTHORITY_ID *ClearanceAuthority,
         const CSSM_TP_AUTHORITY_ID *RepresentedAuthority,
         AccessCredentials *Credentials);
	void RetrieveCredResult(const CssmData &ReferenceIdentifier,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
         sint32 &EstimatedTime,
         CSSM_BOOL &ConfirmationRequired,
         CSSM_TP_RESULT_SET_PTR &RetrieveOutput);

private:
	void CertGroupConstructPriv(CSSM_CL_HANDLE clHand,
		CSSM_CSP_HANDLE 		cspHand,
		TPCertGroup 			&inCertGroup,
		const CSSM_DL_DB_LIST 	*DBList,			// optional here
		const char 				*cssmTimeStr,		// optional
		uint32 					numAnchorCerts,		// optional
		const CSSM_DATA			*anchorCerts,
		
		/* CSSM_TP_ACTION_FETCH_CERT_FROM_NET, CSSM_TP_ACTION_TRUST_SETTINGS */
		CSSM_APPLE_TP_ACTION_FLAGS	actionFlags,
		
		/* optional user trust parameters */
		const CSSM_OID			*policyOid,
		const char				*policyStr,
		uint32					policyStrLen,
		CSSM_KEYUSE				keyUse,

		/* 
		 * Certs to be freed by caller (i.e., TPCertInfo which we allocate
		 * as a result of using a cert from anchorCerts of dbList) are added
		 * to this group.
		 */
		TPCertGroup				&certsToBeFreed,

		/* returned */
		CSSM_BOOL				&verifiedToRoot,	// end of chain self-verifies
		CSSM_BOOL				&verifiedToAnchor,	// end of chain in anchors
		CSSM_BOOL				&verifiedViaTrustSetting,	// chain ends per Trust Setting
		TPCertGroup 			&outCertGroup);		// RETURNED
			
	/* in tpCredRequest.cp */
	CSSM_X509_NAME * buildX509Name(const CSSM_APPLE_TP_NAME_OID *nameArray,
		unsigned numNames);
	void freeX509Name(CSSM_X509_NAME *top);
	CSSM_X509_TIME *buildX509Time(unsigned secondsFromNow);
	void freeX509Time(CSSM_X509_TIME *xtime);
	void refKeyToRaw(
		CSSM_CSP_HANDLE	cspHand,
		const CSSM_KEY	*refKey,	
		CSSM_KEY_PTR	rawKey);
	void makeCertTemplate(
		/* required */
		CSSM_CL_HANDLE			clHand,
		CSSM_CSP_HANDLE			cspHand,		// for converting ref to raw key
		uint32					serialNumber,
		const CSSM_X509_NAME	*issuerName,	
		const CSSM_X509_NAME	*subjectName,
		const CSSM_X509_TIME	*notBefore,	
		const CSSM_X509_TIME	*notAfter,	
		const CSSM_KEY			*subjectPubKey,
		const CSSM_OID			&sigOid,		// e.g., CSSMOID_SHA1WithRSA
		/* optional */
		const CSSM_DATA			*subjectUniqueId,
		const CSSM_DATA			*issuerUniqueId,
		CSSM_X509_EXTENSION		*extensions,
		unsigned				numExtensions,
		CSSM_DATA_PTR			&rawCert);

	void SubmitCsrRequest(
		const CSSM_TP_REQUEST_SET &RequestInput,
		const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
		sint32 					&EstimatedTime,	
		CssmData 				&ReferenceIdentifier);
		
	/* 
	 * Per-session storage of SubmitCredRequest results.
	 *
	 * A TpCredHandle is just an address of a cert, cast to a CSSM_INTPTR. It's 
	 * what ReferenceIdentifier.Data points to.
	 */ 
	typedef CSSM_INTPTR TpCredHandle;
	typedef std::map<TpCredHandle, 
				     const CSSM_DATA * /* the actual cert */ > credMap;
	credMap					tpCredMap;
	Mutex					tpCredMapLock;
	
	/* given a cert and a ReferenceIdentifier, fill in ReferenceIdentifier and 
	 * add it and the cert to tpCredMap. */
	void addCertToMap(
		const CSSM_DATA		*cert,
		CSSM_DATA_PTR		refId);
		
	/* given a ReferenceIdentifier, obtain associated cert and remove from the map */
	CSSM_DATA_PTR getCertFromMap(
		const CSSM_DATA		*refId);

};

#endif	/* _H_APPLE_TP_SESSION */
