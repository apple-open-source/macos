/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// codesigning - support for signing and verifying "bags o' bits" on disk.
//
// This file defines CodeSigner objects that sign, SignableCode objects
// that can be signed, and CodeSignature objects that represent signatures.
// Anything that can be "enumerated" into a stream of bits is fair game as
// a SignableCode, though the primary intent is to sign files or directories
// of files on disk.
//
#ifndef _CODESIGNING
#define _CODESIGNING

#include <Security/utilities.h>
#include <string>

#ifdef _CPP_CODESIGNING
#pragma export on
#endif

#undef verify

namespace Security
{

namespace CodeSigning
{

//
// Type codes for signatures. Each represents a particular type of signature.
//
enum {
	standardOSXSignature = 1			// standard MacOS X signature (SHA1)
};


//
// A CodeSignature is an abstract object representing a complete signature.
// You may think of this as a cryptographic hash of some kind together with
// type information and enough abstraction to make changing the algorithms
// easier.
//
class Signature {
public:
	virtual ~Signature() { }
	
	virtual bool operator == (const Signature &other) const = 0;
	bool operator != (const Signature &other) const		{ return !(*this == other); }
	
	virtual uint32 type() const = 0;		// yield type code
	virtual const void *data() const = 0;	// yield data pointer
	virtual size_t length() const = 0;		// yield length of data
};


//
// A Signer is the engine that can sign and verify. It may have configuration,
// but it should have NO state that carries over between signing/verifying
// operations. In other words, once a signing/verifyng operation is complete,
// the signer should forget about what it did.
//
class Signer {
	friend class Signable;
public:
	virtual ~Signer() { }
	
public:
	class State {
	public:
		virtual void enumerateContents(const void *data, size_t length) = 0;
		
		Signer &signer;
		
	protected:
		State(Signer &sgn) : signer(sgn) { }
	};
	
public:
	virtual Signature *sign(const Signable &target) = 0;
	virtual bool verify(const Signable &target, const Signature *signature) = 0;
	
	virtual Signature *restore(uint32 type, const void *data, size_t length) = 0;
	Signature *restore(uint32 type, const CssmData &data)
	{ return restore(type, data.data(), data.length()); }

protected:
	void scanContents(State &state, const Signable &target);
};


//
// A Signable object represents something that can be signed
//
class Signable {
	friend class Signer;
public:
	virtual ~Signable() { }
	
	Signature *sign(Signer &signer) const
	{ return signer.sign(*this); }
	bool verify(const Signature *signature, Signer &signer) const
	{ return signer.verify(*this, signature); }

protected:
	virtual void scanContents(Signer::State &state) const = 0;
};


//
// Close mutually recursive calls
//
inline void Signer::scanContents(State &state, const Signable &target)
{
	target.scanContents(state);
}

} // end namespace CodeSigning

} // end namespace Security

#ifdef _CPP_CODESIGNING
#pragma export off
#endif


#endif //_CODESIGNING
