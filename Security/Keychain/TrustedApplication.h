/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// TrustedApplication.h - TrustedApplication control wrappers
//
#ifndef _SECURITY_TRUSTEDAPPLICATION_H_
#define _SECURITY_TRUSTEDAPPLICATION_H_

#include <Security/SecRuntime.h>
#include <Security/SecTrustedApplication.h>
#include <Security/cssmdata.h>
#include <Security/cssmaclpod.h>


namespace Security {
namespace KeychainCore {


//
// TrustedApplication actually denotes a signed executable
// on disk as used by the ACL subsystem. Much useful
// information is encapsulated in the 'comment' field that
// is stored with the ACL subject. TrustedApplication does
// not interpret this value, leaving its meaning to its caller.
//
class TrustedApplication : public SecCFObject {
	NOCOPY(TrustedApplication)
public:
	SECCFFUNCTIONS(TrustedApplication, SecTrustedApplicationRef, errSecInvalidItemRef)

	TrustedApplication(const TypedList &subject);
	TrustedApplication(const CssmData &signature, const CssmData &comment);
	TrustedApplication(const char *path);
	TrustedApplication();	// for current application
    virtual ~TrustedApplication() throw();

	const CssmData &signature() const;

	// data (aka "comment") access
	const CssmData &data() const	{ return mData; }
	const char *path() const;
	template <class Data>
	void data(const Data &data)		{ mData = data; }
	
	TypedList makeSubject(CssmAllocator &allocator);

	bool sameSignature(const char *path); // return true if object at path has same signature
	
protected:
	void calcSignature(const char *path, CssmOwnedData &signature); // generate a signature

private:
	CssmAutoData mSignature;
	CssmAutoData mData;
};


//
// A simple implementation of a caching path database in the system.
//
class PathDatabase {
public:
    PathDatabase(const char *path = "/var/db/CodeEquivalenceCandidates");

    bool operator [] (const std::string &path)
        { return mQualifyAll || lookup(path); }

private:
    bool mQualifyAll;
    set<std::string> mPaths;
	
	bool lookup(const std::string &path);
};


} // end namespace KeychainCore
} // end namespace Security

#endif // !_SECURITY_TRUSTEDAPPLICATION_H_
