/*
 * Copyright (c) 2000,2002,2005 Apple Computer, Inc. All Rights Reserved.
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

/* fetch via HTTP POST */
CSSM_RETURN ocspdHttpPost(
	SecAsn1CoderRef		coder, 
	const CSSM_DATA 	&url,
	const CSSM_DATA		&ocspReq,	// DER encoded
	CSSM_DATA			&fetched);	// mallocd in coder space and RETURNED

#ifdef	__cplusplus
}
#endif

#endif	/* _OCSPD_NETWORK_H_ */
