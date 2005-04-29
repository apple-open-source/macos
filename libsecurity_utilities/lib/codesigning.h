/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// codesigning - support for signing and verifying "bags o' bits" on disk.
//
// This file defines CodeSigner objects that sign, SignableCode objects
// that can be signed, and CodeSignature objects that represent signatures.
// Anything that can be "enumerated" into a stream of bits is fair game as
// a SignableCode, though the primary intent is to sign files or directories
// of files on disk.
//
#ifndef _H_CODESIGNING
#define _H_CODESIGNING

#include <string>

namespace Security {
namespace CodeSigning {


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
	
	virtual u_int32_t type() const = 0;		// yield type code
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
	
	virtual Signature *restore(u_int32_t type, const void *data, size_t length) = 0;
	
	template <class Data>
	Signature *restore(u_int32_t type, const Data &data)
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


#endif //_H_CODESIGNING
