/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * AppleDotMacTPSession.h - .mac TP session functions.
 * 
 * Created 4/7/2004 by Doug Mitchell.
 */
 
#ifndef _APPLE_DOT_MAC_TP_SESSION_H
#define _APPLE_DOT_MAC_TP_SESSION_H

#include <security_cdsa_plugin/TPsession.h>
#include "dotMacTpRpcGlue.h"

class AppleDotMacTPSession : public TPPluginSession {

public:

	AppleDotMacTPSession(
		CSSM_MODULE_HANDLE theHandle,
		CssmPlugin &plug,
		const CSSM_VERSION &version,
		uint32 subserviceId,
		CSSM_SERVICE_TYPE subserviceType,
		CSSM_ATTACH_FLAGS attachFlags,
		const CSSM_UPCALLS &upcalls);

	~AppleDotMacTPSession();
	
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
	void SubmitArchiveRequest(
		DotMacArchiveType archiveType,					// OID preparsed
		const CSSM_DATA &hostName,						// required
		CSSM_TP_AUTHORITY_REQUEST_TYPE RequestType,
		const CSSM_TP_REQUEST_SET &RequestInput,
		const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
		sint32 &EstimatedTime,
		CssmData &ReferenceIdentifier);
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

};

#endif	/* _APPLE_DOT_MAC_TP_SESSION_H */
