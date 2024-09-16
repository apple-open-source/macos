/*
 * Copyright (c) 2006-2012 Apple Inc. All Rights Reserved.
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
// requirement - Code Requirement Blob description
//
#include "requirement.h"
#include "reqinterp.h"
#include "codesigning_dtrace.h"
#include <security_utilities/errors.h>
#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/hashing.h>
#include "LWCRHelper.h"

#ifdef DEBUGDUMP
#include "reqdumper.h"
#endif

namespace Security {
namespace CodeSigning {


//
// Canonical names for requirement types
//
const char *const Requirement::typeNames[] = {
	"invalid",
	"host",
	"guest",
	"designated",
	"library",
	"plugin",
};


//
// validate a requirement against a code context
//
void Requirement::validate(const Requirement::Context &ctx, OSStatus failure /* = errSecCSReqFailed */) const
{
	if (!this->validates(ctx, failure))
		MacOSError::throwMe(failure);
}

bool Requirement::validates(const Requirement::Context &ctx, OSStatus failure /* = errSecCSReqFailed */) const
{
	CODESIGN_EVAL_REQINT_START((void*)this, (int)this->length());
	switch (kind()) {
	case exprForm:
		if (Requirement::Interpreter(this, &ctx).evaluate()) {
			CODESIGN_EVAL_REQINT_END(this, 0);
			return true;
		} else {
			CODESIGN_EVAL_REQINT_END(this, failure);
			return false;
		}
#if !TARGET_OS_SIMULATOR
	case lwcrForm: {
		CFRef<CFDataRef> lwcr = createlwcrFormData();
		if (evaluateLightweightCodeRequirement(ctx, lwcr)) {
			CODESIGN_EVAL_REQINT_END(this, 0);
			return true;
		} else {
			CODESIGN_EVAL_REQINT_END(this, failure);
			return false;
		}
	}
#endif
	default:
		CODESIGN_EVAL_REQINT_END(this, errSecCSReqUnsupported);
		MacOSError::throwMe(errSecCSReqUnsupported);
	}
}

CFDataRef Requirement::createlwcrFormData() const
{
	if (kind() == lwcrForm) {
		Requirement::Reader reader(this);
		const UInt8* data = NULL;
		size_t length = 0;
		reader.getData(data, length);
		return CFDataCreate(NULL, data, length);
	} else {
		MacOSError::throwMe(errSecCSReqInvalid);
	}
}


//
// Retrieve one certificate from the cert chain.
// Positive and negative indices can be used:
//    [ leaf, intermed-1, ..., intermed-n, anchor ]
//        0       1       ...     -2         -1
// Returns NULL if unavailable for any reason.
//	
SecCertificateRef Requirement::Context::cert(int ix) const
{
	if (certs) {
		if (ix < 0)
			ix += certCount();
		if (ix >= CFArrayGetCount(certs))
		    return NULL;
		if (CFTypeRef element = CFArrayGetValueAtIndex(certs, ix))
			return SecCertificateRef(element);
	}
	return NULL;
}

unsigned int Requirement::Context::certCount() const
{
	if (certs)
		return (unsigned int)CFArrayGetCount(certs);
	else
		return 0;
}


//
// Produce the hash of a fake Apple root (only if compiled for internal testing)
//
#if defined(TEST_APPLE_ANCHOR)

const char Requirement::testAppleAnchorEnv[] = "TEST_APPLE_ANCHOR";

const SHA1::Digest &Requirement::testAppleAnchorHash()
{
	static bool tried = false;
	static SHA1::Digest testHash;
	if (!tried) {
		// see if we have one configured
		if (const char *path = getenv(testAppleAnchorEnv))
			try {
				UnixPlusPlus::FileDesc fd(path);
				char buffer[2048];		// arbitrary limit
				size_t size = fd.read(buffer, sizeof(buffer));
				SHA1 hash;
				hash(buffer, size);
				hash.finish(testHash);
				Syslog::alert("ACCEPTING TEST AUTHORITY %s FOR APPLE CODE IDENTITY", path);
			} catch (...) { }
		tried = true;
	}
	return testHash;		// will be zeroes (no match) if not configured
}

#endif //TEST_APPLE_ANCHOR

//
// Debug dump support
//
#if TARGET_OS_OSX
#ifdef DEBUGDUMP

void Requirement::dump() const
{
	Debug::dump("%s\n", Dumper::dump(this).c_str());
}

#endif //DEBUGDUMP
#endif


}	// CodeSigning
}	// Security
