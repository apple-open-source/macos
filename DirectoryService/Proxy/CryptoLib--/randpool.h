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
#ifndef CRYPTOPP_RANDPOOL_H
#define CRYPTOPP_RANDPOOL_H

#include "cryptopp.h"
#include "filters.h"

NAMESPACE_BEGIN(CryptoPP)

//! Randomness Pool
/*! This class can be used to generate
pseudorandom bytes after seeding the pool with
the Put() methods */
class RandomPool : public RandomNumberGenerator,
				   public Sink
{
public:
	//! poolSize must be greater than 16
	RandomPool(unsigned int poolSize=384);

	//! seed the pool
	void Put(byte inByte);
	//! seed the pool
	void Put(const byte *inString, unsigned int length);
	
	byte GenerateByte();
	void GenerateBlock(byte *output, unsigned int size);

protected:
	void Stir();

private:
	SecByteBlock pool, key;
	unsigned int addPos, getPos;
};

NAMESPACE_END

#endif
