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
#ifndef CRYPTOPP_ITERHASH_H
#define CRYPTOPP_ITERHASH_H

#include "cryptopp.h"
#include "cryptopp_misc.h"

NAMESPACE_BEGIN(CryptoPP)

/*! The following classes are explicitly instantiated in iterhash.cpp

	IteratedHashBase<word32>
	IteratedHashBase<word64>	// #ifdef WORD64_AVAILABLE
*/
template <class T>
class IteratedHashBase : public HashModuleWithTruncation
{
public:
	typedef T HashWordType;

	IteratedHashBase(unsigned int blockSize, unsigned int digestSize);
	unsigned int DigestSize() const {return digest.size * sizeof(T);};
	void Update(const byte *input, unsigned int length);

protected:
	virtual unsigned int HashMultipleBlocks(const T *input, unsigned int length);
	void PadLastBlock(unsigned int lastBlockSize, byte padFirst=0x80);
	void Reinit();
	virtual void Init() =0;
	virtual void HashBlock(const T *input) =0;

	unsigned int blockSize;
	word32 countLo, countHi;	// 64-bit bit count
	SecBlock<T> data;			// Data buffer
	SecBlock<T> digest;			// Message digest
};

//! .
template <class T, bool H, unsigned int S>
class IteratedHash : public IteratedHashBase<T>
{
public:
	typedef T HashWordType;
	enum {HIGHFIRST = H, BLOCKSIZE = S};
	
	IteratedHash(unsigned int digestSize) : IteratedHashBase<T>(BLOCKSIZE, digestSize) {}

	inline static void CorrectEndianess(HashWordType *out, const HashWordType *in, unsigned int byteCount)
	{
		if (!CheckEndianess(HIGHFIRST))
			byteReverse(out, in, byteCount);
		else if (in!=out)
			memcpy(out, in, byteCount);
	}

	void TruncatedFinal(byte *hash, unsigned int size)
	{
		assert(size <= DigestSize());

		PadLastBlock(BLOCKSIZE - 2*sizeof(HashWordType));
		CorrectEndianess(data, data, BLOCKSIZE - 2*sizeof(HashWordType));

		data[data.size-2] = HIGHFIRST ? countHi : countLo;
		data[data.size-1] = HIGHFIRST ? countLo : countHi;

		vTransform(data);
		CorrectEndianess(digest, digest, DigestSize());
		memcpy(hash, digest, size);

		Reinit();		// reinit for next use
	}

protected:
	void HashBlock(const HashWordType *input)
	{
		if (CheckEndianess(HIGHFIRST))
			vTransform(input);
		else
		{
			byteReverse(data.ptr, input, (unsigned int)BLOCKSIZE);
			vTransform(data);
		}
	}

	virtual void vTransform(const HashWordType *data) =0;
};

NAMESPACE_END

#endif
