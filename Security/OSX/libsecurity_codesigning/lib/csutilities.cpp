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

} // end namespace CodeSigning
} // end namespace Security
