/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// pkcs_7_8.h - encode/decode key blobs in PKCS7 and 
//				  PKCS8 format.
//

#ifndef	_PKCS_7_8_H_
#define _PKCS_7_8_H_

#include <Security/cssmtype.h>
#include <Security/utilities.h>
#include <Security/cssmalloc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Given a symmetric CssmKey in raw format, and its encrypted blob,
 * cook up a PKCS-7 encoded blob.
 */
void cspEncodePkcs7(
	CSSM_ALGORITHMS		alg,			// encryption alg, used by PKCS7
	CSSM_ENCRYPT_MODE	mode,			// ditto
	const CssmData		&encryptedBlob,
	CssmData			&encodedBlob,	// mallocd and RETURNED
	CssmAllocator		&allocator);
	
/*
 * Given a symmetric key in (encrypted, encoded) PKCS-7 format, 
 * obtain its encrypted key blob.
 */
void cspDecodePkcs7(
	const CssmKey		&wrappedKey,	// for inferring format
	CssmData			&decodedBlob,	// mallocd and RETURNED
	CSSM_KEYBLOB_FORMAT	&format,		// RETURNED
	CssmAllocator		&allocator);

/*
 * Given an asymmetric CssmKey in raw format, and its encrypted blob,
 * cook up a PKCS-8 encoded blob.
 */
void cspEncodePkcs8(
	CSSM_ALGORITHMS		alg,			// encryption alg, used by PKCS8
	CSSM_ENCRYPT_MODE	mode,			// ditto
	const CssmData		&encryptedBlob,
	CssmData			&encodedBlob,	// mallocd and RETURNED
	CssmAllocator		&allocator);
	
/*
 * Given a an asymmetric key in (encrypted, encoded) PKCS-8 format, 
 * obtain its encrypted key blob.
 */
void cspDecodePkcs8(
	const CssmKey		&wrappedKey,	// for inferring format
	CssmData			&decodedBlob,	// mallocd and RETURNED
	CSSM_KEYBLOB_FORMAT	&format,		// RETURNED
	CssmAllocator		&allocator);

#ifdef	__cplusplus
}
#endif

#endif	/* _PKCS_7_8_H_ */