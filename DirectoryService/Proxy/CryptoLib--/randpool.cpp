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
// randpool.cpp - written and placed in the public domain by Wei Dai
// The algorithm in this module comes from PGP's randpool.c

#include "pch.h"
#include "randpool.h"
#include "mdc.h"
#include "sha.h"
#include "modes.h"

NAMESPACE_BEGIN(CryptoPP)

typedef MDC<SHA> RandomPoolCipher;

RandomPool::RandomPool(unsigned int poolSize)
	: pool(poolSize), key(RandomPoolCipher::DEFAULT_KEYLENGTH)
{
	assert(poolSize > key.size);

	addPos=0;
	getPos=poolSize;
	memset(pool, 0, poolSize);
	memset(key, 0, key.size);
}

void RandomPool::Stir()
{
	for (int i=0; i<2; i++)
	{
		RandomPoolCipher cipher(key);
		CFBEncryption cfb(cipher, pool+pool.size-cipher.BlockSize());
		cfb.ProcessString(pool, pool.size);
		memcpy(key, pool, key.size);
	}

	addPos = 0;
	getPos = key.size;
}

void RandomPool::Put(byte inByte)
{
	if (addPos == pool.size)
		Stir();

	pool[addPos++] ^= inByte;
	getPos = pool.size; // Force stir on get
}

void RandomPool::Put(const byte *inString, unsigned int length)
{
	unsigned t;

	while (length > (t = pool.size - addPos))
	{
		xorbuf(pool+addPos, inString, t);
		inString += t;
		length -= t;
		Stir();
	}

	if (length)
	{
		xorbuf(pool+addPos, inString, length);
		addPos += length;
		getPos = pool.size; // Force stir on get
	}
}

byte RandomPool::GenerateByte()
{
	if (getPos == pool.size)
		Stir();

	return pool[getPos++];
}

void RandomPool::GenerateBlock(byte *outString, unsigned int size)
{
	unsigned t;

	while (size > (t = pool.size - getPos))
	{
		memcpy(outString, pool+getPos, t);
		outString += t;
		size -= t;
		Stir();
	}

	if (size)
	{
		memcpy(outString, pool+getPos, size);
		getPos += size;
	}
}

NAMESPACE_END
