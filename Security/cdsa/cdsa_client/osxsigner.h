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
// osxsigner - MacOS X's standard code signing algorithm.
//
#ifndef _H_OSXSIGNER
#define _H_OSXSIGNER

#include <Security/osxsigning.h>
#include <Security/cspclient.h>
#include <string>

#ifdef _CPP_OSXSIGNER
#pragma export on
#endif

namespace Security
{

namespace CodeSigning
{

//
// The OSX standard signer object
//
class OSXSigner : public Signer {
	class Digester; friend class Digester;
public:
	class OSXSignature;

	OSXSigner();
	OSXSignature *sign(const Signable &target);
	bool verify(const Signable &target, const Signature *signature);
	
	OSXSignature *restore(uint32 type, const void *data, size_t length);

public:
	class OSXSignature : public Signature {
	public:
		static const size_t hashLength = 20;	// length of signature data
		typedef uint8 Hash[hashLength];
		
		OSXSignature(const void *src)	{ memcpy(mData, src, hashLength); }
		
		bool operator == (const Signature &other) const
		{
			if (const OSXSignature *sig = dynamic_cast<const OSXSignature *>(&other))
				return !memcmp(mData, sig->mData, hashLength);
			else
				return false;
		}
		
		bool operator == (void *bytes) const
		{ return !memcmp(mData, bytes, hashLength); }
		
		uint32 type() const		{ return standardOSXSignature; }
		const void *data() const { return mData; }
		size_t length() const	{ return hashLength; }
	
	private:
		uint8 mData[hashLength];
	};
	
private:
	class Digester : public State, public CssmClient::Digest {
	public:
		Digester(OSXSigner &sgn) : State(sgn), CssmClient::Digest(sgn.csp, CSSM_ALGID_SHA1) { }
		
		void enumerateContents(const void *addr, size_t length);
	};

private:
	// CDSA resources
	CssmClient::CSP csp;
};

} // end namespace CodeSigning

} // end namespace Security

#ifdef _CPP_OSXSIGNER
#pragma export off
#endif


#endif //_H_OSXSIGNER
