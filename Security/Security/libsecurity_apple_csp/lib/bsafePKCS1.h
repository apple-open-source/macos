/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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

#ifdef	BSAFE_CSP_ENABLE


/*
 * bsafePKCS1.h - support for PKCS1 format RSA public key blobs, which for some
 * 					reason, BSAFE doesn't know about.
 */

#ifndef	_BSAFE_PKCS1_H_
#define _BSAFE_PKCS1_H_

#include <aglobal.h>
#include <bsafe.h>
#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>
#include <Security/asn-type.h>
#include <security_cdsa_utilities/cssmdata.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* DER-decode any AsnType object */
CSSM_RETURN CL_decodeAsnObj(
	const CssmData		&derEncoded,
	AsnType				&asnObj);

/* DER-encode any AsnType object. */
CSSM_RETURN CL_encodeAsnObj(
	AsnType				&asnObj,
	CssmOwnedData		&derEncoded,
	size_t				maxEncodedSize);

/*
 * Given a PKCS1-formatted key blob, decode the blob into components and do 
 * a B_SetKeyInfo on the specified BSAFE key.
 */
void BS_setKeyPkcs1(
	const CssmData 		&pkcs1Blob, 
	B_KEY_OBJ 			bsKey);

/*
 * Obtain public key blob info, PKCS1 format. 
 */
void BS_GetKeyPkcs1(
	const B_KEY_OBJ 	bsKey, 
	CssmOwnedData 		&pkcs1Blob);

#ifdef	__cplusplus
}
#endif

#endif	/* _BSAFE_PKCS1_H_ */

#endif	/* BSAFE_CSP_ENABLE */

