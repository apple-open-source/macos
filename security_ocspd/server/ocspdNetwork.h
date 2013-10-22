/*
 * Copyright (c) 2002,2012 Apple Inc. All rights reserved.
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
 * ocspdNetwork.h - Network support for ocspd
 */
 
#ifndef	_OCSPD_NETWORK_H_
#define _OCSPD_NETWORK_H_

#include <Security/cssmtype.h>
#include <Security/SecAsn1Coder.h>
#include <security_utilities/alloc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* fetch via HTTP using GET (preferred) or POST (if required) */
CSSM_RETURN ocspdHttpFetch(
	SecAsn1CoderRef		coder, 
	const CSSM_DATA 	&url,
	const CSSM_DATA		&ocspReq,	// DER encoded
	CSSM_DATA			&fetched);	// mallocd in coder space and RETURNED

/* Fetch cert or CRL from net, we figure out the schema */

typedef enum {
	LT_Crl = 1,
	LT_Cert
} LF_Type;

CSSM_RETURN ocspdNetFetch(
	Allocator			&alloc, 
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched);	// mallocd in coder space and RETURNED

/* Fetch cert or CRL from net asynchronously */

typedef struct crl_names_t {
	char *				crlFile;
	char *				pemFile;
	char *				updateFile;
	char *				revokedFile;
} crl_names_t;

typedef struct async_fetch_t {
	Allocator			*alloc;		// IN: set by caller; used to alloc fetched data
	CSSM_DATA			url;		// IN: mallocd by caller, struct owner must free
	char *				outFile;	// IN: mallocd by caller, struct owner must free
	LF_Type				lfType;		// IN: resource type to fetch
	crl_names_t			crlNames;	// IN: mallocd by caller, struct owner must free
	int					finished;	// 1 when download is finished
	int					freeOnDone;	// 1 if async function should own & free this struct
	CSSM_RETURN			result;		// OUT: error result if download did not complete
	CSSM_DATA			fetched;	// OUT: only valid if freeOnDone is 0 (caller frees)
} async_fetch_t;

CSSM_RETURN ocspdStartNetFetch(
	async_fetch_t		*fetchParams);

/* Called after download completes, writes received data to outFile */
CSSM_RETURN ocspdFinishNetFetch(
	async_fetch_t		*fetchParams);

#ifdef	__cplusplus
}
#endif

#endif	/* _OCSPD_NETWORK_H_ */
