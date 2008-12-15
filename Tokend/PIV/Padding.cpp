/*
 *  Copyright (c) 2008 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *
 *  @APPLE_LICENSE_HEADER_END@
 */

#include "Padding.h"

#include <Security/cssmerr.h>
#include "PIVUtilities.h"

using namespace Security;

/* PKCS#1 DigestInfo header for SHA1 */
static const unsigned char sha1sigheader[] =
{
	0x30, // SEQUENCE
	0x21, // LENGTH
		0x30, // SEQUENCE
		0x09, // LENGTH
			0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1a, // SHA1 OID (1 4 14 3 2 26)
			0x05, 0x00, // OPTIONAL ANY algorithm params (NULL)
		0x04, 0x14 // OCTECT STRING (20 bytes)
};

/* PKCS#1 DigestInfo header for MD5 */
static const unsigned char md5sigheader[] =
{
	0x30, // SEQUENCE
	0x20, // LENGTH
		0x30, // SEQUENCE
		0x0C, // LENGTH
			// MD5 OID (1 2 840 113549 2 5)
			0x06, 0x08, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05,
			0x05, 0x00, // OPTIONAL ANY algorithm params (NULL)
		0x04, 0x10 // OCTECT STRING (16 bytes)
};

void Padding::apply(byte_string &data, size_t keySize, CSSM_PADDING padding, CSSM_ALGORITHMS hashAlg) throw(CssmError) {
	// Calculate which hash-header to use
	const unsigned char *header;
	size_t headerLength;
	switch(hashAlg) {
	case CSSM_ALGID_SHA1:
		if (data.size() != 20)
			CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);
		header = sha1sigheader;
		headerLength = sizeof(sha1sigheader);
		break;
	case CSSM_ALGID_MD5:
		if (data.size() != 16)
			CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);
		header = md5sigheader;
		headerLength = sizeof(md5sigheader);
		break;
	case CSSM_ALGID_NONE:
		// Special case used by SSL it's an RSA signature, without the ASN1 stuff
		header = NULL;
		headerLength = 0;
		break;
	default:
		CssmError::throwMe(CSSMERR_CSP_INVALID_DIGEST_ALGORITHM);
	}
	// Reserve memory and insert the header before the data
	data.reserve(keySize);
	if(headerLength > 0) {
		data.insert(data.begin(), header, header + headerLength);
	}
	// Calculate and apply padding
	switch (padding) {
	case CSSM_PADDING_NONE:
		if(data.size() != keySize)
			CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);
		break;
	case CSSM_PADDING_PKCS1:
		// Pad using PKCS1 v1.5 signature padding ( 00 01 FF FF.. 00 | M)
		if(data.size() + 11 > keySize)
			CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);
		int markerByteLocation = keySize - data.size() - 1;
		data.insert(data.begin(), keySize - data.size(), 0xFF);
		data[0] = 0;
		data[1] = 1;
		data[markerByteLocation] = 0;
		break;
	default:
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
	}
}

void Padding::remove(byte_string &data, CSSM_PADDING padding) throw(CssmError) {
	// Calculate and remove padding while validating
	switch (padding) {
	case CSSM_PADDING_NONE:
		break;
	case CSSM_PADDING_PKCS1:
		unsigned i;
		/* Handles PKCS1 v1.5
		 * signatures         00 01 FF FF.. 00 | M
		 * and encrypted data 00 02 NZ NZ.. 00 | M   (NZ = non-zero random value)
		 */
		if(data[0] != 0 || (data[1] != 1 && data[1] != 2))
			CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
		for(i = 2; i < data.size() && data[i] != 0x00; i++) {}
		/* Assume empty data is invalid */
		if(data.size() - i == 0)
			CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
		secure_erase(data, data.begin(), data.begin() + i + 1);
		break;
	default:
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
	}
}

bool Padding::canApply(CSSM_PADDING padding, CSSM_ALGORITHMS hashAlg) throw() {
	switch(padding) {
	case CSSM_PADDING_NONE:
	case CSSM_PADDING_PKCS1:
		break;
	default:
		return false;
	}
	switch(hashAlg) {
	case CSSM_ALGID_NONE:
	case CSSM_ALGID_SHA1:
	case CSSM_ALGID_MD5:
		break;
	default:
		return false;
	}
	return true;
}

bool Padding::canRemove(CSSM_PADDING padding) throw() {
	switch(padding) {
	case CSSM_PADDING_NONE:
	case CSSM_PADDING_PKCS1:
		break;
	default:
		return false;
	}
	return true;
}