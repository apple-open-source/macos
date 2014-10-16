/* Copyright (c) 2002-2003,2006,2008 Apple Inc.
 *
 * dbAttrs.cpp - Apple DL/DB/Keychain attributes and name/value pairs.
 *               The attribute lists here are not necessarily complete lists
 *				 of the attrs in any given schema; they are only the ones we want
 *			     to examine with dbTool.
 */

#include "dbAttrs.h"
#include <Security/cssmapple.h>
#include <Security/SecKeychainItem.h>
#include <Security/cssmapplePriv.h>
#include <security_cdsa_utilities/Schema.h>

/* declare a CSSM_DB_ATTRIBUTE_INFO with NAME_AS_STRING */
#define DB_ATTRIBUTE(name, type) \
	{  CSSM_DB_ATTRIBUTE_NAME_AS_STRING, \
	   {(char *)#name}, \
	   CSSM_DB_ATTRIBUTE_FORMAT_ ## type \
	}

/* declare a CSSM_DB_ATTRIBUTE_INFO with NAME_AS_INTEGER */
#define DB_INT_ATTRIBUTE(name, type) \
	{  CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER, \
	   { (char *)name }, \
	   CSSM_DB_ATTRIBUTE_FORMAT_ ## type \
	}


/* declare one entry in a table of nameValuePairs */
#define NVP(attr)		{attr, #attr}

/* the NULL entry which terminates all nameValuePair tables */
#define NVP_END			{0, NULL}

/* declare a RelationInfo */
#define RELATION_INFO(relationId, attributes, nameValues) \
	{ relationId, \
	  #relationId, \
	  sizeof(attributes) / sizeof(CSSM_DB_ATTRIBUTE_INFO), \
	  attributes, \
	  nameValues }

/* CSSM_DB_RECORDTYPE names */
const NameValuePair recordTypeNames[] = 
{
	NVP(CSSM_DL_DB_SCHEMA_INFO),
	NVP(CSSM_DL_DB_SCHEMA_INDEXES),
	NVP(CSSM_DL_DB_SCHEMA_ATTRIBUTES),
	NVP(CSSM_DL_DB_SCHEMA_PARSING_MODULE),
	NVP(CSSM_DL_DB_RECORD_ANY),
	NVP(CSSM_DL_DB_RECORD_CERT),
	NVP(CSSM_DL_DB_RECORD_CRL),
	NVP(CSSM_DL_DB_RECORD_POLICY),
	NVP(CSSM_DL_DB_RECORD_GENERIC),
	NVP(CSSM_DL_DB_RECORD_PUBLIC_KEY),
	NVP(CSSM_DL_DB_RECORD_PRIVATE_KEY),
	NVP(CSSM_DL_DB_RECORD_SYMMETRIC_KEY),
	NVP(CSSM_DL_DB_RECORD_ALL_KEYS),
	/* Apple-specific */
	NVP(CSSM_DL_DB_RECORD_GENERIC_PASSWORD),
	NVP(CSSM_DL_DB_RECORD_INTERNET_PASSWORD),
	NVP(CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD),
	NVP(CSSM_DL_DB_RECORD_X509_CERTIFICATE),
	NVP(CSSM_DL_DB_RECORD_X509_CRL),
	NVP(CSSM_DL_DB_RECORD_USER_TRUST),
	/* private to AppleCSPDL */
	NVP(DBBlobRelationID),
	/* private to Sec layer */
	NVP(CSSM_DL_DB_RECORD_UNLOCK_REFERRAL),
	NVP(CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE),
	NVP_END
};

/* CSSM_CERT_TYPE names */
const NameValuePair certTypeNames[] = 
{
	NVP(CSSM_CERT_UNKNOWN),
	NVP(CSSM_CERT_X_509v1),
	NVP(CSSM_CERT_X_509v2),
	NVP(CSSM_CERT_X_509v3),
	NVP(CSSM_CERT_PGP),
	NVP(CSSM_CERT_SPKI),
	NVP(CSSM_CERT_SDSIv1),
	NVP(CSSM_CERT_Intel),
	NVP(CSSM_CERT_X_509_ATTRIBUTE),
	NVP(CSSM_CERT_X9_ATTRIBUTE),
	NVP(CSSM_CERT_TUPLE),
	NVP(CSSM_CERT_ACL_ENTRY),
	NVP(CSSM_CERT_MULTIPLE),
	NVP_END
};

/* CSSM_CERT_ENCODING names */
const NameValuePair certEncodingNames[] = 
{
	NVP(CSSM_CERT_ENCODING_UNKNOWN),
	NVP(CSSM_CERT_ENCODING_CUSTOM),
	NVP(CSSM_CERT_ENCODING_BER),
	NVP(CSSM_CERT_ENCODING_DER),
	NVP(CSSM_CERT_ENCODING_NDR),
	NVP(CSSM_CERT_ENCODING_SEXPR),
	NVP(CSSM_CERT_ENCODING_PGP),
	NVP(CSSM_CERT_ENCODING_MULTIPLE),
	NVP_END
};

/* CSSM_CRL_TYPE names */
const NameValuePair crlTypeNames[] = 
{
	NVP(CSSM_CRL_TYPE_UNKNOWN),
	NVP(CSSM_CRL_TYPE_X_509v1),
	NVP(CSSM_CRL_TYPE_X_509v2),
	NVP(CSSM_CRL_TYPE_SPKI),
	NVP(CSSM_CRL_TYPE_MULTIPLE),
	NVP_END
};

/* CSSM_CRL_ENCODING names */
const NameValuePair crlEncodingNames[] = 
{
	NVP(CSSM_CRL_ENCODING_UNKNOWN),
	NVP(CSSM_CRL_ENCODING_CUSTOM),
	NVP(CSSM_CRL_ENCODING_BER),
	NVP(CSSM_CRL_ENCODING_DER),
	NVP(CSSM_CRL_ENCODING_BLOOM),
	NVP(CSSM_CRL_ENCODING_SEXPR),
	NVP(CSSM_CRL_ENCODING_MULTIPLE),
	NVP_END
};


/* CSSM_ALGORITHMS names */
const NameValuePair algIdNames[] = 
{
	NVP(CSSM_ALGID_NONE),
	NVP(CSSM_ALGID_DES),
	NVP(CSSM_ALGID_DESX),
	NVP(CSSM_ALGID_3DES_3KEY_EDE),
	NVP(CSSM_ALGID_3DES_3KEY),
	NVP(CSSM_ALGID_RC2),
	NVP(CSSM_ALGID_RC5),
	NVP(CSSM_ALGID_RC4),
	NVP(CSSM_ALGID_RSA),
	NVP(CSSM_ALGID_DSA),
	NVP(CSSM_ALGID_FEE),
	NVP_END
};

/* CSSM_DL_DB_SCHEMA_INFO */
static const CSSM_DB_ATTRIBUTE_INFO schemaInfoAttrs[] = {
	DB_ATTRIBUTE(RelationID, UINT32),
	DB_ATTRIBUTE(RelationName, STRING),
};

static const NameValuePair *schemaInfoNvp[] = {
	recordTypeNames,
	NULL
};

const RelationInfo schemaInfoRelation = 
	RELATION_INFO(CSSM_DL_DB_SCHEMA_INFO, 
		schemaInfoAttrs, 
		schemaInfoNvp);

/* CSSM_DL_DB_RECORD_ALL_KEYS (partial) */
static const CSSM_DB_ATTRIBUTE_INFO allKeysAttrs[] = {
	DB_ATTRIBUTE(KeyClass, UINT32),
	DB_ATTRIBUTE(KeyType, UINT32),
	DB_ATTRIBUTE(PrintName, BLOB),
	DB_ATTRIBUTE(Alias, BLOB),
	DB_ATTRIBUTE(Permanent, UINT32),
	DB_ATTRIBUTE(Private, UINT32),
	DB_ATTRIBUTE(Modifiable, UINT32),
	DB_ATTRIBUTE(Label, BLOB),
	DB_ATTRIBUTE(ApplicationTag, BLOB),
	DB_ATTRIBUTE(KeyCreator, BLOB),
	DB_ATTRIBUTE(KeySizeInBits, UINT32),
	DB_ATTRIBUTE(EffectiveKeySize, UINT32),
	DB_ATTRIBUTE(StartDate, BLOB),
	DB_ATTRIBUTE(EndDate, BLOB),
	DB_ATTRIBUTE(Sensitive, UINT32),
	DB_ATTRIBUTE(AlwaysSensitive, UINT32),
	DB_ATTRIBUTE(Extractable, UINT32),
	DB_ATTRIBUTE(NeverExtractable, UINT32),
	DB_ATTRIBUTE(Encrypt, UINT32),
	DB_ATTRIBUTE(Decrypt, UINT32),
	DB_ATTRIBUTE(Derive, UINT32),
	DB_ATTRIBUTE(Sign, UINT32),
	DB_ATTRIBUTE(Verify, UINT32),
	DB_ATTRIBUTE(SignRecover, UINT32),
	DB_ATTRIBUTE(VerifyRecover, UINT32),
	DB_ATTRIBUTE(Wrap, UINT32),
	DB_ATTRIBUTE(Unwrap, UINT32),	
};

static const NameValuePair *allKeysNvp[] = {
	recordTypeNames,		/* KeyClass - in this context, 
							 * a subset of these */
	algIdNames,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

const RelationInfo allKeysRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_ALL_KEYS, 
		allKeysAttrs, 
		allKeysNvp);

/* CSSM_DL_DB_RECORD_ANY, with the few attrs that all records have in common */
static const CSSM_DB_ATTRIBUTE_INFO anyRecordAttrs[] = {
	DB_ATTRIBUTE(PrintName, BLOB)
};

static const NameValuePair *anyRecordNvp[] = {
	NULL
};

const RelationInfo anyRecordRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_ANY, 
		anyRecordAttrs, 
		anyRecordNvp);

/* CSSM_DL_DB_RECORD_CERT - obsolete */
static const CSSM_DB_ATTRIBUTE_INFO certRecordAttrs[] = {
	DB_ATTRIBUTE(CertType, UINT32),
	DB_ATTRIBUTE(CertEncoding, UINT32),
	DB_ATTRIBUTE(PrintName, BLOB),
	DB_ATTRIBUTE(Alias, BLOB),
	DB_ATTRIBUTE(CertIdentity, BLOB),
	DB_ATTRIBUTE(KeyLabel, BLOB)
};

static const NameValuePair *certRecordNvp[] = {
	certTypeNames,
	certEncodingNames,
	NULL,
	NULL,
	NULL,
	NULL
};

const RelationInfo certRecordRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_CERT, 
		certRecordAttrs, 
		certRecordNvp);

/* Apple-specific CSSM_DL_DB_RECORD_X509_CERTIFICATE */
static const CSSM_DB_ATTRIBUTE_INFO x509CertRecordAttrs[] = {
	DB_ATTRIBUTE(CertType, UINT32),
	DB_ATTRIBUTE(CertEncoding, UINT32),
	DB_ATTRIBUTE(PrintName, BLOB),
	DB_ATTRIBUTE(Alias, BLOB),
	DB_ATTRIBUTE(Subject, BLOB),
	DB_ATTRIBUTE(Issuer, BLOB),
	DB_ATTRIBUTE(SerialNumber, BLOB),
	DB_ATTRIBUTE(SubjectKeyIdentifier, BLOB),
	DB_ATTRIBUTE(PublicKeyHash, BLOB)
};

static const NameValuePair *x509CertRecordNvp[] = {
	certTypeNames,
	certEncodingNames,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

const RelationInfo x509CertRecordRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_X509_CERTIFICATE, 
		x509CertRecordAttrs, 
		x509CertRecordNvp);


/* Apple-specific CSSM_DL_DB_RECORD_X509_CRL */
static const CSSM_DB_ATTRIBUTE_INFO x509CrlRecordAttrs[] = {
	DB_ATTRIBUTE(CrlType, UINT32),
	DB_ATTRIBUTE(CrlEncoding, UINT32),
	DB_ATTRIBUTE(PrintName, BLOB),
	DB_ATTRIBUTE(Alias, BLOB),
	DB_ATTRIBUTE(Issuer, BLOB),
	DB_ATTRIBUTE(ThisUpdate, BLOB),
	DB_ATTRIBUTE(NextUpdate, BLOB),
	DB_ATTRIBUTE(URI, BLOB),
	DB_ATTRIBUTE(CrlNumber, UINT32),
	DB_ATTRIBUTE(DeltaCrlNumber, UINT32),
};

static const NameValuePair *x509CrlRecordNvp[] = {
	crlTypeNames,
	crlEncodingNames,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

const RelationInfo x509CrlRecordRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_X509_CRL, 
		x509CrlRecordAttrs, 
		x509CrlRecordNvp);


/* generic keychain template, when recordType unknown  */
static const CSSM_DB_ATTRIBUTE_INFO genericKcAttrs[] = {
	DB_INT_ATTRIBUTE(kSecInvisibleItemAttr, SINT32),
	DB_ATTRIBUTE(PrintName, BLOB),
	DB_INT_ATTRIBUTE(kSecDescriptionItemAttr, BLOB),
	DB_INT_ATTRIBUTE(kSecTypeItemAttr, UINT32),
	/* more to come */
};

static const NameValuePair *genericKcNvp[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

const RelationInfo genericKcRelation = 
	RELATION_INFO(0,			// not used!
		genericKcAttrs, 
		genericKcNvp);

/* UserTrust */
static const CSSM_DB_ATTRIBUTE_INFO userTrustAttrs[] = {
	DB_ATTRIBUTE(TrustedCertificate, BLOB),
	DB_ATTRIBUTE(TrustedPolicy, BLOB),
	DB_ATTRIBUTE(PrintName, BLOB),
};

static const NameValuePair *userTrustNvp[] = {
	NULL,
	NULL,
	NULL,
	NULL,
};

const RelationInfo userTrustRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_USER_TRUST,
		userTrustAttrs, 
		userTrustNvp);

/* remainder added after the schema were publicly available via Schema.h */

/* unlock referral record */

using namespace Security;
using namespace KeychainCore;

static const CSSM_DB_ATTRIBUTE_INFO unlockReferralRecordAttrs[] = 
{
	Schema::kUnlockReferralType,
	Schema::kUnlockReferralDbName,
	Schema::kUnlockReferralDbGuid,
	Schema::kUnlockReferralDbSSID,
	Schema::kUnlockReferralDbSSType,
	Schema::kUnlockReferralDbNetname,
	Schema::kUnlockReferralKeyLabel,
	Schema::kUnlockReferralKeyAppTag,
	Schema::kUnlockReferralPrintName,
	Schema::kUnlockReferralAlias
};

const NameValuePair referralTypeNames[] = 
{
	NVP(CSSM_APPLE_UNLOCK_TYPE_KEY_DIRECT),
	NVP(CSSM_APPLE_UNLOCK_TYPE_WRAPPED_PRIVATE),
	NVP_END
};


static const NameValuePair *referralNvp[] = {
	referralTypeNames,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

const RelationInfo referralRecordRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_UNLOCK_REFERRAL,
		unlockReferralRecordAttrs, 
		referralNvp);

/* extended attribute record */
static const CSSM_DB_ATTRIBUTE_INFO extendedAttrRecordAttrs[] = 
{
	Schema::kExtendedAttributeRecordType,
	Schema::kExtendedAttributeItemID,
	Schema::kExtendedAttributeAttributeName,
	Schema::kExtendedAttributeModDate,
	Schema::kExtendedAttributeAttributeValue
};

static const NameValuePair *extendedAttrNvp[] = {
	recordTypeNames,
	NULL,
	NULL,
	NULL,
	NULL
};

const RelationInfo extendedAttrRelation = 
	RELATION_INFO(CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE,
		extendedAttrRecordAttrs, 
		extendedAttrNvp);

