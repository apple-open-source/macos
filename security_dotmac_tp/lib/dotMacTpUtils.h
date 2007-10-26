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
 * DotMacTpUtils.h
 */
 
#ifndef	_DOT_MAC_TP_UTILS_H_
#define _DOT_MAC_TP_UTILS_H_

#include <Security/cssmtype.h>
#include <Security/x509defs.h>
#include <security_asn1/SecNssCoder.h>
#include "dotMacTpRpcGlue.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Given an array of name/value pairs, cook up a CSSM_X509_NAME in specified
 * SecNssCoder's address space.
 */
void dotMacTpbuildX509Name(
	SecNssCoder						&coder,
	uint32							numTypeValuePairs,  // size of typeValuePairs[]
	CSSM_X509_TYPE_VALUE_PAIR_PTR	typeValuePairs,
	CSSM_X509_NAME					&x509Name);

/* Convert a reference key to a raw key. */
void dotMacRefKeyToRaw(
	CSSM_CSP_HANDLE	cspHand,
	const CSSM_KEY	*refKey,	
	CSSM_KEY_PTR	rawKey);			// RETURNED

/*
 * Encode/decode ReferenceIdentitifiers for queued requests.
 * We PEM encode/decode here to keep things orthogonal, since returned
 * certs and URLs are also in PEM or at least UTF8 format. 
 */
OSStatus dotMacEncodeRefId(  
	const CSSM_DATA				&userName,	// UTF8, no NULL
	DotMacCertTypeTag			signType,
	SecNssCoder					&coder,		// results mallocd in this address space
	CSSM_DATA					&refId);	// RETURNED, PEM encoded

OSStatus dotMacDecodeRefId(
	SecNssCoder					&coder,		// results mallocd in this address space
	const CSSM_DATA				&refId,		// PEM encoded
	CSSM_DATA					&userName,	// RETURNED, UTF8, no NULL
	DotMacCertTypeTag			*signType);  // RETURNED

/* fetch cert via HTTP */
CSSM_RETURN dotMacTpCertFetch(
	const CSSM_DATA				&userName,  // UTF8, no NULL
	DotMacCertTypeTag			signType,
	Allocator					&alloc,		// results mallocd here
	CSSM_DATA					&result);	// RETURNED

#ifdef __cplusplus
}
#endif

#endif	/* _DOT_MAC_TP_UTILS_H_ */

