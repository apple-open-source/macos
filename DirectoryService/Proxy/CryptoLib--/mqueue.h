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
#ifndef CRYPTOPP_MQUEUE_H
#define CRYPTOPP_MQUEUE_H

#include "queue.h"
#include "filters.h"
#include <deque>

NAMESPACE_BEGIN(CryptoPP)

//! Message Queue
class MessageQueue : public BufferedTransformationWithAutoSignal
{
public:
	MessageQueue(unsigned int nodeSize=256);

	void Put(byte inByte)
		{m_queue.Put(inByte); m_lengths.back()++;}
	void Put(const byte *inString, unsigned int length)
		{m_queue.Put(inString, length); m_lengths.back()+=length;}

	unsigned long MaxRetrievable() const
		{return m_lengths.front();}
	bool AnyRetrievable() const
		{return m_lengths.front() > 0;}

	unsigned long TransferTo(BufferedTransformation &target, unsigned long transferMax=ULONG_MAX)
		{return Got(m_queue.TransferTo(target, STDMIN(MaxRetrievable(), transferMax)));}
	unsigned long CopyTo(BufferedTransformation &target, unsigned long copyMax=ULONG_MAX) const
		{return m_queue.CopyTo(target, STDMIN(MaxRetrievable(), copyMax));}

	void MessageEnd(int=-1)
		{m_lengths.push_back(0);}

	unsigned long TotalBytesRetrievable() const
		{return m_queue.MaxRetrievable();}
	unsigned int NumberOfMessages() const
		{return m_lengths.size()-1;}
	bool GetNextMessage();

	unsigned int CopyMessagesTo(BufferedTransformation &target, unsigned int count=UINT_MAX) const;

	void swap(MessageQueue &rhs);

private:
	unsigned long Got(unsigned long length)
		{assert(m_lengths.front() >= length); m_lengths.front() -= length; return length;}

	ByteQueue m_queue;
	std::deque<unsigned long> m_lengths;
};

NAMESPACE_END

NAMESPACE_BEGIN(std)
template<> inline void swap(CryptoPP::MessageQueue &a, CryptoPP::MessageQueue &b)
{
	a.swap(b);
}
NAMESPACE_END

#endif
