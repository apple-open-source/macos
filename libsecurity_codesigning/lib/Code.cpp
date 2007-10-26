/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
// Code - SecCode API objects
//
#include "Code.h"
#include "StaticCode.h"
#include <Security/SecCodeHost.h>
#include "cskernel.h"
#include "cfmunge.h"
#include <security_utilities/debugging.h>
#include <sys/codesign.h>

namespace Security {
namespace CodeSigning {


//
// Construction
//
SecCode::SecCode(SecCode *host)
	: mHost(host)
{
}


//
// Clean up a SecCode object
//
SecCode::~SecCode() throw()
{
}


//
// Yield the host Code
//
SecCode *SecCode::host() const
{
	return mHost;
}


//
// Yield the static code. This is cached.
// The caller does not own the object returned; it lives (at least) as long
// as the SecCode it was derived from.
//
SecStaticCode *SecCode::staticCode()
{
	if (!mStaticCode) {
		mStaticCode.take(this->getStaticCode());
		secdebug("seccode", "%p got static=%p", this, mStaticCode.get());
	}
	assert(mStaticCode);
	return mStaticCode;
}


//
// By default, we have no guests
//
SecCode *SecCode::locateGuest(CFDictionaryRef)
{
	return NULL;
}


//
// By default, we map ourselves to disk using our host's mapping facility.
// (This is currently only overridden in the root-of-trust (kernel) implementation.)
// The caller owns the object returned.
//
SecStaticCode *SecCode::getStaticCode()
{
	return host()->mapGuestToStatic(this);
}


//
// The default implementation cannot map guests to disk
//
SecStaticCode *SecCode::mapGuestToStatic(SecCode *guest)
{
	MacOSError::throwMe(errSecCSNoSuchCode);
}


//
// Master validation function.
//
// This is the most important function in all of Code Signing. It performs
// dynamic validation on running code. Despite its simple structure, it does
// everything that's needed to establish whether a Code is currently valid...
// with a little help from StaticCode, format drivers, type drivers, and so on.
//
// This function validates internal requirements in the hosting chain. It does
// not validate external requirements - the caller needs to do that with a separate call.
//
static const uint8_t interim_hosting_default_requirement[] = {
	// anchor apple and (identifier com.apple.translate or identifier com.apple.LaunchCFMApp)
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
	0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13,
	0x63, 0x6f, 0x6d, 0x2e, 0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x74, 0x72, 0x61, 0x6e, 0x73, 0x6c,
	0x61, 0x74, 0x65, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x16, 0x63, 0x6f, 0x6d, 0x2e,
	0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x4c, 0x61, 0x75, 0x6e, 0x63, 0x68, 0x43, 0x46, 0x4d, 0x41,
	0x70, 0x70, 0x00, 0x00,
};

void SecCode::checkValidity(SecCSFlags flags)
{
	if (this->isRoot()) {
		// the root-of-trust is valid by definition
		secdebug("validator", "%p root of trust is presumed valid", this);
		return;
	}
	secdebug("validator", "%p begin validating %s",
		this, this->staticCode()->mainExecutablePath().c_str());
	
	//
	// Do not reorder the operations below without thorough cogitation. There are
	// interesting dependencies and significant performance issues.
	//
	// For the most part, failure of (secure) identity will cause exceptions to be
	// thrown, and success is indicated by survival. If you make it to the end,
	// you have won the validity race. (Good rat.)
	//

	// check my host first, recursively
	this->host()->checkValidity(flags);

	SecStaticCode *myDisk = this->staticCode();
	SecStaticCode *hostDisk = this->host()->staticCode();

	// check my static state
	myDisk->validateDirectory();

	// check my own dynamic state
	if (!(this->host()->getGuestStatus(this) & CS_VALID))
		MacOSError::throwMe(errSecCSGuestInvalid);

	// check host/guest constraints
	if (!this->host()->isRoot()) {	// not hosted by root of trust
		myDisk->validateRequirements(kSecHostRequirementType, hostDisk, errSecCSHostReject);
		hostDisk->validateRequirements(kSecGuestRequirementType, myDisk);
	}
	
	secdebug("validator", "%p validation successful", this);
}


//
// By default, we track no validity for guests (we don't have any)
//
uint32_t SecCode::getGuestStatus(SecCode *guest)
{
	MacOSError::throwMe(errSecCSNoSuchCode);
}


//
// Given a bag of attribute values, automagically come up with a SecCode
// without any other information.
// This is meant to be the "just do what makes sense" generic call, for callers
// who don't want to engage in the fascinating dance of manual guest enumeration.
//
// Note that we expect the logic embedded here to change over time (in backward
// compatible fashion, one hopes), and that it's all right to use heuristics here
// as long as it's done sensibly.
//
// Be warned that the present logic is quite a bit ad-hoc, and will likely not
// handle arbitrary combinations of proxy hosting, dynamic hosting, and dedicated
// hosting all that well.
//
SecCode *SecCode::autoLocateGuest(CFDictionaryRef attributes, SecCSFlags flags)
{
	// main logic: we need a pid, and we'll take a canonical guest id as an option
	int pid = 0;
	if (!cfscan(attributes, "{%O=%d}", kSecGuestAttributePid, &pid))
		CSError::throwMe(errSecCSUnsupportedGuestAttributes, kSecCFErrorGuestAttributes, attributes);
	if (SecCode *process =
			KernelCode::active()->locateGuest(CFTemp<CFDictionaryRef>("{%O=%d}", kSecGuestAttributePid, pid))) {
		SecPointer<SecCode> code;
		code.take(process);		// locateGuest gave us a retained object
		if (code->staticCode()->flag(kSecCodeSignatureHost)) {
			// might be a code host. Let's find out
			CFRef<CFMutableDictionaryRef> rest = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
			CFDictionaryRemoveValue(rest, kSecGuestAttributePid);
			if (SecCode *guest = code->locateGuest(rest))
				return guest;
		}
		if (!CFDictionaryGetValue(attributes, kSecGuestAttributeCanonical)) {
			// only "soft" attributes, and no hosting is happening. Return the (non-)host itself
			return code.yield();
		}
	}
	MacOSError::throwMe(errSecCSNoSuchCode);
}


} // end namespace CodeSigning
} // end namespace Security
