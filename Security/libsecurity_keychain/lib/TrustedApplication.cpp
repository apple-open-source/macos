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
// TrustedApplication.cpp
//
#include <security_keychain/TrustedApplication.h>
#include <security_keychain/ACL.h>
#include <security_utilities/osxcode.h>
#include <security_utilities/trackingallocator.h>
#include <security_cdsa_utilities/acl_codesigning.h>
#include <sys/syslimits.h>
#include <memory>

using namespace KeychainCore;


//
// Create a TrustedApplication from a code-signing ACL subject.
// Throws ACL::ParseError if the subject is unexpected.
//
TrustedApplication::TrustedApplication(const TypedList &subject)
{
	try {
		CodeSignatureAclSubject::Maker maker;
		mForm = maker.make(subject);
		secdebug("trustedapp", "%p created from list form", this);
		IFDUMPING("codesign", mForm->AclSubject::dump("STApp created from list"));
	} catch (...) {
		throw ACL::ParseError();
	}
}


//
// Create a TrustedApplication from a path-to-object-on-disk
//
TrustedApplication::TrustedApplication(const std::string &path)
{
	RefPointer<OSXCode> code(OSXCode::at(path));
	mForm = new CodeSignatureAclSubject(OSXVerifier(code));
	secdebug("trustedapp", "%p created from path %s", this, path.c_str());
	IFDUMPING("codesign", mForm->AclSubject::dump("STApp created from path"));
}


//
// Create a TrustedApplication for the calling process
//
TrustedApplication::TrustedApplication()
{
	//@@@@ should use CS's idea of "self"
	RefPointer<OSXCode> me(OSXCode::main());
	mForm = new CodeSignatureAclSubject(OSXVerifier(me));
	secdebug("trustedapp", "%p created from self", this);
	IFDUMPING("codesign", mForm->AclSubject::dump("STApp created from self"));
}


//
// Create a TrustedApplication from a SecRequirementRef.
// Note that the path argument is only stored for documentation;
// it is NOT used to denote anything on disk.
//
TrustedApplication::TrustedApplication(const std::string &path, SecRequirementRef reqRef)
{
	CFRef<CFDataRef> reqData;
	MacOSError::check(SecRequirementCopyData(reqRef, kSecCSDefaultFlags, &reqData.aref()));
	mForm = new CodeSignatureAclSubject(NULL, path);
	mForm->add((const BlobCore *)CFDataGetBytePtr(reqData));
	secdebug("trustedapp", "%p created from path %s and requirement %p",
		this, path.c_str(), reqRef);
	IFDUMPING("codesign", mForm->debugDump());
}


TrustedApplication::~TrustedApplication()
{ /* virtual */ }


//
// Convert from/to external data form.
//
// Since a TrustedApplication's data is essentially a CodeSignatureAclSubject,
// we just use the subject's externalizer to produce the data. That requires us
// to use the somewhat idiosyncratic linearizer used by CSSM ACL subjects, but
// that's a small price to pay for consistency.
//
TrustedApplication::TrustedApplication(CFDataRef external)
{
	AclSubject::Reader pubReader(CFDataGetBytePtr(external)), privReader;
	mForm = CodeSignatureAclSubject::Maker().make(0, pubReader, privReader);
}

CFDataRef TrustedApplication::externalForm() const
{
	AclSubject::Writer::Counter pubCounter, privCounter;
	mForm->exportBlob(pubCounter, privCounter);
	if (privCounter > 0)	// private exported data - format violation
		CssmError::throwMe(CSSMERR_CSSM_INTERNAL_ERROR);
	CFRef<CFMutableDataRef> data = CFDataCreateMutable(NULL, pubCounter);
	CFDataSetLength(data, pubCounter);
	if (CFDataGetLength(data) < CFIndex(pubCounter))
		CFError::throwMe();
	AclSubject::Writer pubWriter(CFDataGetMutableBytePtr(data)), privWriter;
	mForm->exportBlob(pubWriter, privWriter);
	return data.yield();
}

void TrustedApplication::data(CFDataRef data)
{
	const char *p = (const char *)CFDataGetBytePtr(data);
	const std::string path(p, p + CFDataGetLength(data));
	RefPointer<OSXCode> code(OSXCode::at(path));
	mForm = new CodeSignatureAclSubject(OSXVerifier(code));
}

//
// Direct verification interface.
// If path == NULL, we verify against the running code itself.
//
bool TrustedApplication::verifyToDisk(const char *path)
{
	if (SecRequirementRef requirement = mForm->requirement()) {
		secdebug("trustedapp", "%p validating requirement against path %s", this, path);
		CFRef<SecStaticCodeRef> ondisk;
		if (path)
			MacOSError::check(SecStaticCodeCreateWithPath(CFTempURL(path),
				kSecCSDefaultFlags, &ondisk.aref()));
		else
			MacOSError::check(SecCodeCopySelf(kSecCSDefaultFlags, (SecCodeRef *)&ondisk.aref()));
		return SecStaticCodeCheckValidity(ondisk, kSecCSDefaultFlags, requirement) == errSecSuccess;
	} else {
		secdebug("trustedapp", "%p validating hash against path %s", this, path);
		RefPointer<OSXCode> code = path ? OSXCode::at(path) : OSXCode::main();
		SHA1::Digest ondiskDigest;
		OSXVerifier::makeLegacyHash(code, ondiskDigest);
		return memcmp(ondiskDigest, mForm->legacyHash(), sizeof(ondiskDigest)) == 0;
	}
}


//
// Produce a TypedList representing a code-signing ACL subject
// for this application.
// Memory is allocated from the allocator given, and belongs to
// the caller.
//
CssmList TrustedApplication::makeSubject(Allocator &allocator)
{
	return mForm->toList(allocator);
}


//
// On a completely different note...
// Read a simple text file from disk and cache the lines in a set.
// This is used during re-prebinding to cut down on the number of
// equivalency records being generated.
// This feature is otherwise completely unconnected to anything else here.
//
PathDatabase::PathDatabase(const char *path)
{
    if (FILE *f = fopen(path, "r")) {
        mQualifyAll = false;
        char path[PATH_MAX+1];
        while (fgets(path, sizeof(path), f)) {
            path[strlen(path)-1] = '\0';	// strip NL
            mPaths.insert(path);
        }
		fclose(f);
        secdebug("equivdb", "read %ld paths from %s", mPaths.size(), path);
    } else {
        mQualifyAll = true;
        secdebug("equivdb", "cannot open %s, will qualify all application paths", path);
    }
}


bool PathDatabase::lookup(const string &inPath)
{
	string path = inPath;
	string::size_type lastSlash = path.rfind('/');
	string::size_type bundleCore = path.find("/Contents/MacOS/");
	if (lastSlash != string::npos && bundleCore != string::npos)
		if (bundleCore + 15 == lastSlash)
			path = path.substr(0, bundleCore); // @@@ path is being modified here so it can't be const.
	return mPaths.find(path) != mPaths.end();
}
