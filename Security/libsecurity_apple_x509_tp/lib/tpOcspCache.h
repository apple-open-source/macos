/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * tpOcspCache.h - local OCSP response cache.
 */
 
#ifndef	_TP_OCSP_CACHE_H_
#define _TP_OCSP_CACHE_H_

#include <security_ocspd/ocspResponse.h>

/* max default TTL currently 12 hours */
#define TP_OCSP_CACHE_TTL	(60.0 * 60.0 * 12.0)

extern "C" {

/*
 * Lookup locally cached response. Caller must free the returned OCSPSingleResponse.
 * Never returns a stale entry; we always check the enclosed SingleResponse for
 * temporal validity.
 */
OCSPSingleResponse *tpOcspCacheLookup(
	OCSPClientCertID	&certID,
	const CSSM_DATA		*localResponderURI);		// optional 

/* 
 * Add a fully verified OCSP response to cache. 
 */
void tpOcspCacheAdd(
	const CSSM_DATA		&ocspResp,				// we'll decode it and own the result
	const CSSM_DATA		*localResponderURI);	// optional 

/*
 * Delete any entry associated with specified certID from cache.
 */
void tpOcspCacheFlush(
	OCSPClientCertID	&certID);

}
#endif	/* _TP_OCSP_CACHE_H_ */

