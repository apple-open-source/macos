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
// modes.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "modes.h"

NAMESPACE_BEGIN(CryptoPP)

CipherMode::CipherMode(const BlockTransformation &c, const byte *IV)
	: cipher(c),
	  S(cipher.BlockSize()),
	  reg(IV, S),
	  buffer(S)
{
}

FeedBackMode::FeedBackMode(const BlockTransformation &cipher, const byte *IV, int fbs)
	: CipherMode(cipher, IV), FBS(fbs ? fbs : S)
{
	cipher.ProcessBlock(reg, buffer);
	counter = 0;
}

void FeedBackMode::DoFeedBack()
{
	for (int i=0; i<(S-FBS); i++)
		reg[i] = reg[FBS+i];
	memcpy(reg+S-FBS, buffer, FBS);
	cipher.ProcessBlock(reg, buffer);
	counter = 0;
}

void CFBEncryption::ProcessString(byte *outString, const byte *inString, unsigned int length)
{
	while(length--)
		*outString++ = CFBEncryption::ProcessByte(*inString++);
}

void CFBEncryption::ProcessString(byte *inoutString, unsigned int length)
{
	while(length--)
		*inoutString++ = CFBEncryption::ProcessByte(*inoutString);
}

void CFBDecryption::ProcessString(byte *outString, const byte *inString, unsigned int length)
{
	while(length--)
		*outString++ = CFBDecryption::ProcessByte(*inString++);
}

void CFBDecryption::ProcessString(byte *inoutString, unsigned int length)
{
	while(length--)
		*inoutString++ = CFBDecryption::ProcessByte(*inoutString);
}

void OFB::ProcessString(byte *outString, const byte *inString, unsigned int length)
{
	while(length--)
		*outString++ = *inString++ ^ OFB::GetByte();
}

void OFB::ProcessString(byte *inoutString, unsigned int length)
{
	while(length--)
		*inoutString++ ^= OFB::GetByte();
}

CounterMode::CounterMode(const BlockTransformation &cipher, const byte *IVin)
	: CipherMode(cipher, IVin), IV(IVin, S)
{
	cipher.ProcessBlock(reg, buffer);
	size=0;
}

void CounterMode::ProcessString(byte *outString, const byte *inString, unsigned int length)
{
	while(length--)
		*outString++ = *inString++ ^ CounterMode::GetByte();
}

void CounterMode::ProcessString(byte *inoutString, unsigned int length)
{
	while(length--)
		*inoutString++ ^= CounterMode::GetByte();
}

void CounterMode::Seek(unsigned long position)
{
	unsigned long blockIndex = position / S;

	// set register to IV+blockIndex
	int carry=0;
	for (int i=S-1; i>=0; i--)
	{
		int sum = IV[i] + byte(blockIndex) + carry;
		reg[i] = (byte) sum;
		carry = sum >> 8;
		blockIndex >>= 8;
	}

	cipher.ProcessBlock(reg, buffer);
	size = int(position % S);
}

void CounterMode::IncrementCounter()
{
	for (int i=S-1, carry=1; i>=0 && carry; i--)
    	carry=!++reg[i];

	cipher.ProcessBlock(reg, buffer);
	size=0;
}

void PGP_CFBEncryption::Sync()
{
	if (counter)
	{
		for (int i=0; i<counter; i++)
			buffer[S-counter+i] = buffer[i];
		memcpy(buffer, reg+counter, S-counter);
		counter = 0;
	}
}

// this is exactly the same function as above
void PGP_CFBDecryption::Sync()
{
	if (counter)
	{
		for (int i=0; i<counter; i++)
			buffer[S-counter+i] = buffer[i];
		memcpy(buffer, reg+counter, S-counter);
		counter = 0;
	}
}

NAMESPACE_END
