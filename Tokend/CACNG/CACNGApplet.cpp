/*
 *  CACNGApplet.cpp
 *  Tokend
 *
 *  Created by harningt on 9/30/09.
 *  Copyright 2009 TrustBearer Labs. All rights reserved.
 *
 */


#include "CACNGApplet.h"
#include <security_utilities/pcsc++.h>

#include "CACNGToken.h"
#include "CACNGError.h"

#include "CompressionTool.h"

#include "TLV.h"

/* FOR KEYSIZE CALCULATION */
#include <Security/Security.h>

#define PIV_CLA_STANDARD				0x00
#define PIV_INS_GET_DATA				0xCB	// [SP800731 7.1.2]

//										0x00				0xCB
#define PIV_GETDATA_APDU			PIV_CLA_STANDARD, PIV_INS_GET_DATA, 0x3F, 0xFF
#define PIV_GETDATA_CONT_APDU	0x00, 0xC0, 0x00, 0x00

#define PIV_GETDATA_RESPONSE_TAG		0x53
#define PIV_GETDATA_TAG_CERTIFICATE		0x70
#define PIV_GETDATA_TAG_CERTINFO		0x71
#define PIV_GETDATA_TAG_MSCUID			0x72
#define PIV_GETDATA_TAG_ERRORDETECTION	0xFE

#define PIV_GETDATA_COMPRESSION_MASK	0x81

CACNGCacApplet::CACNGCacApplet(CACNGToken &token, const byte_string &applet, const byte_string &object)
:token(token), applet(applet), object(object)
{
}

void CACNGCacApplet::select()
{
	byte_string result;
	uint32_t code = token.exchangeAPDU(applet, result);
	CACNGError::check(code);
	if (!object.empty()) {
		result.resize(0);
		code = token.exchangeAPDU(object, result);
		CACNGError::check(code);
	}
}

CACNGIDObject::CACNGIDObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, const std::string &description)
:token(token), applet(applet), keySize(0), description(description)
{
}


size_t CACNGIDObject::getKeySize()
{
	if (keySize == ~(size_t)0)
		CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	if (keySize != 0)
		return keySize;
	byte_string cert = read();
    SecCertificateRef certRef = 0;
    SecKeyRef keyRef = 0;
    /* Parse certificate for size */
    CSSM_DATA certData;
    certData.Data = (uint8_t*)&cert[0];
    certData.Length = cert.size();
    const CSSM_KEY *cssmKey = NULL;
    OSStatus status = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER, &certRef);
    if(status != noErr) goto done;
    status = SecCertificateCopyPublicKey(certRef, &keyRef);
    if(status != noErr) goto done;
    status = SecKeyGetCSSMKey(keyRef, &cssmKey);
    if(status != noErr) goto done;
    keySize = cssmKey->KeyHeader.LogicalKeySizeInBits;
done:
    if(keyRef)
        CFRelease(keyRef);
    if(certRef)
        CFRelease(certRef);
	if (keySize == 0) {
		keySize = ~(size_t)0;
		CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	}
    return keySize;
}

CACNGCacIDObject::CACNGCacIDObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, const std::string &description)
:CACNGIDObject(token, applet, description)
{
}

/*
 See NIST IR 6887 – 2003 EDITION, GSC-IS VERSION 2.1
 5.3.4 Generic Container Provider Virtual Machine Card Edge Interface
 for a description of how this command works
 
 READ BUFFER 0x80 0x52 Off/H Off/L 0x02 <buffer & number bytes to read> – 
 */
static size_t read_cac_buffer_size(CACNGToken &token, bool isTbuffer)
{
	unsigned char apdu[] = { 0x80, 0x52, 0x00, 0x00, 0x02, isTbuffer ? 0x01 : 0x02, 0x02 };
	unsigned char result[4];
	size_t resultLength = sizeof(result);
	uint32_t cacresult = token.exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	CACNGError::check(cacresult);
	return result[0] | result[1] << 8;
}

static void read_cac_buffer(CACNGToken &token, bool isTbuffer, byte_string &result)
{
	size_t size = read_cac_buffer_size(token, isTbuffer);
	result.resize(size + 2);
    unsigned int offset, bytes_left;
	const unsigned int MAX_READ = 0xFF;
    for (offset = 2, bytes_left = size; bytes_left;)
    {
		//    resultLength = size + 2 - offset;
        unsigned char toread = bytes_left > MAX_READ ? MAX_READ : bytes_left;
		unsigned char apdu[] = {
			0x80, 0x52, offset >> 8, offset & 0xFF, 0x02, isTbuffer ? 0x01 : 0x02, toread
		};
		size_t resultLength = toread + 2;
        uint32_t cacresult = token.exchangeAPDU(apdu, sizeof(apdu),
												&result[offset - 2],
												resultLength);
		
        CACNGError::check(cacresult);
		
        if (resultLength - 2 != toread)
			PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
		
        resultLength -= 2;
        offset += resultLength;
        bytes_left -= resultLength;
    }
	/* Trim off status bytes */
	result.resize(result.size() - 2);	
}

byte_string CACNGCacIDObject::read()
{
	byte_string result;
	CssmData data;
	if (token.cachedObject(0, description.c_str(), data))
	{
		result.assign((uint8_t*)data.data(), (uint8_t*)data.data() + data.length());
		return result;
	}
	
	PCSC::Transaction _(token);
	token.select(applet);

	read_cac_buffer(token, false, result);

	if (result[0] != 0) {
		/* The certificate is compressed */
		result = CompressionTool::zlib_decompress(result.begin() + 1, result.end());
	} else {
		/* Remove marker byte */
		result.erase(result.begin());
	}

	data.Data = &result[0];
	data.Length = result.size();
	token.cacheObject(0, description.c_str(), data);
	return result;
}

byte_string CACNGCacIDObject::crypt(const byte_string &input)
{
	byte_string result;
	if (input.size() > keySize / 8)
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);
	
	//if (sign != mSignOnly)
	//	CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);
	
	PCSC::Transaction _(token);
	token.select(applet);

	byte_string apdu;

	size_t resultLength = keySize / 8 + 2;
	result.resize(resultLength);
	const size_t CHUNK_SIZE = 128;

	for (unsigned i = 0; i < input.size(); i += CHUNK_SIZE)
	{
		const uint8_t next_chunk = min(input.size() - i, CHUNK_SIZE);
		apdu.resize(5 + next_chunk);
		apdu[0] = 0x80;
		apdu[1] = 0x42;
		apdu[2] = ((input.size() - i) > CHUNK_SIZE) ? 0x80 : 0x00;
		apdu[3] = 0x00;
		apdu[4] = next_chunk;
		memcpy(&apdu[5], &input[i], next_chunk);
		resultLength = result.size();
		CACNGError::check(token.exchangeAPDU(&apdu[0], next_chunk + 5, &result[0],
												resultLength));
	}
	if (resultLength != keySize / 8 + 2) {
		secdebug("cac", " %s: computeCrypt: expected size: %ld, got: %ld",
				 description.c_str(), keySize / 8 + 2, resultLength);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}
	/* Trim off status bytes */
	result.resize(resultLength - 2);
	return result;
}

CACNGPivApplet::CACNGPivApplet(CACNGToken &token, const byte_string &applet)
:token(token), applet(applet)
{
}

void CACNGPivApplet::select()
{
	byte_string result;
	uint32_t code = token.exchangeAPDU(applet, result);
	CACNGError::check(code);
}

CACNGPivIDObject::CACNGPivIDObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, const std::string &description, const byte_string &oid, uint8_t keyRef)
:CACNGIDObject(token, applet, description), oid(oid), keyRef(keyRef)
{
}

static void read_piv_object(CACNGToken &token, const byte_string &oid, byte_string &result)
{
	TLV oidValue(0x5C, oid);
	byte_string tagged_oid = oidValue.encode();
	static const unsigned char INITIAL_APDU[] = { PIV_GETDATA_APDU };
	/* TODO: Build from ground-up */
	byte_string initialApdu;
	initialApdu.reserve(sizeof(INITIAL_APDU) + 1 + tagged_oid.size());
	initialApdu.insert(initialApdu.begin(), INITIAL_APDU, INITIAL_APDU + sizeof(INITIAL_APDU));
	initialApdu.push_back((uint8_t)tagged_oid.size());
	initialApdu += tagged_oid;
	
	static const unsigned char CONTINUATION_APDU[] = { PIV_GETDATA_CONT_APDU, 0x00 /* LENGTH LOCATION */ };
	byte_string continuationApdu(CONTINUATION_APDU, CONTINUATION_APDU + sizeof(CONTINUATION_APDU));

	byte_string *apdu = &initialApdu;

	uint32_t rx;
	do
	{
		rx = token.exchangeAPDU(*apdu, result);
		secdebug("pivtokend", "exchangeAPDU result %02X", rx);
		
		if ((rx & 0xFF00) != SCARD_BYTES_LEFT_IN_SW2 &&
			(rx & 0xFF00) != SCARD_SUCCESS)
			CACNGError::check(rx);
		
		// Switch to the continuation APDU after first exchange
		apdu = &continuationApdu;
		
		// Number of bytes to fetch next time around is in the last byte returned.
		// For all except the penultimate read, this is 0, indicating that the
		// token should read all bytes.
		apdu->back() = static_cast<unsigned char>(rx & 0xFF);
	} while ((rx & 0xFF00) == SCARD_BYTES_LEFT_IN_SW2);

	// Start to parse the BER-TLV encoded data. In the end, we only return the
	// main data part of this but we need to step through the rest first
	// The certficates are the only types we parse here
	if (result.size()<=0)
		return;
	if (result[0] != PIV_GETDATA_RESPONSE_TAG)
		CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	
}

byte_string CACNGPivIDObject::read()
{
	byte_string result;
	PCSC::Transaction _(token);
	token.select(applet);

	read_piv_object(token, oid, result);
	/* Decode/decompress the certificate */
	bool hasCertificateData = false;
	bool isCompressed = false;
	
	// 00000000  53 82 04 84 70 82 04 78  78 da 33 68 62 db 61 d0 
	TLV_ref tlv;
	TLVList list;
	try {
		tlv = TLV::parse(result);
		list = tlv->getInnerValues();
	} catch(...) {
		CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	}

	for(TLVList::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
		const byte_string &tagString = (*iter)->getTag();
		const byte_string &value = (*iter)->getValue();
		if(tagString.size() != 1)
			CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
		uint8_t tag = tagString[0];
		switch (tag) {
		case PIV_GETDATA_TAG_CERTIFICATE:			// 0x70
			result = value;
			hasCertificateData = true;
			break;
		case PIV_GETDATA_TAG_CERTINFO:				// 0x71
			if(value.size() != 1)
				CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
			secdebug("pivtokend", "CertInfo byte: %02X", value[0]);
			isCompressed = value[0] & PIV_GETDATA_COMPRESSION_MASK;
			break;
		case PIV_GETDATA_TAG_MSCUID:				// 0x72 -- should be of length 3...
			break;
		case PIV_GETDATA_TAG_ERRORDETECTION:
			break;
		case 0:
		case 0xFF:
			break;
		default:
			CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
			break;
		}
	}
	
	/* No cert data ? */
	if(!hasCertificateData)
		CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	if (isCompressed) {
		return CompressionTool::zlib_decompress(result);
	}
	
	return result;
}

byte_string CACNGPivIDObject::crypt(const byte_string &input)
{
	byte_string result;
	/* Allow all key usage, certificates determine validity */
	unsigned char algRef;
	switch (keySize) {
	case 1024:
		algRef = 0x06;
		break;
	case 2048:
		algRef = 0x07;
		break;
	default:
		/* Cannot use a key ~= 1024 or 2048 bits yet */
		CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);
		break;
	}

	/* Build the BER-Encoded message */
	/* Template: 0x7C L { 0x82 0x00, 0x81 L data } .. 2 tag+lengths + 1 tag-0 */
	TLVList commandList;
	commandList.push_back(TLV_ref(new TLV(0x82)));
	commandList.push_back(TLV_ref(new TLV(0x81, input)));
	TLV_ref command = TLV_ref(new TLV(0x7C, commandList));

	/* TODO: Evaluate result length handling */
	/* At least enough to contain BER-TLV */
	size_t resultLength = keySize / 8;
	resultLength += 1 + TLV::encodedLength(resultLength); // RESPONSE
	resultLength += 1 + 1; // Potential empty response-tlv
	resultLength += 1 + TLV::encodedLength(resultLength); // TLV containing response
	/* Round out resultLength to a multiple of 256 */
	resultLength = resultLength + resultLength % 256 + 256;
	// Ensure that there's enough space to prevent unnecessary resizing
	result.reserve(resultLength);

	byte_string commandString = command->encode();

	PCSC::Transaction _(token);
	token.select(applet);

	CACNGError::check(token.exchangeChainedAPDU(0x00, 0x87, algRef, keyRef, commandString, result));

	/* DECODE 0x7C */
	TLV_ref tlv;
	try {
		tlv = TLV::parse(result);
	} catch(...) {
		secure_zero(result);
		CACNGError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	}
	secure_zero(result);
	if(tlv->getTag() != (unsigned char*)"\x7C") {
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}
	byte_string tagData;
	try {
		TLVList list = tlv->getInnerValues();
		TLVList::const_iterator iter = find_if(list.begin(), list.end(), TagPredicate(0x82));
		if(iter != list.end())
			tagData = (*iter)->getValue();
	} catch(...) {
	}
	if(tagData.size() == 0) {
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}
	
	if(tagData.size() != keySize / 8) { // Not enough data at all..
		secure_zero(tagData);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}
	
	result.swap(tagData);
	/* zero-out tagData */
	secure_zero(tagData);
	
	return result;
}

CACNGCacBufferObject::CACNGCacBufferObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, bool isTbuffer)
:token(token), applet(applet), isTbuffer(isTbuffer)
{
}

byte_string CACNGCacBufferObject::read()
{
	byte_string result;
	
	PCSC::Transaction _(token);
	token.select(applet);
	read_cac_buffer(token, isTbuffer, result);

	return result;
}
