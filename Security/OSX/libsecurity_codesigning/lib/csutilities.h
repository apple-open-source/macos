/*
 * Copyright (c) 2006-2013 Apple Inc. All Rights Reserved.
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
// csutilities - miscellaneous utilities for the code signing implementation
//
// This is a collection of odds and ends that wouldn't fit anywhere else.
// The common theme is that the contents are otherwise naturally homeless.
//
#ifndef _H_CSUTILITIES
#define _H_CSUTILITIES

#include <Security/Security.h>
#include <security_utilities/dispatch.h>
#include <security_utilities/hashing.h>
#include <security_utilities/unix++.h>
#if TARGET_OS_OSX
#include <security_cdsa_utilities/cssmdata.h>
#endif
#include <copyfile.h>
#include <asl.h>
#include <cstdarg>

namespace Security {
namespace CodeSigning {


//
// Test for the canonical Apple CA certificate
//
bool isAppleCA(SecCertificateRef cert);


//
// Calculate canonical hashes of certificate.
// This is simply defined as (always) the SHA1 hash of the DER.
//
void hashOfCertificate(const void *certData, size_t certLength, SHA1::Digest digest);
void hashOfCertificate(SecCertificateRef cert, SHA1::Digest digest);
bool verifyHash(SecCertificateRef cert, const Hashing::Byte *digest);

	
inline size_t scanFileData(UnixPlusPlus::FileDesc fd, size_t limit, void (^handle)(const void *buffer, size_t size))
{
	UnixPlusPlus::FileDesc::UnixStat st;
	size_t total = 0;
	unsigned char *buffer = NULL;

	try {
		fd.fstat(st);
		size_t bufSize = MAX(64 * 1024, st.st_blksize);
		buffer = (unsigned char *)valloc(bufSize);
		if (!buffer)
			return 0;

		for (;;) {
			size_t size = bufSize;
			if (limit && limit < size)
				size = limit;
			size_t got = fd.read(buffer, size);
			total += got;
			if (fd.atEnd())
				break;
			handle(buffer, got);
			if (limit && (limit -= got) == 0)
				break;
		}
	}
	catch(...) {
		/* don't leak this on error */
		if (buffer)
			free(buffer);
		throw;
	}
	
	free(buffer);
	return total;
}


//
// Calculate hashes of (a section of) a file.
// Starts at the current file position.
// Extends to end of file, or (if limit > 0) at most limit bytes.
// Returns number of bytes digested.
//
template <class _Hash>
size_t hashFileData(UnixPlusPlus::FileDesc fd, _Hash *hasher, size_t limit = 0)
{
	return scanFileData(fd, limit, ^(const void *buffer, size_t size) {
		hasher->update(buffer, size);
	});
}

template <class _Hash>
size_t hashFileData(const char *path, _Hash *hasher)
{
	UnixPlusPlus::AutoFileDesc fd(path);
	return hashFileData(fd, hasher);
}
	

//
// Check to see if a certificate contains a particular field, by OID. This works for extensions,
// even ones not recognized by the local CL. It does not return any value, only presence.
//

#if TARGET_OS_OSX
bool certificateHasField(SecCertificateRef cert, const CSSM_OID &oid);
bool certificateHasPolicy(SecCertificateRef cert, const CSSM_OID &policyOid);
CFDateRef certificateCopyFieldDate(SecCertificateRef cert, const CSSM_OID &policyOid);
#endif

//
// Encapsulation of the copyfile(3) API.
// This is slated to go into utilities once stable.
//
class Copyfile {
public:
	Copyfile();
	~Copyfile()	{ copyfile_state_free(mState); }
	
	operator copyfile_state_t () const { return mState; }
	
	void set(uint32_t flag, const void *value);
	void get(uint32_t flag, void *value);
	
	void operator () (const char *src, const char *dst, copyfile_flags_t flags);

private:
	void check(int rc);
	
private:
	copyfile_state_t mState;
};


//
// MessageTracer support
//
class MessageTrace {
public:
	MessageTrace(const char *domain, const char *signature);
	~MessageTrace() { ::asl_free(mAsl); }
	void add(const char *key, const char *format, ...) __attribute__((format(printf,3,4)));
	void send(const char *format, ...) __attribute__((format(printf,2,3)));

private:
	aslmsg mAsl;
};


//
// A reliable uid set/reset bracket
//
class UidGuard {
public:
	UidGuard() : mPrevious(-1) { }
	UidGuard(uid_t uid) : mPrevious(-1) { (void)seteuid(uid); }
	~UidGuard()
	{
		if (active())
			UnixError::check(::seteuid(mPrevious));
	}
	
	bool seteuid(uid_t uid)
	{
		if (uid == geteuid())
			return true;	// no change, don't bother the kernel
		if (!active())
			mPrevious = ::geteuid();
		return ::seteuid(uid) == 0;
	}
	
	bool active() const { return mPrevious != uid_t(-1); }
	operator bool () const { return active(); }
	uid_t saved() const { assert(active()); return mPrevious; }

private:
	uid_t mPrevious;
};


// This class provides resource limited parallelization,
// used for work on nested bundles (e.g. signing or validating them).

// We only spins off async workers if they are available right now,
// otherwise we continue synchronously in the current thread.
// This is important because we must progress at all times, otherwise
// deeply nested bundles will deadlock on waiting for resource validation,
// with no available workers to actually do so.
// Their nested resources, however, may again spin off async workers if
// available.

class LimitedAsync {
	NOCOPY(LimitedAsync)
public:
	LimitedAsync(bool async);
	LimitedAsync(LimitedAsync& limitedAsync);
	virtual ~LimitedAsync();

	bool perform(Dispatch::Group &groupRef, void (^block)());

private:
	Dispatch::Semaphore *mResourceSemaphore;
};

// Check if the path is on the root filesystem, protected by the OS.
bool isOnRootFilesystem(const char *path);

// Check if a path exists.
bool pathExists(const char *path);

// Check if the path name represents an extended attribute file (on file systems which don't support
// them natively).
bool pathMatchesXattrFilenameSpec(const char *path);

// Check if path is a regular file.
bool pathIsRegularFile(const char *path);

// Check if a path has any extended attributes.
bool pathHasXattrs(const char *path);

// Check if the path is on a file system that requires files to store extended attributes.
bool pathFileSystemUsesXattrFiles(const char *path);

// Check if path is a valid extended attribute file.
bool pathIsValidXattrFile(const string fullPath, const char *scope = "csutilities");

// Check whether the provided fullPath is prefixed by the prefixPath on a directory boundary.
// Also rejects if the prefixPath is a perfect match since its no longer a strict prefix.
bool isPathPrefix(string prefixPath, string fullPath);

// Retrieves the path remaining of fullPath after the prefixPath is removed, including any leading /'s.
string pathRemaining(string fullPath, string prefix);

// Iterates the path by removing the last path component and calling the handler, which returns
// whether to continue iterating.  Returns whether any pathHandler call resulted
// in stopping the iteration.
bool iterateLargestSubpaths(string path, bool (^pathHandler)(string));

} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_CSUTILITIES
