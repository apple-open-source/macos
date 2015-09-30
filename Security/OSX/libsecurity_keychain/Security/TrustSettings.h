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
 * TrustSettings.h - class to manage trusted certs. 
 */
 
#ifndef	_TRUST_SETTINGS_H_
#define _TRUST_SETTINGS_H_

#include "SecTrust.h"
#include <security_keychain/StorageManager.h>
#include <security_keychain/SecTrustSettings.h>

/*
 * Clarification of the bool arguments to our main constructor.
 */
#define CREATE_YES	true
#define CREATE_NO	false
#define TRIM_YES	true
#define TRIM_NO		false

namespace Security
{

namespace KeychainCore
{

/* 
 * Additional values for the SecTrustSettingsDomain enum.
 */
enum {
	/*
	 * This indicates a TrustSettings that exists only in memory; it
	 * can't be written to disk. 
	 */
	kSecTrustSettingsDomainMemory = 100
};

class TrustSettings
{
private:
	TrustSettings(SecTrustSettingsDomain domain);

public:

	/* 
	 * Normal constructor, from disk.
	 * If create is true, the absence of an on-disk TrustSettings file
	 * results in the creation of a new empty TrustSettings. If create is 
	 * false and no on-disk TrustSettings exists, errSecItemNotFound is
	 * thrown.
	 * If trim is true, the components of the on-disk TrustSettings not
	 * needed for cert evaluation are discarded. This is for TrustSettings
	 * that will be cached in memory long-term. 
	 */
	static OSStatus CreateTrustSettings(
		SecTrustSettingsDomain				domain,
		bool								create,
		bool								trim,
		TrustSettings*&						ts);

	/* 
	 * Create from external data, obtained by createExternal().
	 * If externalData is NULL, we'll create an empty mTrustDict.
	 */
	static OSStatus CreateTrustSettings(
		SecTrustSettingsDomain				domain,
		CFDataRef							externalData,
		TrustSettings*&						ts);

	~TrustSettings();
	
	/* 
	 * Evaluate specified cert. Returns true if we found a matching 
 	 * record for the cert. 
	 */
	bool evaluateCert(
		CFStringRef				certHashStr,
		const CSSM_OID			*policyOID,			/* optional */
		const char				*policyString,		/* optional */
		SecTrustSettingsKeyUsage keyUsage,			/* optional */
		bool					isRootCert,			/* for checking default setting */
		CSSM_RETURN				**allowedErrors,	/* mallocd and RETURNED */
		uint32					*numAllowedErrors,	/* RETURNED */
		SecTrustSettingsResult	*resultType,		/* RETURNED */
		bool					*foundAnyEntry);	/* RETURNED - there is SOME entry for 
													 *   this cert */
		
	/* 
	 * Only certs with a SecTrustSettingsResult of kSecTrustSettingsResultTrustRoot
	 * or kSecTrustSettingsResultTrustAsRoot will be returned.  
	 */
	void findQualifiedCerts(
		StorageManager::KeychainList	&keychains,
		/* 
		 * If findAll is true, all certs are returned and the subsequent 
		 * qualifiers are ignored 
		 */
		bool							findAll,
		/* if true, only return root (self-signed) certs */
		bool							onlyRoots,
		const CSSM_OID					*policyOID,		/* optional */
		const char						*policyString,	/* optional */
		SecTrustSettingsKeyUsage		keyUsage,		/* optional */
		CFMutableArrayRef				certArray);		/* certs appended here */
		
	/*
	 * Find all certs in specified keychain list which have entries in this trust record.
	 * Certs already in the array are not added.
	 */
	void findCerts(
		StorageManager::KeychainList	&keychains,
		CFMutableArrayRef				certArray);
	
	/*
	 * Obtain trust settings for the specified cert. Returned settings array
	 * is in the public API form; caller must release. Returns NULL
	 * (does not throw) if the cert is not present in this TrustRecord. 
 	 * The certRef argument can be kSecTrustSettingsDefaultRootCertSetting. 
	 */
	CFArrayRef copyTrustSettings(
		SecCertificateRef	certRef);
		
	/* 
 	 * Obtain the mod date for the specified cert's trust settings.
     * Returns NULL (does not throw) if the cert is not present in this 
	 * TrustRecord.
 	 * The certRef argument can be kSecTrustSettingsDefaultRootCertSetting. 
	 */
	CFDateRef copyModDate(
		SecCertificateRef	certRef);

	/*
	 * Modify cert's trust settings, or add a new cert to the record. 
 	 * The certRef argument can be kSecTrustSettingsDefaultRootCertSetting. 
	 */
	void setTrustSettings(
		SecCertificateRef	certRef,
		CFTypeRef			trustSettingsDictOrArray);
		
	/*
	 * Delete a certificate's trust settings. 
	 * Throws errSecItemNotFound if there currently are no settings.
 	 * The certRef argument can be kSecTrustSettingsDefaultRootCertSetting. 
	 */
	void deleteTrustSettings(
		SecCertificateRef	certRef);
		
	/* 
	 * Flush property list data out to disk if dirty.
	 */
	void flushToDisk();

	/*
	 * Obtain external representation of TrustSettings data.
	 */
	CFDataRef createExternal();

private:
	/* common code to init mPropList from raw data */
	void initFromData(
		CFDataRef			trustSettingsData);

	/*
	 * Find a given cert's entry in mTrustDict. 
	 * Returned dictionary is not refcounted. 
	 */ 
	CFDictionaryRef findDictionaryForCert(
		SecCertificateRef	certRef);

	/*
	 * Find entry in mTrustDict given cert hash string. 
	 */
	CFDictionaryRef findDictionaryForCertHash(
		CFStringRef		certHashStr);
		
	/*
	 * Validate incoming API-style trust settings, which may be NULL, a 
	 * dictionary, or an array of dictionaries. We return a deep-copied, 
	 * refcounted CFArray, in internal format, in any case as long as the 
	 * incoming parameter is good.
	 */
	CFArrayRef validateApiTrustSettings(
		CFTypeRef trustSettingsDictOrArray,
		Boolean isSelfSigned);

	/* 
	 * Validate an usage constraint array from disk as part of our mPropDict
	 * array. Returns true if OK, else returns false. 
	 */
	bool validateTrustSettingsArray(
		CFArrayRef trustSettings);
		
	/* 
	 * Obtain issuer and serial number for specified cert, both 
	 * returned as CFDataRefs owned by caller. 
	 */
	void copyIssuerAndSerial(
		SecCertificateRef	cert,
		CFDataRef			*issuer,		/* optional, RETURNED */
		CFDataRef			*serial);		/* RETURNED */
		
	/*
	 * Validate mPropDict after it's read from disk. Allows subsequent use of 
	 * mPropDict and mTrustDict to proceed with relative impunity. 
	 * If trim is true, we remove fields in the per-cert dictionaries which 
	 * are not needed for cert evaluation. We also release the top-level
 	 * mPropList, which serves as a "this is trimmed" indicator if NULL. 
	 */
	void validatePropList(bool trim);

	/* fatal error abort */
	void abort(
		const char			*why,
		OSStatus			err);

	/* the overall parsed TrustSettings - may be NULL if this is trimmed */
	CFMutableDictionaryRef			mPropList;
	
	/* and the main thing we work with, the dictionary of per-cert trust settings */
	CFMutableDictionaryRef			mTrustDict;
	
	/* version number of mPropDict */
	SInt32							mDictVersion;

	SecTrustSettingsDomain			mDomain;
	bool							mDirty;		/* we've changed mPropDict since creation */
};

} /* end namespace KeychainCore */

} /* end namespace Security */

#endif	/* _TRUST_SETTINGS_H_ */

