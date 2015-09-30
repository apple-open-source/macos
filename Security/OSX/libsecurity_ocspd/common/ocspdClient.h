/*
 * Copyright (c) 2000,2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * ocspdClient.h - Client interface to OCSP helper daemon
 */
 
#ifndef	_OCSPD_CLIENT_H_
#define _OCSPD_CLIENT_H_

#include <Security/cssmtype.h>
#include <Security/SecTrustSettings.h>
#include <security_utilities/alloc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#pragma mark ----- OCSP routines -----

/*
 * Normal OCSP request. Depending on contents of encoded SecAsn1OCSPToolRequest,
 * this optionally performs cache lookup, local responder OCSP, and normal
 * OCSP, in that order. If OCSP response is fetched from the net the netFetch
 * outParam is true on return. 
 */
CSSM_RETURN ocspdFetch(
	Allocator			&alloc,
	const CSSM_DATA		&ocspdReq,		// DER-encoded SecAsn1OCSPDRequests
	CSSM_DATA			&ocspdResp);	// DER-encoded kSecAsn1OCSPDReplies
										// mallocd via alloc and RETURNED

/* 
 * Flush all OCSP responses associated with specifed CertID from cache. 
 */
CSSM_RETURN ocspdCacheFlush(
	const CSSM_DATA		&certID);
	
/* 
 * Flush stale entries from cache. 
 */
CSSM_RETURN ocspdCacheFlushStale();

#pragma mark ----- CRL/Cert routines -----
/* 
 * fetch a certificate from the net. 
 */
CSSM_RETURN ocspdCertFetch(
	Allocator			&alloc,
	const CSSM_DATA		&certURL,
	CSSM_DATA			&certData);		// mallocd via alloc and RETURNED

/* 
 * fetch a CRL from the net with optional cache lookup and/or store.
 * VerifyTime argument only used for cache lookup; it must be in 
 * CSSM_TIMESTRING format. 
 * crlIssuer is optional, and is only specified when the client knows
 * that the issuer of the CRL is the same as the issuer of the cert
 * being verified. 
 */
CSSM_RETURN ocspdCRLFetch(
	Allocator			&alloc,
	const CSSM_DATA		&crlURL,
	const CSSM_DATA		*crlIssuer,		// optional
	bool				cacheReadEnable,
	bool				cacheWriteEnable,
	CSSM_TIMESTRING 	verifyTime,
	CSSM_DATA			&crlData);		// mallocd via alloc and RETURNED

/*
 * fetch CRL revocation status, given a certificate's serial number and
 * its issuers, along with an identifier for the CRL (either its issuer name
 * or distribution point URL)
 *
 * This may return one of the following result codes:
 *
 * CSSM_OK (valid CRL was found for this issuer, serial number is not on it)
 * CSSMERR_TP_CERT_REVOKED (valid CRL was found, serial number is revoked)
 * CSSMERR_APPLETP_NETWORK_FAILURE (crl not available, download in progress)
 * CSSMERR_APPLETP_CRL_NOT_FOUND (crl not available, and not in progress)
 *
 * The first three error codes can be considered definitive answers (with the
 * NETWORK_FAILURE case indicating a possible retry later if required); the
 * last error requires a subsequent call to ocspdCRLFetch to either retrieve
 * the CRL from the on-disk cache or initiate a download of the CRL.
 *
 * Note: CSSMERR_TP_INTERNAL_ERROR can also be returned if there is a problem
 * with the provided arguments, or an error communicating with ocspd.
 */
CSSM_RETURN ocspdCRLStatus(
	const CSSM_DATA		&serialNumber,
	const CSSM_DATA		&certIssuers,
	const CSSM_DATA		*crlIssuer,		// optional if URL is supplied
	const CSSM_DATA		*crlURL);		// optional if issuer is supplied

/* 
 * Refresh the CRL cache. 
 */
CSSM_RETURN ocspdCRLRefresh(
	unsigned			staleDays,
	unsigned			expireOverlapSeconds,
	bool				purgeAll,
	bool				fullCryptoVerify);

/* 
 * Flush all CRLs obtained from specified URL from cache. Called by client when 
 * *it* detects a bad CRL. 
 */
CSSM_RETURN ocspdCRLFlush(
	const CSSM_DATA		&crlURL);
	
/*
 * Obtain TrustSettings. 
 */
OSStatus ocspdTrustSettingsRead(
	Allocator				&alloc,
	SecTrustSettingsDomain 	domain,
	CSSM_DATA				&trustSettings);		// mallocd via alloc and RETURNED

/*
 * Write TrustSettings to disk. Results in authentication dialog.
 */
OSStatus ocspdTrustSettingsWrite(
	SecTrustSettingsDomain 	domain,
	const CSSM_DATA			&authBlob,
	const CSSM_DATA			&trustSettings);

#ifdef	__cplusplus
}
#endif

#endif	/* _OCSPD_CLIENT_H_ */
