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
// misc.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "cryptopp_misc.h"
#include "words.h"

NAMESPACE_BEGIN(CryptoPP)

byte OAEP_P_DEFAULT[1];

void xorbuf(byte *buf, const byte *mask, unsigned int count)
{
	if (((unsigned int)buf | (unsigned int)mask | count) % WORD_SIZE == 0)
		XorWords((word *)buf, (const word *)mask, count/WORD_SIZE);
	else
	{
		for (unsigned int i=0; i<count; i++)
			buf[i] ^= mask[i];
	}
}

void xorbuf(byte *output, const byte *input, const byte *mask, unsigned int count)
{
	if (((unsigned int)output | (unsigned int)input | (unsigned int)mask | count) % WORD_SIZE == 0)
		XorWords((word *)output, (const word *)input, (const word *)mask, count/WORD_SIZE);
	else
	{
		for (unsigned int i=0; i<count; i++)
			output[i] = input[i] ^ mask[i];
	}
}

unsigned int Parity(unsigned long value)
{
	for (unsigned int i=8*sizeof(value)/2; i>0; i/=2)
		value ^= value >> i;
	return (unsigned int)value&1;
}

unsigned int BytePrecision(unsigned long value)
{
	unsigned int i;
	for (i=sizeof(value); i; --i)
		if (value >> (i-1)*8)
			break;

	return i;
}

unsigned int BitPrecision(unsigned long value)
{
	if (!value)
		return 0;

	unsigned int l=0, h=8*sizeof(value);

	while (h-l > 1)
	{
		unsigned int t = (l+h)/2;
		if (value >> t)
			l = t;
		else
			h = t;
	}

	return h;
}

unsigned long Crop(unsigned long value, unsigned int size)
{
	if (size < 8*sizeof(value))
    	return (value & ((1L << size) - 1));
	else
		return value;
}

NAMESPACE_END
