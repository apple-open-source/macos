/*
 * Copyright (c) 2000-2001,2011,2013-2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_OSXVERIFIER
#define _H_OSXVERIFIER

#include <security_utilities/hashing.h>
#include <security_utilities/osxcode.h>
#include <security_utilities/blob.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <Security/CodeSigning.h>
#include <string>
#include <map>

namespace Security {


//
// A standard OS X style signature verifier.
// This encapsulates the different modes of signing/verifying currently
// supported. It knows nothing about the way this is represented in
// keychain access control lists; this knowledge resides exclusively
// in acl_codesigning.
//
class OSXVerifier {
public:
	static const size_t legacyHashLimit = 16 * 1024;
	static const uint32_t commentAlignment = 4;
	
public:
	// make a Verifier from a code reference object
	OSXVerifier(OSXCode *code);		// makes both legacy hash and SecRequirement
	OSXVerifier(const SHA1::Byte *hash, const std::string &path); // just hash
	~OSXVerifier();

	// components
	const unsigned char *legacyHash() const { return mLegacyHash; }
	const std::string& path() const { return mPath; }
	SecRequirementRef requirement() const { return mRequirement; }

public:
	// handle other (not explicitly understood) information in the verifier
	class AuxMap : public std::map<BlobCore::Magic, BlobCore *> {
	public:
		AuxMap() { }
		AuxMap(const AuxMap &src);
		~AuxMap();
	};

	AuxMap::const_iterator beginAux() const { return mAuxiliary.begin(); }
	AuxMap::const_iterator endAux() const { return mAuxiliary.end(); }
	
	void add(const BlobCore *info);
	const BlobCore *find(BlobCore::Magic magic);

	template <class BlobType>
	static const BlobType *find()
	{ return static_cast<BlobType *>(find(BlobType::typeMagic)); }
	
public:
	static void makeLegacyHash(OSXCode *code, SHA1::Digest digest);

	IFDUMP(void dump() const);
	
private:
	SHA1::Digest mLegacyHash;		// legacy page hash
	std::string mPath;				// path to originating code (comment)
	CFCopyRef<SecRequirementRef> mRequirement; // CS-style requirement
	AuxMap mAuxiliary;				// other data (does not include mRequirement)
};

} // end namespace Security


#endif //_H_OSXVERIFIER
