/*
 * Copyright (c) 2005,2011,2014 Apple Inc. All Rights Reserved.
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
 * TrustSettingsSchema.h - Dictionary keys used in on-disk TrustSettings plist. 
 */
 
#ifndef	_TRUST_SETTINGS_SCHEMA_H_
#define _TRUST_SETTINGS_SCHEMA_H_

#include <CoreFoundation/CFString.h>

/*
 * A TrustSettings Record contains the XML encoding of a CFDictionary. This dictionary
 * currently contains two name/value pairs:
 *
 * key = kTrustRecordVersion,   value = SInt32 version number
 * key = kTrustRecordTrustList, value = CFDictionary
 *
 * Each key/value pair of the CFDictionary associated with key kTrustRecordTrustList
 * consists of:
 * -- key   = the ASCII representation (with alpha characters in upper case) of the
 *            cert's SHA1 digest.
 * -- value = a CFDictionary representing one cert. 
 * 
 * Key/value pairs in the per-cert dictionary are as follows:
 *
 * -- key = kTrustRecordIssuer, value = non-normalized issuer as CFData
 * -- key = kTrustRecordSerialNumber, value = serial number as CFData
 * -- key = kTrustRecordModDate, value = CFDateRef of the last modification 
			date of the per-cert entry.
 * -- key = kTrustRecordTrustSettings, value = array of dictionaries. The
 *          dictionaries are as described in the API in SecUserTrust.h
 *			although we store the values differently (see below). 
 *			As written to disk, this key/value is always present although
 *			the usageConstraints array may be empty. 
 *
 * A usageConstraints dictionary is like so (all elements are optional). These key 
 * strings are defined in SecUserTrust.h.
 *
 * key = kSecTrustSettingsPolicy		value = policy OID as CFData or CFString
 * key = kSecTrustSettingsPolicyName    value = policy name as CFString
 * key = kSecTrustSettingsApplication	value = application as CFData
 * key = kSecTrustSettingsPolicyString	value = CFString, policy-specific
 * key = kSecTrustSettingsAllowedError	value = CFNumber, an SInt32 CSSM_RETURN 
 * key = kSecTrustSettingsResult		value = CFNumber, an SInt32 SecTrustSettingsResult
 * key = kSecTrustSettingsKeyUsage		value = CFNumber, an SInt32 key usage
 * key = kSecTrustSettingsModifyDate	value = CFDate, last modification 
 */
 
/*
 * Keys in the top-level dictionary
 */
#define kTrustRecordVersion				CFSTR("trustVersion")
#define kTrustRecordTrustList			CFSTR("trustList")

#define kSecTrustRecordNumTopDictKeys		2

/* 
 * Keys in the per-cert dictionary in the TrustedRootList record. 
 */
/* value = non-normalized issuer as CFData */
#define kTrustRecordIssuer				CFSTR("issuerName")

/* value = serial number as CFData */
#define kTrustRecordSerialNumber		CFSTR("serialNumber")

/* value = CFDateRef representation of modification date */
#define kTrustRecordModDate				CFSTR("modDate")

/* 
 * value = array of CFDictionaries as used in public API
 * Not present for a cert which has no usage Constraints (i.e.
 * "wide open" unrestricted, kSecTrustSettingsResultTrustRoot as
 * the default SecTrustSettingsResult).
 */
#define kTrustRecordTrustSettings		CFSTR("trustSettings")

#define kSecTrustRecordNumCertDictKeys	4

/*
 * Version of the top-level dictionary.
 */
enum {
	kSecTrustRecordVersionInvalid	= 0,	/* should never be seen on disk */
	kSecTrustRecordVersionCurrent	= 1
};

/*
 * Key for the (optional) default entry in a TrustSettings record. This
 * appears in place of the cert's hash string, and corresponds to 
 * kSecTrustSettingsDefaultRootCertSetting at the public API.
 * If you change this, make sure it has characters other than those 
 * appearing in a normal cert hash string (0..9 and A..F).
 */
#define kSecTrustRecordDefaultRootCert		CFSTR("kSecTrustRecordDefaultRootCert")

/* 
 * The location of the system root keychain and its associated TrustSettings. 
 * These are immutable; this module never modifies either of them.
 */
#define SYSTEM_ROOT_STORE_PATH			"/System/Library/Keychains/SystemRootCertificates.keychain"
#define SYSTEM_TRUST_SETTINGS_PATH		"/System/Library/Keychains/SystemTrustSettings.plist"

/*
 * The local admin cert store.
 */
#define ADMIN_CERT_STORE_PATH			"/Library/Keychains/System.keychain"

/*
 * Per-user and local admin TrustSettings are stored in this directory. 
 * Per-user settings are of the form <uuid>.plist.
 */
#define TRUST_SETTINGS_PATH				"/Library/Security/Trust Settings"
#define ADMIN_TRUST_SETTINGS			"Admin.plist"

/* 
 * The location of the system intermediate cert store keychain.
 */
#define SYSTEM_CERT_STORE_PATH			"/System/Library/Keychains/SystemCACertificates.keychain"

/*
 * The domain and key for the system preference to disable User-level domain TrustSettings.
 * If this pref exists in /Library/Preferences, and has a value of true, then 
 * per-user TrustSettings will be ignored. 
 */
#define kSecTrustSettingsPrefsDomain				"com.apple.security"
#define kSecTrustSettingsDisableUserTrustSettings	CFSTR("DisableUserTrustSettings")

#endif	/* _TRUST_SETTINGS_SCHEMA_H_ */

