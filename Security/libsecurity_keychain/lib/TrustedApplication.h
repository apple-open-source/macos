/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
// TrustedApplication.h - TrustedApplication control wrappers
//
#ifndef _SECURITY_TRUSTEDAPPLICATION_H_
#define _SECURITY_TRUSTEDAPPLICATION_H_

#include <Security/SecTrustedApplication.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/acl_codesigning.h>
#include <security_utilities/seccfobject.h>
#include "SecCFTypes.h"


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
	SECCFFUNCTIONS(TrustedApplication, SecTrustedApplicationRef, errSecInvalidItemRef, gTypes().TrustedApplication)

	TrustedApplication(const TypedList &subject);	// from ACL subject form
	TrustedApplication(const std::string &path);	// from code on disk
	TrustedApplication();							// for current application
	TrustedApplication(const std::string &path, SecRequirementRef requirement); // with requirement and aux. path
	TrustedApplication(CFDataRef external);			// from external representation
	~TrustedApplication();

	const char *path() const { return mForm->path().c_str(); }
	CssmData legacyHash() const	{ return CssmData::wrap(mForm->legacyHash(), SHA1::digestLength); }
	SecRequirementRef requirement() const { return mForm->requirement(); }

	void data(CFDataRef data);
	CFDataRef externalForm() const;

	CssmList makeSubject(Allocator &allocator);

	bool verifyToDisk(const char *path);		// verify against on-disk image

private:
	RefPointer<CodeSignatureAclSubject> mForm;
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
