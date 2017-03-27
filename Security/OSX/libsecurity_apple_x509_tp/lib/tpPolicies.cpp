/*
 * Copyright (c) 2000-2014 Apple Inc. All Rights Reserved.
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
	policies.cpp - TP module policy implementation
*/

#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include "tpPolicies.h"
#include <Security/cssmerr.h>
#include "tpdebugging.h"
#include "certGroupUtils.h"
#include <Security/x509defs.h>
#include <Security/oidsalg.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/certextensions.h>
#include <Security/cssmapple.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <CoreFoundation/CFString.h>
#include <CommonCrypto/CommonDigest.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

/*
 * Our private per-extension info. One of these per (understood) extension per
 * cert.
 */
typedef struct {
	CSSM_BOOL		present;
	CSSM_BOOL		critical;
	CE_Data  		*extnData;		// mallocd by CL
	CSSM_DATA		*valToFree;		// the data we pass to freeField()
} iSignExtenInfo;

/*
 * Struct to keep track of info pertinent to one cert.
 */
typedef struct {

	/* extensions we're interested in */
	iSignExtenInfo		authorityId;
	iSignExtenInfo		subjectId;
	iSignExtenInfo		keyUsage;
	iSignExtenInfo		extendKeyUsage;
	iSignExtenInfo		basicConstraints;
	iSignExtenInfo		netscapeCertType;
	iSignExtenInfo		subjectAltName;
	iSignExtenInfo		certPolicies;
	iSignExtenInfo		qualCertStatements;
	iSignExtenInfo		nameConstraints;
	iSignExtenInfo		policyMappings;
	iSignExtenInfo		policyConstraints;
	iSignExtenInfo		inhibitAnyPolicy;
	iSignExtenInfo		certificatePolicies;

	/* flag indicating presence of CSSMOID_APPLE_EXTENSION_PROVISIONING_PROFILE_SIGNING */
	CSSM_BOOL			foundProvisioningProfileSigningMarker;
	/* flag indicating presence of CSSMOID_APPLE_EXTENSION_PASSBOOK_SIGNING */
	CSSM_BOOL			foundPassbookSigningMarker;
	/* flag indicating presence of CSSMOID_APPLE_EXTENSION_SYSINT2_INTERMEDIATE */
	CSSM_BOOL			foundAppleSysInt2Marker;
	/* flag indicating presence of CSSMOID_APPLE_EXTENSION_SERVER_AUTHENTICATION */
	CSSM_BOOL			foundAppleServerAuthMarker;
	/* flag indicating presence of CSSMOID_APPLE_EXTENSION_ESCROW_SERVICE */
	CSSM_BOOL			foundEscrowServiceMarker;
	/* flag indicating presence of CSSMOID_APPLE_EXTENSION_WWDR_INTERMEDIATE */
	CSSM_BOOL			foundAppleWWDRIntMarker;
	/* flag indicating presence of a critical extension we don't understand */
	CSSM_BOOL			foundUnknownCritical;
	/* flag indicating that this certificate was signed with a known-broken algorithm */
	CSSM_BOOL			untrustedSigAlg;

} iSignCertInfo;

/*
 * The list of Qualified Cert Statement statementIds we understand, even though
 * we don't actually do anything with them; if these are found in a Qualified
 * Cert Statement that's critical, we can truthfully say "yes we understand this".
 */
static const CSSM_OID_PTR knownQualifiedCertStatements[] =
{
	(const CSSM_OID_PTR)&CSSMOID_OID_QCS_SYNTAX_V1,
	(const CSSM_OID_PTR)&CSSMOID_OID_QCS_SYNTAX_V2,
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_COMPLIANCE,
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_LIMIT_VALUE,
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_RETENTION,
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_SSCD
};
#define NUM_KNOWN_QUAL_CERT_STATEMENTS (sizeof(knownQualifiedCertStatements) / sizeof(CSSM_OID_PTR))

static CSSM_RETURN tp_verifyMacAppStoreReceiptOpts(TPCertGroup &certGroup,
	const CSSM_DATA *fieldOpts,	const iSignCertInfo *certInfo);

static CSSM_RETURN tp_verifyPassbookSigningOpts(TPCertGroup &certGroup,
	const CSSM_DATA *fieldOpts, const iSignCertInfo *certInfo);

bool certificatePoliciesContainsOID(const CE_CertPolicies *certPolicies, const CSSM_OID *oidToFind);

#define kSecPolicySHA1Size 20
static const UInt8 kAppleCASHA1[kSecPolicySHA1Size] = {
    0x61, 0x1E, 0x5B, 0x66, 0x2C, 0x59, 0x3A, 0x08, 0xFF, 0x58,
    0xD1, 0x4A, 0xE2, 0x24, 0x52, 0xD1, 0x98, 0xDF, 0x6C, 0x60
};

static const UInt8 kMobileRootSHA1[kSecPolicySHA1Size] =  {
    0xBD, 0xD6, 0x7C, 0x34, 0xD0, 0xB2, 0x68, 0x5D, 0x31, 0x82,
    0xCD, 0x32, 0xCB, 0xF4, 0x54, 0x69, 0xA1, 0xF1, 0x6B, 0x09
};

static const UInt8 kAppleCorpCASHA1[kSecPolicySHA1Size] = {
    0xA1, 0x71, 0xDC, 0xDE, 0xE0, 0x8B, 0x1B, 0xAE, 0x30, 0xA1,
    0xAE, 0x6C, 0xC6, 0xD4, 0x03, 0x3B, 0xFD, 0xEF, 0x91, 0xCE
};

/*
 * Certificate policy OIDs
 */

/* 2.5.29.32.0 */
#define ANY_POLICY_OID				OID_EXTENSION, 0x32, 0x00
#define ANY_POLICY_OID_LEN			OID_EXTENSION_LENGTH + 2

/* 2.5.29.54 */
#define INHIBIT_ANY_POLICY_OID		OID_EXTENSION, 0x54
#define INHIBIT_ANY_POLICY_OID_LEN	OID_EXTENSION_LENGTH + 1

/* 2.16.840.1.101.2.1 */
#define US_DOD_INFOSEC				0x60, 0x86, 0x48, 0x01, 0x65, 0x02, 0x01
#define US_DOD_INFOSEC_LEN			7

/* 2.16.840.1.101.2.1.11.10 */
#define PIV_AUTH_OID				US_DOD_INFOSEC, 0x0B, 0x0A
#define PIV_AUTH_OID_LEN			US_DOD_INFOSEC_LEN + 2

/* 2.16.840.1.101.2.1.11.20 */
#define PIV_AUTH_2048_OID			US_DOD_INFOSEC, 0x0B, 0x14
#define PIV_AUTH_2048_OID_LEN		US_DOD_INFOSEC_LEN + 2

static const uint8 	OID_ANY_POLICY[] = {ANY_POLICY_OID};
const CSSM_OID	CSSMOID_ANY_POLICY   = {ANY_POLICY_OID_LEN, (uint8 *)OID_ANY_POLICY};
static const uint8 	OID_INHIBIT_ANY_POLICY[] = {INHIBIT_ANY_POLICY_OID};
const CSSM_OID	CSSMOID_INHIBIT_ANY_POLICY   = {INHIBIT_ANY_POLICY_OID_LEN, (uint8 *)OID_INHIBIT_ANY_POLICY};
static const uint8 	OID_PIV_AUTH[] = {PIV_AUTH_OID};
const CSSM_OID	CSSMOID_PIV_AUTH   = {PIV_AUTH_OID_LEN, (uint8 *)OID_PIV_AUTH};
static const uint8 	OID_PIV_AUTH_2048[] = {PIV_AUTH_2048_OID};
const CSSM_OID	CSSMOID_PIV_AUTH_2048   = {PIV_AUTH_2048_OID_LEN, (uint8 *)OID_PIV_AUTH_2048};

static CSSM_RETURN tp_verifyAppleIDSharingOpts(TPCertGroup &certGroup,
												const CSSM_DATA *fieldOpts,			// optional Common Name
												const iSignCertInfo *certInfo);
/*
 * Setup a single iSignExtenInfo. Called once per known extension
 * per cert.
 */
static CSSM_RETURN tpSetupExtension(
	Allocator			&alloc,
	CSSM_DATA 			*extnData,
	iSignExtenInfo		*extnInfo)		// which component of certInfo
{
	if(extnData->Length != sizeof(CSSM_X509_EXTENSION)) {
		tpPolicyError("tpSetupExtension: malformed CSSM_FIELD");
		return CSSMERR_TP_UNKNOWN_FORMAT;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)extnData->Data;
	extnInfo->present   = CSSM_TRUE;
	extnInfo->critical  = cssmExt->critical;
	extnInfo->extnData  = (CE_Data *)cssmExt->value.parsedValue;
	extnInfo->valToFree = extnData;
	return CSSM_OK;
}

/*
 * Fetch a known extension, set up associated iSignExtenInfo if present.
 */
static CSSM_RETURN iSignFetchExtension(
	Allocator			&alloc,
	TPCertInfo			*tpCert,
	const CSSM_OID		*fieldOid,		// which extension to fetch
	iSignExtenInfo		*extnInfo)		// where the info goes
{
	CSSM_DATA_PTR	fieldValue;			// mallocd by CL
	CSSM_RETURN		crtn;

	crtn = tpCert->fetchField(fieldOid, &fieldValue);
	switch(crtn) {
		case CSSM_OK:
			break;
		case CSSMERR_CL_NO_FIELD_VALUES:
			/* field not present, OK */
			return CSSM_OK;
		default:
			return crtn;
	}
	return tpSetupExtension(alloc,
			fieldValue,
			extnInfo);
}

/*
 * This function performs a check of an extension marked 'critical'
 * to see if it's one we understand. Returns CSSM_OK if the extension
 * is acceptable, CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN if unknown.
 */
static CSSM_RETURN iSignVerifyCriticalExtension(
	CSSM_X509_EXTENSION	*cssmExt)
{
	if (!cssmExt || !cssmExt->extnId.Data)
		return CSSMERR_TP_INVALID_FIELD_POINTER;

	if (!cssmExt->critical)
		return CSSM_OK;

	/* FIXME: remove when policyConstraints NSS template is fixed */
	if (!memcmp(cssmExt->extnId.Data, CSSMOID_PolicyConstraints.Data, CSSMOID_PolicyConstraints.Length))
		return CSSM_OK;

	if (cssmExt->extnId.Length > APPLE_EXTENSION_OID_LENGTH &&
		!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION.Data, APPLE_EXTENSION_OID_LENGTH)) {
		/* This extension's OID is under the appleCertificateExtensions arc */
		return CSSM_OK;

	}
	return CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN;
}

/*
 * Search for all unknown extensions. If we find one which is flagged critical,
 * flag certInfo->foundUnknownCritical. Only returns error on gross errors.
 */
static CSSM_RETURN iSignSearchUnknownExtensions(
	TPCertInfo			*tpCert,
	iSignCertInfo		*certInfo)
{
	CSSM_RETURN 	crtn;
	CSSM_DATA_PTR 	fieldValue = NULL;
	CSSM_HANDLE		searchHand = CSSM_INVALID_HANDLE;
	uint32 			numFields = 0;

	certInfo->foundProvisioningProfileSigningMarker = CSSM_FALSE;
	certInfo->foundPassbookSigningMarker = CSSM_FALSE;
	certInfo->foundAppleSysInt2Marker = CSSM_FALSE;
	certInfo->foundAppleWWDRIntMarker = CSSM_FALSE;
	certInfo->foundEscrowServiceMarker = CSSM_FALSE;

	crtn = CSSM_CL_CertGetFirstCachedFieldValue(tpCert->clHand(),
		tpCert->cacheHand(),
		&CSSMOID_X509V3CertificateExtensionCStruct,
		&searchHand,
		&numFields,
		&fieldValue);
	switch(crtn) {
		case CSSM_OK:
			/* found one, proceed */
			break;
		case CSSMERR_CL_NO_FIELD_VALUES:
			/* no unknown extensions present, OK */
			return CSSM_OK;
		default:
			return crtn;
	}

	if(fieldValue->Length != sizeof(CSSM_X509_EXTENSION)) {
		tpPolicyError("iSignSearchUnknownExtensions: malformed CSSM_FIELD");
		return CSSMERR_TP_UNKNOWN_FORMAT;
	}

	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)fieldValue->Data;
	if (cssmExt->extnId.Length == APPLE_EXTENSION_CODE_SIGNING_LENGTH+1 &&
		!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_PASSBOOK_SIGNING.Data,
		APPLE_EXTENSION_CODE_SIGNING_LENGTH+1)) {
		/* this is the Passbook Signing extension */
		certInfo->foundPassbookSigningMarker = CSSM_TRUE;
	}
	if (cssmExt->extnId.Length == APPLE_EXTENSION_SYSINT2_INTERMEDIATE_LENGTH &&
		!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_SYSINT2_INTERMEDIATE.Data,
		 APPLE_EXTENSION_SYSINT2_INTERMEDIATE_LENGTH)) {
		/* this is the Apple System Integration 2 Signing extension */
		certInfo->foundAppleSysInt2Marker = CSSM_TRUE;
	}
	if (cssmExt->extnId.Length == APPLE_EXTENSION_ESCROW_SERVICE_LENGTH &&
		!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_ESCROW_SERVICE.Data,
		 APPLE_EXTENSION_ESCROW_SERVICE_LENGTH)) {
		/* this is the Escrow Service Signing extension */
		certInfo->foundEscrowServiceMarker = CSSM_TRUE;
	}
	if (cssmExt->extnId.Length == APPLE_EXTENSION_PROVISIONING_PROFILE_SIGNING_LENGTH &&
		!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_PROVISIONING_PROFILE_SIGNING.Data,
		 APPLE_EXTENSION_PROVISIONING_PROFILE_SIGNING_LENGTH)) {
		/* this is the Provisioning Profile Signing extension */
		certInfo->foundProvisioningProfileSigningMarker = CSSM_TRUE;
	}
	if (cssmExt->extnId.Length == APPLE_EXTENSION_WWDR_INTERMEDIATE_LENGTH &&
		!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_WWDR_INTERMEDIATE.Data,
		 APPLE_EXTENSION_WWDR_INTERMEDIATE_LENGTH)) {
		/* this is the Apple WWDR extension */
		certInfo->foundAppleWWDRIntMarker = CSSM_TRUE;
	}

	if(iSignVerifyCriticalExtension(cssmExt) != CSSM_OK) {
		/* BRRZAPP! Found an unknown extension marked critical */
		certInfo->foundUnknownCritical = CSSM_TRUE;
		goto fini;
	}
	CSSM_CL_FreeFieldValue(tpCert->clHand(),
		&CSSMOID_X509V3CertificateExtensionCStruct,
		fieldValue);
	fieldValue = NULL;

	/* process remaining unknown extensions */
	for(unsigned i=1; i<numFields; i++) {
		crtn = CSSM_CL_CertGetNextCachedFieldValue(tpCert->clHand(),
			searchHand,
			&fieldValue);
		if(crtn) {
			/* should never happen */
			tpPolicyError("searchUnknownExtensions: GetNextCachedFieldValue"
				"error");
			break;
		}
		if(fieldValue->Length != sizeof(CSSM_X509_EXTENSION)) {
			tpPolicyError("iSignSearchUnknownExtensions: "
				"malformed CSSM_FIELD");
			crtn = CSSMERR_TP_UNKNOWN_FORMAT;
			break;
		}

		CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)fieldValue->Data;
		if (cssmExt->extnId.Length == APPLE_EXTENSION_CODE_SIGNING_LENGTH+1 &&
				!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_PASSBOOK_SIGNING.Data,
					APPLE_EXTENSION_CODE_SIGNING_LENGTH+1)) {
			/* this is the Passbook Signing extension */
			certInfo->foundPassbookSigningMarker = CSSM_TRUE;
		}
		if (cssmExt->extnId.Length == APPLE_EXTENSION_SYSINT2_INTERMEDIATE_LENGTH &&
				!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_SYSINT2_INTERMEDIATE.Data,
					APPLE_EXTENSION_SYSINT2_INTERMEDIATE_LENGTH)) {
			/* this is the Apple System Integration 2 Signing extension */
			certInfo->foundAppleSysInt2Marker = CSSM_TRUE;
		}
		if (cssmExt->extnId.Length == APPLE_EXTENSION_SERVER_AUTHENTICATION_LENGTH &&
				!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_SERVER_AUTHENTICATION.Data,
					APPLE_EXTENSION_SERVER_AUTHENTICATION_LENGTH)) {
			/* this is the Apple Server Authentication extension */
			certInfo->foundAppleServerAuthMarker = CSSM_TRUE;
		}
		if (cssmExt->extnId.Length == APPLE_EXTENSION_ESCROW_SERVICE_LENGTH &&
				!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_ESCROW_SERVICE.Data,
					APPLE_EXTENSION_ESCROW_SERVICE_LENGTH)) {
			/* this is the Escrow Service Signing extension */
			certInfo->foundEscrowServiceMarker = CSSM_TRUE;
		}
		if (cssmExt->extnId.Length == APPLE_EXTENSION_PROVISIONING_PROFILE_SIGNING_LENGTH &&
				!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_PROVISIONING_PROFILE_SIGNING.Data,
					APPLE_EXTENSION_PROVISIONING_PROFILE_SIGNING_LENGTH)) {
			/* this is the Provisioning Profile Signing extension */
			certInfo->foundProvisioningProfileSigningMarker = CSSM_TRUE;
		}
		if (cssmExt->extnId.Length == APPLE_EXTENSION_WWDR_INTERMEDIATE_LENGTH &&
				!memcmp(cssmExt->extnId.Data, CSSMOID_APPLE_EXTENSION_WWDR_INTERMEDIATE.Data,
					APPLE_EXTENSION_WWDR_INTERMEDIATE_LENGTH)) {
			/* this is the Apple WWDR extension */
			certInfo->foundAppleWWDRIntMarker = CSSM_TRUE;
		}

		if(iSignVerifyCriticalExtension(cssmExt) != CSSM_OK) {
			/* BRRZAPP! Found an unknown extension marked critical */
			certInfo->foundUnknownCritical = CSSM_TRUE;
			break;
		}
		CSSM_CL_FreeFieldValue(tpCert->clHand(),
			&CSSMOID_X509V3CertificateExtensionCStruct,
			fieldValue);
		fieldValue = NULL;
	} /* for additional fields */

fini:
	if(fieldValue) {
		CSSM_CL_FreeFieldValue(tpCert->clHand(),
			&CSSMOID_X509V3CertificateExtensionCStruct,
			fieldValue);
	}
	if(searchHand != CSSM_INVALID_HANDLE) {
		CSSM_CL_CertAbortQuery(tpCert->clHand(), searchHand);
	}
	return crtn;
}

/*
 * Check the signature algorithm. If it's known to be untrusted,
 * flag certInfo->untrustedSigAlg.
 */
static void iSignCheckSignatureAlgorithm(
	TPCertInfo			*tpCert,
	iSignCertInfo		*certInfo)
{
	CSSM_X509_ALGORITHM_IDENTIFIER	*algId = NULL;
	CSSM_DATA_PTR					valueToFree = NULL;

	algId = tp_CertGetAlgId(tpCert, &valueToFree);
	if(!algId ||
		tpCompareCssmData(&algId->algorithm, &CSSMOID_MD2) ||
		tpCompareCssmData(&algId->algorithm, &CSSMOID_MD2WithRSA) ||
		tpCompareCssmData(&algId->algorithm, &CSSMOID_MD5) ||
		tpCompareCssmData(&algId->algorithm, &CSSMOID_MD5WithRSA) ) {
		certInfo->untrustedSigAlg = CSSM_TRUE;
	} else {
		certInfo->untrustedSigAlg = CSSM_FALSE;
	}

	if (valueToFree) {
		tp_CertFreeAlgId(tpCert->clHand(), valueToFree);
	}
}

/*
 * Given a TPCertInfo, fetch the associated iSignCertInfo fields.
 * Returns CSSM_FAIL on error.
 */
static CSSM_RETURN iSignGetCertInfo(
	Allocator			&alloc,
	TPCertInfo			*tpCert,
	iSignCertInfo		*certInfo)
{
	CSSM_RETURN			crtn;

	/* first grind thru the extensions we're interested in */
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_AuthorityKeyIdentifier,
		&certInfo->authorityId);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_SubjectKeyIdentifier,
		&certInfo->subjectId);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_KeyUsage,
		&certInfo->keyUsage);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_ExtendedKeyUsage,
		&certInfo->extendKeyUsage);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_BasicConstraints,
		&certInfo->basicConstraints);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_NetscapeCertType,
		&certInfo->netscapeCertType);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_SubjectAltName,
		&certInfo->subjectAltName);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_CertificatePolicies,
		&certInfo->certPolicies);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_QC_Statements,
		&certInfo->qualCertStatements);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_NameConstraints,
		&certInfo->nameConstraints);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_PolicyMappings,
		&certInfo->policyMappings);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_PolicyConstraints,
		&certInfo->policyConstraints);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_InhibitAnyPolicy,
		&certInfo->inhibitAnyPolicy);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_CertificatePolicies,
		&certInfo->certificatePolicies);
	if(crtn) {
		return crtn;
	}

	/* check signature algorithm field */
	iSignCheckSignatureAlgorithm(tpCert, certInfo);

	/* now look for extensions we don't understand - the only thing we're interested
	 * in is the critical flag. */
	return iSignSearchUnknownExtensions(tpCert, certInfo);
}

/*
 * Free (via CL) the fields allocated in iSignGetCertInfo().
 */
static void iSignFreeCertInfo(
	CSSM_CL_HANDLE	clHand,
	iSignCertInfo	*certInfo)
{
	if(certInfo->authorityId.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_AuthorityKeyIdentifier,
			certInfo->authorityId.valToFree);
	}
	if(certInfo->subjectId.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_SubjectKeyIdentifier,
			certInfo->subjectId.valToFree);
	}
	if(certInfo->keyUsage.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_KeyUsage,
			certInfo->keyUsage.valToFree);
	}
	if(certInfo->extendKeyUsage.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_ExtendedKeyUsage,
			certInfo->extendKeyUsage.valToFree);
	}
	if(certInfo->basicConstraints.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_BasicConstraints,
			certInfo->basicConstraints.valToFree);
	}
	if(certInfo->netscapeCertType.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_NetscapeCertType,
			certInfo->netscapeCertType.valToFree);
	}
	if(certInfo->subjectAltName.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_SubjectAltName,
			certInfo->subjectAltName.valToFree);
	}
	if(certInfo->certPolicies.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_CertificatePolicies,
			certInfo->certPolicies.valToFree);
	}
//	if(certInfo->policyConstraints.present) {
//		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_PolicyConstraints,
//			certInfo->policyConstraints.valToFree);
//	}
	if(certInfo->qualCertStatements.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_QC_Statements,
			certInfo->qualCertStatements.valToFree);
	}
	if(certInfo->certificatePolicies.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_CertificatePolicies,
			certInfo->certificatePolicies.valToFree);
	}
}

/*
 * See if cert's Subject.{commonName,EmailAddress} matches caller-specified
 * string. Returns CSSM_TRUE if match, else returns CSSM_FALSE.
 * Also indicates whether *any* of the specified fields were found, regardless
 * of match state.
 */
typedef enum {
	SN_CommonName,			// CSSMOID_CommonName, host name format
	SN_Email,				// CSSMOID_EmailAddress
	SN_UserID,				// CSSMOID_UserID
	SN_OrgUnit				// CSSMOID_OrganizationalUnitName
} SubjSubjNameSearchType;

static CSSM_BOOL tpCompareSubjectName(
	TPCertInfo 				&cert,
	SubjSubjNameSearchType	searchType,
	bool					normalizeAll,		// for SN_Email case: lower-case all of
												// the cert's value, not just the portion
												// after the '@'
	const char 				*callerStr,			// already tpToLower'd
	uint32					callerStrLen,
	bool					&fieldFound)
{
	char 			*certName = NULL;			// from cert's subject name
	uint32 			certNameLen = 0;
	CSSM_DATA_PTR 	subjNameData = NULL;
	CSSM_RETURN 	crtn;
	CSSM_BOOL		ourRtn = CSSM_FALSE;
	const CSSM_OID	*oidSrch;

	const unsigned char x500_userid_oid[] = { 0x09,0x92,0x26,0x89,0x93,0xF2,0x2C,0x64,0x01,0x01 };
	CSSM_OID X500_UserID_OID = { sizeof(x500_userid_oid), (uint8*)x500_userid_oid };

	fieldFound = false;
	switch(searchType) {
		case SN_CommonName:
			oidSrch = &CSSMOID_CommonName;
			break;
		case SN_Email:
			oidSrch = &CSSMOID_EmailAddress;
			break;
		case SN_UserID:
			oidSrch = &X500_UserID_OID;
			break;
		case SN_OrgUnit:
			oidSrch = &CSSMOID_OrganizationalUnitName;
			break;
		default:
			assert(0);
			return CSSM_FALSE;
	}
	crtn = cert.fetchField(&CSSMOID_X509V1SubjectNameCStruct, &subjNameData);
	if(crtn) {
		/* should never happen, we shouldn't be here if there is no subject */
		tpPolicyError("tpCompareSubjectName: error retrieving subject name");
		return CSSM_FALSE;
	}
	CSSM_X509_NAME_PTR x509name = (CSSM_X509_NAME_PTR)subjNameData->Data;
	if((x509name == NULL) || (subjNameData->Length != sizeof(CSSM_X509_NAME))) {
		tpPolicyError("tpCompareSubjectName: malformed CSSM_X509_NAME");
		cert.freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
		return CSSM_FALSE;
	}

	/* Now grunge thru the X509 name looking for a common name */
	CSSM_X509_TYPE_VALUE_PAIR 	*ptvp;
	CSSM_X509_RDN_PTR    		rdnp;
	unsigned					rdnDex;
	unsigned					pairDex;

	for(rdnDex=0; rdnDex<x509name->numberOfRDNs; rdnDex++) {
		rdnp = &x509name->RelativeDistinguishedName[rdnDex];
		for(pairDex=0; pairDex<rdnp->numberOfPairs; pairDex++) {
			ptvp = &rdnp->AttributeTypeAndValue[pairDex];
			if(tpCompareOids(&ptvp->type, oidSrch)) {
				fieldFound = true;
				certName = (char *)ptvp->value.Data;
				certNameLen = (uint32)ptvp->value.Length;
				switch(searchType) {
					case SN_CommonName:
					{
						/* handle odd encodings that we need to convert to 8-bit */
						CFStringBuiltInEncodings encoding = kCFStringEncodingUnicode;
						CFDataRef cfd = NULL;
						bool doConvert = false;
						switch(ptvp->valueType) {
							case BER_TAG_T61_STRING:
								/* a.k.a. Teletex */
								encoding = kCFStringEncodingISOLatin1;
								doConvert = true;
								break;
							case BER_TAG_PKIX_BMP_STRING:
								encoding = kCFStringEncodingUnicode;
								doConvert = true;
								break;
							/*
							 * All others - either take as is, or let it fail due to
							 * illegal/incomprehensible format
							 */
							default:
								break;
						}
						if(doConvert) {
							/* raw data ==> CFString */
							cfd = CFDataCreate(NULL, (UInt8 *)certName,	certNameLen);
							if(cfd == NULL) {
								/* try next component */
								break;
							}
							CFStringRef cfStr = CFStringCreateFromExternalRepresentation(
								NULL, cfd, encoding);
							CFRelease(cfd);
							if(cfStr == NULL) {
								tpPolicyError("tpCompareSubjectName: bad str (1)");
								break;
							}

							/* CFString ==> straight ASCII */
							cfd = CFStringCreateExternalRepresentation(NULL,
								cfStr, kCFStringEncodingASCII, 0);
							CFRelease(cfStr);
							if(cfd == NULL) {
								tpPolicyError("tpCompareSubjectName: bad str (2)");
								break;
							}
							certNameLen = (uint32)CFDataGetLength(cfd);
							certName = (char *)CFDataGetBytePtr(cfd);
						}
						ourRtn = tpCompareHostNames(callerStr, callerStrLen,
							certName, certNameLen);
						if(doConvert) {
							assert(cfd != NULL);
							CFRelease(cfd);
						}
						break;
					}
					case SN_Email:
						ourRtn = tpCompareEmailAddr(callerStr, callerStrLen,
							certName, certNameLen, normalizeAll);
						break;
					case SN_UserID:
					case SN_OrgUnit:
						/* exact match only here, for now */
						ourRtn = ((callerStrLen == certNameLen) &&
								!memcmp(callerStr, certName, certNameLen)) ?
								CSSM_TRUE : CSSM_FALSE;
						break;
				}
				if(ourRtn) {
					/* success */
					break;
				}
				/* else keep going, maybe there's another common name */
			}
		}
		if(ourRtn) {
			break;
		}
	}
	cert.freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
	return ourRtn;
}

/*
 * Compare ASCII form of an IP address to a CSSM_DATA containing
 * the IP address's numeric components. Returns true on match.
 */
static CSSM_BOOL tpCompIpAddrStr(
	const char *str,
	unsigned strLen,
	const CSSM_DATA *numeric)
{
	const char *cp = str;
	const char *nextDot;
	char buf[100];

	if((numeric == NULL) || (numeric->Length == 0) || (str == NULL)) {
		return CSSM_FALSE;
	}
	if(cp[strLen - 1] == '\0') {
		/* ignore NULL terminator */
		strLen--;
	}
	for(unsigned dex=0; dex<numeric->Length; dex++) {
		/* cp points to start of current string digit */
		/* find next dot */
		const char *lastChar = cp + strLen;
		nextDot = cp + 1;
		for( ; nextDot<lastChar; nextDot++) {
			if(*nextDot == '.') {
				break;
			}
		}
		if(nextDot == lastChar) {
			/* legal and required on last digit */
			if(dex != (numeric->Length - 1)) {
				return CSSM_FALSE;
			}
		}
		else if(dex == (numeric->Length - 1)) {
			return CSSM_FALSE;
		}
		ptrdiff_t digLen = nextDot - cp;
		if(digLen >= sizeof(buf)) {
			/* preposterous */
			return CSSM_FALSE;
		}
		memmove(buf, cp, digLen);
		buf[digLen] = '\0';
		/* incr digLen to include the next dot */
		digLen++;
		cp += digLen;
		strLen -= digLen;
		int digVal = atoi(buf);
		if(digVal != numeric->Data[dex]) {
			return CSSM_FALSE;
		}
	}
	return CSSM_TRUE;
}

/*
 * See if cert's subjectAltName contains an element matching caller-specified
 * string, hostname, in the following forms:
 *
 * SAN_HostName : dnsName, iPAddress
 * SAN_Email    : RFC822Name
 *
 * Returns CSSM_TRUE if match, else returns CSSM_FALSE.
 *
 * Also indicates whether or not a dnsName (search type HostName) or
 * RFC822Name (search type SAM_Email) was found, regardless of result
 * of comparison.
 *
 * The appStr/appStrLen args are optional - if NULL/0, only the
 * search for dnsName/RFC822Name is done.
 */
typedef enum {
	SAN_HostName,
	SAN_Email
} SubjAltNameSearchType;

static CSSM_BOOL tpCompareSubjectAltName(
	const iSignExtenInfo	&subjAltNameInfo,
	const char 				*appStr,			// caller has lower-cased as appropriate
	uint32					appStrLen,
	SubjAltNameSearchType 	searchType,
	bool					normalizeAll,		// for SAN_Email case: lower-case all of
												// the cert's value, not just the portion
												// after the '@'
	bool					&dnsNameFound,		// RETURNED, SAN_HostName case
	bool					&emailFound)		// RETURNED, SAN_Email case
{
	dnsNameFound = false;
	emailFound = false;
	if(!subjAltNameInfo.present) {
		/* common failure, no subjectAltName found */
		return CSSM_FALSE;
	}

	CE_GeneralNames *names = &subjAltNameInfo.extnData->subjectAltName;
	CSSM_BOOL		ourRtn = CSSM_FALSE;
	char 			*certName;
	uint32                  certNameLen;

	/* Search thru the CE_GeneralNames looking for the appropriate attribute */
	for(unsigned dex=0; dex<names->numNames; dex++) {
		CE_GeneralName *name = &names->generalName[dex];
		switch(searchType) {
			case SAN_HostName:
				switch(name->nameType) {
					case GNT_IPAddress:
						if(appStr == NULL) {
							/* nothing to do here */
							break;
						}
						ourRtn = tpCompIpAddrStr(appStr, appStrLen, &name->name);
						break;

					case GNT_DNSName:
						if(name->berEncoded) {
							tpErrorLog("tpCompareSubjectAltName: malformed "
								"CE_GeneralName (1)\n");
							break;
						}
						certName = (char *)name->name.Data;
						if(certName == NULL) {
							tpErrorLog("tpCompareSubjectAltName: malformed "
								"CE_GeneralName (2)\n");
							break;
						}
						certNameLen = (uint32)(name->name.Length);
						dnsNameFound = true;
						if(appStr != NULL) {
							/* skip if caller passed in NULL */
							ourRtn = tpCompareHostNames(appStr, appStrLen,
								certName, certNameLen);
						}
						break;

					default:
						/* not interested, proceed to next name */
						break;
				}
				break;	/* from case HostName */

			case SAN_Email:
				if(name->nameType != GNT_RFC822Name) {
					/* not interested */
					break;
				}
				certName = (char *)name->name.Data;
				if(certName == NULL) {
					tpErrorLog("tpCompareSubjectAltName: malformed "
						"GNT_RFC822Name\n");
					break;
				}
				certNameLen = (uint32)(name->name.Length);
				emailFound = true;
				if(appStr != NULL) {
					ourRtn = tpCompareEmailAddr(appStr, appStrLen, certName,
						certNameLen, normalizeAll);
				}
				break;
		}
		if(ourRtn) {
			/* success */
			break;
		}
	}
	return ourRtn;
}

/* is host name in the form of a.b.c.d, where a,b,c, and d are digits? */
static CSSM_BOOL tpIsNumeric(
	const char *hostName,
	unsigned hostNameLen)
{
	if(hostName[hostNameLen - 1] == '\0') {
		/* ignore NULL terminator */
		hostNameLen--;
	}
	for(unsigned i=0; i<hostNameLen; i++) {
		char c = *hostName++;
		if(isdigit(c)) {
			continue;
		}
		if(c != '.') {
			return CSSM_FALSE;
		}
	}
	return CSSM_TRUE;
}

/*
 * Convert a typed string represented by a CSSM_X509_TYPE_VALUE_PAIR to a
 * CFStringRef. Caller owns and must release the result. NULL return means
 * unconvertible input "string".
 */
static CFStringRef tpTvpToCfString(
	const CSSM_X509_TYPE_VALUE_PAIR	*tvp)
{
	CFStringBuiltInEncodings encoding;
	switch(tvp->valueType) {
		case BER_TAG_T61_STRING:
			/* a.k.a. Teletex */
			encoding = kCFStringEncodingISOLatin1;
			break;
		case BER_TAG_PKIX_BMP_STRING:
			encoding = kCFStringEncodingUnicode;
			break;
		case BER_TAG_PRINTABLE_STRING:
		case BER_TAG_IA5_STRING:
		case BER_TAG_PKIX_UTF8_STRING:
			encoding = kCFStringEncodingUTF8;
			break;
		default:
			return NULL;
	}

	/* raw data ==> CFString */
	CFDataRef cfd = CFDataCreate(NULL, tvp->value.Data,	tvp->value.Length);
	if(cfd == NULL) {
			return NULL;
	}
	CFStringRef cfStr = CFStringCreateFromExternalRepresentation(NULL, cfd, encoding);
	CFRelease(cfd);
	return cfStr;
}

/*
 * Compare a CFString and a string represented by a CSSM_X509_TYPE_VALUE_PAIR.
 * Returns CSSM_TRUE if they are equal.
 */
static bool tpCompareTvpToCfString(
	const CSSM_X509_TYPE_VALUE_PAIR	*tvp,
	CFStringRef refStr,
	CFOptionFlags flags)		// e.g., kCFCompareCaseInsensitive
{
	CFStringRef cfStr = tpTvpToCfString(tvp);
	if(cfStr == NULL) {
		return false;
	}
	CFComparisonResult res = CFStringCompare(refStr, cfStr, flags);
	CFRelease(cfStr);
	if(res == kCFCompareEqualTo) {
		return true;
	}
	else {
		return false;
	}
}

/*
 * Given one iSignCertInfo, determine whether or not the specified
 * EKU OID, or - optionally - CSSMOID_ExtendedKeyUsageAny - is present.
 * Returns true if so, else false.
 */
static bool tpVerifyEKU(
	const iSignCertInfo &certInfo,
	const CSSM_OID &ekuOid,
	bool ekuAnyOK)			// if true, CSSMOID_ExtendedKeyUsageAny counts as "found"
{
	if(!certInfo.extendKeyUsage.present) {
		return false;
	}
	CE_ExtendedKeyUsage *eku = &certInfo.extendKeyUsage.extnData->extendedKeyUsage;
	assert(eku != NULL);

	for(unsigned i=0; i<eku->numPurposes; i++) {
		const CSSM_OID *foundEku = &eku->purposes[i];
		if(tpCompareOids(foundEku, &ekuOid)) {
			return true;
		}
		if(ekuAnyOK && tpCompareOids(foundEku, &CSSMOID_ExtendedKeyUsageAny)) {
			return true;
		}
	}
	return false;
}

/*
 * Given one iSignCertInfo, determine whether or not the specified
 * Certificate Policy OID, or - optionally - CSSMOID_ANY_POLICY - is present.
 * Returns true if so, else false.
 */
static bool tpVerifyCPE(
	const iSignCertInfo &certInfo,
	const CSSM_OID &cpOid,
	bool anyPolicyOK)		// if true, CSSMOID_ANY_POLICY counts as "found"
{
	if(!certInfo.certPolicies.present) {
		return false;
	}
	CE_CertPolicies *cp = &certInfo.certPolicies.extnData->certPolicies;
	assert(cp != NULL);

	for(unsigned i=0; i<cp->numPolicies; i++) {
		const CE_PolicyInformation *foundPolicy = &cp->policies[i];
		if(tpCompareOids(&foundPolicy->certPolicyId, &cpOid)) {
			return true;
		}
		if(anyPolicyOK && tpCompareOids(&foundPolicy->certPolicyId, &CSSMOID_ANY_POLICY)) {
			return true;
		}
	}
	return false;
}

/*
 * Verify iChat handle. We search for a matching (case-insensitive) string
 * comprised of:
 *
 *   -- name component ("dmitch") from subject name's CommonName
 *   -- implicit '@'
 *   -- domain name from subject name's organizationalUnit
 *
 * Plus we require an Organization component of "Apple Computer, Inc." or "Apple Inc."
 */
static bool tpCompareIChatHandleName(
	TPCertInfo 				&cert,
	const char 				*iChatHandle,		// UTF8
	uint32					iChatHandleLen)
{
	CSSM_DATA_PTR				subjNameData = NULL;		// from fetchField
	CSSM_RETURN					crtn;
	bool						ourRtn = false;
	CSSM_X509_NAME_PTR			x509name;
	CSSM_X509_TYPE_VALUE_PAIR 	*ptvp;
	CSSM_X509_RDN_PTR    		rdnp;
	unsigned					rdnDex;
	unsigned					pairDex;

	/* search until all of these are true */
	CSSM_BOOL	commonNameMatch = CSSM_FALSE;		// name before '@'
	CSSM_BOOL	orgUnitMatch = CSSM_FALSE;			// domain after '@
	CSSM_BOOL	orgMatch = CSSM_FALSE;				// Apple Computer, Inc. (or Apple Inc.)

	/*
	 * incoming UTF8 handle ==> two components.
	 * First convert to CFString.
	 */
	if(iChatHandle[iChatHandleLen - 1] == '\0') {
	  /* avoid NULL when creating CFStrings */
	  iChatHandleLen--;
	}
	CFDataRef cfd = CFDataCreate(NULL, (const UInt8 *)iChatHandle, iChatHandleLen);
	if(cfd == NULL) {
		return false;
	}
	CFStringRef handleStr = CFStringCreateFromExternalRepresentation(NULL, cfd,
		kCFStringEncodingUTF8);
	CFRelease(cfd);
	if(handleStr == NULL) {
		tpPolicyError("tpCompareIChatHandleName: bad incoming handle (1)");
		return false;
	}

	/*
	 * Find the '@' delimiter
	 */
	CFRange whereIsAt;
	whereIsAt = CFStringFind(handleStr, CFSTR("@"), 0);
	if(whereIsAt.length == 0) {
		tpPolicyError("tpCompareIChatHandleName: bad incoming handle: no @");
		CFRelease(handleStr);
		return false;
	}

	/*
	 * Two components, before and after delimiter
	 */
	CFRange r = {0, whereIsAt.location};
	CFStringRef	iChatName = CFStringCreateWithSubstring(NULL, handleStr, r);
	if(iChatName == NULL) {
		tpPolicyError("tpCompareIChatHandleName: bad incoming handle (2)");
		CFRelease(handleStr);
		return false;
	}
	r.location = whereIsAt.location + 1;		// after the '@'
	r.length = CFStringGetLength(handleStr) - r.location;
	CFStringRef iChatDomain = CFStringCreateWithSubstring(NULL, handleStr, r);
	CFRelease(handleStr);
	if(iChatDomain == NULL) {
		tpPolicyError("tpCompareIChatHandleName: bad incoming handle (3)");
		CFRelease(iChatName);
		return false;
	}
	/* subsequent errors to errOut: */

	/* get subject name in CSSM form, all subsequent ops work on that */
	crtn = cert.fetchField(&CSSMOID_X509V1SubjectNameCStruct, &subjNameData);
	if(crtn) {
		/* should never happen, we shouldn't be here if there is no subject */
		tpPolicyError("tpCompareIChatHandleName: error retrieving subject name");
		goto errOut;
	}

	x509name = (CSSM_X509_NAME_PTR)subjNameData->Data;
	if((x509name == NULL) || (subjNameData->Length != sizeof(CSSM_X509_NAME))) {
		tpPolicyError("tpCompareIChatHandleName: malformed CSSM_X509_NAME");
		goto errOut;
	}

	/* Now grunge thru the X509 name looking for three fields */

	for(rdnDex=0; rdnDex<x509name->numberOfRDNs; rdnDex++) {
		rdnp = &x509name->RelativeDistinguishedName[rdnDex];
		for(pairDex=0; pairDex<rdnp->numberOfPairs; pairDex++) {
			ptvp = &rdnp->AttributeTypeAndValue[pairDex];
			if(!commonNameMatch &&
			   tpCompareOids(&ptvp->type, &CSSMOID_CommonName) &&
			   tpCompareTvpToCfString(ptvp, iChatName, kCFCompareCaseInsensitive)) {
					commonNameMatch = CSSM_TRUE;
			}

			if(!orgUnitMatch &&
			   tpCompareOids(&ptvp->type, &CSSMOID_OrganizationalUnitName) &&
			   tpCompareTvpToCfString(ptvp, iChatDomain, kCFCompareCaseInsensitive)) {
					orgUnitMatch = CSSM_TRUE;
			}

			if(!orgMatch &&
			   tpCompareOids(&ptvp->type, &CSSMOID_OrganizationName) &&
			   /* this one is case sensitive */
			   (tpCompareTvpToCfString(ptvp, CFSTR("Apple Computer, Inc."), 0) ||
			    tpCompareTvpToCfString(ptvp, CFSTR("Apple Inc."), 0))) {
					orgMatch = CSSM_TRUE;
			}

			if(commonNameMatch && orgUnitMatch && orgMatch) {
				/* TA DA */
				ourRtn = true;
				goto errOut;
			}
		}
	}
errOut:
	cert.freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
	CFRelease(iChatName);
	CFRelease(iChatDomain);
	return ourRtn;
}

/*
 * Verify SSL options. Currently this just consists of matching the
 * leaf cert's subject common name against the caller's (optional)
 * server name.
 */
static CSSM_RETURN tp_verifySslOpts(
	TPPolicy policy,
	TPCertGroup &certGroup,
	const CSSM_DATA *sslFieldOpts,
	const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	const iSignCertInfo &leafCertInfo = certInfo[0];
	CSSM_APPLE_TP_SSL_OPTIONS *sslOpts = NULL;
	unsigned hostNameLen = 0;
	const char *serverName = NULL;
	TPCertInfo *leaf = certGroup.certAtIndex(0);
	assert(leaf != NULL);

	/* CSSM_APPLE_TP_SSL_OPTIONS is optional */
	if((sslFieldOpts != NULL) && (sslFieldOpts->Data != NULL)) {
		sslOpts = (CSSM_APPLE_TP_SSL_OPTIONS *)sslFieldOpts->Data;
		switch(sslOpts->Version) {
			case CSSM_APPLE_TP_SSL_OPTS_VERSION:
				if(sslFieldOpts->Length != sizeof(CSSM_APPLE_TP_SSL_OPTIONS)) {
					return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
				}
				break;
			/* handle backwards compatibility here if necessary */
			default:
				return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
		}
		hostNameLen = sslOpts->ServerNameLen;
		serverName = sslOpts->ServerName;
	}

	/* host name check is optional */
	if(hostNameLen != 0) {
		if(serverName == NULL) {
			return CSSMERR_TP_INVALID_POINTER;
		}

		/* convert caller's hostname string to lower case */
		char *hostName = (char *)certGroup.alloc().malloc(hostNameLen);
		memmove(hostName, serverName, hostNameLen);
		tpToLower(hostName, hostNameLen);

		CSSM_BOOL match = CSSM_FALSE;

		/* First check subjectAltName... */
		bool dnsNameFound = false;
		bool dummy;
		match = tpCompareSubjectAltName(leafCertInfo.subjectAltName,
			hostName, hostNameLen,
			SAN_HostName, false, dnsNameFound, dummy);

		/*
		 * Then common name, if
		 *  -- no match from subjectAltName, AND
		 *  -- dnsName was NOT found, AND
		 *  -- hostName is not strictly numeric form (1.2.3.4)
		 */
		if(!match && !dnsNameFound && !tpIsNumeric(hostName, hostNameLen)) {
			bool fieldFound;
			match = tpCompareSubjectName(*leaf, SN_CommonName, false, hostName, hostNameLen,
				fieldFound);
		}

		/*
		 * Limit allowed domains for specific anchors
		 */
		CSSM_BOOL domainMatch = CSSM_TRUE;
		if(match) {
			TPCertInfo *tpCert = certGroup.lastCert();
			if (tpCert) {
				const CSSM_DATA *certData = tpCert->itemData();
				unsigned char digest[CC_SHA1_DIGEST_LENGTH];
				CC_SHA1(certData->Data, (CC_LONG)certData->Length, digest);
				if (!memcmp(digest, kAppleCorpCASHA1, sizeof(digest))) {
					const char *dnlist[] = { "apple.com", "icloud.com" };
					unsigned int idx, dncount=2;
					domainMatch = CSSM_FALSE;
					for(idx=0;idx<dncount;idx++) {
						uint32 len=(uint32)strlen(dnlist[idx]);
						char *domainName=(char*)certGroup.alloc().malloc(len);
						memmove(domainName, (char*)dnlist[idx], len);
						if(tpCompareDomainSuffix(hostName, hostNameLen,
						   domainName, len)) {
							domainMatch = CSSM_TRUE;
						}
						certGroup.alloc().free(domainName);
						if (domainMatch) {
							break;
						}
					}
				}
			}
		}

		certGroup.alloc().free(hostName);
		if(!match) {
			if(leaf->addStatusCode(CSSMERR_APPLETP_HOSTNAME_MISMATCH)) {
				return CSSMERR_APPLETP_HOSTNAME_MISMATCH;
			}
		}
		if(!domainMatch) {
			if(leaf->addStatusCode(CSSMERR_APPLETP_CA_PIN_MISMATCH)) {
				return CSSMERR_APPLETP_CA_PIN_MISMATCH;
			}
		}
	}

	/*
	 * Ensure that, if an extendedKeyUsage extension is present in the
	 * leaf, that either anyExtendedKeyUsage or the appropriate
	 * CSSMOID_{Server,Client}Auth, or a SeverGatedCrypto usage is present.
	 */
	const iSignExtenInfo &ekuInfo = leafCertInfo.extendKeyUsage;
	if(ekuInfo.present) {
		bool foundGoodEku = false;
		bool isServer = true;
		CE_ExtendedKeyUsage *eku = (CE_ExtendedKeyUsage *)ekuInfo.extnData;
		assert(eku != NULL);

		/*
		 * Determine appropriate extended key usage; default is SSL server
		 */
		const CSSM_OID *extUse = &CSSMOID_ServerAuth;
		if((sslOpts != NULL) &&				/* optional, default server side */
		   (sslOpts->Version > 0) &&		/* this was added in struct version 1 */
		   (sslOpts->Flags & CSSM_APPLE_TP_SSL_CLIENT)) {
		   extUse = &CSSMOID_ClientAuth;
		   isServer = false;
		}

		/* search for that one or for "any" indicator */
		for(unsigned i=0; i<eku->numPurposes; i++) {
			const CSSM_OID *purpose = &eku->purposes[i];
			if(tpCompareOids(purpose, extUse)) {
				foundGoodEku = true;
				break;
			}
			if(tpCompareOids(purpose, &CSSMOID_ExtendedKeyUsageAny)) {
				foundGoodEku = true;
				break;
			}
			if((policy == kTP_IPSec) && (tpCompareOids(purpose, &CSSMOID_EKU_IPSec))) {
				foundGoodEku = true;
				break;
			}
			if(isServer) {
				/* server gated crypto: server side only */
				if(tpCompareOids(purpose, &CSSMOID_NetscapeSGC)) {
					foundGoodEku = true;
					break;
				}
				if(tpCompareOids(purpose, &CSSMOID_MicrosoftSGC)) {
					foundGoodEku = true;
					break;
				}
			}
		}
		if(!foundGoodEku) {
			if(leaf->addStatusCode(CSSMERR_APPLETP_SSL_BAD_EXT_KEY_USE)) {
				return CSSMERR_TP_VERIFY_ACTION_FAILED;
			}
		}
	}

	/*
	 * Check for additional options flag (2nd lowest bit set) which indicates
	 * we must be issued by an Apple intermediate with a particular extension.
	 * (This flag is set by SecPolicyCreateAppleSSLService in SecPolicy.cpp.)
	 */
	if((sslOpts != NULL) &&
	   (sslOpts->Version > 0) &&		/* this was added in struct version 1 */
	   (sslOpts->Flags & 0x00000002)) {

		if (certGroup.numCerts() > 1) {
			const iSignCertInfo *isCertInfo = &certInfo[1];
			if (!(isCertInfo->foundAppleServerAuthMarker == CSSM_TRUE)) {
				TPCertInfo *tpCert = certGroup.certAtIndex(1);
				tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
				return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
			}
		}
		else {
			/* we only have the leaf? */
			if(leaf->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION)) {
				return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
			}
		}
	}
	return CSSM_OK;
}

/*
 * Verify SMIME and iChat options.
 * This deals with both S/MIME and iChat policies; within the iChat domain it
 * deals with Apple-specific .mac certs as well as what we call "generic AIM"
 * certs, as used in the Windows AIM client.
 */
#define CE_CIPHER_MASK	(~(CE_KU_EncipherOnly | CE_KU_DecipherOnly))

static CSSM_RETURN tp_verifySmimeOpts(
	TPPolicy policy,
	TPCertGroup &certGroup,
	const CSSM_DATA *smimeFieldOpts,
	const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	const iSignCertInfo &leafCertInfo = certInfo[0];
	bool iChat = (policy == kTP_iChat) ? true : false;
	/*
	 * The CSSM_APPLE_TP_SMIME_OPTIONS pointer is optional as is everything in it.
	 */
	CSSM_APPLE_TP_SMIME_OPTIONS *smimeOpts = NULL;
	if(smimeFieldOpts != NULL) {
		smimeOpts = (CSSM_APPLE_TP_SMIME_OPTIONS *)smimeFieldOpts->Data;
	}
	if(smimeOpts != NULL) {
		switch(smimeOpts->Version) {
			case CSSM_APPLE_TP_SMIME_OPTS_VERSION:
				if(smimeFieldOpts->Length !=
						sizeof(CSSM_APPLE_TP_SMIME_OPTIONS)) {
					return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
				}
				break;
			/* handle backwards compatibility here if necessary */
			default:
				return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
		}
	}

	TPCertInfo *leaf = certGroup.certAtIndex(0);
	assert(leaf != NULL);

	/* Verify optional email address, a.k.a. handle for iChat policy */
	unsigned emailLen = 0;
	if(smimeOpts != NULL) {
		emailLen = smimeOpts->SenderEmailLen;
	}

	bool match = false;
	bool emailFoundInSAN = false;
	bool iChatHandleFound = false;		/* indicates a genuine Apple iChat cert */
	bool emailFoundInDN = false;
	if(emailLen != 0) {
		if(smimeOpts->SenderEmail == NULL) {
			return CSSMERR_TP_INVALID_POINTER;
		}

		/* iChat - first try the Apple custom format */
		if(iChat) {
			iChatHandleFound = tpCompareIChatHandleName(*leaf,	smimeOpts->SenderEmail,
				emailLen);
			if(iChatHandleFound) {
				match = true;
			}

		}

		if(!match) {
			/*
			 * normalize caller's email string
			 *  SMIME - lowercase only the portion after '@'
			 *  iChat - lowercase all of it
			 */
			char *email = (char *)certGroup.alloc().malloc(emailLen);
			memmove(email, smimeOpts->SenderEmail, emailLen);
			tpNormalizeAddrSpec(email, emailLen, iChat);


			/*
			 * First check subjectAltName. The emailFound bool indicates
			 * that *some* email address was found, regardless of a match
			 * condition.
			 */
			bool dummy;
			match = tpCompareSubjectAltName(leafCertInfo.subjectAltName,
				email, emailLen,
				SAN_Email, iChat, dummy, emailFoundInSAN);

			/*
			 * Then subject DN, CSSMOID_EmailAddress, if no match from
			 * subjectAltName. In this case the whole email address is
			 * case insensitive (RFC 3280, section 4.1.2.6), so
			 * renormalize.
			 */
			if(!match) {
				tpNormalizeAddrSpec(email, emailLen, true);
				match = tpCompareSubjectName(*leaf, SN_Email, true, email, emailLen,
					emailFoundInDN);
			}
			certGroup.alloc().free(email);

			/*
			 * Error here if no match found but there was indeed *some*
			 * email address in the cert.
			 */
			if(!match && (emailFoundInSAN || emailFoundInDN)) {
				if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND)) {
					tpPolicyError("SMIME email addrs in cert but no match");
					return CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND;
				}
			}
		}

		/*
		 * iChat only: error if app specified email address but there was
		 * none in the cert.
		 */
		if(iChat && !emailFoundInSAN && !emailFoundInDN && !iChatHandleFound) {
			if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS)) {
				tpPolicyError("iChat: no email address or handle in cert");
				return CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS;
			}
		}
	}

	/*
	 * Going by the letter of the law, here's what RFC 2632 has to say
	 * about the legality of an empty Subject Name:
	 *
	 *    ...the subject DN in a user's (i.e. end-entity) certificate MAY
	 *    be an empty SEQUENCE in which case the subjectAltName extension
	 *    will include the subject's identifier and MUST be marked as
	 *    critical.
	 *
	 * OK, first examine the leaf cert's subject name.
	 */
	CSSM_RETURN crtn;
	CSSM_DATA_PTR subjNameData = NULL;
	const iSignExtenInfo &kuInfo = leafCertInfo.keyUsage;
	const iSignExtenInfo &ekuInfo = leafCertInfo.extendKeyUsage;
	const CSSM_X509_NAME *x509Name = NULL;

	if(iChat) {
		/* empty subject name processing is S/MIME only */
		goto checkEku;
	}

	crtn = leaf->fetchField(&CSSMOID_X509V1SubjectNameCStruct, &subjNameData);
	if(crtn) {
		/* This should really never happen */
		tpPolicyError("SMIME policy: error fetching subjectName");
		leaf->addStatusCode(CSSMERR_TP_INVALID_CERTIFICATE);
		return CSSMERR_TP_INVALID_CERTIFICATE;
	}
	/* must do a leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct on exit */

	x509Name = (const CSSM_X509_NAME *)subjNameData->Data;
	if(x509Name->numberOfRDNs == 0) {
		/*
		 * Empty subject name. If we haven't already seen a valid
		 * email address in the subject alternate name (by looking
		 * for a specific address specified by app), try to find
		 * one now.
		 */
		if(!emailFoundInSAN &&		// haven't found one, and
		   (emailLen == 0)) {		// didn't even look yet
			bool dummy;
			tpCompareSubjectAltName(leafCertInfo.subjectAltName,
					NULL, 0,				// email, emailLen,
					SAN_Email, false, dummy,
					emailFoundInSAN);		// the variable we're updating
		}
		if(!emailFoundInSAN) {
			tpPolicyError("SMIME policy fail: empty subject name and "
				"no Email Addrs in SubjectAltName");
			if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS)) {
				leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
				return CSSMERR_TP_VERIFY_ACTION_FAILED;
			}
			else {
				/* have to skip the next block */
				goto postSAN;
			}
		}

		/*
		 * One more thing: this leaf must indeed have a subjAltName
		 * extension and it must be critical. We would not have gotten this
		 * far if the subjAltName extension was not actually present....
		 */
		assert(leafCertInfo.subjectAltName.present);
		if(!leafCertInfo.subjectAltName.critical) {
			tpPolicyError("SMIME policy fail: empty subject name and "
				"no Email Addrs in SubjectAltName");
			if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_SUBJ_ALT_NAME_NOT_CRIT)) {
				leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
				return CSSMERR_TP_VERIFY_ACTION_FAILED;
			}
		}
	}
postSAN:
	leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);

	/*
	 * Enforce the usage of the key associated with the leaf cert.
	 * Cert's KeyUsage must be a superset of what the app is trying to do.
	 * Note the {en,de}cipherOnly flags are handled separately....
	 */
	if(kuInfo.present && (smimeOpts != NULL)) {
		CE_KeyUsage certKu = *((CE_KeyUsage *)kuInfo.extnData);
		CE_KeyUsage appKu = smimeOpts->IntendedUsage;
		CE_KeyUsage intersection = certKu & appKu;
		if((intersection & CE_CIPHER_MASK) != (appKu & CE_CIPHER_MASK)) {
			tpPolicyError("SMIME KeyUsage err: appKu 0x%x  certKu 0x%x",
				appKu, certKu);
			if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE)) {
				return CSSMERR_TP_VERIFY_ACTION_FAILED;
			}
		}

		/* Now the en/de cipher only bits - for keyAgreement only */
		if(appKu & CE_KU_KeyAgreement) {
			/*
			 * 1. App wants to use this for key agreement; it must
			 *    say what it wants to do with the derived key.
			 *    In this context, the app's XXXonly bit means that
			 *    it wants to use the key for that op - not necessarliy
			 *    "only".
			 */
			if((appKu & (CE_KU_EncipherOnly | CE_KU_DecipherOnly)) == 0) {
				tpPolicyError("SMIME KeyUsage err: KeyAgreement with "
					"no Encipher or Decipher");
				if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE)) {
					return CSSMERR_TP_VERIFY_ACTION_FAILED;
				}
			}

			/*
			 * 2. If cert restricts to encipher only make sure the
			 *    app isn't trying to decipher.
			 */
			if((certKu & CE_KU_EncipherOnly) &&
			   (appKu & CE_KU_DecipherOnly)) {
				tpPolicyError("SMIME KeyUsage err: cert EncipherOnly, "
					"app wants to decipher");
				if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE)) {
					return CSSMERR_TP_VERIFY_ACTION_FAILED;
				}
			}

			/*
			 * 3. If cert restricts to decipher only make sure the
			 *    app isn't trying to encipher.
			 */
			if((certKu & CE_KU_DecipherOnly) &&
			   (appKu & CE_KU_EncipherOnly)) {
				tpPolicyError("SMIME KeyUsage err: cert DecipherOnly, "
					"app wants to encipher");
				if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE)) {
					return CSSMERR_TP_VERIFY_ACTION_FAILED;
				}
			}
		}
	}

	/*
	 * Extended Key Use verification, which is different for the two policies.
	 */
checkEku:
	if(iChat && !ekuInfo.present) {
		/*
		 * iChat: whether generic AIM cert or Apple .mac/iChat cert, we must have an
		 * extended key use extension.
		 */
		tpPolicyError("iChat: No extended Key Use");
		if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE)) {
			return CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE;
		}
	}

	if(!iChatHandleFound) {
		/*
		 * S/MIME and generic AIM certs when evaluating iChat policy.
		 * Look for either emailProtection or anyExtendedKeyUsage usages.
		 *
		 * S/MIME : the whole extension is optional.
		 * iChat  : extension must be there (which we've already covered, above)
		 *          and we must find one of those extensions.
		 */
		if(ekuInfo.present) {
			bool foundGoodEku = false;
			CE_ExtendedKeyUsage *eku = (CE_ExtendedKeyUsage *)ekuInfo.extnData;
			assert(eku != NULL);
			for(unsigned i=0; i<eku->numPurposes; i++) {
				if(tpCompareOids(&eku->purposes[i], &CSSMOID_EmailProtection)) {
					foundGoodEku = true;
					break;
				}
				if(tpCompareOids(&eku->purposes[i], &CSSMOID_ExtendedKeyUsageAny)) {
					foundGoodEku = true;
					break;
				}
			}
			if(!foundGoodEku) {
				tpPolicyError("iChat/SMIME: No appropriate extended Key Use");
				if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE)) {
					return CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE;
				}
			}
		}
	}
	else {
		/*
		 * Apple iChat cert. Look for anyExtendedKeyUsage, iChatSigning,
		 * ichatEncrypting - the latter of two which can optionally be
		 * required by app.
		 */
		assert(iChat);	/* or we could not have even looked for an iChat style handle */
		assert(ekuInfo.present);	/* checked above */
		bool foundAnyEku = false;
		bool foundIChatSign = false;
		bool foundISignEncrypt = false;
		CE_ExtendedKeyUsage *eku = (CE_ExtendedKeyUsage *)ekuInfo.extnData;
		assert(eku != NULL);

		for(unsigned i=0; i<eku->numPurposes; i++) {
			if(tpCompareOids(&eku->purposes[i],
					&CSSMOID_APPLE_EKU_ICHAT_SIGNING)) {
				foundIChatSign = true;
			}
			else if(tpCompareOids(&eku->purposes[i],
					&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION)) {
				foundISignEncrypt = true;
			}
			else if(tpCompareOids(&eku->purposes[i], &CSSMOID_ExtendedKeyUsageAny)) {
				foundAnyEku = true;
			}
		}

		if(!foundAnyEku && !foundISignEncrypt && !foundIChatSign) {
			/* No go - no acceptable uses found */
			tpPolicyError("iChat: No valid extended Key Uses found");
			if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE)) {
				return CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE;
			}
		}

		/* check for specifically required uses */
		if((smimeOpts != NULL) && (smimeOpts->IntendedUsage != 0)) {
			if(smimeOpts->IntendedUsage & CE_KU_DigitalSignature) {
				if(!foundIChatSign) {
					tpPolicyError("iChat: ICHAT_SIGNING required, but missing");
					if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE)) {
						return CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE;
					}
				}
			}
			if(smimeOpts->IntendedUsage & CE_KU_DataEncipherment) {
				if(!foundISignEncrypt) {
					tpPolicyError("iChat: ICHAT_ENCRYPT required, but missing");
					if(leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE)) {
						return CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE;
					}
				}
			}
		}	/* checking IntendedUsage */
	}	/* iChat cert format */

	return CSSM_OK;
}

/*
 * Verify Apple SW Update signing (was Apple Code Signing, pre-Leopard) options.
 *
 * -- Must have one intermediate cert
 * -- intermediate must have basic constraints with path length 0
 * -- intermediate has CSSMOID_APPLE_EKU_CODE_SIGNING EKU
 * -- leaf cert has either CODE_SIGNING or CODE_SIGN_DEVELOPMENT EKU (the latter of
 *    which triggers a CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT error)
 */
static CSSM_RETURN tp_verifySWUpdateSigningOpts(
	TPCertGroup &certGroup,
	const CSSM_DATA *fieldOpts,			// currently unused
	const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
//	const CE_BasicConstraints *bc;		// currently unused
	CE_ExtendedKeyUsage *eku;
	CSSM_RETURN crtn = CSSM_OK;

	if(numCerts != 3) {
		if(!certGroup.isAllowedError(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH)) {
			tpPolicyError("tp_verifySWUpdateSigningOpts: numCerts %u", numCerts);
			return CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		}
		else if(numCerts < 3) {
			/* this error allowed, but no intermediate...check leaf */
			goto checkLeaf;
		}
	}

	/* verify intermediate cert */
	isCertInfo = &certInfo[1];
	tpCert = certGroup.certAtIndex(1);

	if(!isCertInfo->basicConstraints.present) {
		tpPolicyError("tp_verifySWUpdateSigningOpts: no basicConstraints in intermediate");
		if(tpCert->addStatusCode(CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS)) {
			return CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS;
		}
	}

	/* ExtendedKeyUse required, one legal value */
	if(!isCertInfo->extendKeyUsage.present) {
		tpPolicyError("tp_verifySWUpdateSigningOpts: no extendedKeyUse in intermediate");
		if(tpCert->addStatusCode(CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE)) {
			return CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE;
		}
		else {
			goto checkLeaf;
		}
	}

	eku = &isCertInfo->extendKeyUsage.extnData->extendedKeyUsage;
	assert(eku != NULL);
	if(eku->numPurposes != 1) {
		tpPolicyError("tp_verifySWUpdateSigningOpts: bad eku->numPurposes in intermediate (%lu)",
			(unsigned long)eku->numPurposes);
		if(tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
			return CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		}
		else if(eku->numPurposes == 0) {
			/* ignore that error but no EKU - skip EKU check */
			goto checkLeaf;
		}
		/* else ignore error and we have an intermediate EKU; proceed */
	}

	if(!tpCompareOids(&eku->purposes[0], &CSSMOID_APPLE_EKU_CODE_SIGNING)) {
		tpPolicyError("tp_verifySWUpdateSigningOpts: bad EKU");
		if(tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
			crtn = CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		}
	}

checkLeaf:

	/* verify leaf cert */
	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);
	if(!isCertInfo->extendKeyUsage.present) {
		tpPolicyError("tp_verifySWUpdateSigningOpts: no extendedKeyUse in leaf");
		if(tpCert->addStatusCode(CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE)) {
			return crtn ? crtn : CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE;
		}
		else {
			/* have to skip remainder */
			return CSSM_OK;
		}
	}

	eku = &isCertInfo->extendKeyUsage.extnData->extendedKeyUsage;
	assert(eku != NULL);
	if(eku->numPurposes != 1) {
		tpPolicyError("tp_verifySWUpdateSigningOpts: bad eku->numPurposes (%lu)",
			(unsigned long)eku->numPurposes);
		if(tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
			if(crtn == CSSM_OK) {
				crtn = CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
			}
		}
		return crtn;
	}
	if(!tpCompareOids(&eku->purposes[0], &CSSMOID_APPLE_EKU_CODE_SIGNING)) {
		if(tpCompareOids(&eku->purposes[0], &CSSMOID_APPLE_EKU_CODE_SIGNING_DEV)) {
			tpPolicyError("tp_verifySWUpdateSigningOpts: DEVELOPMENT cert");
			if(tpCert->addStatusCode(CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT)) {
				if(crtn == CSSM_OK) {
					crtn = CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT;
				}
			}
		}
		else {
			tpPolicyError("tp_verifySWUpdateSigningOpts: bad EKU in leaf");
			if(tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
				if(crtn == CSSM_OK) {
					crtn = CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
				}
			}
		}
	}

	return crtn;
}

/*
 * Verify Apple Resource Signing options.
 *
 * -- leaf cert must have CSSMOID_APPLE_EKU_RESOURCE_SIGNING EKU
 * -- chain length must be >= 2
 * -- mainline code already verified that leaf KeyUsage = digitalSignature (only)
 */
static CSSM_RETURN tp_verifyResourceSigningOpts(
	TPCertGroup &certGroup,
	const CSSM_DATA *fieldOpts,			// currently unused
	const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	unsigned numCerts = certGroup.numCerts();
	if(numCerts < 2) {
		if(!certGroup.isAllowedError(CSSMERR_APPLETP_RS_BAD_CERT_CHAIN_LENGTH)) {
			tpPolicyError("tp_verifyResourceSigningOpts: numCerts %u", numCerts);
			return CSSMERR_APPLETP_RS_BAD_CERT_CHAIN_LENGTH;
		}
	}
	const iSignCertInfo &leafCert = certInfo[0];
	TPCertInfo *leaf = certGroup.certAtIndex(0);

	/* leaf ExtendedKeyUse required, one legal value */
	if(!tpVerifyEKU(leafCert, CSSMOID_APPLE_EKU_RESOURCE_SIGNING, false)) {
		tpPolicyError("tp_verifyResourceSigningOpts: no RESOURCE_SIGNING EKU");
		if(leaf->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
			return CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		}
	}

	return CSSM_OK;
}

/*
 * Common code for Apple Code Signing and Apple Package Signing.
 * For now we just require an RFC3280-style CodeSigning EKU in the leaf
 * for both policies.
 */
static CSSM_RETURN tp_verifyCodePkgSignOpts(
	TPPolicy policy,
	TPCertGroup &certGroup,
	const CSSM_DATA *fieldOpts,			// currently unused
	const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	const iSignCertInfo &leafCert = certInfo[0];

	/* leaf ExtendedKeyUse required, one legal value */
	if(!tpVerifyEKU(leafCert, CSSMOID_ExtendedUseCodeSigning, false)) {
		TPCertInfo *leaf = certGroup.certAtIndex(0);
		tpPolicyError("tp_verifyCodePkgSignOpts: no CodeSigning EKU");
		if(leaf->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
			return CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		}
	}

	return CSSM_OK;

}

/*
 * Verify MacAppStore receipt verification policy options.
 *
 * -- Must have one intermediate cert
 * -- intermediate must be the FairPlay intermediate
 * -- leaf cert has the CSSMOID_APPLE_EXTENSION_MACAPPSTORE_RECEIPT marker extension
 */
static CSSM_RETURN tp_verifyMacAppStoreReceiptOpts(
	TPCertGroup &certGroup,
	const CSSM_DATA *fieldOpts,			// currently unused
	const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	unsigned numCerts = certGroup.numCerts();
	if (numCerts < 3)
	{
		if (!certGroup.isAllowedError(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH))
		{
			tpPolicyError("tp_verifyMacAppStoreReceiptOpts: numCerts %u", numCerts);
			return CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		}
	}

	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;

	/* verify intermediate cert */
	isCertInfo = &certInfo[1];
	tpCert = certGroup.certAtIndex(1);

	if (!isCertInfo->basicConstraints.present)
	{
		tpPolicyError("tp_verifyAppleIDSharingOpts: no basicConstraints in intermediate");
		if (tpCert->addStatusCode(CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS))
			return CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS;
	}

	// Now check the leaf
	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);
	if (certInfo->certificatePolicies.present)
	{
	//	syslog(LOG_ERR, "tp_verifyMacAppStoreReceiptOpts: found certificatePolicies");
		const CE_CertPolicies *certPolicies =
				&isCertInfo->certificatePolicies.extnData->certPolicies;
		if (!certificatePoliciesContainsOID(certPolicies, &CSSMOID_MACAPPSTORE_RECEIPT_CERT_POLICY))
			if (tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION))
				return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
	}
	else
	{
	//	syslog(LOG_ERR, "tp_verifyMacAppStoreReceiptOpts: no certificatePolicies present");	// DEBUG
        tpPolicyError("tp_verifyMacAppStoreReceiptOpts: no certificatePolicies present in leaf");
        if (tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION))
			return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
	}

	return CSSM_OK;
}

bool certificatePoliciesContainsOID(const CE_CertPolicies *certPolicies, const CSSM_OID *oidToFind)
{
    // returns true if the given OID is present in the cert policies

    if (!certPolicies || !oidToFind)
		return false;

    const uint32 maxIndex = 100;		// sanity check
    for (uint32 policyIndex = 0; policyIndex < certPolicies->numPolicies && policyIndex < maxIndex; policyIndex++)
	{
        CE_PolicyInformation *certPolicyInfo = &certPolicies->policies[policyIndex];
        CSSM_OID_PTR oid = &certPolicyInfo->certPolicyId;
		if (oid && tpCompareOids(oid, oidToFind))	// found it
			return true;
    }

	return false;
}


/*
 * Verify Apple ID Sharing options.
 *
 * -- Do basic cert validation (OCSP-based certs)
 * -- Validate that the cert is an Apple ID sharing cert:
 *		has a custom extension: OID: Apple ID Sharing Certificate ( 1 2 840 113635 100 4 7 )
 *			(CSSMOID_APPLE_EXTENSION_APPLEID_SHARING)
 *		EKU should have both client and server authentication
 *		chains to the "Apple Application Integration Certification Authority" intermediate
 * -- optionally has a client-specified common name, which is the Apple ID account's UUID.

 * -- Must have one intermediate cert ("Apple Application Integration Certification Authority")
 * -- intermediate must have basic constraints with path length 0
 * -- intermediate has CSSMOID_APPLE_EXTENSION_AAI_INTERMEDIATE extension (OID 1 2 840 113635 100 6 2 3)
 OR APPLE_EXTENSION_AAI_INTERMEDIATE_2
 */

static CSSM_RETURN tp_verifyAppleIDSharingOpts(TPCertGroup &certGroup,
												const CSSM_DATA *fieldOpts,			// optional Common Name
												const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
	//	const CE_BasicConstraints *bc;		// currently unused
	CE_ExtendedKeyUsage *eku;
	CSSM_RETURN crtn = CSSM_OK;
	unsigned int serverNameLen = 0;
	const char *serverName = NULL;

	// The CSSM_APPLE_TP_SMIME_OPTIONS pointer is optional as is everything in it.
    if (fieldOpts && fieldOpts->Data)
    {
		CSSM_APPLE_TP_SSL_OPTIONS *sslOpts = (CSSM_APPLE_TP_SSL_OPTIONS *)fieldOpts->Data;
        switch (sslOpts->Version)
        {
        case CSSM_APPLE_TP_SSL_OPTS_VERSION:
            if (fieldOpts->Length != sizeof(CSSM_APPLE_TP_SSL_OPTIONS))
                return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
            break;
            /* handle backwards compatibility here if necessary */
        default:
            return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
		}
		serverNameLen = sslOpts->ServerNameLen;
		serverName = sslOpts->ServerName;
	}

	//------------------------------------------------------------------------

	if (numCerts != 3)
	{
		if (!certGroup.isAllowedError(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH))
		{
			tpPolicyError("tp_verifyAppleIDSharingOpts: numCerts %u", numCerts);
			return CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		}
		else
		if (numCerts < 3)
		{
			/* this error allowed, but no intermediate...check leaf */
			goto checkLeaf;
		}
	}

	/* verify intermediate cert */
	isCertInfo = &certInfo[1];
	tpCert = certGroup.certAtIndex(1);

	if (!isCertInfo->basicConstraints.present)
	{
		tpPolicyError("tp_verifyAppleIDSharingOpts: no basicConstraints in intermediate");
		if (tpCert->addStatusCode(CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS))
			return CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS;
	}

checkLeaf:

	/* verify leaf cert */
	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);

	/* host name check is optional */
	if (serverNameLen != 0)
	{
		if (serverName == NULL)
			return CSSMERR_TP_INVALID_POINTER;

		/* convert caller's hostname string to lower case */
		char *hostName = (char *)certGroup.alloc().malloc(serverNameLen);
		memmove(hostName, serverName, serverNameLen);
		tpToLower(hostName, serverNameLen);

		/* Check common name... */

		bool fieldFound;
		CSSM_BOOL match = tpCompareSubjectName(*tpCert, SN_CommonName, false, hostName,
									 serverNameLen, fieldFound);

		certGroup.alloc().free(hostName);
		if (!match && tpCert->addStatusCode(CSSMERR_APPLETP_HOSTNAME_MISMATCH))
            return CSSMERR_APPLETP_HOSTNAME_MISMATCH;
	}

	if (certInfo->certificatePolicies.present)
	{
		const CE_CertPolicies *certPolicies =
				&isCertInfo->certificatePolicies.extnData->certPolicies;
		if (!certificatePoliciesContainsOID(certPolicies, &CSSMOID_APPLEID_SHARING_CERT_POLICY))
			if (tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION))
				return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
	}
	else
	if (tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION))
		return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;

	if (!isCertInfo->extendKeyUsage.present)
    {
		tpPolicyError("tp_verifyAppleIDSharingOpts: no extendedKeyUse in leaf");
		if (tpCert->addStatusCode(CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE))
			return crtn ? crtn : CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE;

        /* have to skip remainder */
        return CSSM_OK;
	}

	// Check that certificate can do Client and Server Authentication (EKU)
	eku = &isCertInfo->extendKeyUsage.extnData->extendedKeyUsage;
	assert(eku != NULL);
	if(eku->numPurposes != 2)
	{
		tpPolicyError("tp_verifyAppleIDSharingOpts: bad eku->numPurposes (%lu)",
					  (unsigned long)eku->numPurposes);
		if (tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE))
		{
			if (crtn == CSSM_OK)
				crtn = CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		}
		return crtn;
	}
	bool canDoClientAuth = false, canDoServerAuth = false, ekuError = false;
	for (int ix=0;ix<2;ix++)
	{
		if (tpCompareOids(&eku->purposes[ix], &CSSMOID_ClientAuth))
			canDoClientAuth = true;
		else
		if (tpCompareOids(&eku->purposes[ix], &CSSMOID_ServerAuth))
			canDoServerAuth = true;
		else
		{
			ekuError = true;
			break;
		}
	}

	if (!(canDoClientAuth && canDoServerAuth))
		ekuError = true;
	if (ekuError)
	{
		tpPolicyError("tp_verifyAppleIDSharingOpts: bad EKU in leaf");
		if (tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE))
		{
			if (crtn == CSSM_OK)
				crtn = CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		}
	}

	return crtn;
}

/*
 * Verify Time Stamping (RFC3161) policy options.
 *
 * -- Leaf must contain Extended Key Usage (EKU), marked critical
 * -- The EKU must contain the id-kp-timeStamping purpose and no other
 */
static CSSM_RETURN tp_verifyTimeStampingOpts(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,		// currently unused
											 const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
    //unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
	CE_ExtendedKeyUsage *eku;

	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);

	if (!isCertInfo->extendKeyUsage.present)
	{
		tpPolicyError("tp_verifyTimeStampingOpts: no extendedKeyUse in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
	}

	if(!isCertInfo->extendKeyUsage.critical)
	{
		tpPolicyError("tp_verifyTimeStampingOpts: extended key usage !critical");
		tpCert->addStatusCode(CSSMERR_APPLETP_EXT_KEYUSAGE_NOT_CRITICAL);
		return CSSMERR_APPLETP_EXT_KEYUSAGE_NOT_CRITICAL;
	}

	eku = &isCertInfo->extendKeyUsage.extnData->extendedKeyUsage;
	assert(eku != NULL);

	if(eku->numPurposes != 1)
	{
		tpPolicyError("tp_verifyTimeStampingOpts: bad eku->numPurposes (%lu)",
					  (unsigned long)eku->numPurposes);
		tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE);
		return CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
	}

	if(!tpCompareOids(&eku->purposes[0], &CSSMOID_TimeStamping))
	{
		tpPolicyError("tp_verifyTimeStampingOpts: TimeStamping purpose not found");
		tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE);
		return CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
	}

	return CSSM_OK;
}

/*
 * Verify Passbook Signing policy options.
 *
 * -- Do basic cert validation (OCSP-based certs)
 * -- Chains to the Apple root CA
 * -- Has custom marker extension (1.2.840.113635.100.6.1.16)
 *		(CSSMOID_APPLE_EXTENSION_PASSBOOK_SIGNING)
 * -- EKU contains Passbook Signing purpose (1.2.840.113635.100.4.14)
 *		(CSSMOID_APPLE_EKU_PASSBOOK_SIGNING)
 * -- UID field of Subject must contain provided card signer string
 * -- OU field of Subject must contain provided team identifier string
 */
static CSSM_RETURN tp_verifyPassbookSigningOpts(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,
											 const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
	CE_ExtendedKeyUsage *eku;
	CSSM_RETURN crtn = CSSM_OK;
	unsigned int nameLen = 0;
	const char *name = NULL;
	char *p, *signerName = NULL, *teamIdentifier = NULL;
	bool found;

	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);

	/* The CSSM_APPLE_TP_SMIME_OPTIONS pointer is required. */
    if (!fieldOpts || !fieldOpts->Data)
		return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
    else {
		CSSM_APPLE_TP_SMIME_OPTIONS *opts = (CSSM_APPLE_TP_SMIME_OPTIONS *)fieldOpts->Data;
        switch (opts->Version)
        {
        case CSSM_APPLE_TP_SMIME_OPTS_VERSION:
            if (fieldOpts->Length != sizeof(CSSM_APPLE_TP_SMIME_OPTIONS))
                return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
            break;
            /* handle backwards compatibility here if necessary */
        default:
            return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
		}
		nameLen = opts->SenderEmailLen;
		name = opts->SenderEmail;
		if (!name || !nameLen)
			return CSSMERR_APPLETP_IDENTIFIER_MISSING;
	}

	/* Split the provided name into signer name and team identifier
	 * (allocates memory, which must be freed at end) */
	signerName = (char *)certGroup.alloc().malloc(nameLen);
	teamIdentifier = (char *)certGroup.alloc().malloc(nameLen);
	memmove(signerName, name, nameLen);
	teamIdentifier[0] = '\0';
	if ((p = strchr(signerName, '\t')) != NULL) {
		*p++ = '\0';
		memmove(teamIdentifier, p, strlen(p)+1);
	}

	/* Check signer name in UID field */
	if (CSSM_FALSE == tpCompareSubjectName(*tpCert,
		SN_UserID, false, signerName, (unsigned int)strlen(signerName), found)) {
		tpPolicyError("tp_verifyPassbookSigningOpts: signer name not in subject UID field");
		tpCert->addStatusCode(CSSMERR_APPLETP_IDENTIFIER_MISSING);
		crtn = CSSMERR_APPLETP_IDENTIFIER_MISSING;
		goto cleanup;
	}

	/* Check team identifier in OU field */
	if (CSSM_FALSE == tpCompareSubjectName(*tpCert,
		SN_OrgUnit, false, teamIdentifier, (unsigned int)strlen(teamIdentifier), found)) {
		tpPolicyError("tp_verifyPassbookSigningOpts: team identifier not in subject OU field");
		tpCert->addStatusCode(CSSMERR_APPLETP_IDENTIFIER_MISSING);
		crtn = CSSMERR_APPLETP_IDENTIFIER_MISSING;
		goto cleanup;
	}

	/* Check that EKU extension is present */
	if (!isCertInfo->extendKeyUsage.present) {
		tpPolicyError("tp_verifyPassbookSigningOpts: no extendedKeyUse in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

	/* Check that EKU contains Passbook Signing purpose */
	eku = &isCertInfo->extendKeyUsage.extnData->extendedKeyUsage;
	assert(eku != NULL);
	found = false;
	for (int ix=0;ix<eku->numPurposes;ix++) {
		if (tpCompareOids(&eku->purposes[ix], &CSSMOID_APPLE_EKU_PASSBOOK_SIGNING)) {
			found = true;
			break;
		}
	}
	if (!found) {
		tpPolicyError("tp_verifyPassbookSigningOpts: Passbook Signing purpose not found");
		tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE);
		crtn = CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		goto cleanup;
	}

	/* Check that Passbook Signing marker extension is present */
	if (!(isCertInfo->foundPassbookSigningMarker == CSSM_TRUE)) {
		tpPolicyError("tp_verifyPassbookSigningOpts: no Passbook Signing extension in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

	/* Check that cert chain is anchored by the Apple Root CA */
	if (numCerts < 3) {
		tpPolicyError("tp_verifyPassbookSigningOpts: numCerts %u", numCerts);
		crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		goto cleanup;
	}
	else {
	 	tpCert = certGroup.certAtIndex(numCerts-1);
		const CSSM_DATA *certData = tpCert->itemData();
		unsigned char digest[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1(certData->Data, (CC_LONG)certData->Length, digest);
		if (memcmp(digest, kAppleCASHA1, sizeof(digest))) {
			tpPolicyError("tp_verifyPassbookSigningOpts: invalid anchor for policy");
			tpCert->addStatusCode(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH);
			crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
			goto cleanup;
		}
	}

cleanup:
	if (signerName)
		certGroup.alloc().free(signerName);
	if (teamIdentifier)
		certGroup.alloc().free(teamIdentifier);

	return crtn;
}

/*
 * Verify Mobile Store policy options.
 *
 * -- Do basic cert validation.
 * -- Chain length must be exactly 3.
 * -- Must chain to known Mobile Store root.
 * -- Intermediate must have CSSMOID_APPLE_EXTENSION_SYSINT2_INTERMEDIATE marker
 *		(1.2.840.113635.100.6.2.10)
 * -- Key usage in leaf certificate must be Digital Signature.
 * -- Leaf has certificatePolicies extension with appropriate policy:
 *		(1.2.840.113635.100.5.12) if testPolicy is false
 *		(1.2.840.113635.100.5.12.1) if testPolicy is true
 */
static CSSM_RETURN tp_verifyMobileStoreSigningOpts(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,
											 const iSignCertInfo *certInfo,		// all certs, size certGroup.numCerts()
											 bool testPolicy)
{
	unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
	CE_KeyUsage ku;
	CSSM_RETURN crtn = CSSM_OK;

	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);

	/* Check that KU extension is present */
	if (!isCertInfo->keyUsage.present) {
		tpPolicyError("tp_verifyMobileStoreSigningOpts: no keyUsage in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

	/* Check that KU contains Digital Signature usage */
	ku = isCertInfo->keyUsage.extnData->keyUsage;
	if (!(ku & CE_KU_DigitalSignature)) {
		tpPolicyError("tp_verifyMobileStoreSigningOpts: DigitalSignature usage not found");
		tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE);
		crtn = CSSMERR_APPLETP_INVALID_KEY_USAGE;
		goto cleanup;
	}

	/* Check that Mobile Store Signing certicate policy is present in leaf */
	if (isCertInfo->certificatePolicies.present)
	{
		const CE_CertPolicies *certPolicies =
				&isCertInfo->certificatePolicies.extnData->certPolicies;
		const CSSM_OID *policyOID = (testPolicy) ?
				&CSSMOID_TEST_MOBILE_STORE_SIGNING_POLICY :
				&CSSMOID_MOBILE_STORE_SIGNING_POLICY;
		if (!certificatePoliciesContainsOID(certPolicies, policyOID))
			if (tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION))
				return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
	}
	else
	{
        tpPolicyError("tp_verifyMobileStoreSigningOpts: no certificatePolicies present in leaf");
        if (tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION))
			return CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
	}

	/* Check that cert chain length is 3 */
	if (numCerts != 3) {
		tpPolicyError("tp_verifyMobileStoreSigningOpts: numCerts %u", numCerts);
		crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		goto cleanup;
	}

	/* Check that cert chain is anchored by a known root */
	{
		tpCert = certGroup.certAtIndex(numCerts-1);
		const CSSM_DATA *certData = tpCert->itemData();
		unsigned char digest[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1(certData->Data, (CC_LONG)certData->Length, digest);
		if (memcmp(digest, kMobileRootSHA1, sizeof(digest))) {
			tpPolicyError("tp_verifyMobileStoreSigningOpts: invalid anchor for policy");
			tpCert->addStatusCode(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH);
			crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
			goto cleanup;
		}
	}

	/* Check that Apple System Integration 2 marker extension is present in intermediate */
	isCertInfo = &certInfo[1];
	tpCert = certGroup.certAtIndex(1);
	if (!(isCertInfo->foundAppleSysInt2Marker == CSSM_TRUE)) {
		tpPolicyError("tp_verifyMobileStoreSigningOpts: intermediate marker extension not found");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

cleanup:
	return crtn;
}

/*
 * Verify Escrow Service policy options.
 *
 * -- Chain length must be exactly 2.
 * -- Must be issued by known escrow root.
 * -- Key usage in leaf certificate must be Key Encipherment.
 * -- Leaf has CSSMOID_APPLE_EXTENSION_ESCROW_SERVICE_MARKER extension
 *		(1.2.840.113635.100.6.23.1)
 */
static CSSM_RETURN tp_verifyEscrowServiceCommon(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,
											 const iSignCertInfo *certInfo,		// all certs, size certGroup.numCerts()
											 SecCertificateEscrowRootType rootType)
{
	unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
	CE_KeyUsage ku;
	CSSM_RETURN crtn = CSSM_OK;

	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);

	/* Check that KU extension is present */
	if (!isCertInfo->keyUsage.present) {
		tpPolicyError("tp_verifyEscrowServiceCommon: no keyUsage in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

	/* Check that KU contains Key Encipherment usage */
	ku = isCertInfo->keyUsage.extnData->keyUsage;
	if (!(ku & CE_KU_KeyEncipherment)) {
		tpPolicyError("tp_verifyEscrowServiceCommon: KeyEncipherment usage not found");
		tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE);
		crtn = CSSMERR_APPLETP_INVALID_KEY_USAGE;
		goto cleanup;
	}

	/* Check that Escrow Service marker extension is present */
	if (!(isCertInfo->foundEscrowServiceMarker == CSSM_TRUE)) {
		tpPolicyError("tp_verifyEscrowServiceCommon: no Escrow Service extension in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

	/* Check that cert chain length is 2 */
	if (numCerts != 2) {
		tpPolicyError("tp_verifyEscrowServiceCommon: numCerts %u", numCerts);
		crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		goto cleanup;
	}

	/* Check that cert chain is anchored by a known root */
	{
		tpCert = certGroup.certAtIndex(numCerts-1);
		const CSSM_DATA *certData = tpCert->itemData();
		bool anchorMatch = false;
		SecCertificateRef anchor = NULL;
		OSStatus status = SecCertificateCreateFromData(certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &anchor);
		if (!status) {
			CFArrayRef anchors = SecCertificateCopyEscrowRoots(rootType);
			CFIndex idx, count = (anchors) ? CFArrayGetCount(anchors) : 0;
			for (idx = 0; idx < count; idx++) {
				SecCertificateRef cert = (SecCertificateRef) CFArrayGetValueAtIndex(anchors, idx);
				if (cert && CFEqual(cert, anchor)) {
					anchorMatch = true;
					break;
				}
			}
			if (anchors)
				CFRelease(anchors);
		}
		if (anchor)
			CFRelease(anchor);

		if (!anchorMatch) {
			tpPolicyError("tp_verifyEscrowServiceCommon: invalid anchor for policy");
			tpCert->addStatusCode(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH);
			crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
			goto cleanup;
		}
	}

cleanup:
	return crtn;
}

static CSSM_RETURN tp_verifyEscrowServiceSigningOpts(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,
											 const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	return tp_verifyEscrowServiceCommon(certGroup, fieldOpts, certInfo, kSecCertificateProductionEscrowRoot);
}

static CSSM_RETURN tp_verifyPCSEscrowServiceSigningOpts(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,
											 const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	return tp_verifyEscrowServiceCommon(certGroup, fieldOpts, certInfo, kSecCertificateProductionPCSEscrowRoot);
}

/*
 * Verify Provisioning Profile Signing policy options.
 *
 * -- Do basic cert validation (OCSP-based certs)
 * -- Chains to the Apple root CA
 * -- Leaf has Provisioning Profile marker OID (1.2.840.113635.100.4.11)
 * -- Intermediate has WWDR marker OID (1.2.840.113635.100.6.2.1)
 */
static CSSM_RETURN tp_verifyProvisioningProfileSigningOpts(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,
											 const iSignCertInfo *certInfo)		// all certs, size certGroup.numCerts()
{
	unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
	CSSM_RETURN crtn = CSSM_OK;

	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);

	/* Check that cert chain is anchored by the Apple Root CA */
	if (numCerts < 3) {
		tpPolicyError("tp_verifyProvisioningProfileSigningOpts: numCerts %u", numCerts);
		crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		goto cleanup;
	}
	else {
		tpCert = certGroup.certAtIndex(numCerts-1);
		const CSSM_DATA *certData = tpCert->itemData();
		unsigned char digest[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1(certData->Data, (CC_LONG)certData->Length, digest);
		if (memcmp(digest, kAppleCASHA1, sizeof(digest))) {
			tpPolicyError("tp_verifyProvisioningProfileSigningOpts: invalid anchor for policy");
			tpCert->addStatusCode(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH);
			crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
			goto cleanup;
		}
	}

	/* Check that Provisioning Profile Signing marker extension is present */
	if (!(isCertInfo->foundProvisioningProfileSigningMarker == CSSM_TRUE)) {
		tpPolicyError("tp_verifyProvisioningProfileSigningOpts: no Provisioning Profile Signing extension in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

	/* Check that Apple WWDR marker extension is present in intermediate */
	isCertInfo = &certInfo[1];
	tpCert = certGroup.certAtIndex(1);
	if (!(isCertInfo->foundAppleWWDRIntMarker == CSSM_TRUE)) {
		tpPolicyError("tp_verifyProvisioningProfileSigningOpts: intermediate marker extension not found");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

cleanup:
	return crtn;

}

/*
 * Verify Configuration Profile Signing policy options.
 *
 * -- Do basic cert validation (OCSP-based certs)
 * -- Chains to the Apple root CA
 * -- Leaf has EKU extension with appropriate purpose:
 *		(1.2.840.113635.100.4.16) if testPolicy is false
 *		(1.2.840.113635.100.4.17) if testPolicy is true
 */
static CSSM_RETURN tp_verifyProfileSigningOpts(TPCertGroup &certGroup,
											 const CSSM_DATA *fieldOpts,
											 const iSignCertInfo *certInfo,		// all certs, size certGroup.numCerts()
											 bool testPolicy)
{
	unsigned numCerts = certGroup.numCerts();
	const iSignCertInfo *isCertInfo;
	TPCertInfo *tpCert;
	CE_ExtendedKeyUsage *eku;
	CSSM_RETURN crtn = CSSM_OK;
	bool found;

	isCertInfo = &certInfo[0];
	tpCert = certGroup.certAtIndex(0);

	/* Check that EKU extension is present */
	if (!isCertInfo->extendKeyUsage.present) {
		tpPolicyError("tp_verifyProfileSigningOpts: no extendedKeyUse in leaf");
		tpCert->addStatusCode(CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION);
		crtn = CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION;
		goto cleanup;
	}

	/* Check that EKU contains appropriate Profile Signing purpose */
	eku = &isCertInfo->extendKeyUsage.extnData->extendedKeyUsage;
	assert(eku != NULL);
	found = false;
	for (int ix=0;ix<eku->numPurposes;ix++) {
		if (tpCompareOids(&eku->purposes[ix], (testPolicy) ?
			&CSSMOID_APPLE_EKU_QA_PROFILE_SIGNING :
			&CSSMOID_APPLE_EKU_PROFILE_SIGNING)) {
			found = true;
			break;
		}
	}
	if (!found) {
		tpPolicyError("tp_verifyProfileSigningOpts: Profile Signing purpose not found");
		tpCert->addStatusCode(CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE);
		crtn = CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE;
		goto cleanup;
	}

	/* Check that cert chain is anchored by the Apple Root CA */
	if (numCerts < 3) {
		tpPolicyError("tp_verifyProfileSigningOpts: numCerts %u", numCerts);
		crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
		goto cleanup;
	}
	else {
		tpCert = certGroup.certAtIndex(numCerts-1);
		const CSSM_DATA *certData = tpCert->itemData();
		unsigned char digest[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1(certData->Data, (CC_LONG)certData->Length, digest);
		if (memcmp(digest, kAppleCASHA1, sizeof(digest))) {
			tpPolicyError("tp_verifyProfileSigningOpts: invalid anchor for policy");
			tpCert->addStatusCode(CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH);
			crtn = CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH;
			goto cleanup;
		}
	}

cleanup:
	return crtn;
}

/*
 * RFC2459 says basicConstraints must be flagged critical for
 * CA certs, but Verisign doesn't work that way.
 */
#define BASIC_CONSTRAINTS_MUST_BE_CRITICAL		0

/*
 * TP iSign spec says Extended Key Usage required for leaf certs,
 * but Verisign doesn't work that way.
 */
#define EXTENDED_KEY_USAGE_REQUIRED_FOR_LEAF	0

/*
 * TP iSign spec says Subject Alternate Name required for leaf certs,
 * but Verisign doesn't work that way.
 */
#define SUBJECT_ALT_NAME_REQUIRED_FOR_LEAF		0

/*
 * TP iSign spec originally required KeyUsage for all certs, but
 * Verisign doesn't have that in their roots.
 */
#define KEY_USAGE_REQUIRED_FOR_ROOT				0

/*
 * RFC 2632, "S/MIME Version 3 Certificate Handling", section
 * 4.4.2, says that KeyUsage extensions MUST be flagged critical,
 * but Thawte's intermediate cert (common name "Thawte Personal
 * Freemail Issuing CA") does not meet this requirement.
 */
#define SMIME_KEY_USAGE_MUST_BE_CRITICAL		0

/*
 * Public routine to perform TP verification on a constructed
 * cert group.
 * Returns CSSM_OK on success.
 * Assumes the chain has passed basic subject/issuer verification. First cert of
 * incoming certGroup is end-entity (leaf).
 *
 * Per-policy details:
 *   iSign: Assumes that last cert in incoming certGroup is a root cert.
 *			Also assumes a cert group of more than one cert.
 *   kTPx509Basic: CertGroup of length one allowed.
 */
CSSM_RETURN tp_policyVerify(
	TPPolicy						policy,
	Allocator						&alloc,
	CSSM_CL_HANDLE					clHand,
	CSSM_CSP_HANDLE					cspHand,
	TPCertGroup 					*certGroup,
	CSSM_BOOL						verifiedToRoot,		// last cert is good root
	CSSM_BOOL						verifiedViaTrustSetting,	// last cert verified via
															//     user trust
	CSSM_APPLE_TP_ACTION_FLAGS		actionFlags,
	const CSSM_DATA					*policyFieldData,	// optional
	void							*policyOpts)		// future options
{
	iSignCertInfo 			*certInfo = NULL;
	uint32					numCerts;
	iSignCertInfo			*thisCertInfo;
	uint16					expUsage;
	uint16					actUsage;
	unsigned				certDex;
	CSSM_BOOL				cA = CSSM_FALSE;		// init for compiler warning
	bool					isLeaf;					// end entity
	bool					isRoot;					// root cert
	CE_ExtendedKeyUsage		*extendUsage;
	CE_AuthorityKeyID		*authorityId;
	CSSM_KEY_PTR			pubKey;
	CSSM_RETURN				outErr = CSSM_OK;		// for gross, non-policy errors
	CSSM_BOOL				policyFail = CSSM_FALSE;// generic CSSMERR_TP_VERIFY_ACTION_FAILED
	CSSM_RETURN				policyError = CSSM_OK;	// policy-specific failure

	/* First, kTPDefault is a nop here */
	if(policy == kTPDefault) {
		return CSSM_OK;
	}

	if(certGroup == NULL) {
		return CSSMERR_TP_INVALID_CERTGROUP;
	}
	numCerts = certGroup->numCerts();
	if(numCerts == 0) {
		return CSSMERR_TP_INVALID_CERTGROUP;
	}
	if(policy == kTPiSign) {
		if(!verifiedToRoot) {
			/* no way, this requires a root cert */
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
		if(numCerts <= 1) {
			/* nope, not for iSign */
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
	}

	/* cook up an iSignCertInfo array */
	certInfo = (iSignCertInfo *)tpCalloc(alloc, numCerts, sizeof(iSignCertInfo));
	/* subsequent errors to errOut: */

	/* fill it with interesting info from parsed certs */
	for(certDex=0; certDex<numCerts; certDex++) {
		if(iSignGetCertInfo(alloc,
				certGroup->certAtIndex(certDex),
				&certInfo[certDex])) {
			(certGroup->certAtIndex(certDex))->addStatusCode(
				CSSMERR_TP_INVALID_CERTIFICATE);
			/* this one is fatal (and can't ignore) */
			outErr = CSSMERR_TP_INVALID_CERTIFICATE;
			goto errOut;
		}
	}

	/*
	 * OK, the heart of TP enforcement.
	 */
	for(certDex=0; certDex<numCerts; certDex++) {
		thisCertInfo = &certInfo[certDex];
		TPCertInfo *thisTpCertInfo = certGroup->certAtIndex(certDex);

		/*
		 * First check for presence of required extensions and
		 * critical extensions we don't understand.
		 */
		if(thisCertInfo->foundUnknownCritical) {
			/* illegal for all policies */
			tpPolicyError("tp_policyVerify: critical flag in unknown extension");
			if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN)) {
				policyFail = CSSM_TRUE;
			}
		}

		/*
		 * Check for unsupported key length, per <rdar://6892837>
		 */
		if((pubKey=thisTpCertInfo->pubKey()) != NULL) {
			CSSM_KEYHEADER *keyHdr = &pubKey->KeyHeader;
			if(keyHdr->AlgorithmId == CSSM_ALGID_RSA && keyHdr->LogicalKeySizeInBits < 1024) {
				tpPolicyError("tp_policyVerify: RSA key size too small");
				if(thisTpCertInfo->addStatusCode(CSSMERR_CSP_UNSUPPORTED_KEY_SIZE)) {
					policyFail = CSSM_TRUE;
				}
			}
		}

		/*
		 * Note it's possible for both of these to be true, for a chain
		 * of length one (kTPx509Basic, kCrlPolicy only!)
		 * FIXME: should this code work if the last cert in the chain is NOT a root?
		 */
		isLeaf = thisTpCertInfo->isLeaf();
		isRoot = thisTpCertInfo->isSelfSigned(true);

		/*
		 * BasicConstraints.cA
		 * iSign:   	 required in all but leaf and root,
		 *          	 for which it is optional (with default values of false
		 *         	 	 for leaf and true for root).
		 * all others:   always optional, default of false for leaf and
		 *				 true for others
		 * All:     	 cA must be false for leaf, true for others
		 */
		if(!thisCertInfo->basicConstraints.present) {
			/*
			 * No basicConstraints present; infer a cA value if appropriate.
			 */
			if(isLeaf) {
				/* cool, use default; note that kTPx509Basic with
				 * certGroup length of one may take this case */
				cA = CSSM_FALSE;
			}
			else if(isRoot) {
				/* cool, use default */
				cA = CSSM_TRUE;
			}
			else {
				switch(policy) {
					default:
						/*
						 * not present, not leaf, not root....
						 * ....RFC2459 says this can not be a CA
						 */
						cA = CSSM_FALSE;
						break;
					case kTPiSign:
						/* required for iSign in this position */
						tpPolicyError("tp_policyVerify: no "
								"basicConstraints");
						if(thisTpCertInfo->addStatusCode(
								CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS)) {
							policyFail = CSSM_TRUE;
						}
						break;
				}
			}
		}	/* inferred a default value */
		else {
			/* basicConstraints present */
			#if		BASIC_CONSTRAINTS_MUST_BE_CRITICAL
			/* disabled for verisign compatibility */
			if(!thisCertInfo->basicConstraints.critical) {
				/* per RFC 2459 */
				tpPolicyError("tp_policyVerify: basicConstraints marked "
					"not critical");
				if(thisTpCertInfo->addStatusCode(CSSMERR_TP_VERIFY_ACTION_FAILED)) {
					policyFail = CSSM_TRUE;
				}
			}
			#endif	/* BASIC_CONSTRAINTS_MUST_BE_CRITICAL */

			const CE_BasicConstraints *bcp =
				&thisCertInfo->basicConstraints.extnData->basicConstraints;

			cA = bcp->cA;

			/* Verify pathLenConstraint if present */
			if(!isLeaf &&							// leaf, certDex=0, don't care
			   cA && 								// p.l.c. only valid for CAs
			   bcp->pathLenConstraintPresent) {		// present?
				/*
				 * pathLenConstraint=0 legal for certDex 1 only
				 * pathLenConstraint=1 legal for certDex {1,2}
				 * etc.
				 */
				if(certDex > (bcp->pathLenConstraint + 1)) {
					tpPolicyError("tp_policyVerify: pathLenConstraint "
						"exceeded");
					if(thisTpCertInfo->addStatusCode(
							CSSMERR_APPLETP_PATH_LEN_CONSTRAINT)) {
						policyFail = CSSM_TRUE;
					}
				}
			}
		}

		if(isLeaf) {
			/*
			 * Special cases to allow a chain of length 1, leaf and root
			 * both true, and for caller to override the "leaf can't be a CA"
			 * requirement when a CA cert is explicitly being evaluated as the
			 * leaf.
			 */
			if(cA && !isRoot &&
			   !(actionFlags & CSSM_TP_ACTION_LEAF_IS_CA)) {
				tpPolicyError("tp_policyVerify: cA true for leaf");
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_CA)) {
					policyFail = CSSM_TRUE;
				}
			}
		} else if(!cA) {
			tpPolicyError("tp_policyVerify: cA false for non-leaf");
			if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_CA)) {
				policyFail = CSSM_TRUE;
			}
		}

		/*
		 * Authority Key Identifier optional
		 * iSign   		: only allowed in !root.
		 *           	  If present, must not be critical.
		 * all others   : ignored (though used later for chain verification)
		 */
		if((policy == kTPiSign) && thisCertInfo->authorityId.present) {
			if(isRoot) {
				tpPolicyError("tp_policyVerify: authorityId in root");
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_AUTHORITY_ID)) {
					policyFail = CSSM_TRUE;
				}
			}
			if(thisCertInfo->authorityId.critical) {
				/* illegal per RFC 2459 */
				tpPolicyError("tp_policyVerify: authorityId marked "
					"critical");
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_AUTHORITY_ID)) {
					policyFail = CSSM_TRUE;
				}
			}
		}

		/*
		 * Subject Key Identifier optional
		 * iSign   		 : can't be critical.
		 * all others    : ignored (though used later for chain verification)
		 */
		if(thisCertInfo->subjectId.present) {
			if((policy == kTPiSign) && thisCertInfo->subjectId.critical) {
				tpPolicyError("tp_policyVerify: subjectId marked critical");
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_SUBJECT_ID)) {
					policyFail = CSSM_TRUE;
				}
			}
		}

		/*
		 * Key Usage optional except required as noted
		 * iSign    	: required for non-root/non-leaf
		 *            	  Leaf cert : if present, usage = digitalSignature
		 *				  Exception : if leaf, and keyUsage not present,
		 *					          netscape-cert-type must be present, with
		 *							  Object Signing bit set
		 * kCrlPolicy   : Leaf: usage = CRLSign
		 * kTP_SMIME   	: if present, must be critical
		 * kTP_SWUpdateSign, kTP_ResourceSign, kTP_CodeSigning, kTP_PackageSigning : Leaf :
						  usage = digitalSignature
		 * all others   : non-leaf  : usage = keyCertSign
		 *			  	  Leaf : don't care
		 */
		if(thisCertInfo->keyUsage.present) {
			/*
			 * Leaf cert:
			 *    iSign and *Signing: usage = digitalSignature
			 *    all others : don't care
			 * Others:    usage = keyCertSign
			 * We only require that one bit to be set, we ignore others.
			 */
			if(isLeaf) {
				switch(policy) {
					case kTPiSign:
					case kTP_SWUpdateSign:
					case kTP_ResourceSign:
					case kTP_CodeSigning:
					case kTP_PackageSigning:
						expUsage = CE_KU_DigitalSignature;
						break;
					case kCrlPolicy:
						/* if present, this bit must be set */
						expUsage = CE_KU_CRLSign;
						break;
					default:
						/* accept whatever's there */
						expUsage = thisCertInfo->keyUsage.extnData->keyUsage;
						break;
				}
			}
			else {
				/* !leaf: this is true for all policies */
				expUsage = CE_KU_KeyCertSign;
			}
			actUsage = thisCertInfo->keyUsage.extnData->keyUsage;
			if(!(actUsage & expUsage)) {
				tpPolicyError("tp_policyVerify: bad keyUsage (leaf %s; "
					"usage 0x%x)",
					(certDex == 0) ? "TRUE" : "FALSE", actUsage);
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE)) {
					policyFail = CSSM_TRUE;
				}
			}

			#if 0
			/*
			 * Radar 3523221 renders this whole check obsolete, but I'm leaving
			 * the code here to document its conspicuous functional absence.
			 */
			if((policy == kTP_SMIME) && !thisCertInfo->keyUsage.critical) {
				/*
				 * Per Radar 3410245, allow this for intermediate certs.
				 */
				if(SMIME_KEY_USAGE_MUST_BE_CRITICAL || isLeaf || isRoot) {
					tpPolicyError("tp_policyVerify: key usage, !critical, SMIME");
					if(thisTpCertInfo->addStatusCode(
							CSSMERR_APPLETP_SMIME_KEYUSAGE_NOT_CRITICAL)) {
						policyFail = CSSM_TRUE;
					}
				}
			}
			#endif
		}
		else if(policy == kTPiSign) {
			/*
			 * iSign requires keyUsage present for non root OR
			 * netscape-cert-type/ObjectSigning for leaf
			 */
			if(isLeaf && thisCertInfo->netscapeCertType.present) {
				CE_NetscapeCertType ct =
					thisCertInfo->netscapeCertType.extnData->netscapeCertType;

				if(!(ct & CE_NCT_ObjSign)) {
					tpPolicyError("tp_policyVerify: netscape-cert-type, "
						"!ObjectSign");
					if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE)) {
						policyFail = CSSM_TRUE;
					}
				}
			}
			else if(!isRoot) {
				tpPolicyError("tp_policyVerify: !isRoot, no keyUsage, "
					"!(leaf and netscapeCertType)");
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE)) {
					policyFail = CSSM_TRUE;
				}
			}
		}

		/*
		 * RFC 3280, 4.1.2.6, says that an empty subject name can only appear in a
		 * leaf cert, and only if subjectAltName is present and marked critical.
		 */
		if(isLeaf && thisTpCertInfo->hasEmptySubjectName()) {
			bool badEmptySubject = false;
			if(actionFlags & CSSM_TP_ACTION_LEAF_IS_CA) {
				/*
				 * True when evaluating a CA cert as well as when
				 * evaluating a CRL's cert chain. Note the odd case of a CRL's
				 * signer having an empty subject matching an empty issuer
				 * in the CRL. That'll be caught here.
				 */
				badEmptySubject = true;
			}
			else if(!thisCertInfo->subjectAltName.present ||	/* no subjectAltName */
					!thisCertInfo->subjectAltName.critical) {	/* not critical */
				badEmptySubject = true;
			}
			if(badEmptySubject) {
				tpPolicyError("tp_policyVerify: bad empty subject");
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_EMPTY_SUBJECT)) {
					policyFail = CSSM_TRUE;
				}
			}
		}

		/*
 		 * RFC 3739: if this cert has a Qualified Cert Statements extension, and
		 * it's Critical, make sure we understand all of the extension's statementIds.
		 */
		if(thisCertInfo->qualCertStatements.present &&
		   thisCertInfo->qualCertStatements.critical) {
			CE_QC_Statements *qcss =
				&thisCertInfo->qualCertStatements.extnData->qualifiedCertStatements;
			uint32 numQcs = qcss->numQCStatements;
			for(unsigned qdex=0; qdex<numQcs; qdex++) {
				CSSM_OID_PTR qid = &qcss->qcStatements[qdex].statementId;
				bool ok = false;
				for(unsigned kdex=0; kdex<NUM_KNOWN_QUAL_CERT_STATEMENTS; kdex++) {
					if(tpCompareCssmData(qid, knownQualifiedCertStatements[kdex])) {
						ok = true;
						break;
					}
				}
				if(!ok) {
					if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_UNKNOWN_QUAL_CERT_STATEMENT)) {
						policyFail = CSSM_TRUE;
						break;
					}
				}
			}
		}	/* critical Qualified Cert Statement */

		/*
		 * Certificate Policies extension validation, per section 1.2 of:
		 * http://iase.disa.mil/pki/dod_cp_v10_final_2_mar_09_signed.pdf
		 */
		if (tpVerifyCPE(*thisCertInfo, CSSMOID_PIV_AUTH, false) ||
			tpVerifyCPE(*thisCertInfo, CSSMOID_PIV_AUTH_2048, false)) {
			/*
			 * Certificate asserts one of the PIV-Auth Certificate Policy OIDs;
			 * check the required Key Usage extension for compliance.
			 *
			 * Leaf cert:
			 *    usage = digitalSignature (only; no other bits asserted)
			 * Others:
			 *    usage = keyCertSign (required; other bits ignored)
			 */
			if(thisCertInfo->keyUsage.present) {
				actUsage = thisCertInfo->keyUsage.extnData->keyUsage;
			} else {
				/* No key usage! Policy fail. */
				actUsage = 0;
			}
			if(!(actionFlags & CSSM_TP_ACTION_LEAF_IS_CA) && (certDex == 0)) {
				expUsage = CE_KU_DigitalSignature;
			} else {
				expUsage = actUsage | CE_KU_KeyCertSign;
			}
			if(!(actUsage == expUsage)) {
				tpPolicyError("tp_policyVerify: bad keyUsage for PIV-Auth policy (leaf %s; "
					"usage 0x%x)",
					(certDex == 0) ? "TRUE" : "FALSE", actUsage);
				if(thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE)) {
					policyFail = CSSM_TRUE;
				}
			}
		}	/* Certificate Policies */


	}	/* for certDex, checking presence of extensions */

	/*
	 * Special case checking for leaf (end entity) cert
	 *
	 * iSign only: Extended key usage, optional for leaf,
	 * value CSSMOID_ExtendedUseCodeSigning
	 */
	if((policy == kTPiSign) && certInfo[0].extendKeyUsage.present) {
		extendUsage = &certInfo[0].extendKeyUsage.extnData->extendedKeyUsage;
		if(extendUsage->numPurposes != 1) {
			tpPolicyError("tp_policyVerify: bad extendUsage->numPurposes "
				"(%d)",
				(int)extendUsage->numPurposes);
			if((certGroup->certAtIndex(0))->addStatusCode(
					CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
				policyFail = CSSM_TRUE;
			}
		}
		if(!tpCompareOids(extendUsage->purposes,
				&CSSMOID_ExtendedUseCodeSigning)) {
			tpPolicyError("tp_policyVerify: bad extendKeyUsage");
			if((certGroup->certAtIndex(0))->addStatusCode(
					CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE)) {
				policyFail = CSSM_TRUE;
			}
		}
	}

	/*
	 * Verify authorityId-->subjectId linkage.
	 * All optional - skip if needed fields not present.
	 * Also, always skip last (root) cert.
	 */
	for(certDex=0; certDex<(numCerts-1); certDex++) {
		if(!certInfo[certDex].authorityId.present ||
		   !certInfo[certDex+1].subjectId.present) {
		 	continue;
		}
		authorityId = &certInfo[certDex].authorityId.extnData->authorityKeyID;
		if(!authorityId->keyIdentifierPresent) {
			/* we only know how to compare keyIdentifier */
			continue;
		}
		if(!tpCompareCssmData(&authorityId->keyIdentifier,
				&certInfo[certDex+1].subjectId.extnData->subjectKeyID)) {
			tpPolicyError("tp_policyVerify: bad key ID linkage");
			if((certGroup->certAtIndex(certDex))->addStatusCode(
					CSSMERR_APPLETP_INVALID_ID_LINKAGE)) {
				policyFail = CSSM_TRUE;
			}
		}
	}

	/*
	 * Check signature algorithm on all non-root certs,
	 * reject if known to be untrusted
	 */
	for(certDex=0; certDex<(numCerts-1); certDex++) {
		if(certInfo[certDex].untrustedSigAlg) {
			tpPolicyError("tp_policyVerify: untrusted signature algorithm");
			if((certGroup->certAtIndex(certDex))->addStatusCode(
					CSSMERR_TP_INVALID_CERTIFICATE)) {
				policyFail = CSSM_TRUE;
			}
		}
	}

	/* specific per-policy checking */
	switch(policy) {
		case kTP_SSL:
		case kTP_EAP:
		case kTP_IPSec:
			/*
			 * SSL, EAP, IPSec: optionally verify common name; all are identical
			 * other than their names.
			 * FIXME - should this be before or after the root cert test? How can
			 * we return both errors?
			 */
			policyError = tp_verifySslOpts(policy, *certGroup, policyFieldData, certInfo);
			break;

		case kTP_iChat:
			tpDebug("iChat policy");
			/* fall thru */
		case kTP_SMIME:
			policyError = tp_verifySmimeOpts(policy, *certGroup, policyFieldData, certInfo);
			break;
		case kTP_SWUpdateSign:
			policyError = tp_verifySWUpdateSigningOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_ResourceSign:
			policyError = tp_verifyResourceSigningOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_CodeSigning:
		case kTP_PackageSigning:
			policyError = tp_verifyCodePkgSignOpts(policy, *certGroup, policyFieldData, certInfo);
			break;
		case kTP_MacAppStoreRec:
			policyError = tp_verifyMacAppStoreReceiptOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_AppleIDSharing:
			policyError = tp_verifyAppleIDSharingOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_TimeStamping:
			policyError = tp_verifyTimeStampingOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_PassbookSigning:
			policyError = tp_verifyPassbookSigningOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_MobileStore:
			policyError = tp_verifyMobileStoreSigningOpts(*certGroup, policyFieldData, certInfo, false);
			break;
		case kTP_TestMobileStore:
			policyError = tp_verifyMobileStoreSigningOpts(*certGroup, policyFieldData, certInfo, true);
			break;
		case kTP_EscrowService:
			policyError = tp_verifyEscrowServiceSigningOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_ProfileSigning:
			policyError = tp_verifyProfileSigningOpts(*certGroup, policyFieldData, certInfo, false);
			break;
		case kTP_QAProfileSigning:
			policyError = tp_verifyProfileSigningOpts(*certGroup, policyFieldData, certInfo, true);
			break;
		case kTP_PCSEscrowService:
			policyError = tp_verifyPCSEscrowServiceSigningOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTP_ProvisioningProfileSigning:
			policyError = tp_verifyProvisioningProfileSigningOpts(*certGroup, policyFieldData, certInfo);
			break;
		case kTPx509Basic:
		case kTPiSign:
		case kCrlPolicy:
		case kTP_PKINIT_Client:
		default:
			break;

	}

	if(outErr == CSSM_OK) {
		/* policy-specific error takes precedence here */
		if(policyError != CSSM_OK) {
			outErr = policyError;
		}
		else if(policyFail) {
			/* plain vanilla error return from this module */
			outErr = CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
	}
errOut:
	/* free resources */
	for(certDex=0; certDex<numCerts; certDex++) {
		thisCertInfo = &certInfo[certDex];
		iSignFreeCertInfo(clHand, thisCertInfo);
	}
	tpFree(alloc, certInfo);
	return outErr;
}

/*
 * Obtain policy-specific User Trust parameters
 */
void tp_policyTrustSettingParams(
	TPPolicy				policy,
	const CSSM_DATA			*policyData,		// optional
	/* returned values - not mallocd */
	const char				**policyStr,
	uint32					*policyStrLen,
	SecTrustSettingsKeyUsage	*keyUse)
{
	/* default values */
	*policyStr = NULL;
	*keyUse = kSecTrustSettingsKeyUseAny;

	if((policyData == NULL) || (policyData->Data == NULL)) {
		/* currently, no further action possible */
		return;
	}
	switch(policy) {
		case kTP_SSL:
		case kTP_EAP:
		case kTP_IPSec:
		{
			if(policyData->Length != sizeof(CSSM_APPLE_TP_SSL_OPTIONS)) {
				/* this error will be caught later */
				return;
			}
			CSSM_APPLE_TP_SSL_OPTIONS *sslOpts =
				(CSSM_APPLE_TP_SSL_OPTIONS *)policyData->Data;
			*policyStr = sslOpts->ServerName;
			*policyStrLen = sslOpts->ServerNameLen;
			if(sslOpts->Flags & CSSM_APPLE_TP_SSL_CLIENT) {
				/*
				 * Client signs with its priv key. Server end,
				 * which (also) verifies the client cert, verifies.
				 */
				*keyUse = kSecTrustSettingsKeyUseSignature;
			}
			else {
				/* server decrypts */
				*keyUse = kSecTrustSettingsKeyUseEnDecryptKey;
			}
			return;
		}

		case kTP_iChat:
		case kTP_SMIME:
		{
			if(policyData->Length != sizeof(CSSM_APPLE_TP_SMIME_OPTIONS)) {
				/* this error will be caught later */
				return;
			}
			CSSM_APPLE_TP_SMIME_OPTIONS *smimeOpts =
				(CSSM_APPLE_TP_SMIME_OPTIONS *)policyData->Data;
			*policyStr = smimeOpts->SenderEmail;
			*policyStrLen = smimeOpts->SenderEmailLen;
			SecTrustSettingsKeyUsage ku = 0;
			CE_KeyUsage smimeKu = smimeOpts->IntendedUsage;
			if(smimeKu & (CE_KU_DigitalSignature | CE_KU_KeyCertSign | CE_KU_CRLSign)) {
				ku |= kSecTrustSettingsKeyUseSignature;
			}
			if(smimeKu & (CE_KU_KeyEncipherment | CE_KU_DataEncipherment)) {
				ku |= kSecTrustSettingsKeyUseEnDecryptKey;
			}
			*keyUse = ku;
			return;
		}

		default:
			/* no other options */
			return;
	}
}

#pragma clang diagnostic pop
