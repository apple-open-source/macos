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
 // mdc.h - written and placed in the public domain by Wei Dai

#ifndef CRYPTOPP_MDC_H
#define CRYPTOPP_MDC_H

/** \file
*/

#include "cryptopp.h"
#include "cryptopp_misc.h"

NAMESPACE_BEGIN(CryptoPP)

/// <a href="http://www.weidai.com/scan-mirror/cs.html#MDC">MDC</a>
template <class T> class MDC : public FixedBlockSize<T::DIGESTSIZE>, public FixedKeyLength<T::BLOCKSIZE>
{
public:
	MDC(const byte *userKey, unsigned int = 0)
		: key(KEYLENGTH/4)
	{
		T::CorrectEndianess(key, (word32 *)userKey, KEYLENGTH);
	}

	void ProcessBlock(byte *inoutBlock) const
	{
		T::CorrectEndianess((word32 *)inoutBlock, (word32 *)inoutBlock, BLOCKSIZE);
		T::Transform((word32 *)inoutBlock, key);
		T::CorrectEndianess((word32 *)inoutBlock, (word32 *)inoutBlock, BLOCKSIZE);
	}

	void ProcessBlock(const byte *inBlock, byte *outBlock) const
	{
		T::CorrectEndianess((word32 *)outBlock, (word32 *)inBlock, BLOCKSIZE);
		T::Transform((word32 *)outBlock, key);
		T::CorrectEndianess((word32 *)outBlock, (word32 *)outBlock, BLOCKSIZE);
	}

private:
	SecBlock<word32> key;
};

NAMESPACE_END

#endif
