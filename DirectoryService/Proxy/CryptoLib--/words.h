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
#ifndef CRYPTOPP_WORDS_H
#define CRYPTOPP_WORDS_H

#include "cryptopp_misc.h"

NAMESPACE_BEGIN(CryptoPP)

inline unsigned int CountWords(const word *X, unsigned int N)
{
	while (N && X[N-1]==0)
		N--;
	return N;
}

inline void SetWords(word *r, word a, unsigned int n)
{
	for (unsigned int i=0; i<n; i++)
		r[i] = a;
}

inline void CopyWords(word *r, const word *a, unsigned int n)
{
	for (unsigned int i=0; i<n; i++)
		r[i] = a[i];
}

inline void XorWords(word *r, const word *a, const word *b, unsigned int n)
{
	for (unsigned int i=0; i<n; i++)
		r[i] = a[i] ^ b[i];
}

inline void XorWords(word *r, const word *a, unsigned int n)
{
	for (unsigned int i=0; i<n; i++)
		r[i] ^= a[i];
}

inline void AndWords(word *r, const word *a, const word *b, unsigned int n)
{
	for (unsigned int i=0; i<n; i++)
		r[i] = a[i] & b[i];
}

inline void AndWords(word *r, const word *a, unsigned int n)
{
	for (unsigned int i=0; i<n; i++)
		r[i] &= a[i];
}

inline word ShiftWordsLeftByBits(word *r, unsigned int n, unsigned int shiftBits)
{
	assert (shiftBits<WORD_BITS);
	word u, carry=0;
	if (shiftBits)
		for (unsigned int i=0; i<n; i++)
		{
			u = r[i];
			r[i] = (u << shiftBits) | carry;
			carry = u >> (WORD_BITS-shiftBits);
		}
	return carry;
}

inline word ShiftWordsRightByBits(word *r, unsigned int n, unsigned int shiftBits)
{
	assert (shiftBits<WORD_BITS);
	word u, carry=0;
	if (shiftBits)
		for (int i=n-1; i>=0; i--)
		{
			u = r[i];
			r[i] = (u >> shiftBits) | carry;
			carry = u << (WORD_BITS-shiftBits);
		}
	return carry;
}

inline void ShiftWordsLeftByWords(word *r, unsigned int n, unsigned int shiftWords)
{
	shiftWords = STDMIN(shiftWords, n);
	if (shiftWords)
	{
		for (unsigned int i=n-1; i>=shiftWords; i--)
			r[i] = r[i-shiftWords];
		SetWords(r, 0, shiftWords);
	}
}

inline void ShiftWordsRightByWords(word *r, unsigned int n, unsigned int shiftWords)
{
	shiftWords = STDMIN(shiftWords, n);
	if (shiftWords)
	{
		for (unsigned int i=0; i+shiftWords<n; i++)
			r[i] = r[i+shiftWords];
		SetWords(r+n-shiftWords, 0, shiftWords);
	}
}

NAMESPACE_END

#endif
