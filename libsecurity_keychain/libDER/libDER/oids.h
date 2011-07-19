/* Copyright (c) 2005-2009 Apple Inc. All Rights Reserved. */

/*
 * oids.h - declaration of OID consts 
 *
 * Created Nov. 11 2005 by dmitch
 */

#ifndef	_LIB_DER_OIDS_H_
#define _LIB_DER_OIDS_H_

#include <libDER/libDER.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Algorithm oids. */
extern const DERItem
    oidRsa,         /* PKCS1 RSA encryption, used to identify RSA keys */
    oidMd2Rsa,      /* PKCS1 md2withRSAEncryption signature alg */
    oidMd5Rsa,      /* PKCS1 md5withRSAEncryption signature alg */
    oidSha1Rsa,     /* PKCS1 sha1withRSAEncryption signature alg */
    oidSha1,        /* OID_OIW_ALGORITHM 26 */
    oidSha256Rsa;    /* PKCS1 sha256WithRSAEncryption signature alg */
    
/* Standard X.509 Cert and CRL extensions. */
extern const DERItem
    oidSubjectKeyIdentifier,
    oidKeyUsage,
    oidPrivateKeyUsagePeriod,
    oidSubjectAltName,
    oidIssuerAltName,
    oidBasicConstraints,
    oidCrlDistributionPoints,
    oidCertificatePolicies,
    oidAnyPolicy,
    oidPolicyMappings,
    oidAuthorityKeyIdentifier,
    oidPolicyConstraints,
    oidExtendedKeyUsage,
    oidAnyExtendedKeyUsage,
    oidInhibitAnyPolicy,
    oidAuthorityInfoAccess,
    oidSubjectInfoAccess,
    oidAdOCSP,
    oidAdCAIssuer,
    oidNetscapeCertType,
    oidEntrustVersInfo,
    oidMSNTPrincipalName,
    /* Policy Qualifier IDs for Internet policy qualifiers. */
    oidQtCps,
    oidQtUNotice,
    /* X.501 Name IDs. */
	oidCommonName,
    oidCountryName,
    oidLocalityName,
    oidStateOrProvinceName,
	oidOrganizationName,
	oidOrganizationalUnitName,
	oidDescription,
	oidEmailAddress,
    oidFriendlyName,
    oidLocalKeyId,
    oidExtendedKeyUsageServerAuth,
    oidExtendedKeyUsageClientAuth,
    oidExtendedKeyUsageCodeSigning,
    oidExtendedKeyUsageEmailProtection,
    oidExtendedKeyUsageOCSPSigning,
    oidExtendedKeyUsageIPSec,
    oidExtendedKeyUsageMicrosoftSGC,
    oidExtendedKeyUsageNetscapeSGC,
	/* Secure Boot Spec oid */
	oidAppleSecureBootCertSpec,
    oidAppleProvisioningProfile,
    oidAppleApplicationSigning;

/* Compare two decoded OIDs.  Returns true iff they are equivalent. */
bool DEROidCompare(const DERItem *oid1, const DERItem *oid2);

#ifdef __cplusplus
}
#endif

#endif	/* _LIB_DER_UTILS_H_ */
