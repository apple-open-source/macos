/*
 * Copyright (c) 2005-2010 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

//
// Fast hash support
//
#ifndef _H_HASHING
#define _H_HASHING

#include <CommonCrypto/CommonDigest.h>
#include <cstring>
#include <memory>
#include <sys/types.h>
#include <CommonCrypto/CommonDigestSPI.h>	// SPI slated to become API

namespace Security {


//
// An unconditional base class for all hash objects.
// Not much in here; there's no point in declaring one yourself.
//
class Hashing {
public:
	typedef unsigned char Byte;
};


//
// Common prefix for all static hashers.
// There's no magic here; we're just setting up a regular framework for duck typing.
//
// If you write template code based on "any static hasher", you can directly tap here
// (and learn the actual hash in use through the match on _HashType). But note that
// a DynamicHash is not a subclass of Hash, though a DynamicHashInstance will be, duck-like.
//
template <uint32_t _size, class _HashType>
class Hash : public Hashing {
	_HashType &_self() { return static_cast<_HashType &>(*this); }	// "my real type"
public:
	static const size_t digestLength = _size;			// how many bytes in my digest?
	typedef Byte Digest[_size];							// my digest as a Byte array
	struct SDigest {									// my digest as a struct
		Digest data;
		
		SDigest() { }
		SDigest(const Byte *source) { ::memcpy(data, source, digestLength); }
		friend bool operator < (const SDigest &x, const SDigest &y)	// usable as collection key
			{ return ::memcmp(x.data, y.data, digestLength) < 0; }
		bool operator == (const SDigest &other) const
			{ return ::memcmp(this->data, other.data, digestLength) == 0; }
		bool operator != (const SDigest &other) const
			{ return ::memcmp(this->data, other.data, digestLength) != 0; }
		bool operator == (const Byte *other) const
			{ return ::memcmp(this->data, other, digestLength) == 0; }
		bool operator != (const Byte *other) const
			{ return ::memcmp(this->data, other, digestLength) != 0; }
	};
	
	void operator () (const void *data, size_t length)	// just an alias for update()
		{ _self().update(data, length); }
	
	void finish(SDigest &digest)
		{ _self().finish(digest.data); }
	
	bool verify(const Byte *digest)
		{ Digest d; _self().finish(d); return memcmp(d, digest, digestLength) == 0; }
};


//
// A dynamic switch for digest generators.
// This isn't a subclass of Hash (which is static-fast), but it's duck-typed to it.
// Note that digestLength is a function here, not a constant. Obviously.
//
class DynamicHash : public Hashing {
public:
	virtual ~DynamicHash();
	
	virtual size_t digestLength() const = 0;
	virtual void update(const void *data, size_t length) = 0;
	virtual void finish(Byte *digest) = 0;
	
	void operator () (const void *data, size_t length)
		{ return this->update(data, length); }

	bool verify(const Byte *digest)
		{ Byte d[this->digestLength()]; this->finish(d); return memcmp(d, digest, this->digestLength()) == 0; }
};


//
// Make a DynamicHash from a static Hash class.
//
template <class _HashType>
class DynamicHashInstance : public DynamicHash, public _HashType {
public:
	// (wish we had C++0x already...)
	DynamicHashInstance() { }
	template <class Arg1>
	DynamicHashInstance(const Arg1 &arg1) : _HashType(arg1) { }
	template <class Arg1, class Arg2>
	DynamicHashInstance(const Arg1 &arg1, const Arg2 &arg2) : _HashType(arg1, arg2) { }
	template <class Arg1, class Arg2, class Arg3>
	DynamicHashInstance(const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3) : _HashType(arg1, arg2, arg3) { }

	size_t digestLength() const
		{ return _HashType::digestLength; }
	void update(const void *data, size_t length)
		{ return _HashType::update(data, length); }
	void finish(unsigned char *digest)
		{ return _HashType::finish(digest); }
};


//
// Make a DynamicHash from a CommonCrypto hash algorithm identifier
//
class CCHashInstance : public DynamicHash {
public:
	CCHashInstance(CCDigestAlg alg);
	~CCHashInstance()
		{ CCDigestDestroy(mDigest); }
	
	size_t digestLength() const
		{ return CCDigestOutputSize(mDigest); }
	void update(const void *data, size_t length)
		{ CCDigestUpdate(mDigest, data, length); }
	void finish(unsigned char *digest)
		{ CCDigestFinal(mDigest, digest); }
	
private:
	CCDigestRef mDigest;
};


//
// A shorthand for holding a DynamicHash subclass we got from some
// object out there by asking nicely (by default, calling its getHash() method).
//
template <class _Giver, DynamicHash *(_Giver::*_fetcher)() const = &_Giver::getHash>
class MakeHash : public std::auto_ptr<DynamicHash> {
public:
	MakeHash(const _Giver *giver) : std::auto_ptr<DynamicHash>((giver->*_fetcher)()) { }
	
	operator DynamicHash *() const { return this->get(); }
};


//
// A concrete SHA1 class, used in a very many different places.
// Note that its digestLength is a constant (not a function).
//
class SHA1 : public CC_SHA1_CTX, public Hash<CC_SHA1_DIGEST_LENGTH, SHA1>	{
public:
	SHA1() { CC_SHA1_Init(this); }
	void update(const void *data, size_t length)
		{ CC_SHA1_Update(this, data, length); }
	void finish(Byte *digest) { CC_SHA1_Final(digest, this); }
	using Hash<CC_SHA1_DIGEST_LENGTH, SHA1>::finish;
};


}	// Security

#endif //_H_HASHING
