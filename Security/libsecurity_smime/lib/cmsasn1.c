/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * CMS ASN.1 templates
 */

#include <Security/SecCmsContentInfo.h>

#include "cmslocal.h"

#include "secoid.h"
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

extern const SecAsn1Template nss_cms_set_of_attribute_template[];

//SEC_ASN1_MKSUB(CERT_IssuerAndSNTemplate)
//SEC_ASN1_MKSUB(CERT_SetOfSignedCrlTemplate)
SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(kSecAsn1BitStringTemplate)
SEC_ASN1_MKSUB(kSecAsn1OctetStringTemplate)
SEC_ASN1_MKSUB(kSecAsn1PointerToOctetStringTemplate)
SEC_ASN1_MKSUB(kSecAsn1SetOfAnyTemplate)

/* -----------------------------------------------------------------------------
 * MESSAGE
 * (uses SecCmsContentInfo)
 */

/* forward declaration */
static const SecAsn1Template *
nss_cms_choose_content_template(void *src_or_dest, Boolean encoding, const char *buf, size_t len, void *dest);

static const SecAsn1TemplateChooserPtr nss_cms_chooser
	= nss_cms_choose_content_template;

const SecAsn1Template SecCmsMessageTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SecCmsMessage) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SecCmsMessage,contentInfo.contentType) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_DYNAMIC | SEC_ASN1_MAY_STREAM
     | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SecCmsMessage,contentInfo.content),
	  &nss_cms_chooser },
    { 0 }
};

#if 0
static const SecAsn1Template NSS_PointerToCMSMessageTemplate[] = {
    { SEC_ASN1_POINTER, 0, SecCmsMessageTemplate }
};
#endif

/* -----------------------------------------------------------------------------
 * ENCAPSULATED & ENCRYPTED CONTENTINFO
 * (both use a SecCmsContentInfo)
 */
static const SecAsn1Template SecCmsEncapsulatedContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SecCmsContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SecCmsContentInfo,contentType) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT | SEC_ASN1_MAY_STREAM |
	SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(SecCmsContentInfo,rawContent),
	  SEC_ASN1_SUB(kSecAsn1PointerToOctetStringTemplate) },
    { 0 }
};

static const SecAsn1Template SecCmsEncryptedContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SecCmsContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SecCmsContentInfo,contentType) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsContentInfo,contentEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_POINTER | SEC_ASN1_MAY_STREAM | 
      SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(SecCmsContentInfo,rawContent),
	  SEC_ASN1_SUB(kSecAsn1OctetStringTemplate) },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * SIGNED DATA
 */

const SecAsn1Template SecCmsSignerInfoTemplate[];


const SecAsn1Template SecCmsSignedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SecCmsSignedData) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsSignedData,version) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN,
	  offsetof(SecCmsSignedData,digestAlgorithms),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsSignedData,contentInfo),
	  SecCmsEncapsulatedContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 0,
	  offsetof(SecCmsSignedData,rawCerts),
	  SEC_ASN1_SUB(kSecAsn1SetOfAnyTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 1,
	  offsetof(SecCmsSignedData,rawCrls),
	  SEC_ASN1_SUB(kSecAsn1SetOfAnyTemplate) },
    { SEC_ASN1_SET_OF,
	  offsetof(SecCmsSignedData,signerInfos),
	  SecCmsSignerInfoTemplate },
    { 0 }
};

const SecAsn1Template NSS_PointerToCMSSignedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SecCmsSignedDataTemplate }
};

/* -----------------------------------------------------------------------------
 * signeridentifier
 */

static const SecAsn1Template SecCmsSignerIdentifierTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(SecCmsSignerIdentifier,identifierType), NULL,
	  sizeof(SecCmsSignerIdentifier) },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(SecCmsSignerIdentifier,id.subjectKeyID),
	  SEC_ASN1_SUB(kSecAsn1OctetStringTemplate) ,
	  SecCmsRecipientIDSubjectKeyID },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(SecCmsSignerIdentifier,id.issuerAndSN),
	  SEC_ASN1_SUB(SecCmsIssuerAndSNTemplate),
	  SecCmsRecipientIDIssuerSN },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * signerinfo
 */

const SecAsn1Template SecCmsSignerInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsSignerInfo) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsSignerInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsSignerInfo,signerIdentifier),
	  SecCmsSignerIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsSignerInfo,digestAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SecCmsSignerInfo,authAttr),
	  nss_cms_set_of_attribute_template },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsSignerInfo,digestEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsSignerInfo,encDigest) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(SecCmsSignerInfo,unAuthAttr),
	  nss_cms_set_of_attribute_template },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * ENVELOPED DATA
 */

static const SecAsn1Template SecCmsOriginatorInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsOriginatorInfo) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 0,
	  offsetof(SecCmsOriginatorInfo,rawCerts),
	  SEC_ASN1_SUB(kSecAsn1SetOfAnyTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 1,
	  offsetof(SecCmsOriginatorInfo,rawCrls),
	  SEC_ASN1_SUB(kSecAsn1SetOfAnyTemplate) },
    { 0 }
};

const SecAsn1Template SecCmsRecipientInfoTemplate[];

const SecAsn1Template SecCmsEnvelopedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SecCmsEnvelopedData) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsEnvelopedData,version) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_POINTER | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SecCmsEnvelopedData,originatorInfo),
	  SecCmsOriginatorInfoTemplate },
    { SEC_ASN1_SET_OF,
	  offsetof(SecCmsEnvelopedData,recipientInfos),
	  SecCmsRecipientInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsEnvelopedData,contentInfo),
	  SecCmsEncryptedContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(SecCmsEnvelopedData,unprotectedAttr),
	  nss_cms_set_of_attribute_template },
    { 0 }
};

const SecAsn1Template NSS_PointerToCMSEnvelopedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SecCmsEnvelopedDataTemplate }
};

/* here come the 15 gazillion templates for all the v3 varieties of RecipientInfo */

/* -----------------------------------------------------------------------------
 * key transport recipient info
 */

static const SecAsn1Template SecCmsRecipientIdentifierTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(SecCmsRecipientIdentifier,identifierType), NULL,
	  sizeof(SecCmsRecipientIdentifier) },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(SecCmsRecipientIdentifier,id.subjectKeyID),
	  SEC_ASN1_SUB(kSecAsn1OctetStringTemplate) ,
	  SecCmsRecipientIDSubjectKeyID },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(SecCmsRecipientIdentifier,id.issuerAndSN),
	  SEC_ASN1_SUB(SecCmsIssuerAndSNTemplate),
	  SecCmsRecipientIDIssuerSN },
    { 0 }
};


static const SecAsn1Template SecCmsKeyTransRecipientInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsKeyTransRecipientInfo) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsKeyTransRecipientInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsKeyTransRecipientInfo,recipientIdentifier),
	  SecCmsRecipientIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsKeyTransRecipientInfo,keyEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsKeyTransRecipientInfo,encKey) },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * key agreement recipient info
 */

static const SecAsn1Template SecCmsOriginatorPublicKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsOriginatorPublicKey) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsOriginatorPublicKey,algorithmIdentifier),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsOriginatorPublicKey,publicKey),
	  SEC_ASN1_SUB(kSecAsn1BitStringTemplate) },
    { 0 }
};


static const SecAsn1Template SecCmsOriginatorIdentifierOrKeyTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(SecCmsOriginatorIdentifierOrKey,identifierType), NULL,
	  sizeof(SecCmsOriginatorIdentifierOrKey) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(SecCmsOriginatorIdentifierOrKey,id.issuerAndSN),
	  SEC_ASN1_SUB(SecCmsIssuerAndSNTemplate),
	  SecCmsOriginatorIDOrKeyIssuerSN },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 0,
	  offsetof(SecCmsOriginatorIdentifierOrKey,id.subjectKeyID),
	  SEC_ASN1_SUB(kSecAsn1PointerToOctetStringTemplate) ,
	  SecCmsOriginatorIDOrKeySubjectKeyID },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(SecCmsOriginatorIdentifierOrKey,id.originatorPublicKey),
	  SecCmsOriginatorPublicKeyTemplate,
	  SecCmsOriginatorIDOrKeyOriginatorPublicKey },
    { 0 }
};

const SecAsn1Template SecCmsRecipientKeyIdentifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsRecipientKeyIdentifier) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsRecipientKeyIdentifier,subjectKeyIdentifier) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsRecipientKeyIdentifier,date) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsRecipientKeyIdentifier,other) },
    { 0 }
};


static const SecAsn1Template SecCmsKeyAgreeRecipientIdentifierTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(SecCmsKeyAgreeRecipientIdentifier,identifierType), NULL,
	  sizeof(SecCmsKeyAgreeRecipientIdentifier) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(SecCmsKeyAgreeRecipientIdentifier,id.issuerAndSN),
	  SEC_ASN1_SUB(SecCmsIssuerAndSNTemplate),
	  SecCmsKeyAgreeRecipientIDIssuerSN },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SecCmsKeyAgreeRecipientIdentifier,id.recipientKeyIdentifier),
	  SecCmsRecipientKeyIdentifierTemplate,
	  SecCmsKeyAgreeRecipientIDRKeyID },
    { 0 }
};

static const SecAsn1Template SecCmsRecipientEncryptedKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsRecipientEncryptedKey) },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsRecipientEncryptedKey,recipientIdentifier),
	  SecCmsKeyAgreeRecipientIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsRecipientEncryptedKey,encKey),
	  SEC_ASN1_SUB(kSecAsn1OctetStringTemplate) },
    { 0 }
};

static const SecAsn1Template SecCmsKeyAgreeRecipientInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsKeyAgreeRecipientInfo) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsKeyAgreeRecipientInfo,version) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SecCmsKeyAgreeRecipientInfo,originatorIdentifierOrKey),
	  SecCmsOriginatorIdentifierOrKeyTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 1,
	  offsetof(SecCmsKeyAgreeRecipientInfo,ukm),
	  SEC_ASN1_SUB(kSecAsn1OctetStringTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsKeyAgreeRecipientInfo,keyEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(SecCmsKeyAgreeRecipientInfo,recipientEncryptedKeys),
	  SecCmsRecipientEncryptedKeyTemplate },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * KEK recipient info
 */

static const SecAsn1Template SecCmsKEKIdentifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsKEKIdentifier) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsKEKIdentifier,keyIdentifier) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsKEKIdentifier,date) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsKEKIdentifier,other) },
    { 0 }
};

static const SecAsn1Template SecCmsKEKRecipientInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsKEKRecipientInfo) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsKEKRecipientInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsKEKRecipientInfo,kekIdentifier),
	  SecCmsKEKIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsKEKRecipientInfo,keyEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsKEKRecipientInfo,encKey) },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * recipient info
 */
const SecAsn1Template SecCmsRecipientInfoTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(SecCmsRecipientInfo,recipientInfoType), NULL,
	  sizeof(SecCmsRecipientInfo) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(SecCmsRecipientInfo,ri.keyAgreeRecipientInfo),
	  SecCmsKeyAgreeRecipientInfoTemplate,
	  SecCmsRecipientInfoIDKeyAgree },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(SecCmsRecipientInfo,ri.kekRecipientInfo),
	  SecCmsKEKRecipientInfoTemplate,
	  SecCmsRecipientInfoIDKEK },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsRecipientInfo,ri.keyTransRecipientInfo),
	  SecCmsKeyTransRecipientInfoTemplate,
	  SecCmsRecipientInfoIDKeyTrans },
    { 0 }
};

/* -----------------------------------------------------------------------------
 *
 */

const SecAsn1Template SecCmsDigestedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SecCmsDigestedData) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsDigestedData,version) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SecCmsDigestedData,digestAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsDigestedData,contentInfo),
	  SecCmsEncapsulatedContentInfoTemplate },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsDigestedData,digest) },
    { 0 }
};

const SecAsn1Template NSS_PointerToCMSDigestedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SecCmsDigestedDataTemplate }
};

const SecAsn1Template SecCmsEncryptedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SecCmsEncryptedData) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecCmsEncryptedData,version) },
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsEncryptedData,contentInfo),
	  SecCmsEncryptedContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(SecCmsEncryptedData,unprotectedAttr),
	  nss_cms_set_of_attribute_template },
    { 0 }
};

const SecAsn1Template NSS_PointerToCMSEncryptedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SecCmsEncryptedDataTemplate }
};

/* -----------------------------------------------------------------------------
 * SetOfSignedCrlTemplate
 */
const SecAsn1Template SecCmsIssuerAndSNTemplate[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(SecCmsIssuerAndSN) },
#if 1 // @@@ Switch to using NSS_NameTemplate
    { SEC_ASN1_ANY,
          offsetof(SecCmsIssuerAndSN,derIssuer) },
#else
    { SEC_ASN1_INLINE,
	  offsetof(SecCmsIssuerAndSN,issuer),
	  NSS_NameTemplate },
#endif
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
          offsetof(SecCmsIssuerAndSN,serialNumber) },
    { 0 }
};


/* -----------------------------------------------------------------------------
 * FORTEZZA KEA
 */
const SecAsn1Template NSS_SMIMEKEAParamTemplateSkipjack[] = {
	{ SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsSMIMEKEAParameters) },
	{ SEC_ASN1_OCTET_STRING /* | SEC_ASN1_OPTIONAL */,
	  offsetof(SecCmsSMIMEKEAParameters,originatorKEAKey) },
	{ SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsSMIMEKEAParameters,originatorRA) },
	{ 0 }
};

const SecAsn1Template NSS_SMIMEKEAParamTemplateNoSkipjack[] = {
	{ SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsSMIMEKEAParameters) },
	{ SEC_ASN1_OCTET_STRING /* | SEC_ASN1_OPTIONAL */,
	  offsetof(SecCmsSMIMEKEAParameters,originatorKEAKey) },
	{ SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsSMIMEKEAParameters,originatorRA) },
	{ SEC_ASN1_OCTET_STRING  | SEC_ASN1_OPTIONAL ,
	  offsetof(SecCmsSMIMEKEAParameters,nonSkipjackIV) },
	{ 0 }
};

const SecAsn1Template NSS_SMIMEKEAParamTemplateAllParams[] = {
	{ SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecCmsSMIMEKEAParameters) },
	{ SEC_ASN1_OCTET_STRING /* | SEC_ASN1_OPTIONAL */,
	  offsetof(SecCmsSMIMEKEAParameters,originatorKEAKey) },
	{ SEC_ASN1_OCTET_STRING,
	  offsetof(SecCmsSMIMEKEAParameters,originatorRA) },
	{ SEC_ASN1_OCTET_STRING  | SEC_ASN1_OPTIONAL ,
	  offsetof(SecCmsSMIMEKEAParameters,nonSkipjackIV) },
	{ SEC_ASN1_OCTET_STRING  | SEC_ASN1_OPTIONAL ,
	  offsetof(SecCmsSMIMEKEAParameters,bulkKeySize) },
	{ 0 }
};

const SecAsn1Template *
nss_cms_get_kea_template(SecCmsKEATemplateSelector whichTemplate)
{
	const SecAsn1Template *returnVal = NULL;

	switch(whichTemplate)
	{
	case SecCmsKEAUsesNonSkipjack:
		returnVal = NSS_SMIMEKEAParamTemplateNoSkipjack;
		break;
	case SecCmsKEAUsesSkipjack:
		returnVal = NSS_SMIMEKEAParamTemplateSkipjack;
		break;
	case SecCmsKEAUsesNonSkipjackWithPaddedEncKey:
	default:
		returnVal = NSS_SMIMEKEAParamTemplateAllParams;
		break;
	}
	return returnVal;
}

/* -----------------------------------------------------------------------------
 *
 */
static const SecAsn1Template *
nss_cms_choose_content_template(void *src_or_dest, Boolean encoding, const char *buf, size_t len, void *dest)
{
    const SecAsn1Template *theTemplate;
    SecCmsContentInfoRef cinfo;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    cinfo = (SecCmsContentInfoRef)src_or_dest;
    switch (SecCmsContentInfoGetContentTypeTag(cinfo)) {
    default:
	theTemplate = SEC_ASN1_GET(kSecAsn1PointerToAnyTemplate);
	break;
    case SEC_OID_PKCS7_DATA:
	theTemplate = SEC_ASN1_GET(kSecAsn1PointerToOctetStringTemplate);
	break;
    case SEC_OID_PKCS7_SIGNED_DATA:
	theTemplate = NSS_PointerToCMSSignedDataTemplate;
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	theTemplate = NSS_PointerToCMSEnvelopedDataTemplate;
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	theTemplate = NSS_PointerToCMSDigestedDataTemplate;
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	theTemplate = NSS_PointerToCMSEncryptedDataTemplate;
	break;
    }
    return theTemplate;
}
