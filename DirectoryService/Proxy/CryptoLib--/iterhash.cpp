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
// iterhash.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "iterhash.h"

NAMESPACE_BEGIN(CryptoPP)

template <class T>
IteratedHashBase<T>::IteratedHashBase(unsigned int blockSize, unsigned int digestSize)
	: blockSize(blockSize), countLo(0), countHi(0)
	, data(blockSize/sizeof(T)), digest(digestSize/sizeof(T))
{
}

template <class T> void IteratedHashBase<T>::Update(const byte *input, unsigned int len)
{
	HashWordType tmp = countLo;
	if ((countLo = tmp + ((word32)len << 3)) < tmp)
		countHi++;             // Carry from low to high
	countHi += len >> (8*sizeof(HashWordType)-3);

	assert((blockSize & (blockSize-1)) == 0);	// blockSize is a power of 2
	unsigned int num = (unsigned int)(tmp >> 3) & (blockSize-1);

	if (num != 0)
	{
		if ((num+len) >= blockSize)
		{
			memcpy((byte *)data.ptr+num, input, blockSize-num);
			HashBlock(data);
			input += (blockSize-num);
			len-=(blockSize - num);
			num=0;
			// drop through and do the rest
		}
		else
		{
			memcpy((byte *)data.ptr+num, input, len);
			return;
		}
	}

	// we now can process the input data in blocks of blockSize
	// chars and save the leftovers to this->data.
	if (len >= blockSize)
	{
		if (IsAligned<T>(input))
		{
			unsigned int leftOver = HashMultipleBlocks((T *)input, len);
			input += (len - leftOver);
			len = leftOver;
		}
		else
			do
			{   // copy input first if it's not aligned correctly
				memcpy(data, input, blockSize);
				HashBlock(data);
				input+=blockSize;
				len-=blockSize;
			} while (len >= blockSize);
	}

	memcpy(data, input, len);
}

template <class T> unsigned int IteratedHashBase<T>::HashMultipleBlocks(const T *input, unsigned int length)
{
	do
	{
		HashBlock(input);
		input += blockSize/sizeof(T);
		length -= blockSize;
	}
	while (length >= blockSize);
	return length;
}

template <class T> void IteratedHashBase<T>::PadLastBlock(unsigned int lastBlockSize, byte padFirst)
{
	unsigned int num = (unsigned int)(countLo >> 3) & (blockSize-1);
	assert(num < blockSize);
	((byte *)data.ptr)[num++]=padFirst;
	if (num <= lastBlockSize)
		memset((byte *)data.ptr+num, 0, lastBlockSize-num);
	else
	{
		memset((byte *)data.ptr+num, 0, blockSize-num);
		HashBlock(data);
		memset(data, 0, lastBlockSize);
	}
}

template <class T> void IteratedHashBase<T>::Reinit()
{
	countLo = countHi = 0;
	Init();
}

// provide empty definitions to avoid instantiation warnings
template <class T> void IteratedHashBase<T>::Init() {}
template <class T> void IteratedHashBase<T>::HashBlock(const T *input) {}

#ifdef WORD64_AVAILABLE
template class IteratedHashBase<word64>;
#endif

template class IteratedHashBase<word32>;

NAMESPACE_END
