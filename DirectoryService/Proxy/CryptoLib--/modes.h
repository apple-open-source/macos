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
#ifndef CRYPTOPP_MODES_H
#define CRYPTOPP_MODES_H

/** \file
*/

#include "cryptopp.h"
#include "cryptopp_misc.h"

NAMESPACE_BEGIN(CryptoPP)

class CipherMode
{
protected:
	CipherMode(const BlockTransformation &cipher, const byte *IV);

	const BlockTransformation &cipher;
	const int S;
	SecByteBlock reg, buffer;    // register is a reserved word
};

class FeedBackMode : protected CipherMode
{
protected:
	// feedBackSize = 0 means use maximum feedback size (i.e., the cipher block size)
	FeedBackMode(const BlockTransformation &cipher, const byte *IV, int feedBackSize);
	void DoFeedBack();

	const int FBS;
	int counter;
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#mode_CFB">Cipher-Feedback mode</a>
class CFBEncryption : public StreamCipher, protected FeedBackMode
{
public:
	/// cipher should be an *encryption* object
	CFBEncryption(const BlockTransformation &cipher, const byte *IV, int feedBackSize = 0)
    	: FeedBackMode(cipher, IV, feedBackSize) {}

	byte ProcessByte(byte input)
	{
		if (counter==FBS)
			DoFeedBack();
		buffer[counter] ^= input;
		return buffer[counter++];
	}

	void ProcessString(byte *outString, const byte *inString, unsigned int length);
	void ProcessString(byte *inoutString, unsigned int length);
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#mode_CFB">Cipher-Feedback mode</a>
class CFBDecryption : public StreamCipher, protected FeedBackMode
{
public:
	/// cipher should be an *encryption* object
	CFBDecryption(const BlockTransformation &cipher, const byte *IV, int feedBackSize = 0)
    	: FeedBackMode(cipher, IV, feedBackSize) {}

	byte ProcessByte(byte input)
	{
		if (counter==FBS)
			DoFeedBack();
		byte b = buffer[counter] ^ input;
		buffer[counter++] = input;
		return (b);
	}

	void ProcessString(byte *outString, const byte *inString, unsigned int length);
	void ProcessString(byte *inoutString, unsigned int length);
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#mode_OFB">Output-Feedback mode</a>
class OFB : public RandomNumberGenerator, public StreamCipher, protected FeedBackMode
{
public:
	/// cipher should be an *encryption* object
	OFB(const BlockTransformation &cipher, const byte *IV, int feedBackSize = 0)
    	: FeedBackMode(cipher, IV, feedBackSize) {}

	byte GenerateByte()
	{
		if (counter==FBS)
			DoFeedBack();
		return buffer[counter++];
	}

	byte ProcessByte(byte input)
		{return (input ^ OFB::GenerateByte());}

	void ProcessString(byte *outString, const byte *inString, unsigned int length);
	void ProcessString(byte *inoutString, unsigned int length);
};

/// <a href="http://www.weidai.com/scan-mirror/cs.html#mode_Counter-BE">Counter-BE Mode</a>
class CounterMode : public RandomNumberGenerator, public RandomAccessStreamCipher, protected CipherMode
{
public:
	// cipher should be an *encryption* object
	CounterMode(const BlockTransformation &cipher, const byte *IV);

	byte GenerateByte()
	{
		if (size==S)
			IncrementCounter();
		return buffer[size++];
	}

	byte ProcessByte(byte input)
		{return (input ^ CounterMode::GenerateByte());}

	void ProcessString(byte *outString, const byte *inString, unsigned int length);
	void ProcessString(byte *inoutString, unsigned int length);

	void Seek(unsigned long position);

private:
	void IncrementCounter();

	SecByteBlock IV;
	int size;
};

/// CFB with a sync method used by PGP
class PGP_CFBEncryption : public CFBEncryption
{
public:
	/// cipher should be an *encryption* object
	PGP_CFBEncryption(const BlockTransformation &cipher, const byte *IV)
    	: CFBEncryption(cipher, IV, 0) {}

	void Sync();
};

/// CFB with a sync method used by PGP
class PGP_CFBDecryption : public CFBDecryption
{
public:
	/// cipher should be an *encryption* object
	PGP_CFBDecryption(const BlockTransformation &cipher, const byte *IV)
    	: CFBDecryption(cipher, IV, 0) {}

	void Sync();
};

NAMESPACE_END

#endif
