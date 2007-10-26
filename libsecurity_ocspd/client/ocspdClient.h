/*
 * Copyright (c) 2002,2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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
