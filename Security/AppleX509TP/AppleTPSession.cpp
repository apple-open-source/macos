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
 * AppleTPSession.cpp - general session support and (mostly) unimplemented functions
 */

#include "AppleTPSession.h"

AppleTPSession::AppleTPSession(
	CSSM_MODULE_HANDLE theHandle,
	CssmPlugin &plug,
	const CSSM_VERSION &version,
	uint32 subserviceId,
	CSSM_SERVICE_TYPE subserviceType,
	CSSM_ATTACH_FLAGS attachFlags,
	const CSSM_UPCALLS &upcalls)
		: TPPluginSession(theHandle, plug, version, subserviceId, 
							subserviceType,attachFlags, upcalls)
{
	/* TBD session stuff here...
	mCspHand = CSSM_INVALID_HANDLE;
	mCspDlHand = CSSM_INVALID_HANDLE;
	...*/
}

AppleTPSession::~AppleTPSession()
{
	/* TBD 
	if(mCspHand != CSSM_INVALID_HANDLE) {
		CSSM_ModuleDetach(mCspHand); 
	}
	if(mCspDlHand != CSSM_INVALID_HANDLE) {
		CSSM_ModuleDetach(mCspDlHand); 
	}
	*/
}

void AppleTPSession::CertCreateTemplate(CSSM_CL_HANDLE CLHandle,
		uint32 NumberOfFields,
		const CSSM_FIELD CertFields[],
		CssmData &CertTemplate)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CrlVerify(CSSM_CL_HANDLE CLHandle,
		CSSM_CSP_HANDLE CSPHandle,
		const CSSM_ENCODED_CRL &CrlToBeVerified,
		const CSSM_CERTGROUP &SignerCertGroup,
		const CSSM_TP_VERIFY_CONTEXT &VerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT &RevokerVerifyResult)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CertReclaimKey(const CSSM_CERTGROUP &CertGroup,
		uint32 CertIndex,
		CSSM_LONG_HANDLE KeyCacheHandle,
		CSSM_CSP_HANDLE CSPHandle,
		const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

/*** CertGroupVerify, CertGroupConstruct in TPCertGroup.cpp ***/

void AppleTPSession::CertSign(CSSM_CL_HANDLE CLHandle,
		CSSM_CC_HANDLE CCHandle,
		const CssmData &CertTemplateToBeSigned,
		const CSSM_CERTGROUP &SignerCertGroup,
		const CSSM_TP_VERIFY_CONTEXT &SignerVerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT &SignerVerifyResult,
		CssmData &SignedCert)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::TupleGroupToCertGroup(CSSM_CL_HANDLE CLHandle,
		const CSSM_TUPLEGROUP &TupleGroup,
		CSSM_CERTGROUP_PTR &CertTemplates)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::ReceiveConfirmation(const CssmData &ReferenceIdentifier,
		CSSM_TP_CONFIRM_RESPONSE_PTR &Responses,
		sint32 &ElapsedTime)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::PassThrough(CSSM_CL_HANDLE CLHandle,
		CSSM_CC_HANDLE CCHandle,
		const CSSM_DL_DB_LIST *DBList,
		uint32 PassThroughId,
		const void *InputParams,
		void **OutputParams)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CertRemoveFromCrlTemplate(CSSM_CL_HANDLE CLHandle,
		CSSM_CSP_HANDLE CSPHandle,
		const CssmData *OldCrlTemplate,
		const CSSM_CERTGROUP &CertGroupToBeRemoved,
		const CSSM_CERTGROUP &RevokerCertGroup,
		const CSSM_TP_VERIFY_CONTEXT &RevokerVerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT &RevokerVerifyResult,
		CssmData &NewCrlTemplate)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CertRevoke(CSSM_CL_HANDLE CLHandle,
		CSSM_CSP_HANDLE CSPHandle,
		const CssmData *OldCrlTemplate,
		const CSSM_CERTGROUP &CertGroupToBeRevoked,
		const CSSM_CERTGROUP &RevokerCertGroup,
		const CSSM_TP_VERIFY_CONTEXT &RevokerVerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT &RevokerVerifyResult,
		CSSM_TP_CERTCHANGE_REASON Reason,
		CssmData &NewCrlTemplate)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CertReclaimAbort(CSSM_LONG_HANDLE KeyCacheHandle)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CrlCreateTemplate(CSSM_CL_HANDLE CLHandle,
		uint32 NumberOfFields,
		const CSSM_FIELD CrlFields[],
		CssmData &NewCrlTemplate)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CertGroupToTupleGroup(CSSM_CL_HANDLE CLHandle,
		const CSSM_CERTGROUP &CertGroup,
		CSSM_TUPLEGROUP_PTR &TupleGroup)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::FormRequest(const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
		CSSM_TP_FORM_TYPE FormType,
		CssmData &BlankForm)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CrlSign(CSSM_CL_HANDLE CLHandle,
		CSSM_CC_HANDLE CCHandle,
		const CSSM_ENCODED_CRL &CrlToBeSigned,
		const CSSM_CERTGROUP &SignerCertGroup,
		const CSSM_TP_VERIFY_CONTEXT &SignerVerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT &SignerVerifyResult,
		CssmData &SignedCrl)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CertGroupPrune(CSSM_CL_HANDLE CLHandle,
		const CSSM_DL_DB_LIST &DBList,
		const CSSM_CERTGROUP &OrderedCertGroup,
		CSSM_CERTGROUP_PTR &PrunedCertGroup)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::ApplyCrlToDb(CSSM_CL_HANDLE CLHandle,
		CSSM_CSP_HANDLE CSPHandle,
		const CSSM_ENCODED_CRL &CrlToBeApplied,
		const CSSM_CERTGROUP &SignerCertGroup,
		const CSSM_TP_VERIFY_CONTEXT *ApplyCrlVerifyContext,
		CSSM_TP_VERIFY_CONTEXT_RESULT &ApplyCrlVerifyResult)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::CertGetAllTemplateFields(CSSM_CL_HANDLE CLHandle,
		const CssmData &CertTemplate,
		uint32 &NumberOfFields,
		CSSM_FIELD_PTR &CertFields)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::ConfirmCredResult(const CssmData &ReferenceIdentifier,
		const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
		const CSSM_TP_CONFIRM_RESPONSE &Responses,
		const CSSM_TP_AUTHORITY_ID *PreferredAuthority)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void AppleTPSession::FormSubmit(CSSM_TP_FORM_TYPE FormType,
		const CssmData &Form,
		const CSSM_TP_AUTHORITY_ID *ClearanceAuthority,
		const CSSM_TP_AUTHORITY_ID *RepresentedAuthority,
		AccessCredentials *Credentials)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

