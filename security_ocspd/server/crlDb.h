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
 * crlDb.h - CRL cache
 *
 * Created 30 September 2004 by dmitch at Apple
 */
 
#ifndef	_OCSPD_CRL_DB_H_
#define _OCSPD_CRL_DB_H_

#include <Security/cssmtype.h>
#include <security_utilities/alloc.h>
#include <security_utilities/debugging.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lookup cached CRL by URL or issuer, and verifyTime. 
 * Just a boolean returned; we found it, or not.
 * Exactly one of {url, issuer} should be non-NULL.
 */
bool crlCacheLookup(
	Allocator			&alloc,
	const CSSM_DATA		*url,
	const CSSM_DATA		*issuer,			// optional
	const CSSM_DATA		&verifyTime,
	CSSM_DATA			&crlData);			// allocd in alloc space and RETURNED

/* 
 * Add a CRL response to cache. Incoming response is completely unverified;
 * we just verify that we can parse it. 
 */
CSSM_RETURN crlCacheAdd(
	const CSSM_DATA		&crlData,			// as it came from the server
	const CSSM_DATA		&url);				// where it came from 

/*
 * Delete any CRL associated with specified URL from cache.
 */
void crlCacheFlush(
	const CSSM_DATA		&url);

/* 
 * Refresh the CRL cache. 
 */
void crlCacheRefresh(
	unsigned			staleDays,
	unsigned			expireOverlapSeconds,
	bool				purgeAll,
	bool				fullCryptoVerify,
	bool				doRefresh);

#ifdef __cplusplus
}
#endif

#endif	/* _OCSPD_CRL_DB_H_ */

