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


/*
 * cdsaUtils.h - utility functions for CDSA-related code
 */

#ifndef	_SNACC_CDSA_UTILS_H_
#define _SNACC_CDSA_UTILS_H_

#include <Security/asn-incl.h>
#include <Security/utilities.h>
#include <Security/cssmdata.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* all decoding errors throw CSSMERR_CL_UNKNOWN_FORMAT */

/* malloc/copy AsnBits.bits -->CssmOwnedData */
void SC_asnBitsToCssmData(
	const AsnBits &bits,
	CssmOwnedData &oData);

/* given DER-encoded bit string, decoded it and malloc/copy results 
 * back to a CssmOwnedData */
void SC_decodeAsnBitsToCssmData(
	const CssmData encodedBits,
	CssmOwnedData &oData);

/* DER-decode any AsnType object */
void SC_decodeAsnObj(
	const CssmData		&derEncoded,
	AsnType				&asnObj);

/* DER-encode any AsnType object. */
void SC_encodeAsnObj(
	AsnType				&asnObj,
	CssmOwnedData		&derEncoded,
	size_t				maxEncodedSize);

/*
 * Given a contentLength, obtain the length of the DER length encoding.
 */
size_t SC_lengthOfLength(
	size_t 				contentLen);

/*
 * Encode a DER length field. Pass in the lengthOfLength if you've obtained
 * it in a previous call to CL_lengthOfLength.
 */
void SC_encodeLength(
	size_t 	contentLen,
	void 	*cp,
	size_t	lengthOfLength = 0);

#ifdef	__cplusplus
}
#endif

#endif	/* _SNACC_CDSA_UTILS_H_ */