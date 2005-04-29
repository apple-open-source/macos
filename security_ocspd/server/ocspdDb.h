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
 * ocspdDb.h - API for OCSP daemon database
 */
 
#ifndef	_OCSPD_DB_H_
#define _OCSPD_DB_H_

#include <Security/cssmtype.h>
#include <Security/SecAsn1Coder.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lookup cached response. Result is a DER-encoded OCSP response, the same bits
 * originally obtained from the net. Result is allocated in specified 
 * SecAsn1CoderRef's memory. Never returns a stale entry; we always check the 
 * enclosed SingleResponse for temporal validity. 
 *
 * Just a boolean returned; we found it, or not.
 */
bool ocspdDbCacheLookup(
	SecAsn1CoderRef		coder,
	const CSSM_DATA		&certID,
	const CSSM_DATA		*localResponder,	// optional; if present, must match
											// entry's URI
	CSSM_DATA			&derResp);			// RETURNED

/* 
 * Add an OCSP response to cache. Incoming response is completely unverified;
 * we just verify that we can parse it and is has at least one SingleResponse
 * which is temporally valid. 
 */
void ocspdDbCacheAdd(
	const CSSM_DATA		&ocspResp,			// as it came from the server
	const CSSM_DATA		&URI);				// where it came from 

/*
 * Delete any entry associated with specified certID from cache.
 */
void ocspdDbCacheFlush(
	const CSSM_DATA		&certID);

/*
 * Flush stale entries from cache. 
 */
void ocspdDbCacheFlushStale();

#ifdef __cplusplus
}
#endif

#endif	/* _OCSPD_DB_H_ */

