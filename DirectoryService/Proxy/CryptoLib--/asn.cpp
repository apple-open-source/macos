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
// asn.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "asn.h"

#include <iomanip>
#include <time.h>

NAMESPACE_BEGIN(CryptoPP)
USING_NAMESPACE(std)

/// DER Length
unsigned int DERLengthEncode(BufferedTransformation &bt, unsigned int length)
{
	unsigned int i=0;
	if (length <= 0x7f)
	{
		bt.Put(byte(length));
		i++;
	}
	else
	{
		bt.Put(byte(BytePrecision(length) | 0x80));
		i++;
		for (int j=BytePrecision(length); j; --j)
		{
			bt.Put(byte(length >> (j-1)*8));
			i++;
		}
	}
	return i;
}

bool BERLengthDecode(BufferedTransformation &bt, unsigned int &length)
{
	byte b;

	if (!bt.Get(b))
		BERDecodeError();

	if (!(b & 0x80))
		length = b;
	else
	{
		unsigned int lengthBytes = b & 0x7f;

		if (lengthBytes == 0)
			return false;	// indefinite length

		length = 0;
		while (lengthBytes--)
		{
			if (length >> (8*(sizeof(length)-1)))
				BERDecodeError();	// length about to overflow

			if (!bt.Get(b))
				BERDecodeError();

			length = (length << 8) | b;
		}
	}
	return true;
}

void DEREncodeNull(BufferedTransformation &out)
{
	out.Put(TAG_NULL);
	out.Put(0);
}

void BERDecodeNull(BufferedTransformation &in)
{
	byte b;
	if (!in.Get(b) || b != TAG_NULL)
		BERDecodeError();
	unsigned int length;
	if (!BERLengthDecode(in, length) || length != 0)
		BERDecodeError();
}

/// ASN Strings
unsigned int DEREncodeOctetString(BufferedTransformation &bt, const byte *str, unsigned int strLen)
{
	bt.Put(OCTET_STRING);
	unsigned int lengthBytes = DERLengthEncode(bt, strLen);
	bt.Put(str, strLen);
	return 1+lengthBytes+strLen;
}

unsigned int DEREncodeOctetString(BufferedTransformation &bt, const SecByteBlock &str)
{
	return DEREncodeOctetString(bt, str.ptr, str.size);
}

unsigned int BERDecodeOctetString(BufferedTransformation &bt, SecByteBlock &str)
{
	byte b;
	if (!bt.Get(b) || b != OCTET_STRING)
		BERDecodeError();

	unsigned int bc;
	if (!BERLengthDecode(bt, bc))
		BERDecodeError();

	str.Resize(bc);
	if (bc != bt.Get(str, bc))
		BERDecodeError();
	return bc;
}

unsigned int BERDecodeOctetString(BufferedTransformation &bt, BufferedTransformation &str)
{
	byte b;
	if (!bt.Get(b) || b != OCTET_STRING)
		BERDecodeError();

	unsigned int bc;
	if (!BERLengthDecode(bt, bc))
		BERDecodeError();

	bt.TransferTo(str, bc);
	return bc;
}

unsigned int DEREncodeTextString(BufferedTransformation &bt, const std::string &str, byte asnTag)
{
	bt.Put(asnTag);
	unsigned int lengthBytes = DERLengthEncode(bt, str.size());
	bt.Put((const byte *)str.data(), str.size());
	return 1+lengthBytes+str.size();
}

unsigned int BERDecodeTextString(BufferedTransformation &bt, std::string &str, byte asnTag)
{
	byte b;
	if (!bt.Get(b) || b != asnTag)
		BERDecodeError();

	unsigned int bc;
	if (!BERLengthDecode(bt, bc))
		BERDecodeError();

	SecByteBlock temp(bc);
	if (bc != bt.Get(temp, bc))
		BERDecodeError();
	str.assign((char *)temp.ptr, bc);
	return bc;
}

/// ASN BitString
unsigned int DEREncodeBitString(BufferedTransformation &bt, const byte *str, unsigned int strLen, unsigned int unusedBits)
{
	bt.Put(BIT_STRING);
	unsigned int lengthBytes = DERLengthEncode(bt, strLen+1);
	bt.Put((byte)unusedBits);
	bt.Put(str, strLen);
	return 1+lengthBytes+strLen;
}

unsigned int BERDecodeBitString(BufferedTransformation &bt, SecByteBlock &str, unsigned int &unusedBits)
{
	byte b;
	if (!bt.Get(b) || b != BIT_STRING)
		BERDecodeError();

	unsigned int bc;
	if (!BERLengthDecode(bt, bc))
		BERDecodeError();

	byte unused;
	if (!bt.Get(unused))
		BERDecodeError();
	unusedBits = unused;
	str.Resize(bc-1);
	if ((bc-1) != bt.Get(str, bc-1))
		BERDecodeError();
	return bc-1;
}

void OID::EncodeValue(BufferedTransformation &bt, unsigned long v)
{
	for (unsigned int i=RoundUpToMultipleOf(STDMAX(7U,BitPrecision(v)), 7)-7; i != 0; i-=7)
		bt.Put(0x80 | ((v >> i) & 0x7f));
	bt.Put(v & 0x7f);
}

unsigned int OID::DecodeValue(BufferedTransformation &bt, unsigned long &v)
{
	byte b;
	unsigned int i=0;
	v = 0;
	while (true)
	{
		if (!bt.Get(b))
			BERDecodeError();
		i++;
		v <<= 7;
		v += b & 0x7f;
		if (!(b & 0x80))
			return i;
	}
}

void OID::DEREncode(BufferedTransformation &bt) const
{
	assert(m_values.size() >= 2);
	ByteQueue temp;
	temp.Put(byte(m_values[0] * 40 + m_values[1]));
	for (unsigned int i=2; i<m_values.size(); i++)
		EncodeValue(temp, m_values[i]);
	bt.Put(OBJECT_IDENTIFIER);
	DERLengthEncode(bt, temp.CurrentSize());
	temp.TransferTo(bt);
}

void OID::BERDecode(BufferedTransformation &bt)
{
	byte b;
	if (!bt.Get(b) || b != OBJECT_IDENTIFIER)
		BERDecodeError();

	unsigned int length;
	if (!BERLengthDecode(bt, length) || length < 1)
		BERDecodeError();

	if (!bt.Get(b))
		BERDecodeError();
	
	length--;
	m_values.resize(2);
	m_values[0] = b / 40;
	m_values[1] = b % 40;

	while (length > 0)
	{
		unsigned long v;
		unsigned int valueLen = DecodeValue(bt, v);
		if (valueLen > length)
			BERDecodeError();
		m_values.push_back(v);
		length -= valueLen;
	}
}

void OID::BERDecodeAndCheck(BufferedTransformation &bt) const
{
	OID oid(bt);
	if (*this != oid)
		BERDecodeError();
}

BERGeneralDecoder::BERGeneralDecoder(BufferedTransformation &inQueue, byte asnTag)
	: m_inQueue(inQueue), m_finished(false)
{
	byte b;
	if (!m_inQueue.Get(b) || b != asnTag)
		BERDecodeError();

	m_definiteLength = BERLengthDecode(m_inQueue, m_length);
}

BERGeneralDecoder::BERGeneralDecoder(BERGeneralDecoder &inQueue, byte asnTag)
	: m_inQueue(inQueue), m_finished(false)
{
	byte b;
	if (!m_inQueue.Get(b) || b != asnTag)
		BERDecodeError();

	m_definiteLength = BERLengthDecode(m_inQueue, m_length);
	if (!m_definiteLength && !(asnTag | CONSTRUCTED))
		BERDecodeError();	// cannot be primitive have indefinite length
}

BERGeneralDecoder::~BERGeneralDecoder()
{
	try	// avoid throwing in constructor
	{
		if (!m_finished)
			MessageEnd();
	}
	catch (...)
	{
	}
}

bool BERGeneralDecoder::EndReached() const
{
	if (m_definiteLength)
		return m_length == 0;
	else
	{	// check end-of-content octets
		word16 i;
		return (m_inQueue.PeekWord16(i)==2 && i==0);
	}
}

byte BERGeneralDecoder::PeekByte() const
{
	byte b;
	if (!Peek(b))
		BERDecodeError();
	return b;
}

void BERGeneralDecoder::CheckByte(byte check)
{
	byte b;
	if (!Get(b) || b != check)
		BERDecodeError();
}

void BERGeneralDecoder::MessageEnd(int)
{
	m_finished = true;
	if (m_definiteLength)
	{
		if (m_length != 0)
			BERDecodeError();
	}
	else
	{	// remove end-of-content octets
		word16 i;
		if (m_inQueue.GetWord16(i) != 2 || i != 0)
			BERDecodeError();
	}
}

unsigned long BERGeneralDecoder::TransferTo(BufferedTransformation &target, unsigned long transferMax)
{
	return ReduceLength(m_inQueue.TransferTo(target, m_definiteLength ? STDMIN(transferMax, (unsigned long)m_length) : transferMax));
}

unsigned long BERGeneralDecoder::CopyTo(BufferedTransformation &target, unsigned long copyMax) const
{
	return m_inQueue.CopyTo(target, m_definiteLength ? STDMIN(copyMax, (unsigned long)m_length) : copyMax);
}

unsigned int BERGeneralDecoder::ReduceLength(unsigned int delta)
{
	if (m_definiteLength)
	{
		if (m_length < delta)
			BERDecodeError();
		m_length -= delta;
	}
	return delta;
}

DERGeneralEncoder::DERGeneralEncoder(BufferedTransformation &outQueue, byte asnTag)
	: m_outQueue(outQueue), m_finished(false), m_asnTag(asnTag)
{
}

DERGeneralEncoder::DERGeneralEncoder(DERGeneralEncoder &outQueue, byte asnTag)
	: m_outQueue(outQueue), m_finished(false), m_asnTag(asnTag)
{
}

DERGeneralEncoder::~DERGeneralEncoder()
{
	try	// avoid throwing in constructor
	{
		if (!m_finished)
			MessageEnd();
	}
	catch (...)
	{
	}
}

void DERGeneralEncoder::MessageEnd(int)
{
	m_finished = true;
	unsigned int length = (unsigned int)CurrentSize();
	m_outQueue.Put(m_asnTag);
	DERLengthEncode(m_outQueue, length);
	TransferTo(m_outQueue);
}

NAMESPACE_END
