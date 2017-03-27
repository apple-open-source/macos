/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 *
 */

/*!
 @header SecRevocationDb
 The functions in SecRevocationDb.h provide an interface to look up
 revocation information, and refresh that information periodically.
 */

#ifndef _SECURITY_SECREVOCATIONDB_H_
#define _SECURITY_SECREVOCATIONDB_H_

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <dispatch/dispatch.h>
#include <Security/SecBase.h>

__BEGIN_DECLS

/* issuer group data format */
typedef CF_ENUM(uint32_t, SecValidInfoFormat) {
    kSecValidInfoFormatUnknown      = 0,
    kSecValidInfoFormatSerial       = 1,
    kSecValidInfoFormatSHA256       = 2,
    kSecValidInfoFormatNto1         = 3
};

/*!
    @typedef SecValidInfoRef
    @abstract Object used to return valid info lookup results.
 */
typedef struct __SecValidInfo *SecValidInfoRef;

struct __SecValidInfo {
    SecValidInfoFormat  format;     // format of per-issuer validity data
    CFDataRef           certHash;   // SHA-256 hash of cert to which the following info applies
    CFDataRef           issuerHash; // SHA-256 hash of issuing CA certificate
    bool                valid;      // true if found on allow list, false if on block list
    bool                complete;   // true if list is complete (i.e. status is definitive)
    bool                checkOCSP;  // true if complete is false and OCSP check is required
    bool                knownOnly;  // true if all intermediates under issuer must be found in database
    bool                requireCT;  // true if this cert must have CT proof
};

/*!
	@function SecValidInfoRelease
	@abstract Releases a SecValidInfo reference previously obtained from a call to SecRevocationDbCopyMatching.
	@param validInfo The SecValidInfo reference to be released.
 */
void SecValidInfoRelease(SecValidInfoRef validInfo);

/*!
	@function SecRevocationDbCheckNextUpdate
	@abstract Periodic hook to poll for updates.
    @result A boolean value indicating whether an update check was dispatched.
 */
bool SecRevocationDbCheckNextUpdate(void);

/*!
	@function SecRevocationDbCopyMatching
	@abstract Returns a SecValidInfo reference if matching revocation (or allow list) info was found.
	@param certificate The certificate whose validity status is being requested.
	@param issuer The issuing CA certificate. If the cert is self-signed, the same reference should be passed in both certificate and issuer parameters. Omitting either cert parameter is an error and NULL will be returned.
	@result A SecValidInfoRef if there was matching revocation info. Caller must release this reference when finished by calling SecValidInfoRelease. NULL is returned if no matching info was found in the database.
 */
SecValidInfoRef SecRevocationDbCopyMatching(SecCertificateRef certificate,
                                            SecCertificateRef issuer);

/*!
	@function SecRevocationDbGetVersion
	@abstract Returns a CFIndex containing the version number of the database.
	@result On success, the returned version will be a value greater than or equal to zero. A version of 0 indicates an empty database which has yet to be populated. If the version cannot be obtained, -1 is returned.
 */
CFIndex SecRevocationDbGetVersion(void);

/*!
	@function SecRevocationDbGetSchemaVersion
	@abstract Returns a CFIndex containing the schema version number of the database.
	@result On success, the returned version will be a value greater than or equal to zero. A version of 0 indicates an empty database which has yet to be populated. If the version cannot be obtained, -1 is returned.
 */
CFIndex SecRevocationDbGetSchemaVersion(void);


__END_DECLS

#endif /* _SECURITY_SECREVOCATIONDB_H_ */
