//
// CL plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#ifndef _H_CLABSTRACTSESSION
#define _H_CLABSTRACTSESSION

#include <security_cdsa_plugin/pluginsession.h>
#include <security_cdsa_utilities/cssmdata.h>


namespace Security {


//
// A pure abstract class to define the CL module interface
//
class CLAbstractPluginSession {
public:
	virtual ~CLAbstractPluginSession();
  virtual void CertGetAllFields(const CssmData &Cert,
         uint32 &NumberOfFields,
         CSSM_FIELD_PTR &CertFields) = 0;
  virtual void CertGetAllTemplateFields(const CssmData &CertTemplate,
         uint32 &NumberOfFields,
         CSSM_FIELD_PTR &CertFields) = 0;
  virtual void CrlSetFields(uint32 NumberOfFields,
         const CSSM_FIELD CrlTemplate[],
         const CssmData &OldCrl,
         CssmData &ModifiedCrl) = 0;
  virtual void CrlAbortCache(CSSM_HANDLE CrlHandle) = 0;
  virtual void CrlGetAllFields(const CssmData &Crl,
         uint32 &NumberOfCrlFields,
         CSSM_FIELD_PTR &CrlFields) = 0;
  virtual void FreeFields(uint32 NumberOfFields,
		 CSSM_FIELD_PTR &FieldArray) = 0;
  virtual bool CrlGetNextFieldValue(CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR &Value) = 0;
  virtual CSSM_HANDLE CrlGetFirstFieldValue(const CssmData &Crl,
         const CssmData &CrlField,
                  uint32 &NumberOfMatchedFields,
         CSSM_DATA_PTR &Value) = 0;
  virtual void CertSign(CSSM_CC_HANDLE CCHandle,
         const CssmData &CertTemplate,
         const CSSM_FIELD *SignScope,
         uint32 ScopeSize,
         CssmData &SignedCert) = 0;
  virtual void CrlCreateTemplate(uint32 NumberOfFields,
         const CSSM_FIELD CrlTemplate[],
         CssmData &NewCrl) = 0;
  virtual bool CertGetNextCachedFieldValue(CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR &Value) = 0;
  virtual void PassThrough(CSSM_CC_HANDLE CCHandle,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams) = 0;
  virtual CSSM_HANDLE CertGetFirstFieldValue(const CssmData &Cert,
         const CssmData &CertField,
                  uint32 &NumberOfMatchedFields,
         CSSM_DATA_PTR &Value) = 0;
  virtual void CrlVerifyWithKey(CSSM_CC_HANDLE CCHandle,
         const CssmData &CrlToBeVerified) = 0;
  virtual void CertCreateTemplate(uint32 NumberOfFields,
         const CSSM_FIELD CertFields[],
         CssmData &CertTemplate) = 0;
  virtual void CertCache(const CssmData &Cert,
         CSSM_HANDLE &CertHandle) = 0;
  virtual bool CertGetNextFieldValue(CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR &Value) = 0;
  virtual void CertAbortQuery(CSSM_HANDLE ResultsHandle) = 0;
  virtual void IsCertInCachedCrl(const CssmData &Cert,
         CSSM_HANDLE CrlHandle,
         CSSM_BOOL &CertFound,
         CssmData &CrlRecordIndex) = 0;
  virtual void CrlSign(CSSM_CC_HANDLE CCHandle,
         const CssmData &UnsignedCrl,
         const CSSM_FIELD *SignScope,
         uint32 ScopeSize,
         CssmData &SignedCrl) = 0;
  virtual void CrlAddCert(CSSM_CC_HANDLE CCHandle,
         const CssmData &Cert,
         uint32 NumberOfFields,
         const CSSM_FIELD CrlEntryFields[],
         const CssmData &OldCrl,
         CssmData &NewCrl) = 0;
  virtual void CertGroupToSignedBundle(CSSM_CC_HANDLE CCHandle,
         const CSSM_CERTGROUP &CertGroupToBundle,
         const CSSM_CERT_BUNDLE_HEADER *BundleInfo,
         CssmData &SignedBundle) = 0;
  virtual void FreeFieldValue(const CssmData &CertOrCrlOid,
         CssmData &Value) = 0;
  virtual void CertDescribeFormat(uint32 &NumberOfFields,
         CSSM_OID_PTR &OidList) = 0;
  virtual void CrlAbortQuery(CSSM_HANDLE ResultsHandle) = 0;
  virtual void CrlDescribeFormat(uint32 &NumberOfFields,
         CSSM_OID_PTR &OidList) = 0;
  virtual void CrlVerify(CSSM_CC_HANDLE CCHandle,
         const CssmData &CrlToBeVerified,
         const CssmData *SignerCert,
         const CSSM_FIELD *VerifyScope,
         uint32 ScopeSize) = 0;
  virtual void CertGetKeyInfo(const CssmData &Cert,
         CSSM_KEY_PTR &Key) = 0;
  virtual void CertVerify(CSSM_CC_HANDLE CCHandle,
         const CssmData &CertToBeVerified,
         const CssmData *SignerCert,
         const CSSM_FIELD *VerifyScope,
         uint32 ScopeSize) = 0;
  virtual CSSM_HANDLE CertGetFirstCachedFieldValue(CSSM_HANDLE CertHandle,
         const CssmData &CertField,
                  uint32 &NumberOfMatchedFields,
         CSSM_DATA_PTR &Value) = 0;
  virtual CSSM_HANDLE CrlGetFirstCachedFieldValue(CSSM_HANDLE CrlHandle,
         const CssmData *CrlRecordIndex,
         const CssmData &CrlField,
                  uint32 &NumberOfMatchedFields,
         CSSM_DATA_PTR &Value) = 0;
  virtual void CertVerifyWithKey(CSSM_CC_HANDLE CCHandle,
         const CssmData &CertToBeVerified) = 0;
  virtual void CrlRemoveCert(const CssmData &Cert,
         const CssmData &OldCrl,
         CssmData &NewCrl) = 0;
  virtual void CrlGetAllCachedRecordFields(CSSM_HANDLE CrlHandle,
         const CssmData &CrlRecordIndex,
         uint32 &NumberOfFields,
         CSSM_FIELD_PTR &CrlFields) = 0;
  virtual void CertGroupFromVerifiedBundle(CSSM_CC_HANDLE CCHandle,
         const CSSM_CERT_BUNDLE &CertBundle,
         const CssmData *SignerCert,
         CSSM_CERTGROUP_PTR &CertGroup) = 0;
  virtual bool CrlGetNextCachedFieldValue(CSSM_HANDLE ResultsHandle,
         CSSM_DATA_PTR &Value) = 0;
  virtual void IsCertInCrl(const CssmData &Cert,
         const CssmData &Crl,
         CSSM_BOOL &CertFound) = 0;
  virtual void CrlCache(const CssmData &Crl,
         CSSM_HANDLE &CrlHandle) = 0;
  virtual void CertAbortCache(CSSM_HANDLE CertHandle) = 0;
};

} // end namespace Security

#endif //_H_CLABSTRACTSESSION
