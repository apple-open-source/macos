//
// TP plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#ifndef _H_TPABSTRACTSESSION
#define _H_TPABSTRACTSESSION

#include <security_cdsa_plugin/pluginsession.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmacl.h>


namespace Security {


//
// A pure abstract class to define the TP module interface
//
class TPAbstractPluginSession {
public:
	virtual ~TPAbstractPluginSession();
  virtual void FormRequest(const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
         CSSM_TP_FORM_TYPE FormType,
         CssmData &BlankForm) = 0;
  virtual void CrlSign(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_ENCODED_CRL &CrlToBeSigned,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *SignerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT *SignerVerifyResult,
         CssmData &SignedCrl) = 0;
  virtual void CertCreateTemplate(CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD CertFields[],
         CssmData &CertTemplate) = 0;
  virtual void CertReclaimKey(const CSSM_CERTGROUP &CertGroup,
         uint32 CertIndex,
         CSSM_LONG_HANDLE KeyCacheHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry) = 0;
  virtual void CertGroupToTupleGroup(CSSM_CL_HANDLE CLHandle,
         const CSSM_CERTGROUP &CertGroup,
         CSSM_TUPLEGROUP_PTR &TupleGroup) = 0;
  virtual void RetrieveCredResult(const CssmData &ReferenceIdentifier,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
         sint32 &EstimatedTime,
         CSSM_BOOL &ConfirmationRequired,
         CSSM_TP_RESULT_SET_PTR &RetrieveOutput) = 0;
  virtual void FormSubmit(CSSM_TP_FORM_TYPE FormType,
         const CssmData &Form,
         const CSSM_TP_AUTHORITY_ID *ClearanceAuthority,
         const CSSM_TP_AUTHORITY_ID *RepresentedAuthority,
         AccessCredentials *Credentials) = 0;
  virtual void CertSign(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CssmData &CertTemplateToBeSigned,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *SignerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT *SignerVerifyResult,
         CssmData &SignedCert) = 0;
  virtual void CrlCreateTemplate(CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD CrlFields[],
         CssmData &NewCrlTemplate) = 0;
  virtual void CertReclaimAbort(CSSM_LONG_HANDLE KeyCacheHandle) = 0;
  virtual void PassThrough(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DL_DB_LIST *DBList,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams) = 0;
  virtual void CertGroupConstruct(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_DL_DB_LIST &DBList,
         const void *ConstructParams,
         const CSSM_CERTGROUP &CertGroupFrag,
         CSSM_CERTGROUP_PTR &CertGroup) = 0;
  virtual void TupleGroupToCertGroup(CSSM_CL_HANDLE CLHandle,
         const CSSM_TUPLEGROUP &TupleGroup,
         CSSM_CERTGROUP_PTR &CertTemplates) = 0;
  virtual void CertGetAllTemplateFields(CSSM_CL_HANDLE CLHandle,
         const CssmData &CertTemplate,
         uint32 &NumberOfFields,
         CSSM_FIELD_PTR &CertFields) = 0;
  virtual void ReceiveConfirmation(const CssmData &ReferenceIdentifier,
         CSSM_TP_CONFIRM_RESPONSE_PTR &Responses,
         sint32 &ElapsedTime) = 0;
  virtual void ConfirmCredResult(const CssmData &ReferenceIdentifier,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
         const CSSM_TP_CONFIRM_RESPONSE &Responses,
         const CSSM_TP_AUTHORITY_ID *PreferredAuthority) = 0;
  virtual void SubmitCredRequest(const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
         CSSM_TP_AUTHORITY_REQUEST_TYPE RequestType,
         const CSSM_TP_REQUEST_SET &RequestInput,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
         sint32 &EstimatedTime,
         CssmData &ReferenceIdentifier) = 0;
  virtual void CertGroupVerify(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_CERTGROUP &CertGroupToBeVerified,
         const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT *VerifyContextResult) = 0;
  virtual void CertRemoveFromCrlTemplate(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CssmData *OldCrlTemplate,
         const CSSM_CERTGROUP &CertGroupToBeRemoved,
         const CSSM_CERTGROUP &RevokerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT &RevokerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT &RevokerVerifyResult,
         CssmData &NewCrlTemplate) = 0;
  virtual void ApplyCrlToDb(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ENCODED_CRL &CrlToBeApplied,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *ApplyCrlVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT &ApplyCrlVerifyResult) = 0;
  virtual void CertGroupPrune(CSSM_CL_HANDLE CLHandle,
         const CSSM_DL_DB_LIST &DBList,
         const CSSM_CERTGROUP &OrderedCertGroup,
         CSSM_CERTGROUP_PTR &PrunedCertGroup) = 0;
  virtual void CertRevoke(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CssmData *OldCrlTemplate,
         const CSSM_CERTGROUP &CertGroupToBeRevoked,
         const CSSM_CERTGROUP &RevokerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT &RevokerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT &RevokerVerifyResult,
         CSSM_TP_CERTCHANGE_REASON Reason,
         CssmData &NewCrlTemplate) = 0;
  virtual void CrlVerify(CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ENCODED_CRL &CrlToBeVerified,
         const CSSM_CERTGROUP &SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT *RevokerVerifyResult) = 0;
};

} // end namespace Security

#endif //_H_TPABSTRACTSESSION
