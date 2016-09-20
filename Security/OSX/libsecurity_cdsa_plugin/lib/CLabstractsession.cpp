//
// CL plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#include <security_cdsa_plugin/CLsession.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/cssmcli.h>


CLAbstractPluginSession::~CLAbstractPluginSession()
{ /* virtual */ }

static CSSM_RETURN CSSMCLI cssm_CertGetFirstFieldValue(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Cert,
         const CSSM_OID *CertField,
         CSSM_HANDLE_PTR ResultsHandle,
         uint32 *NumberOfMatchedFields,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if ((Required(ResultsHandle) = findSession<CLPluginSession>(CLHandle).CertGetFirstFieldValue(CssmData::required(Cert),
			CssmData::required(CertField),
			Required(NumberOfMatchedFields),
			Required(Value))) == CSSM_INVALID_HANDLE)
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_PassThrough(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).PassThrough(CCHandle,
			PassThroughId,
			InputParams,
			OutputParams);
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGetNextCachedFieldValue(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if (!findSession<CLPluginSession>(CLHandle).CertGetNextCachedFieldValue(ResultsHandle,
			Required(Value)))
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlCreateTemplate(CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD *CrlTemplate,
         CSSM_DATA_PTR NewCrl)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlCreateTemplate(NumberOfFields,
			CrlTemplate,
			CssmData::required(NewCrl));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertSign(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *CertTemplate,
         const CSSM_FIELD *SignScope,
         uint32 ScopeSize,
         CSSM_DATA_PTR SignedCert)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertSign(CCHandle,
			CssmData::required(CertTemplate),
			SignScope,
			ScopeSize,
			CssmData::required(SignedCert));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlGetFirstFieldValue(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Crl,
         const CSSM_OID *CrlField,
         CSSM_HANDLE_PTR ResultsHandle,
         uint32 *NumberOfMatchedFields,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if ((Required(ResultsHandle) = findSession<CLPluginSession>(CLHandle).CrlGetFirstFieldValue(CssmData::required(Crl),
			CssmData::required(CrlField),
			Required(NumberOfMatchedFields),
			Required(Value))) == CSSM_INVALID_HANDLE)
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlGetNextFieldValue(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if (!findSession<CLPluginSession>(CLHandle).CrlGetNextFieldValue(ResultsHandle,
			Required(Value)))
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_FreeFields(CSSM_CL_HANDLE CLHandle,
		 uint32 NumberOfFields,
		 CSSM_FIELD_PTR *FieldArray)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).FreeFields(NumberOfFields,
			Required(FieldArray));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlGetAllFields(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Crl,
         uint32 *NumberOfCrlFields,
         CSSM_FIELD_PTR *CrlFields)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlGetAllFields(CssmData::required(Crl),
			Required(NumberOfCrlFields),
			Required(CrlFields));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlAbortCache(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE CrlHandle)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlAbortCache(CrlHandle);
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGetAllTemplateFields(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *CertTemplate,
         uint32 *NumberOfFields,
         CSSM_FIELD_PTR *CertFields)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertGetAllTemplateFields(CssmData::required(CertTemplate),
			Required(NumberOfFields),
			Required(CertFields));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlSetFields(CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD *CrlTemplate,
         const CSSM_DATA *OldCrl,
         CSSM_DATA_PTR ModifiedCrl)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlSetFields(NumberOfFields,
			CrlTemplate,
			CssmData::required(OldCrl),
			CssmData::required(ModifiedCrl));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGetAllFields(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Cert,
         uint32 *NumberOfFields,
         CSSM_FIELD_PTR *CertFields)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertGetAllFields(CssmData::required(Cert),
			Required(NumberOfFields),
			Required(CertFields));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlAddCert(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *Cert,
         uint32 NumberOfFields,
         const CSSM_FIELD *CrlEntryFields,
         const CSSM_DATA *OldCrl,
         CSSM_DATA_PTR NewCrl)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlAddCert(CCHandle,
			CssmData::required(Cert),
			NumberOfFields,
			CrlEntryFields,
			CssmData::required(OldCrl),
			CssmData::required(NewCrl));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_IsCertInCachedCrl(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Cert,
         CSSM_HANDLE CrlHandle,
         CSSM_BOOL *CertFound,
         CSSM_DATA_PTR CrlRecordIndex)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).IsCertInCachedCrl(CssmData::required(Cert),
			CrlHandle,
			Required(CertFound),
			CssmData::required(CrlRecordIndex));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlSign(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *UnsignedCrl,
         const CSSM_FIELD *SignScope,
         uint32 ScopeSize,
         CSSM_DATA_PTR SignedCrl)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlSign(CCHandle,
			CssmData::required(UnsignedCrl),
			SignScope,
			ScopeSize,
			CssmData::required(SignedCrl));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertAbortQuery(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE ResultsHandle)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertAbortQuery(ResultsHandle);
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGetNextFieldValue(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if (!findSession<CLPluginSession>(CLHandle).CertGetNextFieldValue(ResultsHandle,
			Required(Value)))
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertCreateTemplate(CSSM_CL_HANDLE CLHandle,
         uint32 NumberOfFields,
         const CSSM_FIELD *CertFields,
         CSSM_DATA_PTR CertTemplate)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertCreateTemplate(NumberOfFields,
			CertFields,
			CssmData::required(CertTemplate));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertCache(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Cert,
         CSSM_HANDLE_PTR CertHandle)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertCache(CssmData::required(Cert),
			Required(CertHandle));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlVerifyWithKey(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *CrlToBeVerified)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlVerifyWithKey(CCHandle,
			CssmData::required(CrlToBeVerified));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlGetAllCachedRecordFields(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE CrlHandle,
         const CSSM_DATA *CrlRecordIndex,
         uint32 *NumberOfFields,
         CSSM_FIELD_PTR *CrlFields)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlGetAllCachedRecordFields(CrlHandle,
			CssmData::required(CrlRecordIndex),
			Required(NumberOfFields),
			Required(CrlFields));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlRemoveCert(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Cert,
         const CSSM_DATA *OldCrl,
         CSSM_DATA_PTR NewCrl)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlRemoveCert(CssmData::required(Cert),
			CssmData::required(OldCrl),
			CssmData::required(NewCrl));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertVerifyWithKey(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *CertToBeVerified)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertVerifyWithKey(CCHandle,
			CssmData::required(CertToBeVerified));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlGetFirstCachedFieldValue(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE CrlHandle,
         const CSSM_DATA *CrlRecordIndex,
         const CSSM_OID *CrlField,
         CSSM_HANDLE_PTR ResultsHandle,
         uint32 *NumberOfMatchedFields,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if ((Required(ResultsHandle) = findSession<CLPluginSession>(CLHandle).CrlGetFirstCachedFieldValue(CrlHandle,
			CssmData::optional(CrlRecordIndex),
			CssmData::required(CrlField),
			Required(NumberOfMatchedFields),
			Required(Value))) == CSSM_INVALID_HANDLE)
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGetFirstCachedFieldValue(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE CertHandle,
         const CSSM_OID *CertField,
         CSSM_HANDLE_PTR ResultsHandle,
         uint32 *NumberOfMatchedFields,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if ((Required(ResultsHandle) = findSession<CLPluginSession>(CLHandle).CertGetFirstCachedFieldValue(CertHandle,
			CssmData::required(CertField),
			Required(NumberOfMatchedFields),
			Required(Value))) == CSSM_INVALID_HANDLE)
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertVerify(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *CertToBeVerified,
         const CSSM_DATA *SignerCert,
         const CSSM_FIELD *VerifyScope,
         uint32 ScopeSize)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertVerify(CCHandle,
			CssmData::required(CertToBeVerified),
			CssmData::optional(SignerCert),
			VerifyScope,
			ScopeSize);
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGetKeyInfo(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Cert,
         CSSM_KEY_PTR *Key)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertGetKeyInfo(CssmData::required(Cert),
			Required(Key));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlVerify(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DATA *CrlToBeVerified,
         const CSSM_DATA *SignerCert,
         const CSSM_FIELD *VerifyScope,
         uint32 ScopeSize)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlVerify(CCHandle,
			CssmData::required(CrlToBeVerified),
			CssmData::optional(SignerCert),
			VerifyScope,
			ScopeSize);
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlDescribeFormat(CSSM_CL_HANDLE CLHandle,
         uint32 *NumberOfFields,
         CSSM_OID_PTR *OidList)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlDescribeFormat(Required(NumberOfFields),
			Required(OidList));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlAbortQuery(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE ResultsHandle)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlAbortQuery(ResultsHandle);
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertDescribeFormat(CSSM_CL_HANDLE CLHandle,
         uint32 *NumberOfFields,
         CSSM_OID_PTR *OidList)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertDescribeFormat(Required(NumberOfFields),
			Required(OidList));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_FreeFieldValue(CSSM_CL_HANDLE CLHandle,
         const CSSM_OID *CertOrCrlOid,
         CSSM_DATA_PTR Value)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).FreeFieldValue(CssmData::required(CertOrCrlOid),
			CssmData::required(Value));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGroupToSignedBundle(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CERTGROUP *CertGroupToBundle,
         const CSSM_CERT_BUNDLE_HEADER *BundleInfo,
         CSSM_DATA_PTR SignedBundle)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertGroupToSignedBundle(CCHandle,
			Required(CertGroupToBundle),
			BundleInfo,
			CssmData::required(SignedBundle));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertAbortCache(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE CertHandle)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertAbortCache(CertHandle);
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlCache(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Crl,
         CSSM_HANDLE_PTR CrlHandle)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CrlCache(CssmData::required(Crl),
			Required(CrlHandle));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_IsCertInCrl(CSSM_CL_HANDLE CLHandle,
         const CSSM_DATA *Cert,
         const CSSM_DATA *Crl,
         CSSM_BOOL *CertFound)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).IsCertInCrl(CssmData::required(Cert),
			CssmData::required(Crl),
			Required(CertFound));
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CrlGetNextCachedFieldValue(CSSM_CL_HANDLE CLHandle,
         CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR *Value)
{
  BEGIN_API
  if (!findSession<CLPluginSession>(CLHandle).CrlGetNextCachedFieldValue(ResultsHandle,
			Required(Value)))
    return CSSMERR_CL_NO_FIELD_VALUES;
  END_API(CL)
}

static CSSM_RETURN CSSMCLI cssm_CertGroupFromVerifiedBundle(CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_CERT_BUNDLE *CertBundle,
         const CSSM_DATA *SignerCert,
         CSSM_CERTGROUP_PTR *CertGroup)
{
  BEGIN_API
  findSession<CLPluginSession>(CLHandle).CertGroupFromVerifiedBundle(CCHandle,
			Required(CertBundle),
			CssmData::optional(SignerCert),
			Required(CertGroup));
  END_API(CL)
}


static const CSSM_SPI_CL_FUNCS CLFunctionStruct = {
  cssm_CertCreateTemplate,
  cssm_CertGetAllTemplateFields,
  cssm_CertSign,
  cssm_CertVerify,
  cssm_CertVerifyWithKey,
  cssm_CertGetFirstFieldValue,
  cssm_CertGetNextFieldValue,
  cssm_CertAbortQuery,
  cssm_CertGetKeyInfo,
  cssm_CertGetAllFields,
  cssm_FreeFields,
  cssm_FreeFieldValue,
  cssm_CertCache,
  cssm_CertGetFirstCachedFieldValue,
  cssm_CertGetNextCachedFieldValue,
  cssm_CertAbortCache,
  cssm_CertGroupToSignedBundle,
  cssm_CertGroupFromVerifiedBundle,
  cssm_CertDescribeFormat,
  cssm_CrlCreateTemplate,
  cssm_CrlSetFields,
  cssm_CrlAddCert,
  cssm_CrlRemoveCert,
  cssm_CrlSign,
  cssm_CrlVerify,
  cssm_CrlVerifyWithKey,
  cssm_IsCertInCrl,
  cssm_CrlGetFirstFieldValue,
  cssm_CrlGetNextFieldValue,
  cssm_CrlAbortQuery,
  cssm_CrlGetAllFields,
  cssm_CrlCache,
  cssm_IsCertInCachedCrl,
  cssm_CrlGetFirstCachedFieldValue,
  cssm_CrlGetNextCachedFieldValue,
  cssm_CrlGetAllCachedRecordFields,
  cssm_CrlAbortCache,
  cssm_CrlDescribeFormat,
  cssm_PassThrough,
};

static CSSM_MODULE_FUNCS CLFunctionTable = {
  CSSM_SERVICE_CL,	// service type
  39,	// number of functions
  (const CSSM_PROC_ADDR *)&CLFunctionStruct
};

CSSM_MODULE_FUNCS_PTR CLPluginSession::construct()
{
   return &CLFunctionTable;
}
