/*
 * asn1Templates.c - Common ASN1 templates for use with libNSSDer.
 */

#include <secasn1.h>
#include "X509Templates.h"
#include "keyTemplates.h"
#include <assert.h>

/* 
 * Validity
 */
/*
 * NSS_Time Template chooser.
 */
static const NSS_TagChoice timeChoices[] = {
	{ SEC_ASN1_GENERALIZED_TIME, SEC_GeneralizedTimeTemplate} ,
	{ SEC_ASN1_UTC_TIME, SEC_UTCTimeTemplate },
	{ 0, NULL}
};

static const SEC_ASN1Template * NSS_TimeChooser(
	void *arg, 
	PRBool enc,
	const char *buf,
	void *dest)
{
	return NSS_TaggedTemplateChooser(arg, enc, buf, dest, timeChoices);
}

static const SEC_ASN1TemplateChooserPtr NSS_TimeChooserPtr = NSS_TimeChooser;

const SEC_ASN1Template NSS_ValidityTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_Validity) },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_Validity,notBefore.item),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_Validity,notAfter.item),
	  &NSS_TimeChooserPtr },
    { 0 }
};

/* X509 cert extension */
const SEC_ASN1Template NSS_CertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_CertExtension) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_CertExtension,extnId) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,		/* XXX DER_DEFAULT */
	  offsetof(NSS_CertExtension,critical) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSS_CertExtension,value) },
    { 0, }
};

const SEC_ASN1Template NSS_SequenceOfCertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, NSS_CertExtensionTemplate }
};

/* TBS Cert */
const SEC_ASN1Template NSS_TBSCertificateTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_TBSCertificate) },
	/* optional version, explicit tag 0, default 0 */
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0, 		/* XXX DER_DEFAULT */ 
	  offsetof(NSS_TBSCertificate,version),
	  SEC_IntegerTemplate },
	/* serial number is SIGNED integer */
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(NSS_TBSCertificate,serialNumber) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,signature),
	  NSS_AlgorithmIDTemplate },
	{ SEC_ASN1_SAVE, offsetof(NSS_TBSCertificate,derIssuer) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,issuer),
	  NSS_NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,validity),
	  NSS_ValidityTemplate },
	{ SEC_ASN1_SAVE, offsetof(NSS_TBSCertificate,derSubject) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,subject),
	  NSS_NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,subjectPublicKeyInfo),
	  NSS_SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSS_TBSCertificate,issuerID),
	  SEC_BitStringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(NSS_TBSCertificate,subjectID),
	  SEC_BitStringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 3,
	  offsetof(NSS_TBSCertificate,extensions),
	  NSS_SequenceOfCertExtensionTemplate },
    { 0 }
};

/*
 * For signing and verifying only, treating the TBS portion as an
 * opaque ASN_ANY blob.
 */
const SEC_ASN1Template NSS_SignedCertOrCRLTemplate[] =
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_SignedCertOrCRL) },
    { SEC_ASN1_ANY, 
	  offsetof(NSS_SignedCertOrCRL,tbsBlob) },
    { SEC_ASN1_ANY,
	  offsetof(NSS_SignedCertOrCRL,signatureAlgorithm) },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_SignedCertOrCRL,signature) },
    { 0 }
};

/* Fully specified signed certificate */
const SEC_ASN1Template NSS_SignedCertTemplate[] = 
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_Certificate) },
    { SEC_ASN1_INLINE, 
	  offsetof(NSS_Certificate,tbs),
	  NSS_TBSCertificateTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_Certificate,signatureAlgorithm),
	  NSS_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_Certificate,signature) },
    { 0 }
};

/* Entry in CRL.revokedCerts */
const SEC_ASN1Template NSS_RevokedCertTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_RevokedCert) },
	  /* serial number - signed itneger, just like in the actual cert */
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(NSS_RevokedCert,userCertificate) },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_RevokedCert,revocationDate.item),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_RevokedCert,extensions),
	  NSS_CertExtensionTemplate },
    { 0, }
};

const SEC_ASN1Template NSS_SequenceOfRevokedCertTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, NSS_RevokedCertTemplate }
};

/* NSS_TBSCrl (unsigned CRL) */
const SEC_ASN1Template NSS_TBSCrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_TBSCrl) },
	/* optional version, default 0 */
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, offsetof (NSS_TBSCrl, version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCrl,signature),
	  NSS_AlgorithmIDTemplate },
	{ SEC_ASN1_SAVE, offsetof(NSS_TBSCrl,derIssuer) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCrl,issuer),
	  NSS_NameTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_TBSCrl,thisUpdate.item),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_TBSCrl,nextUpdate),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_TBSCrl,revokedCerts),
	  NSS_RevokedCertTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 0,
	  offsetof(NSS_TBSCrl,extensions),
	  NSS_SequenceOfCertExtensionTemplate },
    { 0, }
};

/* Fully specified signed CRL */
const SEC_ASN1Template NSS_SignedCrlTemplate[] = 
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_Crl) },
    { SEC_ASN1_INLINE, 
	  offsetof(NSS_Crl,tbs),
	  NSS_TBSCrlTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_Crl,signatureAlgorithm),
	  NSS_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_Crl,signature) },
    { 0 }
};
