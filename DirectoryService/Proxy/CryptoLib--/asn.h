/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#ifndef CRYPTOPP_ASN_H
#define CRYPTOPP_ASN_H

#include "filters.h"
#include "queue.h"
#include <vector>

NAMESPACE_BEGIN(CryptoPP)

// these tags and flags are not complete
enum ASNTag
{
	BOOLEAN 			= 0x01,
	INTEGER 			= 0x02,
	BIT_STRING			= 0x03,
	OCTET_STRING		= 0x04,
	TAG_NULL			= 0x05,
	OBJECT_IDENTIFIER	= 0x06,
	OBJECT_DESCRIPTOR	= 0x07,
	EXTERNAL			= 0x08,
	REAL				= 0x09,
	ENUMERATED			= 0x0a,
	UTF8_STRING			= 0x0c,
	SEQUENCE			= 0x10,
	SET 				= 0x11,
	NUMERIC_STRING		= 0x12,
	PRINTABLE_STRING 	= 0x13,
	T61_STRING			= 0x14,
	VIDEOTEXT_STRING 	= 0x15,
	IA5_STRING			= 0x16,
	UTC_TIME 			= 0x17,
	GENERALIZED_TIME 	= 0x18,
	GRAPHIC_STRING		= 0x19,
	VISIBLE_STRING		= 0x1a,
	GENERAL_STRING		= 0x1b
};

enum ASNIdFlag
{
	UNIVERSAL			= 0x00,
	DATA				= 0x01,
	HEADER				= 0x02,
	CONSTRUCTED 		= 0x20,
	APPLICATION 		= 0x40,
	CONTEXT_SPECIFIC	= 0x80,
	PRIVATE 			= 0xc0
};

#define BERDecodeError() throw BERDecodeErr()

//! BER Decoder Exception Class  
class BERDecodeErr : public Exception
{
public: 
	BERDecodeErr() : Exception("BER decode error") {}
	BERDecodeErr(const char *err) : Exception(err) {}
};

class UnknownOID : public BERDecodeErr
{
public:
	UnknownOID() : BERDecodeErr("BER decode error: unknown object identifier") {}
	UnknownOID(const char *err) : BERDecodeErr(err) {}
};

// unsigned int DERLengthEncode(unsigned int length, byte *output=0);
unsigned int DERLengthEncode(BufferedTransformation &out, unsigned int length);
// returns false if indefinite length
bool BERLengthDecode(BufferedTransformation &in, unsigned int &length);

void DEREncodeNull(BufferedTransformation &out);
void BERDecodeNull(BufferedTransformation &in);

unsigned int DEREncodeOctetString(BufferedTransformation &out, const byte *str, unsigned int strLen);
unsigned int DEREncodeOctetString(BufferedTransformation &out, const SecByteBlock &str);
unsigned int BERDecodeOctetString(BufferedTransformation &in, SecByteBlock &str);
unsigned int BERDecodeOctetString(BufferedTransformation &in, BufferedTransformation &str);

// for UTF8_STRING, PRINTABLE_STRING, and IA5_STRING
unsigned int DEREncodeTextString(BufferedTransformation &out, const std::string &str, byte asnTag);
unsigned int BERDecodeTextString(BufferedTransformation &in, std::string &str, byte asnTag);

unsigned int DEREncodeBitString(BufferedTransformation &out, const byte *str, unsigned int strLen, unsigned int unusedBits=0);
unsigned int BERDecodeBitString(BufferedTransformation &in, SecByteBlock &str, unsigned int &unusedBits);

//! Object Identifier
class OID
{
public:
	OID() {}
	OID(unsigned long v) : m_values(1, v) {}
	OID(BufferedTransformation &bt) {BERDecode(bt);}

	bool operator==(const OID &rhs) const {return m_values == rhs.m_values;}
	bool operator!=(const OID &rhs) const {return !operator==(rhs);}
	bool operator<(const OID &rhs) const {return std::lexicographical_compare(m_values.begin(), m_values.end(), rhs.m_values.begin(), rhs.m_values.end());}

	inline OID & operator+=(unsigned long rhs) {m_values.push_back(rhs); return *this;}
	inline OID operator+(unsigned long rhs) const {return OID(*this)+=rhs;}

	void DEREncode(BufferedTransformation &bt) const;
	void BERDecode(BufferedTransformation &bt);

	// throw BERDecodeErr() if decoded value doesn't equal this OID
	void BERDecodeAndCheck(BufferedTransformation &bt) const;

	std::vector<unsigned long> m_values;

private:
	static void EncodeValue(BufferedTransformation &bt, unsigned long v);
	static unsigned int DecodeValue(BufferedTransformation &bt, unsigned long &v);
};

//! BER General Decoder
class BERGeneralDecoder : public Store
{
public:
	explicit BERGeneralDecoder(BufferedTransformation &inQueue, byte asnTag);
	explicit BERGeneralDecoder(BERGeneralDecoder &inQueue, byte asnTag);
	~BERGeneralDecoder();

	bool IsDefiniteLength() const {return m_definiteLength;}
	unsigned int RemainingLength() const {assert(m_definiteLength); return m_length;}
	bool EndReached() const;
	byte PeekByte() const;
	void CheckByte(byte b);

	unsigned long TransferTo(BufferedTransformation &target, unsigned long transferMax);
	unsigned long CopyTo(BufferedTransformation &target, unsigned long copyMax) const;

	// call this to denote end of sequence
	void MessageEnd(int=-1);

protected:
	BufferedTransformation &m_inQueue;
	bool m_finished, m_definiteLength;
	unsigned int m_length;

private:
	unsigned int ReduceLength(unsigned int delta);
};

//! DER General Encoder
class DERGeneralEncoder : public ByteQueue
{
public:
	explicit DERGeneralEncoder(BufferedTransformation &outQueue, byte asnTag = SEQUENCE | CONSTRUCTED);
	explicit DERGeneralEncoder(DERGeneralEncoder &outQueue, byte asnTag = SEQUENCE | CONSTRUCTED);
	~DERGeneralEncoder();

	// call this to denote end of sequence
	void MessageEnd(int=-1);

private:
	BufferedTransformation &m_outQueue;
	bool m_finished;

	byte m_asnTag;
};

//! BER Sequence Decoder
class BERSequenceDecoder : public BERGeneralDecoder
{
public:
	explicit BERSequenceDecoder(BufferedTransformation &inQueue, byte asnTag = SEQUENCE | CONSTRUCTED)
		: BERGeneralDecoder(inQueue, asnTag) {}
	explicit BERSequenceDecoder(BERSequenceDecoder &inQueue, byte asnTag = SEQUENCE | CONSTRUCTED)
		: BERGeneralDecoder(inQueue, asnTag) {}
};

//! DER Sequence Encoder
class DERSequenceEncoder : public DERGeneralEncoder
{
public:
	explicit DERSequenceEncoder(BufferedTransformation &outQueue, byte asnTag = SEQUENCE | CONSTRUCTED)
		: DERGeneralEncoder(outQueue, asnTag) {}
	explicit DERSequenceEncoder(DERSequenceEncoder &outQueue, byte asnTag = SEQUENCE | CONSTRUCTED)
		: DERGeneralEncoder(outQueue, asnTag) {}
};

//! BER Set Decoder
class BERSetDecoder : public BERGeneralDecoder
{
public:
	explicit BERSetDecoder(BufferedTransformation &inQueue, byte asnTag = SET | CONSTRUCTED)
		: BERGeneralDecoder(inQueue, asnTag) {}
	explicit BERSetDecoder(BERSetDecoder &inQueue, byte asnTag = SET | CONSTRUCTED)
		: BERGeneralDecoder(inQueue, asnTag) {}
};

//! DER Set Encoder
class DERSetEncoder : public DERGeneralEncoder
{
public:
	explicit DERSetEncoder(BufferedTransformation &outQueue, byte asnTag = SET | CONSTRUCTED)
		: DERGeneralEncoder(outQueue, asnTag) {}
	explicit DERSetEncoder(DERSetEncoder &outQueue, byte asnTag = SET | CONSTRUCTED)
		: DERGeneralEncoder(outQueue, asnTag) {}
};

// ********************************************************

//! DER Encode Unsigned
/*! for INTEGER, BOOLEAN, and ENUM */
template <class T>
unsigned int DEREncodeUnsigned(BufferedTransformation &out, T w, byte asnTag = INTEGER)
{
	byte buf[sizeof(w)+1];
	unsigned int bc;
	if (asnTag == BOOLEAN)
	{
		buf[sizeof(w)] = w ? 0xff : 0;
		bc = 1;
	}
	else
	{
		buf[0] = 0;
		for (unsigned int i=0; i<sizeof(w); i++)
			buf[i+1] = byte(w >> (sizeof(w)-1-i)*8);
		bc = sizeof(w);
		while (bc > 1 && buf[sizeof(w)+1-bc] == 0)
			--bc;
		if (buf[sizeof(w)+1-bc] & 0x80)
			++bc;
	}
	out.Put(asnTag);
	unsigned int lengthBytes = DERLengthEncode(out, bc);
	out.Put(buf+sizeof(w)+1-bc, bc);
	return 1+lengthBytes+bc;
}

//! BER Decode Unsigned
// VC60 workaround: std::numeric_limits<T>::max conflicts with MFC max macro
// CW41 workaround: std::numeric_limits<T>::max causes a template error
template <class T>
void BERDecodeUnsigned(BufferedTransformation &in, T &w, byte asnTag = INTEGER,
					   T minValue = 0, T maxValue = 0xffffffff)
{
	byte b;
	if (!in.Get(b) || b != asnTag)
		BERDecodeError();

	unsigned int bc;
	BERLengthDecode(in, bc);

	SecByteBlock buf(bc);

	if (bc != in.Get(buf, bc))
		BERDecodeError();

	const byte *ptr = buf;
	while (bc > sizeof(w) && *ptr == 0)
	{
		bc--;
		ptr++;
	}
	if (bc > sizeof(w))
		BERDecodeError();

	w = 0;
	for (unsigned int i=0; i<bc; i++)
		w = (w << 8) | ptr[i];

	if (w < minValue || w > maxValue)
		BERDecodeError();
}

NAMESPACE_END

#endif
