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
// mqueue.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "mqueue.h"

NAMESPACE_BEGIN(CryptoPP)

MessageQueue::MessageQueue(unsigned int nodeSize)
	: m_queue(nodeSize), m_lengths(1, 0)
{
}

bool MessageQueue::GetNextMessage()
{
	if (NumberOfMessages() > 0 && !AnyRetrievable())
	{
		m_lengths.pop_front();
		return true;
	}
	else
		return false;
}

unsigned int MessageQueue::CopyMessagesTo(BufferedTransformation &target, unsigned int count) const
{
	ByteQueue::Walker walker(m_queue);
	std::deque<unsigned long>::const_iterator it = m_lengths.begin();
	unsigned int i;
	for (i=0; i<count && it != --m_lengths.end(); ++i, ++it)
	{
		walker.TransferTo(target, *it);
		if (GetAutoSignalPropagation())
			target.MessageEnd(GetAutoSignalPropagation()-1);
	}
	return i;
}

void MessageQueue::swap(MessageQueue &rhs)
{
	m_queue.swap(rhs.m_queue);
	m_lengths.swap(rhs.m_lengths);
}

NAMESPACE_END
