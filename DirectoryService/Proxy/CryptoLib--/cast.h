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
#ifndef CRYPTOPP_CAST_H
#define CRYPTOPP_CAST_H

/** \file
*/

#include "cryptopp.h"
#include "cryptopp_misc.h"

NAMESPACE_BEGIN(CryptoPP)

class CAST
{
protected:
	static const word32 S[8][256];
};

/// base class, do not use directly
class CAST128 : public FixedBlockSize<8>, public CAST, public VariableKeyLength<16, 5, 16>
{
protected:
	// keylength should be between 5 and 16
	CAST128(const byte *userKey, unsigned int keylength);

	bool reduced;
	SecBlock<word32> K;
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#CAST-128">CAST-128</a>
class CAST128Encryption : public CAST128
{
public:
	CAST128Encryption(const byte *userKey, unsigned int keylength=DEFAULT_KEYLENGTH)
		: CAST128(userKey, keylength) {}

	void ProcessBlock(const byte *inBlock, byte * outBlock) const;
	void ProcessBlock(byte * inoutBlock) const
		{CAST128Encryption::ProcessBlock(inoutBlock, inoutBlock);}
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#CAST-128">CAST-128</a>
class CAST128Decryption : public CAST128
{
public:
	CAST128Decryption(const byte *userKey, unsigned int keylength=DEFAULT_KEYLENGTH)
		: CAST128(userKey, keylength) {}

	void ProcessBlock(const byte *inBlock, byte * outBlock) const;
	void ProcessBlock(byte * inoutBlock) const
		{CAST128Decryption::ProcessBlock(inoutBlock, inoutBlock);}
};

/// base class, do not use directly
class CAST256 : public FixedBlockSize<16>, public CAST, public VariableKeyLength<16, 16, 32>
{
public:
	void ProcessBlock(const byte *inBlock, byte * outBlock) const;
	void ProcessBlock(byte * inoutBlock) const
		{CAST256::ProcessBlock(inoutBlock, inoutBlock);}

protected:
	CAST256(const byte *userKey, unsigned int keylength);

	SecBlock<word32> K;

private:
	static const word32 t_m[8][24];
	static const unsigned int t_r[8][24];

	static void Omega(int i, word32 kappa[8]);
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#CAST-256">CAST-256</a>
class CAST256Encryption : public CAST256
{
public:
	CAST256Encryption(const byte *userKey, unsigned int keylength=DEFAULT_KEYLENGTH)
		: CAST256(userKey, keylength) {}
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#CAST-256">CAST-256</a>
class CAST256Decryption : public CAST256
{
public:
	CAST256Decryption(const byte *userKey, unsigned int keylength=DEFAULT_KEYLENGTH);
};

NAMESPACE_END

#endif
