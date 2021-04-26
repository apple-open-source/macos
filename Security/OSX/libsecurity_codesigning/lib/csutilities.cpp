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

#include "csutilities.h"
#include <libDER/DER_Encode.h>
#include <libDER/DER_Keys.h>
#include <libDER/asn1Types.h>
#include <libDER/oids.h>
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/SecAsn1Templates.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificate.h>
#include <Security/SecPolicyPriv.h>
#include <utilities/SecAppleAnchorPriv.h>
#include <utilities/SecInternalReleasePriv.h>
#include "requirement.h"
#include <security_utilities/hashing.h>
#include <security_utilities/debugging.h>
#include <security_utilities/errors.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <errno.h>
#include <sys/attr.h>
#include <sys/xattr.h>
#include <libgen.h>
#include "debugging.h"

extern "C" {

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
 an absoluteTime if the date was valid and properly decoded.  Return
 NULL_TIME otherwise. */
CFAbsoluteTime SecAbsoluteTimeFromDateContent(DERTag tag, const uint8_t *bytes,
											  size_t length);

}
	
namespace Security {
namespace CodeSigning {


//
// Test for the canonical Apple CA certificate
//
bool isAppleCA(SecCertificateRef cert)
{
	SecAppleTrustAnchorFlags flags = 0;
	if (SecIsInternalRelease())
		flags |= kSecAppleTrustAnchorFlagsIncludeTestAnchors;
	return SecIsAppleTrustAnchor(cert, flags);
}


//
// Calculate the canonical hash of a certificate, given its raw (DER) data.
//
void hashOfCertificate(const void *certData, size_t certLength, SHA1::Digest digest)
{
	SHA1 hasher;
	hasher(certData, certLength);
	hasher.finish(digest);
}


//
// Ditto, given a SecCertificateRef
//
void hashOfCertificate(SecCertificateRef cert, SHA1::Digest digest)
{
	assert(cert);
    hashOfCertificate(SecCertificateGetBytePtr(cert), SecCertificateGetLength(cert), digest);
}


//
// One-stop hash-certificate-and-compare
//
bool verifyHash(SecCertificateRef cert, const Hashing::Byte *digest)
{
	SHA1::Digest dig;
	hashOfCertificate(cert, dig);
	return !memcmp(dig, digest, SHA1::digestLength);
}

#if TARGET_OS_OSX
//
// Check to see if a certificate contains a particular field, by OID. This works for extensions,
// even ones not recognized by the local CL. It does not return any value, only presence.
//
bool certificateHasField(SecCertificateRef cert, const CSSM_OID &oid)
{
	CFDataRef oidData = NULL;
	CFDataRef data = NULL;
	bool isCritical = false;
	bool matched = false;

	oidData = CFDataCreateWithBytesNoCopy(NULL, oid.Data, oid.Length,
	                                      kCFAllocatorNull);
	if (!(cert && oidData)) {
		goto out;
	}
	data = SecCertificateCopyExtensionValue(cert, oidData, &isCritical);
	if (data == NULL) {
		goto out;
	}
	matched = true;
out:
	if (data) {
		CFRelease(data);
	}
	if (oidData) {
		CFRelease(oidData);
	}
	return matched;
}


//
// Retrieve X.509 policy extension OIDs, if any.
// This currently ignores policy qualifiers.
//
bool certificateHasPolicy(SecCertificateRef cert, const CSSM_OID &policyOid)
{
	bool matched = false;
	CFDataRef oidData = CFDataCreateWithBytesNoCopy(NULL, policyOid.Data, policyOid.Length,
	                                      kCFAllocatorNull);
	if (!(cert && oidData)) {
		goto out;
	}
	matched = SecPolicyCheckCertCertificatePolicy(cert, oidData);
out:
	if (oidData) {
		CFRelease(oidData);
	}
	return matched;
}

	
CFDateRef certificateCopyFieldDate(SecCertificateRef cert, const CSSM_OID &policyOid)
{
	CFDataRef oidData = NULL;
	CFDateRef value = NULL;
	CFDataRef data = NULL;
	SecAsn1CoderRef coder = NULL;
	CSSM_DATA str = { 0 };
	CFAbsoluteTime time = 0.0;
	OSStatus status = 0;
	bool isCritical;
	
	oidData = CFDataCreateWithBytesNoCopy(NULL, policyOid.Data, policyOid.Length,
										  kCFAllocatorNull);
	
	if (oidData == NULL) {
		goto out;
	}
	
	data = SecCertificateCopyExtensionValue(cert, oidData, &isCritical);
	
	if (data == NULL) {
		goto out;
	}
	
	status = SecAsn1CoderCreate(&coder);
	if (status != 0) {
		goto out;
	}
	
	// We currently only support UTF8 strings.
	status = SecAsn1Decode(coder, CFDataGetBytePtr(data), CFDataGetLength(data),
						   kSecAsn1UTF8StringTemplate, &str);
	if (status != 0) {
		goto out;
	}
	
	time = SecAbsoluteTimeFromDateContent(ASN1_GENERALIZED_TIME,
										  str.Data, str.Length);
										  
	if (time == 0.0) {
		goto out;
	}

	value = CFDateCreate(NULL, time);
out:
	if (coder) {
		SecAsn1CoderRelease(coder);
	}
	if (data) {
		CFRelease(data);
	}
	if (oidData) {
		CFRelease(oidData);
	}
	
	return value;
}
#endif

//
// Copyfile
//
Copyfile::Copyfile()
{
	if (!(mState = copyfile_state_alloc()))
		UnixError::throwMe();
}
	
void Copyfile::set(uint32_t flag, const void *value)
{
	check(::copyfile_state_set(mState, flag, value));
}

void Copyfile::get(uint32_t flag, void *value)
{
	check(::copyfile_state_set(mState, flag, value));
}
	
void Copyfile::operator () (const char *src, const char *dst, copyfile_flags_t flags)
{
	check(::copyfile(src, dst, mState, flags));
}

void Copyfile::check(int rc)
{
	if (rc < 0)
		UnixError::throwMe();
}


//
// MessageTracer support
//
MessageTrace::MessageTrace(const char *domain, const char *signature)
{
	mAsl = asl_new(ASL_TYPE_MSG);
	if (domain)
		asl_set(mAsl, "com.apple.message.domain", domain);
	if (signature)
		asl_set(mAsl, "com.apple.message.signature", signature);
}

void MessageTrace::add(const char *key, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char value[200];
	vsnprintf(value, sizeof(value), format, args);
	va_end(args);
	asl_set(mAsl, (string("com.apple.message.") + key).c_str(), value);
}
	
void MessageTrace::send(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	asl_vlog(NULL, mAsl, ASL_LEVEL_NOTICE, format, args);
	va_end(args);
}



// Resource limited async workers for doing work on nested bundles
LimitedAsync::LimitedAsync(bool async)
{
	// validate multiple resources concurrently if bundle resides on solid-state media

	// How many async workers to spin off. If zero, validating only happens synchronously.
	long async_workers = 0;

	long ncpu = sysconf(_SC_NPROCESSORS_ONLN);

	if (async && ncpu > 0)
		async_workers = ncpu - 1; // one less because this thread also validates

	mResourceSemaphore = new Dispatch::Semaphore(async_workers);
}

LimitedAsync::LimitedAsync(LimitedAsync &limitedAsync)
{
	mResourceSemaphore = new Dispatch::Semaphore(*limitedAsync.mResourceSemaphore);
}

LimitedAsync::~LimitedAsync()
{
	delete mResourceSemaphore;
}

bool LimitedAsync::perform(Dispatch::Group &groupRef, void (^block)()) {
	__block Dispatch::SemaphoreWait wait(*mResourceSemaphore, DISPATCH_TIME_NOW);

	if (wait.acquired()) {
		dispatch_queue_t defaultQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

		groupRef.enqueue(defaultQueue, ^{
			// Hold the semaphore count until the worker is done validating.
			Dispatch::SemaphoreWait innerWait(wait);
			block();
		});
		return true;
	} else {
		block();
		return false;
	}
}

bool isOnRootFilesystem(const char *path)
{
	int rc = 0;
	struct statfs sfb;

	rc = statfs(path, &sfb);
	if (rc != 0) {
		secerror("Unable to check if path is on rootfs: %d, %s", errno, path);
		return false;
	}
	return ((sfb.f_flags & MNT_ROOTFS) == MNT_ROOTFS);
}

bool pathExists(const char *path)
{
	int rc;

	if (!path) {
		secerror("path is NULL");
		return false;
	}

	rc = access(path, F_OK);
	if (rc != 0) {
		if (errno != ENOENT) {
			secerror("Unable to check if path exists: %d, %s", errno, path);
		}
		return false;
	}

	return true;
}

bool pathMatchesXattrFilenameSpec(const char *path)
{
	char *baseName = NULL;
	bool ret = false;

	if (!path) {
		secerror("path is NULL");
		goto done;
	}

	// Extra byte for NULL storage.
	baseName = (char *)malloc(strlen(path) + 1);
	if (!baseName) {
		secerror("Unable to allocate space for storing basename: %d [%s]", errno, strerror(errno));
		goto done;
	}

	// basename_r will return a "/" if path is only slashes. It will return
	// a "." for a NULL/empty path. Both of these cases are handled by the logic
	// later. The only situation where basename_r will return a NULL is when path
	// is longer than MAXPATHLEN.

	if (basename_r(path, baseName) == NULL) {
		secerror("Could not get basename of %s: %d [%s]", path, errno, strerror(errno));
		goto done;
	}

	// The file name must start with "._", followed by the name
	// of the file for which it stores the xattrs. Hence, its length
	// must be at least three --> 2 for "._" and 1 for a non-empty file
	// name.
	if (strlen(baseName) < 3) {
		goto done;
	}

	if (baseName[0] != '.' || baseName[1] != '_') {
		goto done;
	}

	ret = true;

done:
	if (baseName) {
		free(baseName);
	}

	return ret;
}

bool pathIsRegularFile(const char *path)
{
	if (!path) {
		secerror("path is NULL");
		return false;
	}

	struct stat sb;
	if (stat(path, &sb)) {
		secerror("Unable to stat %s: %d [%s]", path, errno, strerror(errno));
		return false;
	}

	return (sb.st_mode & S_IFREG) == S_IFREG;
}

bool pathHasXattrs(const char *path)
{
	if (!path) {
		secerror("path is NULL");
		return false;
	}

	ssize_t xattrSize = listxattr(path, NULL, 0, 0);
	if (xattrSize == -1) {
		secerror("Unable to acquire the xattr list from %s", path);
		return false;
	}

	return (xattrSize > 0);
}

bool pathFileSystemUsesXattrFiles(const char *path)
{
	struct _VolumeCapabilitiesWrapped {
		uint32_t length;
		vol_capabilities_attr_t volume_capabilities;
	} __attribute__((aligned(4), packed));

	struct attrlist attr_list;
	struct _VolumeCapabilitiesWrapped volume_cap_wrapped;
	struct statfs sfb;

	if (!path) {
		secerror("path is NULL");
		return false;
	}

	int ret = statfs(path, &sfb);
	if (ret != 0) {
		secerror("Unable to convert %s to its filesystem mount [statfs failed]: %d [%s]", path, errno, strerror(errno));
		return false;
	}
	path = sfb.f_mntonname;

	memset(&attr_list, 0, sizeof(attr_list));
	attr_list.bitmapcount = ATTR_BIT_MAP_COUNT;
	attr_list.volattr = ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES;

	ret = getattrlist(path, &attr_list, &volume_cap_wrapped, sizeof(volume_cap_wrapped), 0);
	if (ret) {
		secerror("Unable to get volume capabilities from %s: %d [%s]", path, errno, strerror(errno));
		return false;
	}

	if (volume_cap_wrapped.length != sizeof(volume_cap_wrapped)) {
		secerror("getattrlist return length incorrect, expected %lu, got %u", sizeof(volume_cap_wrapped), volume_cap_wrapped.length);
		return false;
	}

	// The valid bit tells us whether the corresponding bit in capabilities is valid
	// or not. For file systems where the valid bit isn't set, we can safely assume that
	// extended attributes aren't supported natively.

	bool xattr_valid = (volume_cap_wrapped.volume_capabilities.valid[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_EXTENDED_ATTR) == VOL_CAP_INT_EXTENDED_ATTR;
	if (!xattr_valid) {
		return true;
	}

	bool xattr_capability = (volume_cap_wrapped.volume_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_EXTENDED_ATTR) == VOL_CAP_INT_EXTENDED_ATTR;
	if (!xattr_capability) {
		return true;
	}

	return false;
}

bool pathIsValidXattrFile(const string fullPath, const char *scope)
{
	// Confirm that fullPath begins from root.
	if (fullPath[0] != '/') {
		secinfo(scope, "%s isn't a full path, but a relative path", fullPath.c_str());
		return false;
	}

	// Confirm that fullPath is a regular file.
	if (!pathIsRegularFile(fullPath.c_str())) {
		secinfo(scope, "%s isn't a regular file", fullPath.c_str());
		return false;
	}

	// Check that the file name matches the Xattr file spec.
	if (!pathMatchesXattrFilenameSpec(fullPath.c_str())) {
		secinfo(scope, "%s doesn't match Xattr file path spec", fullPath.c_str());
		return false;
	}

	// We are guaranteed to have at least one "/" by virtue of fullPath
	// being a path from the root of the filesystem hierarchy.
	//
	// We construct the real file name by copying everything up to
	// the last "/", adding the "/" back in, then skipping
	// over the backslash (+1) and the "._" (+2) in the rest of the
	// string.

	size_t lastBackSlash = fullPath.find_last_of("/");
	const string realFilePath = fullPath.substr(0, lastBackSlash) + "/" + fullPath.substr(lastBackSlash + 1 + 2);

	if (!pathExists(realFilePath.c_str())) {
		secinfo(scope, "%s does not exist, forcing resource validation on %s", realFilePath.c_str(), fullPath.c_str());
		return false;
	}

	// Lastly, we need to confirm that the real file contains some xattrs. If not,
	// then the file represented by fullPath isn't an xattr file.
	if (!pathHasXattrs(realFilePath.c_str())) {
		secinfo(scope, "%s does not contain xattrs, forcing resource validation on %s", realFilePath.c_str(), fullPath.c_str());
		return false;
	}

	return true;
}

string pathRemaining(string fullPath, string prefix)
{
	if ((fullPath.length() < prefix.length()) ||
		(prefix.length() == 0) ||
		(fullPath.length() == 0) ||
		!isPathPrefix(prefix, fullPath)) {
		return "";
	}

	size_t currentPosition = prefix.length();
	if (prefix[currentPosition-1] != '/') {
		// If the prefix doesn't already end with a /, add one to the position so the remaining
		// doesn't start with one.
		currentPosition += 1;
	}

	// Ensure we're not indexing outside the bounds of fullPath.
	if (currentPosition >= fullPath.length()) {
		return "";
	}

	return fullPath.substr(currentPosition, string::npos);
}

bool isPathPrefix(string prefixPath, string fullPath)
{
	size_t pos = fullPath.find(prefixPath);
	if (pos == 0) {
		// If they're a perfect match, its not really a path prefix.
		if (prefixPath.length() == fullPath.length()) {
			return false;
		}

		// Ensure the prefix starts a relative path under the prefix.
		if (prefixPath.back() == '/') {
			// If the prefix ends with a delimeter, we're good.
			return true;
		} else {
			// Otherwise, the next character in the fullPath needs to be a delimeter.
			return fullPath.at(prefixPath.length()) == '/';
		}
	}
	return false;
}

bool iterateLargestSubpaths(string path, bool (^pathHandler)(string))
{
	size_t lastPossibleSlash = path.length();
	size_t currentPosition = 0;
	bool stopped = false;

	while (!stopped) {
		currentPosition = path.find_last_of("/", lastPossibleSlash);
		if (currentPosition == string::npos || currentPosition == 0) {
			break;
		}

		// Erase from the current position to the end of the string.
		path.erase(currentPosition, string::npos);
		stopped = pathHandler(path);
		if (!stopped) {
			if (currentPosition == 0) {
				break;
			}
			lastPossibleSlash = currentPosition - 1;
		}
	}
	return stopped;
}


} // end namespace CodeSigning
} // end namespace Security
