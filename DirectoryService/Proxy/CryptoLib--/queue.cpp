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
// queue.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "queue.h"
#include "filters.h"

NAMESPACE_BEGIN(CryptoPP)

// this class for use by ByteQueue only
class ByteQueueNode
{
public:
	ByteQueueNode(unsigned int maxSize)
		: buf(maxSize)
	{
		m_head = m_tail = 0;
		next = 0;
	}

	inline unsigned int MaxSize() const {return buf.size;}

	inline unsigned int CurrentSize() const
	{
		return m_tail-m_head;
	}

	inline bool UsedUp() const
	{
		return (m_head==MaxSize());
	}

	inline void Clear()
	{
		m_head = m_tail = 0;
	}

	inline unsigned int Put(byte inByte)
	{
		if (MaxSize()==m_tail)
			return 0;

		buf[m_tail++]=inByte;
		return 1;
	}

	inline unsigned int Put(const byte *inString, unsigned int length)
	{
		unsigned int l = STDMIN(length, MaxSize()-m_tail);
		memcpy(buf+m_tail, inString, l);
		m_tail += l;
		return l;
	}

	inline unsigned int Peek(byte &outByte) const
	{
		if (m_tail==m_head)
			return 0;

		outByte=buf[m_head];
		return 1;
	}

	inline unsigned int Peek(byte *target, unsigned int copyMax) const
	{
		unsigned int len = STDMIN(copyMax, m_tail-m_head);
		memcpy(target, buf+m_head, len);
		return len;
	}

	inline unsigned int CopyTo(BufferedTransformation &target) const
	{
		unsigned int len = m_tail-m_head;
		target.Put(buf+m_head, len);
		return len;
	}

	inline unsigned int CopyTo(BufferedTransformation &target, unsigned int copyMax) const
	{
		unsigned int len = STDMIN(copyMax, m_tail-m_head);
		target.Put(buf+m_head, len);
		return len;
	}

	inline unsigned int Get(byte &outByte)
	{
		unsigned int len = Peek(outByte);
		m_head += len;
		return len;
	}

	inline unsigned int Get(byte *outString, unsigned int getMax)
	{
		unsigned int len = Peek(outString, getMax);
		m_head += len;
		return len;
	}

	inline unsigned int TransferTo(BufferedTransformation &target)
	{
		unsigned int len = CopyTo(target);
		m_head += len;
		return len;
	}

	inline unsigned int TransferTo(BufferedTransformation &target, unsigned int transferMax)
	{
		unsigned int len = CopyTo(target, transferMax);
		m_head += len;
		return len;
	}

	inline unsigned int Skip(unsigned int skipMax)
	{
		unsigned int len = STDMIN(skipMax, m_tail-m_head);
		m_head += len;
		return len;
	}

	inline byte operator[](unsigned int i) const
	{
		return buf[m_head+i];
	}

	ByteQueueNode *next;

	SecByteBlock buf;
	unsigned int m_head, m_tail;
};

// ********************************************************

ByteQueue::ByteQueue(unsigned int m_nodeSize)
	: m_nodeSize(m_nodeSize), m_lazyLength(0)
{
	m_head = m_tail = new ByteQueueNode(m_nodeSize);
}

ByteQueue::ByteQueue(const ByteQueue &copy)
{
	CopyFrom(copy);
}

void ByteQueue::CopyFrom(const ByteQueue &copy)
{
	m_lazyLength = 0;
	m_nodeSize = copy.m_nodeSize;
	m_head = m_tail = new ByteQueueNode(*copy.m_head);

	for (ByteQueueNode *current=copy.m_head->next; current; current=current->next)
	{
		m_tail->next = new ByteQueueNode(*current);
		m_tail = m_tail->next;
	}

	m_tail->next = NULL;

	Put(copy.m_lazyString, copy.m_lazyLength);
}

ByteQueue::~ByteQueue()
{
	Destroy();
}

void ByteQueue::Destroy()
{
	ByteQueueNode *next;

	for (ByteQueueNode *current=m_head; current; current=next)
	{
		next=current->next;
		delete current;
	}
}

unsigned long ByteQueue::CurrentSize() const
{
	unsigned long size=0;

	for (ByteQueueNode *current=m_head; current; current=current->next)
		size += current->CurrentSize();

	return size + m_lazyLength;
}

bool ByteQueue::IsEmpty() const
{
	return m_head==m_tail && m_head->CurrentSize()==0 && m_lazyLength==0;
}

void ByteQueue::Clear()
{
	Destroy();
	m_head = m_tail = new ByteQueueNode(m_nodeSize);
	m_lazyLength = 0;
}

void ByteQueue::Put(byte inByte)
{
	if (m_lazyLength > 0)
		FinalizeLazyPut();

	if (!m_tail->Put(inByte))
	{
		m_tail->next = new ByteQueueNode(m_nodeSize);
		m_tail = m_tail->next;
		m_tail->Put(inByte);
	}
}

void ByteQueue::Put(const byte *inString, unsigned int length)
{
	if (m_lazyLength > 0)
		FinalizeLazyPut();

	unsigned int len;
	while ((len=m_tail->Put(inString, length)) < length)
	{
		m_tail->next = new ByteQueueNode(m_nodeSize);
		m_tail = m_tail->next;
		inString += len;
		length -= len;
	}
}

void ByteQueue::CleanupUsedNodes()
{
	while (m_head != m_tail && m_head->UsedUp())
	{
		ByteQueueNode *temp=m_head;
		m_head=m_head->next;
		delete temp;
	}

	if (m_head->CurrentSize() == 0)
		m_head->Clear();
}

void ByteQueue::LazyPut(const byte *inString, unsigned int size)
{
	if (m_lazyLength > 0)
		FinalizeLazyPut();
	m_lazyString = inString;
	m_lazyLength = size;
}

void ByteQueue::FinalizeLazyPut()
{
	unsigned int len = m_lazyLength;
	m_lazyLength = 0;
	Put(m_lazyString, len);
}

unsigned int ByteQueue::Get(byte &outByte)
{
	if (m_head->Get(outByte))
	{
		if (m_head->UsedUp())
			CleanupUsedNodes();
		return 1;
	}
	else if (m_lazyLength > 0)
	{
		outByte = *m_lazyString++;
		m_lazyLength--;
		return 1;
	}
	else
		return 0;
}

unsigned int ByteQueue::Get(byte *outString, unsigned int getMax)
{
	ArraySink sink(outString, getMax);
	return TransferTo(sink, getMax);
}

unsigned int ByteQueue::Peek(byte &outByte) const
{
	if (m_head->Peek(outByte))
		return 1;
	else if (m_lazyLength > 0)
	{
		outByte = *m_lazyString;
		return 1;
	}
	else
		return 0;
}

unsigned int ByteQueue::Peek(byte *outString, unsigned int peekMax) const
{
	ArraySink sink(outString, peekMax);
	return CopyTo(sink, peekMax);
}

unsigned long ByteQueue::Skip(unsigned long skipMax)
{
	return TransferTo(g_bitBucket, skipMax);
}

unsigned long ByteQueue::TransferTo(BufferedTransformation &target, unsigned long transferMax)
{
	unsigned long bytesLeft = transferMax;
	for (ByteQueueNode *current=m_head; bytesLeft && current; current=current->next)
		bytesLeft -= current->TransferTo(target, bytesLeft);
	CleanupUsedNodes();

	unsigned int len = (unsigned int)STDMIN(bytesLeft, (unsigned long)m_lazyLength);
	if (len)
	{
		target.Put(m_lazyString, len);
		m_lazyString += len;
		m_lazyLength -= len;
		bytesLeft -= len;
	}
	return transferMax - bytesLeft;
}

unsigned long ByteQueue::CopyTo(BufferedTransformation &target, unsigned long copyMax) const
{
	unsigned long bytesLeft = copyMax;
	for (ByteQueueNode *current=m_head; bytesLeft && current; current=current->next)
		bytesLeft -= current->CopyTo(target, bytesLeft);
	if (bytesLeft && m_lazyLength)
	{
		unsigned int len = (unsigned int)STDMIN(bytesLeft, (unsigned long)m_lazyLength);
		target.Put(m_lazyString, len);
		bytesLeft -= len;
	}
	return copyMax - bytesLeft;
}

void ByteQueue::Unget(byte inByte)
{
	Unget(&inByte, 1);
}

void ByteQueue::Unget(const byte *inString, unsigned int length)
{
	ByteQueueNode *newHead = new ByteQueueNode(length);
	newHead->next = m_head;
	m_head = newHead;
	m_head->Put(inString, length);
}
/*
byte * ByteQueue::Spy(unsigned int &contiguousSize)
{
	contiguousSize = m_head->m_tail - m_head->m_head;
	return m_head->buf + m_head->m_head;
}
*/
const byte * ByteQueue::Spy(unsigned int &contiguousSize) const
{
	contiguousSize = m_head->m_tail - m_head->m_head;
	if (contiguousSize == 0 && m_lazyLength > 0)
	{
		contiguousSize = m_lazyLength;
		return m_lazyString;
	}
	else
		return m_head->buf + m_head->m_head;
}

byte * ByteQueue::MakeNewSpace(unsigned int &contiguousSize)
{
	if (m_lazyLength > 0)
		FinalizeLazyPut();

	if (m_tail->m_tail == m_tail->MaxSize())
	{
		m_tail->next = new ByteQueueNode(m_nodeSize);
		m_tail = m_tail->next;
	}

	contiguousSize = m_tail->MaxSize() - m_tail->m_tail;
	return m_tail->buf + m_tail->m_tail;
}

void ByteQueue::OccupyNewSpace(unsigned int size)
{
	m_tail->m_tail += size;
	assert(m_tail->m_tail <= m_tail->MaxSize());
}

ByteQueue & ByteQueue::operator=(const ByteQueue &rhs)
{
	Destroy();
	CopyFrom(rhs);
	return *this;
}

bool ByteQueue::operator==(const ByteQueue &rhs) const
{
	const unsigned long currentSize = CurrentSize();

	if (currentSize != rhs.CurrentSize())
		return false;

	Walker walker1(*this), walker2(rhs);
	byte b1, b2;

	while (walker1.Get(b1) && walker2.Get(b2))
		if (b1 != b2)
			return false;

	return true;
}

byte ByteQueue::operator[](unsigned long i) const
{
	for (ByteQueueNode *current=m_head; current; current=current->next)
	{
		if (i < current->CurrentSize())
			return (*current)[i];
		
		i -= current->CurrentSize();
	}

	assert(i < m_lazyLength);
	return m_lazyString[i];
}

void ByteQueue::swap(ByteQueue &rhs)
{
	std::swap(m_nodeSize, rhs.m_nodeSize);
	std::swap(m_head, rhs.m_head);
	std::swap(m_tail, rhs.m_tail);
	std::swap(m_lazyString, rhs.m_lazyString);
	std::swap(m_lazyLength, rhs.m_lazyLength);
}

// ********************************************************

unsigned int ByteQueue::Walker::Get(byte &outByte)
{
	ArraySink sink(&outByte, 1);
	return TransferTo(sink, 1);
}

unsigned int ByteQueue::Walker::Get(byte *outString, unsigned int getMax)
{
	ArraySink sink(outString, getMax);
	return TransferTo(sink, getMax);
}

unsigned int ByteQueue::Walker::Peek(byte &outByte) const
{
	ArraySink sink(&outByte, 1);
	return CopyTo(sink, 1);
}

unsigned int ByteQueue::Walker::Peek(byte *outString, unsigned int peekMax) const
{
	ArraySink sink(outString, peekMax);
	return CopyTo(sink, peekMax);
}

unsigned long ByteQueue::Walker::TransferTo(BufferedTransformation &target, unsigned long transferMax)
{
	unsigned long bytesLeft = transferMax;
	while (m_node)
	{
		unsigned int len = STDMIN(bytesLeft, (unsigned long)m_node->CurrentSize()-m_offset);
		target.Put(m_node->buf+m_node->m_head+m_offset, len);
		m_position += len;
		bytesLeft -= len;

		if (!bytesLeft)
		{
			m_offset += len;
			break;
		}

		m_node = m_node->next;
		m_offset = 0;
	}

	unsigned int len = (unsigned int)STDMIN(bytesLeft, (unsigned long)m_lazyLength);
	if (len)
	{
		target.Put(m_lazyString, len);
		m_lazyString += len;
		m_lazyLength -= len;
		bytesLeft -= len;
	}
	return transferMax - bytesLeft;
}

unsigned long ByteQueue::Walker::Skip(unsigned long skipMax)
{
	return TransferTo(g_bitBucket, skipMax);
}

unsigned long ByteQueue::Walker::CopyTo(BufferedTransformation &target, unsigned long copyMax) const
{
	return Walker(*this).TransferTo(target, copyMax);
}

NAMESPACE_END
