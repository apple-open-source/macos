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
#ifndef CRYPTOPP_MISC_H
#define CRYPTOPP_MISC_H

#include "cryptopp_config.h"
#include <assert.h>
#include <string.h>		// CodeWarrior doesn't have memory.h
#include <algorithm>
#include <string>

#ifdef INTEL_INTRINSICS
#include <stdlib.h>
#endif

NAMESPACE_BEGIN(CryptoPP)

// ************** misc functions ***************

#define GETBYTE(x, y) (unsigned int)(((x)>>(8*(y)))&255)
// this one may be faster on a Pentium
// #define GETBYTE(x, y) (((byte *)&(x))[y])

unsigned int Parity(unsigned long);
unsigned int BytePrecision(unsigned long);
unsigned int BitPrecision(unsigned long);
unsigned long Crop(unsigned long, unsigned int size);

inline unsigned int bitsToBytes(unsigned int bitCount)
{
	return ((bitCount+7)/(8));
}

inline unsigned int bytesToWords(unsigned int byteCount)
{
	return ((byteCount+WORD_SIZE-1)/WORD_SIZE);
}

inline unsigned int bitsToWords(unsigned int bitCount)
{
	return ((bitCount+WORD_BITS-1)/(WORD_BITS));
}

void xorbuf(byte *buf, const byte *mask, unsigned int count);
void xorbuf(byte *output, const byte *input, const byte *mask, unsigned int count);

inline unsigned int RoundDownToMultipleOf(unsigned int n, unsigned int m)
{
	return n - n%m;
}

inline unsigned int RoundUpToMultipleOf(unsigned int n, unsigned int m)
{
	return RoundDownToMultipleOf(n+m-1, m);
}

template <class T>
inline bool IsAligned(const void *p)
{
	return (unsigned int)p % sizeof(T) == 0;
}

inline bool CheckEndianess(bool highFirst)
{
#ifdef IS_LITTLE_ENDIAN
	return !highFirst;
#else
	return highFirst;
#endif
}

template <class T>		// can't use <sstream> because GCC 2.95.2 doesn't have it
std::string IntToString(T a)
{
	if (a == 0)
		return "0";
	bool negate = false;
	if (a < 0)
	{
		negate = true;
		a = -a;
	}
	std::string result;
	while (a > 0)
	{
		result = char('0' + a % 10) + result;
		a = a / 10;
	}
	if (negate)
		result = "-" + result;
	return result;
}

// ************** rotate functions ***************

template <class T> inline T rotlFixed(T x, unsigned int y)
{
	assert(y < sizeof(T)*8);
	return (x<<y) | (x>>(sizeof(T)*8-y));
}

template <class T> inline T rotrFixed(T x, unsigned int y)
{
	assert(y < sizeof(T)*8);
	return (x>>y) | (x<<(sizeof(T)*8-y));
}

template <class T> inline T rotlVariable(T x, unsigned int y)
{
	assert(y < sizeof(T)*8);
	return (x<<y) | (x>>(sizeof(T)*8-y));
}

template <class T> inline T rotrVariable(T x, unsigned int y)
{
	assert(y < sizeof(T)*8);
	return (x>>y) | (x<<(sizeof(T)*8-y));
}

template <class T> inline T rotlMod(T x, unsigned int y)
{
	y %= sizeof(T)*8;
	return (x<<y) | (x>>(sizeof(T)*8-y));
}

template <class T> inline T rotrMod(T x, unsigned int y)
{
	y %= sizeof(T)*8;
	return (x>>y) | (x<<(sizeof(T)*8-y));
}

#ifdef INTEL_INTRINSICS

template<> inline word32 rotlFixed<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return y ? _lrotl(x, y) : x;
}

template<> inline word32 rotrFixed<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return y ? _lrotr(x, y) : x;
}

template<> inline word32 rotlVariable<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return _lrotl(x, y);
}

template<> inline word32 rotrVariable<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return _lrotr(x, y);
}

template<> inline word32 rotlMod<word32>(word32 x, unsigned int y)
{
	return _lrotl(x, y);
}

template<> inline word32 rotrMod<word32>(word32 x, unsigned int y)
{
	return _lrotr(x, y);
}

#endif // #ifdef INTEL_INTRINSICS

#ifdef PPC_INTRINSICS

template<> inline word32 rotlFixed<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return y ? __rlwinm(x,y,0,31) : x;
}

template<> inline word32 rotrFixed<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return y ? __rlwinm(x,32-y,0,31) : x;
}

template<> inline word32 rotlVariable<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return (__rlwnm(x,y,0,31));
}

template<> inline word32 rotrVariable<word32>(word32 x, unsigned int y)
{
	assert(y < 32);
	return (__rlwnm(x,32-y,0,31));
}

template<> inline word32 rotlMod<word32>(word32 x, unsigned int y)
{
	return (__rlwnm(x,y,0,31));
}

template<> inline word32 rotrMod<word32>(word32 x, unsigned int y)
{
	return (__rlwnm(x,32-y,0,31));
}

#endif // #ifdef PPC_INTRINSICS

// ************** endian reversal ***************

inline word16 byteReverse(word16 value)
{
	return rotlFixed(value, 8U);
}

inline word32 byteReverse(word32 value)
{
#ifdef PPC_INTRINSICS
	// PPC: load reverse indexed instruction
	return (word32)__lwbrx(&value,0);
#elif defined(FAST_ROTATE)
	// 5 instructions with rotate instruction, 9 without
	return (rotrFixed(value, 8U) & 0xff00ff00) | (rotlFixed(value, 8U) & 0x00ff00ff);
#else
	// 6 instructions with rotate instruction, 8 without
	value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
	return rotlFixed(value, 16U);
#endif
}

#ifdef WORD64_AVAILABLE
inline word64 byteReverse(word64 value)
{
#ifdef SLOW_WORD64
	return (word64(byteReverse(word32(value))) << 32) | byteReverse(word32(value>>32));
#else
	value = ((value & W64LIT(0xFF00FF00FF00FF00)) >> 8) | ((value & W64LIT(0x00FF00FF00FF00FF)) << 8);
	value = ((value & W64LIT(0xFFFF0000FFFF0000)) >> 16) | ((value & W64LIT(0x0000FFFF0000FFFF)) << 16);
	return rotlFixed(value, 32U);
#endif
}
#endif

inline byte bitReverse(byte value)
{
	value = ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
	value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
	return rotlFixed(value, 4);
}

inline word16 bitReverse(word16 value)
{
	value = ((value & 0xAAAA) >> 1) | ((value & 0x5555) << 1);
	value = ((value & 0xCCCC) >> 2) | ((value & 0x3333) << 2);
	value = ((value & 0xF0F0) >> 4) | ((value & 0x0F0F) << 4);
	return byteReverse(value);
}

inline word32 bitReverse(word32 value)
{
	value = ((value & 0xAAAAAAAA) >> 1) | ((value & 0x55555555) << 1);
	value = ((value & 0xCCCCCCCC) >> 2) | ((value & 0x33333333) << 2);
	value = ((value & 0xF0F0F0F0) >> 4) | ((value & 0x0F0F0F0F) << 4);
	return byteReverse(value);
}

#ifdef WORD64_AVAILABLE
inline word64 bitReverse(word64 value)
{
#ifdef SLOW_WORD64
	return (word64(bitReverse(word32(value))) << 32) | bitReverse(word32(value>>32));
#else
	value = ((value & W64LIT(0xAAAAAAAAAAAAAAAA)) >> 1) | ((value & W64LIT(0x5555555555555555)) << 1);
	value = ((value & W64LIT(0xCCCCCCCCCCCCCCCC)) >> 2) | ((value & W64LIT(0x3333333333333333)) << 2);
	value = ((value & W64LIT(0xF0F0F0F0F0F0F0F0)) >> 4) | ((value & W64LIT(0x0F0F0F0F0F0F0F0F)) << 4);
	return byteReverse(value);
#endif
}
#endif

template <class T>
inline T bitReverse(T value)
{
	if (sizeof(T) == 1)
		return bitReverse((byte)value);
	else if (sizeof(T) == 2)
		return bitReverse((word16)value);
	else if (sizeof(T) == 4)
		return bitReverse((word32)value);
	else
	{
#ifdef WORD64_AVAILABLE
		assert(sizeof(T) == 8);
		return bitReverse((word64)value);
#else
		assert(false);
		return 0;
#endif
	}
}

template <class T>
void byteReverse(T *out, const T *in, unsigned int byteCount)
{
	unsigned int count = (byteCount+sizeof(T)-1)/sizeof(T);
	for (unsigned int i=0; i<count; i++)
		out[i] = byteReverse(in[i]);
}

template <class T>
inline void GetUserKeyLittleEndian(T *out, unsigned int outlen, const byte *in, unsigned int inlen)
{
	const unsigned int U = sizeof(T);
	assert(inlen <= outlen*U);
	memcpy(out, in, inlen);
	memset((byte *)out+inlen, 0, outlen*U-inlen);
#ifndef IS_LITTLE_ENDIAN
	byteReverse(out, out, inlen);
#endif
}

template <class T>
inline void GetUserKeyBigEndian(T *out, unsigned int outlen, const byte *in, unsigned int inlen)
{
	const unsigned int U = sizeof(T);
	assert(inlen <= outlen*U);
	memcpy(out, in, inlen);
	memset((byte *)out+inlen, 0, outlen*U-inlen);
#ifdef IS_LITTLE_ENDIAN
	byteReverse(out, out, inlen);
#endif
}

// Fetch 2 words from user's buffer into "a", "b" in LITTLE-endian order
template <class T>
inline void GetBlockLittleEndian(const byte *block, T &a, T &b)
{
#ifdef IS_LITTLE_ENDIAN
	a = ((T *)block)[0];
	b = ((T *)block)[1];
#else
	a = byteReverse(((T *)block)[0]);
	b = byteReverse(((T *)block)[1]);
#endif
}

// Put 2 words back into user's buffer in LITTLE-endian order
template <class T>
inline void PutBlockLittleEndian(byte *block, T a, T b)
{
#ifdef IS_LITTLE_ENDIAN
	((T *)block)[0] = a;
	((T *)block)[1] = b;
#else
	((T *)block)[0] = byteReverse(a);
	((T *)block)[1] = byteReverse(b);
#endif
}

// Fetch 4 words from user's buffer into "a", "b", "c", "d" in LITTLE-endian order
template <class T>
inline void GetBlockLittleEndian(const byte *block, T &a, T &b, T &c, T &d)
{
#ifdef IS_LITTLE_ENDIAN
	a = ((T *)block)[0];
	b = ((T *)block)[1];
	c = ((T *)block)[2];
	d = ((T *)block)[3];
#else
	a = byteReverse(((T *)block)[0]);
	b = byteReverse(((T *)block)[1]);
	c = byteReverse(((T *)block)[2]);
	d = byteReverse(((T *)block)[3]);
#endif
}

// Put 4 words back into user's buffer in LITTLE-endian order
template <class T>
inline void PutBlockLittleEndian(byte *block, T a, T b, T c, T d)
{
#ifdef IS_LITTLE_ENDIAN
	((T *)block)[0] = a;
	((T *)block)[1] = b;
	((T *)block)[2] = c;
	((T *)block)[3] = d;
#else
	((T *)block)[0] = byteReverse(a);
	((T *)block)[1] = byteReverse(b);
	((T *)block)[2] = byteReverse(c);
	((T *)block)[3] = byteReverse(d);
#endif
}

// Fetch 2 words from user's buffer into "a", "b" in BIG-endian order
template <class T>
inline void GetBlockBigEndian(const byte *block, T &a, T &b)
{
#ifndef IS_LITTLE_ENDIAN
	a = ((T *)block)[0];
	b = ((T *)block)[1];
#else
	a = byteReverse(((T *)block)[0]);
	b = byteReverse(((T *)block)[1]);
#endif
}

// Put 2 words back into user's buffer in BIG-endian order
template <class T>
inline void PutBlockBigEndian(byte *block, T a, T b)
{
#ifndef IS_LITTLE_ENDIAN
	((T *)block)[0] = a;
	((T *)block)[1] = b;
#else
	((T *)block)[0] = byteReverse(a);
	((T *)block)[1] = byteReverse(b);
#endif
}

// Fetch 4 words from user's buffer into "a", "b", "c", "d" in BIG-endian order
template <class T>
inline void GetBlockBigEndian(const byte *block, T &a, T &b, T &c, T &d)
{
#ifndef IS_LITTLE_ENDIAN
	a = ((T *)block)[0];
	b = ((T *)block)[1];
	c = ((T *)block)[2];
	d = ((T *)block)[3];
#else
	a = byteReverse(((T *)block)[0]);
	b = byteReverse(((T *)block)[1]);
	c = byteReverse(((T *)block)[2]);
	d = byteReverse(((T *)block)[3]);
#endif
}

// Put 4 words back into user's buffer in BIG-endian order
template <class T>
inline void PutBlockBigEndian(byte *block, T a, T b, T c, T d)
{
#ifndef IS_LITTLE_ENDIAN
	((T *)block)[0] = a;
	((T *)block)[1] = b;
	((T *)block)[2] = c;
	((T *)block)[3] = d;
#else
	((T *)block)[0] = byteReverse(a);
	((T *)block)[1] = byteReverse(b);
	((T *)block)[2] = byteReverse(c);
	((T *)block)[3] = byteReverse(d);
#endif
}

template <class T>
std::string WordToString(T value, bool highFirst = true)
{
	if (!CheckEndianess(highFirst))
		value = byteReverse(value);

	return std::string((char *)&value, sizeof(value));
}

template <class T>
T StringToWord(const std::string &str, bool highFirst = true)
{
	T value = 0;
	memcpy(&value, str.data(), STDMIN(sizeof(value), str.size()));
	return CheckEndianess(highFirst) ? value : byteReverse(value);
}

// ************** key length query ***************

/// support query of fixed key length
template <unsigned int N>
class FixedKeyLength
{
public:
	enum {KEYLENGTH=N, MIN_KEYLENGTH=N, MAX_KEYLENGTH=N, DEFAULT_KEYLENGTH=N};
	/// returns the key length
	static unsigned int KeyLength(unsigned int) {return KEYLENGTH;}
};

/// support query of variable key length, template parameters are default, min, max, multiple (default multiple 1)
template <unsigned int D, unsigned int N, unsigned int M, unsigned int Q=1>
class VariableKeyLength
{
public:
	enum {MIN_KEYLENGTH=N, MAX_KEYLENGTH=M, DEFAULT_KEYLENGTH=D, KEYLENGTH_MULTIPLE=Q};
	/// returns the smallest valid key length in bytes that is >= min(n, MAX_KEYLENGTH)
	static unsigned int KeyLength(unsigned int n)
	{
		assert(KEYLENGTH_MULTIPLE > 0 && MIN_KEYLENGTH % KEYLENGTH_MULTIPLE == 0 && MAX_KEYLENGTH % KEYLENGTH_MULTIPLE == 0);
		if (n < MIN_KEYLENGTH)
			return MIN_KEYLENGTH;
		else if (n > MAX_KEYLENGTH)
			return MAX_KEYLENGTH;
		else
			return RoundUpToMultipleOf(n, KEYLENGTH_MULTIPLE);
	}
};

/// support query of key length that's the same as another class
template <class T>
class SameKeyLengthAs
{
public:
	enum {MIN_KEYLENGTH=T::MIN_KEYLENGTH, MAX_KEYLENGTH=T::MAX_KEYLENGTH, DEFAULT_KEYLENGTH=T::DEFAULT_KEYLENGTH};
	/// returns the smallest valid key length in bytes that is >= min(n, MAX_KEYLENGTH)
	static unsigned int KeyLength(unsigned int keylength)
		{return T::KeyLength(keylength);}
};

// ************** secure memory allocation ***************

#ifdef SECALLOC_DEFAULT
#define SecAlloc(type, number) (new type[(number)])
#define SecFree(ptr, number) (memset((ptr), 0, (number)*sizeof(*(ptr))), delete [] (ptr))
#else
#define SecAlloc(type, number) (new type[(number)])
#define SecFree(ptr, number) (delete [] (ptr))
#endif

//! a block of memory allocated using SecAlloc
template <class T> struct SecBlock
{
	explicit SecBlock(unsigned int size=0)
		: size(size) {ptr = SecAlloc(T, size);}
	SecBlock(const SecBlock<T> &t)
		: size(t.size) {ptr = SecAlloc(T, size); memcpy(ptr, t.ptr, size*sizeof(T));}
	SecBlock(const T *t, unsigned int len)
		: size(len) {ptr = SecAlloc(T, len); memcpy(ptr, t, len*sizeof(T));}
	~SecBlock()
		{SecFree(ptr, size);}

#if defined(__GNUC__) || defined(__BCPLUSPLUS__)
	operator const void *() const
		{return ptr;}
	operator void *()
		{return ptr;}
#endif
#if defined(__GNUC__)	// reduce warnings
	operator const void *()
		{return ptr;}
#endif

	operator const T *() const
		{return ptr;}
	operator T *()
		{return ptr;}
#if defined(__GNUC__)	// reduce warnings
	operator const T *()
		{return ptr;}
#endif

// CodeWarrior defines _MSC_VER
#if !defined(_MSC_VER) || defined(__MWERKS__)
	template <typename I>
	T *operator +(I offset)
		{return ptr+offset;}

	template <typename I>
	const T *operator +(I offset) const
		{return ptr+offset;}

	template <typename I>
	T& operator[](I index)
		{assert((unsigned int)index<size); return ptr[index];}

	template <typename I>
	const T& operator[](I index) const
		{assert((unsigned int)index<size); return ptr[index];}
#endif

	const T* Begin() const
		{return ptr;}
	T* Begin()
		{return ptr;}
	const T* End() const
		{return ptr+size;}
	T* End()
		{return ptr+size;}

	unsigned int Size() const {return size;}

	void Assign(const T *t, unsigned int len)
	{
		New(len);
		memcpy(ptr, t, len*sizeof(T));
	}

	void Assign(const SecBlock<T> &t)
	{
		New(t.size);
		memcpy(ptr, t.ptr, size*sizeof(T));
	}

	SecBlock& operator=(const SecBlock<T> &t)
	{
		Assign(t);
		return *this;
	}

	bool operator==(const SecBlock<T> &t) const
	{
		return size == t.size && memcmp(ptr, t.ptr, size*sizeof(T)) == 0;
	}

	bool operator!=(const SecBlock<T> &t) const
	{
		return !operator==(t);
	}

	void New(unsigned int newSize)
	{
		if (newSize != size)
		{
			T *newPtr = SecAlloc(T, newSize);
			SecFree(ptr, size);
			ptr = newPtr;
			size = newSize;
		}
	}

	void CleanNew(unsigned int newSize)
	{
		if (newSize != size)
		{
			T *newPtr = SecAlloc(T, newSize);
			SecFree(ptr, size);
			ptr = newPtr;
			size = newSize;
		}
		memset(ptr, 0, size*sizeof(T));
	}

	void Grow(unsigned int newSize)
	{
		if (newSize > size)
		{
			T *newPtr = SecAlloc(T, newSize);
			memcpy(newPtr, ptr, size*sizeof(T));
			SecFree(ptr, size);
			ptr = newPtr;
			size = newSize;
		}
	}

	void CleanGrow(unsigned int newSize)
	{
		if (newSize > size)
		{
			T *newPtr = SecAlloc(T, newSize);
			memcpy(newPtr, ptr, size*sizeof(T));
			memset(newPtr+size, 0, (newSize-size)*sizeof(T));
			SecFree(ptr, size);
			ptr = newPtr;
			size = newSize;
		}
	}

	void Resize(unsigned int newSize)
	{
		if (newSize != size)
		{
			T *newPtr = SecAlloc(T, newSize);
			memcpy(newPtr, ptr, STDMIN(newSize, size)*sizeof(T));
			SecFree(ptr, size);
			ptr = newPtr;
			size = newSize;
		}
	}

	void swap(SecBlock<T> &b);

	unsigned int size;
	T *ptr;
};

template <class T> void SecBlock<T>::swap(SecBlock<T> &b)
{
	std::swap(size, b.size);
	std::swap(ptr, b.ptr);
}

typedef SecBlock<byte> SecByteBlock;
typedef SecBlock<word> SecWordBlock;

NAMESPACE_END

NAMESPACE_BEGIN(std)
template <class T>
inline void swap(CryptoPP::SecBlock<T> &a, CryptoPP::SecBlock<T> &b)
{
	a.swap(b);
}

NAMESPACE_END

#endif // MISC_H
