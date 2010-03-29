/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * SecTrustSettingsPriv.h - TrustSettings SPI functions.
 */
 
#ifndef	_SEC_TRUST_SETTINGS_PRIV_H_
#define _SEC_TRUST_SETTINGS_PRIV_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/cssmtype.h>
#include <Security/SecPolicy.h>
#include <Security/SecCertificate.h>
#include <Security/SecTrustSettings.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fundamental routine used by TP to ascertain status of one cert.
 *
 * Returns true in *foundMatchingEntry if a trust setting matching
 * specific constraints was found for the cert. Returns true in 
 * *foundAnyEntry if any entry was found for the cert, even if it
 * did not match the specified constraints. The TP uses this to 
 * optimize for the case where a cert is being evaluated for
 * one type of usage, and then later for another type. If
 * foundAnyEntry is false, the second evaluation need not occur. 
 *
 * Returns the domain in which a setting was found in *foundDomain. 
 *
 * Allowed errors applying to the specified cert evaluation 
 * are returned in a mallocd array in *allowedErrors and must
 * be freed by caller. 
 */
OSStatus SecTrustSettingsEvaluateCert(
	CFStringRef					certHashStr,
	/* parameters describing the current cert evalaution */
	const CSSM_OID				*policyOID,
	const char					*policyString,		/* optional */
	uint32						policyStringLen,
	SecTrustSettingsKeyUsage	keyUsage,			/* optional */
	bool						isRootCert,			/* for checking default setting */
	/* RETURNED values */
	SecTrustSettingsDomain		*foundDomain,
	CSSM_RETURN					**allowedErrors,	/* mallocd and RETURNED */
	uint32						*numAllowedErrors,	/* RETURNED */
	SecTrustSettingsResult		*resultType,		/* RETURNED */
	bool						*foundMatchingEntry,/* RETURNED */
	bool						*foundAnyEntry);	/* RETURNED */

/* 
 * Obtain trusted certs which match specified usage. 
 * Only certs with a SecTrustSettingsResult of 
 * kSecTrustSettingsResultTrustRoot or
 * or kSecTrustSettingsResultTrustAsRoot will be returned. 
 * 
 * To be used by SecureTransport for its (hopefully soon-to-be-
 * deprecated) SSLSetTrustedRoots() call; I hope nothing else has 
 * to use this...
 *
 * Caller must CFRelease the returned CFArrayRef.
 */
OSStatus SecTrustSettingsCopyQualifiedCerts(
	const CSSM_OID				*policyOID,
	const char					*policyString,		/* optional */
	uint32						policyStringLen,
	SecTrustSettingsKeyUsage	keyUsage,			/* optional */
	CFArrayRef					*certArray);		/* RETURNED */
	
/*
 * Obtain unrestricted root certificates from the specified domain(s).
 * Only returns root certificates with no usage constraints.
 * Caller must CFRelease the returned CFArrayRef.
 */
OSStatus SecTrustSettingsCopyUnrestrictedRoots(
	Boolean					userDomain,
	Boolean					adminDomain,
	Boolean					systemDomain,
	CFArrayRef				*certArray);		/* RETURNED */

/* 
 * Obtain a string representing a cert's SHA1 digest. This string is
 * the key used to look up per-cert trust settings in a TrustSettings record. 
 */
CFStringRef SecTrustSettingsCertHashStrFromCert(
	SecCertificateRef certRef);

CFStringRef SecTrustSettingsCertHashStrFromData(
	const void *cert,
	size_t certLen);

/*
 * Add a cert's TrustSettings to a non-persistent TrustSettings record.
 * Primarily intended for use in creating a system TrustSettings record
 * (which is itself immutable via this module). 
 * 
 * The settingsIn argument is an external representation of a TrustSettings
 * record, obtained from this function or from 
 * SecTrustSettingsCreateExternalRepresentation().
 * If settingsIn is NULL, a new (empty) TrustSettings will be created. 
 * 
 * The certRef and trustSettingsDictOrArray arguments are as in 
 * SecTrustSettingsSetTrustSettings(). May be NULL, when e.g. creating 
 * a new and empty TrustSettings record. 
 *
 * The external representation is written to the settingOut argument,
 * which must eventually be CFReleased by the caller. 
 */
OSStatus SecTrustSettingsSetTrustSettingsExternal(
	CFDataRef			settingsIn,					/* optional */
	SecCertificateRef	certRef,					/* optional */
	CFTypeRef			trustSettingsDictOrArray,	/* optional */
	CFDataRef			*settingsOut);				/* RETURNED */

#ifdef __cplusplus
}
#endif

#endif	/* _SEC_TRUST_SETTINGS_PRIV_H_ */

