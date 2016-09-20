//
// TP plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#include <security_cdsa_plugin/TPsession.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/cssmtpi.h>


TPAbstractPluginSession::~TPAbstractPluginSession()
{ /* virtual */ }

static CSSM_RETURN CSSMTPI cssm_CertReclaimKey(CSSM_TP_HANDLE TPHandle,
         const CSSM_CERTGROUP *CertGroup,
         uint32 CertIndex,
         CSSM_LONG_HANDLE KeyCacheHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertReclaimKey(Required(CertGroup),
			CertIndex,
			KeyCacheHandle,
			CSPHandle,
			CredAndAclEntry);
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertGroupToTupleGroup(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         const CSSM_CERTGROUP *CertGroup,
         CSSM_TUPLEGROUP_PTR *TupleGroup)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertGroupToTupleGroup(CLHandle,
			Required(CertGroup),
			Required(TupleGroup));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertCreateTemplate(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD *CertFields,
         CSSM_DATA_PTR CertTemplate)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertCreateTemplate(CLHandle,
			NumberOfFields,
			CertFields,
			CssmData::required(CertTemplate));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_FormRequest(CSSM_TP_HANDLE TPHandle,
         const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
         CSSM_TP_FORM_TYPE FormType,
         CSSM_DATA_PTR BlankForm)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).FormRequest(PreferredAuthority,
			FormType,
			CssmData::required(BlankForm));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CrlSign(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_ENCODED_CRL *CrlToBeSigned,
         const CSSM_CERTGROUP *SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *SignerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR SignerVerifyResult,
         CSSM_DATA_PTR SignedCrl)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CrlSign(CLHandle,
			CCHandle,
			Required(CrlToBeSigned),
			Required(SignerCertGroup),
			SignerVerifyContext,
			SignerVerifyResult,
			CssmData::required(SignedCrl));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_TupleGroupToCertGroup(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         const CSSM_TUPLEGROUP *TupleGroup,
         CSSM_CERTGROUP_PTR *CertTemplates)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).TupleGroupToCertGroup(CLHandle,
			Required(TupleGroup),
			Required(CertTemplates));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertGetAllTemplateFields(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *CertTemplate,
         uint32 *NumberOfFields,
         CSSM_FIELD_PTR *CertFields)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertGetAllTemplateFields(CLHandle,
			CssmData::required(CertTemplate),
			Required(NumberOfFields),
			Required(CertFields));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertReclaimAbort(CSSM_TP_HANDLE TPHandle,
         CSSM_LONG_HANDLE KeyCacheHandle)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertReclaimAbort(KeyCacheHandle);
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CrlCreateTemplate(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD *CrlFields,
         CSSM_DATA_PTR NewCrlTemplate)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CrlCreateTemplate(CLHandle,
			NumberOfFields,
			CrlFields,
			CssmData::required(NewCrlTemplate));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertGroupConstruct(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_DL_DB_LIST *DBList,
         const void *ConstructParams,
         const CSSM_CERTGROUP *CertGroupFrag,
         CSSM_CERTGROUP_PTR *CertGroup)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertGroupConstruct(CLHandle,
			CSPHandle,
			Required(DBList),
			ConstructParams,
			Required(CertGroupFrag),
			Required(CertGroup));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_PassThrough(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DL_DB_LIST *DBList,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).PassThrough(CLHandle,
			CCHandle,
			DBList,
			PassThroughId,
			InputParams,
			OutputParams);
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_RetrieveCredResult(CSSM_TP_HANDLE TPHandle,
         const CSSM_DATA *ReferenceIdentifier,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
         sint32 *EstimatedTime,
         CSSM_BOOL *ConfirmationRequired,
         CSSM_TP_RESULT_SET_PTR *RetrieveOutput)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).RetrieveCredResult(CssmData::required(ReferenceIdentifier),
			CallerAuthCredentials,
			Required(EstimatedTime),
			Required(ConfirmationRequired),
			Required(RetrieveOutput));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertSign(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *CertTemplateToBeSigned,
         const CSSM_CERTGROUP *SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *SignerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR SignerVerifyResult,
         CSSM_DATA_PTR SignedCert)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertSign(CLHandle,
			CCHandle,
			CssmData::required(CertTemplateToBeSigned),
			Required(SignerCertGroup),
			SignerVerifyContext,
			SignerVerifyResult,
			CssmData::required(SignedCert));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_FormSubmit(CSSM_TP_HANDLE TPHandle,
         CSSM_TP_FORM_TYPE FormType,
         const CSSM_DATA *Form,
         const CSSM_TP_AUTHORITY_ID *ClearanceAuthority,
         const CSSM_TP_AUTHORITY_ID *RepresentedAuthority,
         CSSM_ACCESS_CREDENTIALS_PTR Credentials)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).FormSubmit(FormType,
			CssmData::required(Form),
			ClearanceAuthority,
			RepresentedAuthority,
			AccessCredentials::optional(Credentials));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertGroupVerify(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_CERTGROUP *CertGroupToBeVerified,
         const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR VerifyContextResult)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertGroupVerify(CLHandle,
			CSPHandle,
			Required(CertGroupToBeVerified),
			VerifyContext,
			VerifyContextResult);
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_SubmitCredRequest(CSSM_TP_HANDLE TPHandle,
         const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
         CSSM_TP_AUTHORITY_REQUEST_TYPE RequestType,
         const CSSM_TP_REQUEST_SET *RequestInput,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
         sint32 *EstimatedTime,
         CSSM_DATA_PTR ReferenceIdentifier)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).SubmitCredRequest(PreferredAuthority,
			RequestType,
			Required(RequestInput),
			CallerAuthContext,
			Required(EstimatedTime),
			CssmData::required(ReferenceIdentifier));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_ReceiveConfirmation(CSSM_TP_HANDLE TPHandle,
         const CSSM_DATA *ReferenceIdentifier,
         CSSM_TP_CONFIRM_RESPONSE_PTR *Responses,
         sint32 *ElapsedTime)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).ReceiveConfirmation(CssmData::required(ReferenceIdentifier),
			Required(Responses),
			Required(ElapsedTime));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_ConfirmCredResult(CSSM_TP_HANDLE TPHandle,
         const CSSM_DATA *ReferenceIdentifier,
         const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
         const CSSM_TP_CONFIRM_RESPONSE *Responses,
         const CSSM_TP_AUTHORITY_ID *PreferredAuthority)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).ConfirmCredResult(CssmData::required(ReferenceIdentifier),
			CallerAuthCredentials,
			Required(Responses),
			PreferredAuthority);
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CrlVerify(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ENCODED_CRL *CrlToBeVerified,
         const CSSM_CERTGROUP *SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *VerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR RevokerVerifyResult)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CrlVerify(CLHandle,
			CSPHandle,
			Required(CrlToBeVerified),
			Required(SignerCertGroup),
			VerifyContext,
			RevokerVerifyResult);
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_ApplyCrlToDb(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_ENCODED_CRL *CrlToBeApplied,
         const CSSM_CERTGROUP *SignerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *ApplyCrlVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR ApplyCrlVerifyResult)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).ApplyCrlToDb(CLHandle,
			CSPHandle,
			Required(CrlToBeApplied),
			Required(SignerCertGroup),
			ApplyCrlVerifyContext,
			Required(ApplyCrlVerifyResult));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertGroupPrune(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         const CSSM_DL_DB_LIST *DBList,
         const CSSM_CERTGROUP *OrderedCertGroup,
         CSSM_CERTGROUP_PTR *PrunedCertGroup)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertGroupPrune(CLHandle,
			Required(DBList),
			Required(OrderedCertGroup),
			Required(PrunedCertGroup));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertRevoke(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_DATA *OldCrlTemplate,
         const CSSM_CERTGROUP *CertGroupToBeRevoked,
         const CSSM_CERTGROUP *RevokerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *RevokerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR RevokerVerifyResult,
         CSSM_TP_CERTCHANGE_REASON Reason,
         CSSM_DATA_PTR NewCrlTemplate)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertRevoke(CLHandle,
			CSPHandle,
			CssmData::optional(OldCrlTemplate),
			Required(CertGroupToBeRevoked),
			Required(RevokerCertGroup),
			Required(RevokerVerifyContext),
			Required(RevokerVerifyResult),
			Reason,
			CssmData::required(NewCrlTemplate));
  END_API(TP)
}

static CSSM_RETURN CSSMTPI cssm_CertRemoveFromCrlTemplate(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CSP_HANDLE CSPHandle,
         const CSSM_DATA *OldCrlTemplate,
         const CSSM_CERTGROUP *CertGroupToBeRemoved,
         const CSSM_CERTGROUP *RevokerCertGroup,
         const CSSM_TP_VERIFY_CONTEXT *RevokerVerifyContext,
         CSSM_TP_VERIFY_CONTEXT_RESULT_PTR RevokerVerifyResult,
         CSSM_DATA_PTR NewCrlTemplate)
{
  BEGIN_API
  findSession<TPPluginSession>(TPHandle).CertRemoveFromCrlTemplate(CLHandle,
			CSPHandle,
			CssmData::optional(OldCrlTemplate),
			Required(CertGroupToBeRemoved),
			Required(RevokerCertGroup),
			Required(RevokerVerifyContext),
			Required(RevokerVerifyResult),
			CssmData::required(NewCrlTemplate));
  END_API(TP)
}


static const CSSM_SPI_TP_FUNCS TPFunctionStruct = {
  cssm_SubmitCredRequest,
  cssm_RetrieveCredResult,
  cssm_ConfirmCredResult,
  cssm_ReceiveConfirmation,
  cssm_CertReclaimKey,
  cssm_CertReclaimAbort,
  cssm_FormRequest,
  cssm_FormSubmit,
  cssm_CertGroupVerify,
  cssm_CertCreateTemplate,
  cssm_CertGetAllTemplateFields,
  cssm_CertSign,
  cssm_CrlVerify,
  cssm_CrlCreateTemplate,
  cssm_CertRevoke,
  cssm_CertRemoveFromCrlTemplate,
  cssm_CrlSign,
  cssm_ApplyCrlToDb,
  cssm_CertGroupConstruct,
  cssm_CertGroupPrune,
  cssm_CertGroupToTupleGroup,
  cssm_TupleGroupToCertGroup,
  cssm_PassThrough,
};

static CSSM_MODULE_FUNCS TPFunctionTable = {
  CSSM_SERVICE_TP,	// service type
  23,	// number of functions
  (const CSSM_PROC_ADDR *)&TPFunctionStruct
};

CSSM_MODULE_FUNCS_PTR TPPluginSession::construct()
{
   return &TPFunctionTable;
}
