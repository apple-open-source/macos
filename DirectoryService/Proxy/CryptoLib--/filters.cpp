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
// filters.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "filters.h"
#include "mqueue.h"
#include <memory>

NAMESPACE_BEGIN(CryptoPP)

BitBucket g_bitBucket;

Filter::Filter(BufferedTransformation *outQ)
	: m_outQueue(outQ ? outQ : new MessageQueue)
{
}

void Filter::Detach(BufferedTransformation *newOut)
{
	m_outQueue.reset(newOut ? newOut : new MessageQueue);
	NotifyAttachmentChange();
}

void Filter::Insert(Filter *filter)
{
	filter->m_outQueue.reset(m_outQueue.release());
	m_outQueue.reset(filter);
	NotifyAttachmentChange();
}

// *************************************************************

void MeterFilter::Put(byte inByte)
{
	m_currentMessageBytes++;
	m_totalBytes++;
	if (m_transparent)
		AttachedTransformation()->Put(inByte);
}

void MeterFilter::Put(const byte *inString, unsigned int length)
{
	m_currentMessageBytes += length;
	m_totalBytes += length;
	if (m_transparent)
		AttachedTransformation()->Put(inString, length);
}

void MeterFilter::MessageEnd(int propagation)
{
	m_currentMessageBytes = 0;
	m_currentSeriesMessages++;
	m_totalMessages++;
	if (m_transparent)
		Filter::MessageEnd(propagation);
}

void MeterFilter::MessageSeriesEnd(int propagation)
{
	m_currentMessageBytes = 0;
	m_currentSeriesMessages = 0;
	m_totalMessageSeries++;
	if (m_transparent)
		Filter::MessageSeriesEnd(propagation);
}

// *************************************************************

FilterWithBufferedInput::BlockQueue::BlockQueue(unsigned int blockSize, unsigned int maxBlocks)
	: m_buffer(blockSize * maxBlocks)
{
	ResetQueue(blockSize, maxBlocks);
}

void FilterWithBufferedInput::BlockQueue::ResetQueue(unsigned int blockSize, unsigned int maxBlocks)
{
	m_buffer.Resize(blockSize * maxBlocks);
	m_blockSize = blockSize;
	m_maxBlocks = maxBlocks;
	m_size = 0;
	m_begin = m_buffer;
}

const byte *FilterWithBufferedInput::BlockQueue::GetBlock()
{
	if (m_size >= m_blockSize)
	{
		const byte *ptr = m_begin;
		if ((m_begin+=m_blockSize) == m_buffer.End())
			m_begin = m_buffer;
		m_size -= m_blockSize;
		return ptr;
	}
	else
		return NULL;
}

const byte *FilterWithBufferedInput::BlockQueue::GetContigousBlocks(unsigned int &numberOfBlocks)
{
	numberOfBlocks = STDMIN(numberOfBlocks, STDMIN((unsigned int)(m_buffer.End()-m_begin), m_size)/m_blockSize);
	const byte *ptr = m_begin;
	if ((m_begin+=m_blockSize*numberOfBlocks) == m_buffer.End())
		m_begin = m_buffer;
	m_size -= m_blockSize*numberOfBlocks;
	return ptr;
}

unsigned int FilterWithBufferedInput::BlockQueue::GetAll(byte *outString)
{
	unsigned int size = m_size;
	unsigned int numberOfBlocks = m_maxBlocks;
	const byte *ptr = GetContigousBlocks(numberOfBlocks);
	memcpy(outString, ptr, numberOfBlocks*m_blockSize);
	memcpy(outString+numberOfBlocks*m_blockSize, m_begin, m_size);
	m_size = 0;
	return size;
}

void FilterWithBufferedInput::BlockQueue::Put(const byte *inString, unsigned int length)
{
	assert(m_size + length <= m_buffer.size);
	byte *end = (m_size < m_buffer+m_buffer.size-m_begin) ? m_begin + m_size : m_begin + m_size - m_buffer.size;
	unsigned int len = STDMIN(length, (unsigned int)(m_buffer+m_buffer.size-end));
	memcpy(end, inString, len);
	if (len < length)
		memcpy(m_buffer, inString+len, length-len);
	m_size += length;
}

FilterWithBufferedInput::FilterWithBufferedInput(unsigned int firstSize, unsigned int blockSize, unsigned int lastSize, BufferedTransformation *outQ)
	: Filter(outQ), m_firstSize(firstSize), m_blockSize(blockSize), m_lastSize(lastSize)
	, m_firstInputDone(false)
	, m_queue(1, m_firstSize)
{
}

void FilterWithBufferedInput::Put(byte inByte)
{
	Put(&inByte, 1);
}

void FilterWithBufferedInput::Put(const byte *inString, unsigned int length)
{
	if (length == 0)
		return;

	unsigned int newLength = m_queue.CurrentSize() + length;

	if (!m_firstInputDone && newLength >= m_firstSize)
	{
		unsigned int len = m_firstSize - m_queue.CurrentSize();
		m_queue.Put(inString, len);
		FirstPut(m_queue.GetContigousBlocks(m_firstSize));
		assert(m_queue.CurrentSize() == 0);
		m_queue.ResetQueue(m_blockSize, (2*m_blockSize+m_lastSize-2)/m_blockSize);

		inString += len;
		newLength -= m_firstSize;
		m_firstInputDone = true;
	}

	if (m_firstInputDone)
	{
		if (m_blockSize == 1)
		{
			while (newLength > m_lastSize && m_queue.CurrentSize() > 0)
			{
				unsigned int len = newLength - m_lastSize;
				const byte *ptr = m_queue.GetContigousBlocks(len);
				NextPut(ptr, len);
				newLength -= len;
			}

			if (newLength > m_lastSize)
			{
				unsigned int len = newLength - m_lastSize;
				NextPut(inString, len);
				inString += len;
				newLength -= len;
			}
		}
		else
		{
			while (newLength >= m_blockSize + m_lastSize && m_queue.CurrentSize() >= m_blockSize)
			{
				NextPut(m_queue.GetBlock(), m_blockSize);
				newLength -= m_blockSize;
			}

			if (newLength >= m_blockSize + m_lastSize && m_queue.CurrentSize() > 0)
			{
				assert(m_queue.CurrentSize() < m_blockSize);
				unsigned int len = m_blockSize - m_queue.CurrentSize();
				m_queue.Put(inString, len);
				inString += len;
				NextPut(m_queue.GetBlock(), m_blockSize);
				newLength -= m_blockSize;
			}

			while (newLength >= m_blockSize + m_lastSize)
			{
				NextPut(inString, m_blockSize);
				inString += m_blockSize;
				newLength -= m_blockSize;
			}
		}
	}

	m_queue.Put(inString, newLength - m_queue.CurrentSize());
}

void FilterWithBufferedInput::MessageEnd(int propagation)
{
	if (!m_firstInputDone && m_firstSize==0)
		FirstPut(NULL);

	SecByteBlock temp(m_queue.CurrentSize());
	m_queue.GetAll(temp);
	LastPut(temp, temp.size);

	m_firstInputDone = false;
	m_queue.ResetQueue(1, m_firstSize);

	Filter::MessageEnd(propagation);
}

void FilterWithBufferedInput::ForceNextPut()
{
	if (m_firstInputDone && m_queue.CurrentSize() >= m_blockSize)
		NextPut(m_queue.GetBlock(), m_blockSize);
}

// *************************************************************



// *************************************************************

ProxyFilter::ProxyFilter(Filter *filter, unsigned int firstSize, unsigned int lastSize, BufferedTransformation *outQ)
	: FilterWithBufferedInput(firstSize, 1, lastSize, outQ), m_filter(filter), m_proxy(NULL)
{
	if (m_filter.get())
		m_filter->Attach(m_proxy = new OutputProxy(*this, false));
}

void ProxyFilter::Flush(bool completeFlush, int propagation)
{
	if (m_filter.get())
	{
		bool passSignal = m_proxy->GetPassSignal();
		m_proxy->SetPassSignal(false);
		m_filter->Flush(completeFlush, -1);
		m_proxy->SetPassSignal(passSignal);
	}
	Filter::Flush(completeFlush, propagation);
}

void ProxyFilter::SetFilter(Filter *filter)
{
	bool passSignal = m_proxy ? m_proxy->GetPassSignal() : false;
	m_filter.reset(filter);
	if (filter)
	{
		std::auto_ptr<OutputProxy> temp(m_proxy = new OutputProxy(*this, passSignal));
		m_filter->TransferAllTo(*m_proxy);
		m_filter->Attach(temp.release());
	}
	else
		m_proxy=NULL;
}

void ProxyFilter::NextPut(const byte *s, unsigned int len) 
{
	if (m_filter.get())
		m_filter->Put(s, len);
}

// *************************************************************

void StreamCipherFilter::Put(const byte *inString, unsigned int length)
{
	SecByteBlock temp(length);
	cipher.ProcessString(temp, inString, length);
	AttachedTransformation()->Put(temp, length);
}

void HashFilter::Put(byte inByte)
{
	m_hashModule.Update(&inByte, 1);
	if (m_putMessage)
		AttachedTransformation()->Put(inByte);
}

void HashFilter::Put(const byte *inString, unsigned int length)
{
	m_hashModule.Update(inString, length);
	if (m_putMessage)
		AttachedTransformation()->Put(inString, length);
}

void HashFilter::MessageEnd(int propagation)
{
	SecByteBlock buf(m_hashModule.DigestSize());
	m_hashModule.Final(buf);
	AttachedTransformation()->Put(buf, buf.size);
	Filter::MessageEnd(propagation);
}

// *************************************************************

HashVerifier::HashVerifier(HashModule &hm, BufferedTransformation *outQueue, word32 flags)
	: FilterWithBufferedInput(flags & HASH_AT_BEGIN ? hm.DigestSize() : 0, 1, flags & HASH_AT_BEGIN ? 0 : hm.DigestSize(), outQueue)
	, m_hashModule(hm), m_flags(flags)
	, m_expectedHash(flags & HASH_AT_BEGIN ? hm.DigestSize() : 0), m_verified(false)
{
}

void HashVerifier::FirstPut(const byte *inString)
{
	if (m_flags & HASH_AT_BEGIN)
	{
		memcpy(m_expectedHash, inString, m_expectedHash.size);
		if (m_flags & PUT_HASH)
			AttachedTransformation()->Put(inString, m_expectedHash.size);
	}
}

void HashVerifier::NextPut(const byte *inString, unsigned int length)
{
	m_hashModule.Update(inString, length);
	if (m_flags & PUT_MESSAGE)
		AttachedTransformation()->Put(inString, length);
}

void HashVerifier::LastPut(const byte *inString, unsigned int length)
{
	if (m_flags & HASH_AT_BEGIN)
	{
		assert(length == 0);
		m_verified = m_hashModule.Verify(m_expectedHash);
	}
	else
	{
		m_verified = (length==m_hashModule.DigestSize() && m_hashModule.Verify(inString));
		if (m_flags & PUT_HASH)
			AttachedTransformation()->Put(inString, length);
	}

	if (m_flags & PUT_RESULT)
		AttachedTransformation()->Put(m_verified);

	if ((m_flags & THROW_EXCEPTION) && !m_verified)
		throw HashVerificationFailed();
}

// *************************************************************

void SignerFilter::MessageEnd(int propagation)
{
	SecByteBlock buf(m_signer.SignatureLength());
	m_signer.Sign(m_rng, m_messageAccumulator.release(), buf);
	AttachedTransformation()->Put(buf, buf.size);
	Filter::MessageEnd(propagation);
	m_messageAccumulator.reset(m_signer.NewMessageAccumulator());
}

void VerifierFilter::PutSignature(const byte *sig)
{
	memcpy(m_signature.ptr, sig, m_signature.size);
}

void VerifierFilter::MessageEnd(int propagation)
{
	AttachedTransformation()->Put((byte)m_verifier.Verify(m_messageAccumulator.release(), m_signature));
	Filter::MessageEnd(propagation);
	m_messageAccumulator.reset(m_verifier.NewMessageAccumulator());
}

// *************************************************************

void Source::PumpAll()
{
	while (PumpMessages()) {}
	while (Pump()) {}
}

StringSource::StringSource(const char *string, bool pumpAll, BufferedTransformation *outQueue)
	: Source(outQueue), m_store(string)
{
	if (pumpAll)
		PumpAll();
}

StringSource::StringSource(const byte *string, unsigned int length, bool pumpAll, BufferedTransformation *outQueue)
	: Source(outQueue), m_store(string, length)
{
	if (pumpAll)
		PumpAll();
}

bool Store::GetNextMessage()
{
	if (!m_messageEnd && !AnyRetrievable())
	{
		m_messageEnd=true;
		return true;
	}
	else
		return false;
}

unsigned int Store::CopyMessagesTo(BufferedTransformation &target, unsigned int count) const
{
	if (m_messageEnd || count == 0)
		return 0;
	else
	{
		CopyTo(target);
		if (GetAutoSignalPropagation())
			target.MessageEnd(GetAutoSignalPropagation()-1);
		return 1;
	}
}

unsigned long StringStore::TransferTo(BufferedTransformation &target, unsigned long transferMax)
{
	unsigned long result = CopyTo(target, transferMax);
	m_count += result;
	return result;
}

unsigned long StringStore::CopyTo(BufferedTransformation &target, unsigned long copyMax) const
{
	unsigned int len = (unsigned int)STDMIN((unsigned long)(m_length-m_count), copyMax);
	target.Put(m_store+m_count, len);
	return len;
}

unsigned long RandomNumberStore::CopyTo(BufferedTransformation &target, unsigned long copyMax) const
{
	unsigned int len = (unsigned int)STDMIN((unsigned long)(m_length-m_count), copyMax);
	for (unsigned int i=0; i<len; i++)
		target.Put(m_rng.GenerateByte());
	return len;
}

unsigned long RandomNumberStore::TransferTo(BufferedTransformation &target, unsigned long transferMax)
{
	unsigned long len = RandomNumberStore::CopyTo(target, transferMax);
	m_count += len;
	return len;
}

unsigned long NullStore::CopyTo(BufferedTransformation &target, unsigned long copyMax) const
{
	static byte nullBytes[128];
	for (unsigned long i=0; i<copyMax; i+=STDMIN(copyMax-i, 128UL))
		target.Put(nullBytes, STDMIN(copyMax-i, 128UL));
	return copyMax;
}

unsigned long NullStore::TransferTo(BufferedTransformation &target, unsigned long transferMax)
{
	return NullStore::CopyTo(target, transferMax);
}

RandomNumberSource::RandomNumberSource(RandomNumberGenerator &rng, unsigned int length, bool pumpAll, BufferedTransformation *outQueue)
	: Source(outQueue), m_store(rng, length)
{
	if (pumpAll)
		PumpAll();
}

NAMESPACE_END
