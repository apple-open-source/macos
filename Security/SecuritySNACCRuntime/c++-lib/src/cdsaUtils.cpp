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
 * cdsaUtils.cpp - utility functions for CDSA-related code
 */

#include "cdsaUtils.h"
#include <Security/cssmerr.h>

#ifdef	NDEBUG

#include <Security/globalizer.h>

/* silent cerr substitute */
ModuleNexus<Asn1ErrorClass> AsnNullError;
#endif	/* NDEBUG */

/* malloc/copy AsnBits.bits -->CssmOwnedData */
void SC_asnBitsToCssmData(
	const AsnBits &bits,
	CssmOwnedData &oData)
{
	size_t len = (bits.BitLen() + 7) / 8;
	oData.copy(reinterpret_cast<const uint8 *>(bits.BitOcts()), len);
}

/* given DER-encoded bit string, decoded it and malloc/copy results 
 * back to a CssmOwnedData */
void SC_decodeAsnBitsToCssmData(
	const CssmData encodedBits,
	CssmOwnedData &oData)
{
	AsnBits decodedBits;
	SC_decodeAsnObj(encodedBits, decodedBits);
	size_t len = (decodedBits.BitLen() + 7) / 8;
	oData.copy(reinterpret_cast<const uint8 *>(decodedBits.BitOcts()), len);
}

/*
 * Universal BDecPdu/BEncPdu replacements, used below in SC_decodeAsnObj and
 * SC_encodeAsnObj.
 *
 * All AsnType subclasses implement this either via PDU_MEMBER_MACROS
 * for SecuritySNACCRuntime built-in types, or explicitly for all
 * other classes using asn-useful.h. To faciliate a global "one
 * routine for encode/decode" which operates on AsnType &'s, we have 
 * to explicitly provide this here. Why this is not in AsnType, I don't 
 * know.
 */
static int SC_BDecPDU(
	AsnType 	&asnObj, 
	BUF_TYPE 	b, 
	AsnLen 		&bytesDecoded)
{
    ENV_TYPE env;

    bytesDecoded = 0;
	try {
         asnObj.BDec(b, bytesDecoded, env);
         return !b.ReadError();
    }
	catch(...) {
        return false;
	}
}

static int SC_BEncPdu(
	AsnType		&asnObj,
	BUF_TYPE 	b, 
	AsnLen 		&bytesEncoded)
{
    bytesEncoded = asnObj.BEnc(b);
    return !b.WriteError();
}

/* DER-decode any AsnType object */
void SC_decodeAsnObj(
	const CssmData		&derEncoded,
	AsnType				&asnObj)
{
	AsnBuf					buf;
	size_t					len = (size_t)derEncoded.length();
	
	buf.InstallData(reinterpret_cast<char *>(derEncoded.data()), len);
	if(!SC_BDecPDU(asnObj, buf, len)) {
		CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
	}
}

/* 
 * DER-encode any AsnType object.
 * Unfortunately the call has to give an estimate of the max encoded size of 
 * the result. There is no way (that I know of) to figure this out at encode
 * time. If this turns out to be a problem we might have to do a retry,
 * doubling the size of the encoded buffer. Be liberal; the maxEncodedSize
 * buffer is only temporary - due to snacc encoding style, a copy out is 
 * necessary in any case, so the mallocd size of encodedBuf is exactly the
 * right size. 
 */
void SC_encodeAsnObj(
	AsnType				&asnObj,
	CssmOwnedData		&derEncoded,
	size_t				maxEncodedSize)
{
	CssmAutoData aData(derEncoded.allocator);		// temp encode target
	aData.malloc(maxEncodedSize);
	memset(aData.data(), 0, maxEncodedSize);
	AsnBuf encBuf;
	encBuf.Init(static_cast<char *>(aData.data()), maxEncodedSize);
	encBuf.ResetInWriteRvsMode();
	AsnLen encoded;
	int rtn = SC_BEncPdu(asnObj, encBuf, encoded);
	if(encoded > maxEncodedSize) {
		CssmError::throwMe(CSSMERR_CSSM_BUFFER_TOO_SMALL);
	}
	if(!rtn) {
		/* not sure how this can happen... */
		CssmError::throwMe(CSSMERR_CSSM_BUFFER_TOO_SMALL);
	}
	/* success; copy out to caller */
	derEncoded.get().clear();
	derEncoded.copy(encBuf.DataPtr(), encBuf.DataLen());
}

/*
 * Given a contentLength, obtain the length of the DER length encoding.
 */
size_t SC_lengthOfLength(
	size_t contentLen)
{
	if(contentLen < 128) {
		return 1;	
	}
	else if(contentLen < 256) {
		return 2;
	}
	else if(contentLen < 65536) {
		return 3;
	}
	else if(contentLen < 16777126) {
		return 4;
	}
	else {
		return 5;
	}
}

/*
 * Encode a DER length field. Pass in the lengthOfLength if you've obtained
 * it in a previous call to SC_lengthOfLength.
 */
void SC_encodeLength(
	size_t 	contentLen,
	void 	*cp,
	size_t	lengthOfLength)
{
	if(lengthOfLength == 0) {
		lengthOfLength = SC_lengthOfLength(contentLen);
	}
	unsigned char *ucp = reinterpret_cast<unsigned char *>(cp);
	if(lengthOfLength == 1) {
		/* easy case */
		*ucp = contentLen;
		return;
	}
	lengthOfLength--;
	*ucp = (0x80 + lengthOfLength);
	ucp += lengthOfLength;
	for(size_t i=0; i<lengthOfLength; i++) {
		*ucp-- = contentLen & 0xff;
		contentLen >>= 8;
	}
}

/*
 * Explicitly non-inlined SnaccError throw 
 */
void SnaccExcep::throwMe(int err)
{
	throw SnaccExcep(err);
}
