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
// cryptlib.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "cryptopp.h"
#include "cryptopp_misc.h"
#include "filters.h"

#include <memory>

NAMESPACE_BEGIN(CryptoPP)

const std::string BufferedTransformation::NULL_CHANNEL;

unsigned int RandomNumberGenerator::GenerateBit()
{
	return Parity(GetByte());
}

void RandomNumberGenerator::GenerateBlock(byte *output, unsigned int size)
{
	while (size--)
		*output++ = GetByte();
}

word32 RandomNumberGenerator::GenerateWord32(word32 min, word32 max)
{
	word32 range = max-min;
	const int maxBytes = BytePrecision(range);
	const int maxBits = BitPrecision(range);

	word32 value;

	do
	{
		value = 0;
		for (int i=0; i<maxBytes; i++)
			value = (value << 8) | GetByte();

		value = Crop(value, maxBits);
	} while (value > range);

	return value+min;
}

void StreamCipher::ProcessString(byte *outString, const byte *inString, unsigned int length)
{
	while(length--)
		*outString++ = ProcessByte(*inString++);
}

void StreamCipher::ProcessString(byte *inoutString, unsigned int length)
{
	while(length--)
		*inoutString++ = ProcessByte(*inoutString);
}

bool HashModule::Verify(const byte *digestIn)
{
	SecByteBlock digest(DigestSize());
	Final(digest);
	return memcmp(digest, digestIn, DigestSize()) == 0;
}

bool HashModuleWithTruncation::TruncatedVerify(const byte *digestIn, unsigned int digestLength)
{
	assert(digestLength <= DigestSize());
	SecByteBlock digest(digestLength);
	TruncatedFinal(digest, digestLength);
	return memcmp(digest, digestIn, digestLength) == 0;
}

BufferedTransformation::Err::Err(ErrorType errorType, const std::string &s)
	: Exception(s), m_errorType(errorType)
{
	if (GetWhat() == "")
	{
		switch (errorType)
		{
		case CANNOT_FLUSH:
			SetWhat("BufferedTransformation: cannot flush buffer");
			break;
		case DATA_INTEGRITY_CHECK_FAILED:
			SetWhat("BufferedTransformation: data integrity check failed");
			break;
		case INVALID_DATA_FORMAT:
			SetWhat("BufferedTransformation: invalid data format");
			break;
		case OUTPUT_ERROR:
			SetWhat("BufferedTransformation: cannot write to output device");
			break;
		case OTHER_ERROR:
			SetWhat("BufferedTransformation: unknown error");
			break;
		default:
			assert(false);
			break;
		}
	}
}

void BufferedTransformation::Put(byte b)
{
	if (AttachedTransformation())
		AttachedTransformation()->Put(b);
}

void BufferedTransformation::Put(const byte *inString, unsigned int length)
{
	if (AttachedTransformation())
		AttachedTransformation()->Put(inString, length);
}

void BufferedTransformation::Flush(bool completeFlush, int propagation)
{
	if (AttachedTransformation() && propagation)
		AttachedTransformation()->Flush(completeFlush, propagation-1);
}

void BufferedTransformation::MessageEnd(int propagation)
{
	if (AttachedTransformation() && propagation)
		AttachedTransformation()->MessageEnd(propagation-1);
}

void BufferedTransformation::MessageSeriesEnd(int propagation)
{
	if (AttachedTransformation() && propagation)
		AttachedTransformation()->MessageSeriesEnd(propagation-1);
}

void BufferedTransformation::PutMessageEnd(const byte *inString, unsigned int length, int propagation)
{
	Put(inString, length);
	MessageEnd(propagation);
}

void BufferedTransformation::ChannelFlush(const std::string &channel, bool completeFlush, int propagation)
{
	if (channel.empty())
		Flush(completeFlush, propagation);
	else if (AttachedTransformation() && propagation)
		AttachedTransformation()->ChannelFlush(channel, completeFlush, propagation-1);
}

void BufferedTransformation::ChannelMessageEnd(const std::string &channel, int propagation)
{
	if (channel.empty())
		MessageEnd(propagation);
	else if (AttachedTransformation() && propagation)
		AttachedTransformation()->ChannelMessageEnd(channel, propagation-1);
}

void BufferedTransformation::ChannelMessageSeriesEnd(const std::string &channel, int propagation)
{
	if (channel.empty())
		MessageSeriesEnd(propagation);
	else if (AttachedTransformation() && propagation)
		AttachedTransformation()->ChannelMessageSeriesEnd(channel, propagation-1);
}

void BufferedTransformation::ChannelPutMessageEnd(const std::string &channel, const byte *inString, unsigned int length, int propagation)
{
	if (channel.empty())
		PutMessageEnd(inString, length, propagation);
	else
	{
		ChannelPut(channel, inString, length);
		ChannelMessageEnd(channel, propagation);
	}
}

unsigned long BufferedTransformation::MaxRetrievable() const
{
	if (AttachedTransformation())
		return AttachedTransformation()->MaxRetrievable();
	else
		return CopyTo(g_bitBucket);
}

bool BufferedTransformation::AnyRetrievable() const
{
	if (AttachedTransformation())
		return AttachedTransformation()->AnyRetrievable();
	else
	{
		byte b;
		return Peek(b) != 0;
	}
}

unsigned int BufferedTransformation::Get(byte &outByte)
{
	if (AttachedTransformation())
		return AttachedTransformation()->Get(outByte);
	else
		return Get(&outByte, 1);
}

unsigned int BufferedTransformation::Get(byte *outString, unsigned int getMax)
{
	if (AttachedTransformation())
		return AttachedTransformation()->Get(outString, getMax);
	else
	{
		ArraySink arraySink(outString, getMax);
		return TransferTo(arraySink, getMax);
	}
}

unsigned int BufferedTransformation::Peek(byte &outByte) const
{
	if (AttachedTransformation())
		return AttachedTransformation()->Peek(outByte);
	else
		return Peek(&outByte, 1);
}

unsigned int BufferedTransformation::Peek(byte *outString, unsigned int peekMax) const
{
	if (AttachedTransformation())
		return AttachedTransformation()->Peek(outString, peekMax);
	else
	{
		ArraySink arraySink(outString, peekMax);
		return CopyTo(arraySink, peekMax);
	}
}

unsigned long BufferedTransformation::Skip(unsigned long skipMax)
{
	if (AttachedTransformation())
		return AttachedTransformation()->Skip(skipMax);
	else
		return TransferTo(g_bitBucket, skipMax);
}

unsigned long BufferedTransformation::CopyTo(BufferedTransformation &target, unsigned long copyMax) const
{
	if (AttachedTransformation())
		return AttachedTransformation()->CopyTo(target, copyMax);
	else
		return 0;
}

unsigned long BufferedTransformation::TransferTo(BufferedTransformation &target, unsigned long size)
{
	if (AttachedTransformation())
		return AttachedTransformation()->TransferTo(target, size);
	else
		return 0;
}

unsigned long BufferedTransformation::TotalBytesRetrievable() const
{
	if (AttachedTransformation())
		return AttachedTransformation()->TotalBytesRetrievable();
	else
		return MaxRetrievable();
}

unsigned int BufferedTransformation::NumberOfMessages() const
{
	if (AttachedTransformation())
		return AttachedTransformation()->NumberOfMessages();
	else
		return CopyMessagesTo(g_bitBucket);
}

bool BufferedTransformation::AnyMessages() const
{
	if (AttachedTransformation())
		return AttachedTransformation()->NumberOfMessages();
	else
		return NumberOfMessages() != 0;
}

bool BufferedTransformation::GetNextMessage()
{
	if (AttachedTransformation())
		return AttachedTransformation()->GetNextMessage();
	else
		return false;
}

unsigned int BufferedTransformation::SkipMessages(unsigned int count)
{
	if (AttachedTransformation())
		return AttachedTransformation()->SkipMessages(count);
	else
		return TransferMessagesTo(g_bitBucket, count);
}

unsigned int BufferedTransformation::TransferMessagesTo(BufferedTransformation &target, unsigned int count)
{
	if (AttachedTransformation())
		return AttachedTransformation()->TransferMessagesTo(target, count);
	else
	{
		unsigned int i;
		for (i=0; i<count && AnyMessages(); i++)
		{
			while (TransferTo(target)) {}
			bool result = GetNextMessage();
			assert(result);
			target.MessageEnd(GetAutoSignalPropagation());
		}
		return i;
	}
}

unsigned int BufferedTransformation::CopyMessagesTo(BufferedTransformation &target, unsigned int count) const
{
	if (AttachedTransformation())
		return AttachedTransformation()->CopyMessagesTo(target, count);
	else
		return 0;
}

void BufferedTransformation::SkipAll()
{
	if (AttachedTransformation())
		AttachedTransformation()->SkipAll();
	else
	{
		while (SkipMessages()) {}
		while (Skip()) {}
	}
}

void BufferedTransformation::TransferAllTo(BufferedTransformation &target)
{
	if (AttachedTransformation())
		AttachedTransformation()->TransferAllTo(target);
	else
	{
		while (TransferMessagesTo(target)) {}
		while (TransferTo(target)) {}
	}
}

void BufferedTransformation::CopyAllTo(BufferedTransformation &target) const
{
	if (AttachedTransformation())
		AttachedTransformation()->CopyAllTo(target);
	else
	{
		CopyMessagesTo(target);
		CopyTo(target);
	}
}

void BufferedTransformation::SetRetrievalChannel(const std::string &channel)
{
	if (AttachedTransformation())
		AttachedTransformation()->SetRetrievalChannel(channel);
}

void BufferedTransformation::ChannelPut(const std::string &channel, byte inByte)
{
	if (channel.empty())
		Put(inByte);
}

void BufferedTransformation::ChannelPut(const std::string &channel, const byte *inString, unsigned int length)
{
	if (channel.empty())
		Put(inString, length);
}

void BufferedTransformation::ChannelPutWord16(const std::string &channel, word16 value, bool highFirst)
{
	if (highFirst)
	{
		ChannelPut(channel, value>>8);
		ChannelPut(channel, byte(value));
	}
	else
	{
		ChannelPut(channel, byte(value));
		ChannelPut(channel, value>>8);
	}
}

void BufferedTransformation::ChannelPutWord32(const std::string &channel, word32 value, bool highFirst)
{
	if (highFirst)
	{
		for (int i=0; i<4; i++)
			ChannelPut(channel, byte(value>>((3-i)*8)));
	}
	else
	{
		for (int i=0; i<4; i++)
			ChannelPut(channel, byte(value>>(i*8)));
	}
}

void BufferedTransformation::PutWord16(word16 value, bool highFirst)
{
	ChannelPutWord16(NULL_CHANNEL, value, highFirst);
}

void BufferedTransformation::PutWord32(word32 value, bool highFirst)
{
	ChannelPutWord32(NULL_CHANNEL, value, highFirst);
}

unsigned int BufferedTransformation::PeekWord16(word16 &value, bool highFirst)
{
	byte buf[2] = {0, 0};
	unsigned int len = Peek(buf, 2);

	if (highFirst)
		value = (buf[0] << 8) | buf[1];
	else
		value = (buf[1] << 8) | buf[0];

	return len;
}

unsigned int BufferedTransformation::PeekWord32(word32 &value, bool highFirst)
{
	byte buf[4] = {0, 0, 0, 0};
	unsigned int len = Peek(buf, 4);

	if (highFirst)
		value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf [3];
	else
		value = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf [0];

	return len;
}

unsigned int BufferedTransformation::GetWord16(word16 &value, bool highFirst)
{
	return Skip(PeekWord16(value, highFirst));
}

unsigned int BufferedTransformation::GetWord32(word32 &value, bool highFirst)
{
	return Skip(PeekWord32(value, highFirst));
}

void BufferedTransformation::Attach(BufferedTransformation *newOut)
{
	if (!Attachable())
		return;

	if (AttachedTransformation() && AttachedTransformation()->Attachable())
		AttachedTransformation()->Attach(newOut);
	else
		Detach(newOut);
}

unsigned int PK_FixedLengthCryptoSystem::MaxPlainTextLength(unsigned int cipherTextLength) const
{
	if (cipherTextLength == CipherTextLength())
		return MaxPlainTextLength();
	else
		return 0;
}

unsigned int PK_FixedLengthCryptoSystem::CipherTextLength(unsigned int plainTextLength) const
{
	if (plainTextLength <= MaxPlainTextLength())
		return CipherTextLength();
	else
		return 0;
}

unsigned int PK_FixedLengthDecryptor::Decrypt(const byte *cipherText, unsigned int cipherTextLength, byte *plainText)
{
	if (cipherTextLength != CipherTextLength())
		return 0;

	return Decrypt(cipherText, plainText);
}

void PK_Signer::SignMessage(RandomNumberGenerator &rng, const byte *message, unsigned int messageLen, byte *signature) const
{
	std::auto_ptr<HashModule> accumulator(NewMessageAccumulator());
	accumulator->Update(message, messageLen);
	Sign(rng, accumulator.release(), signature);
}

bool PK_Verifier::VerifyMessage(const byte *message, unsigned int messageLen, const byte *sig) const
{
	std::auto_ptr<HashModule> accumulator(NewMessageAccumulator());
	accumulator->Update(message, messageLen);
	return Verify(accumulator.release(), sig);
}

NAMESPACE_END
