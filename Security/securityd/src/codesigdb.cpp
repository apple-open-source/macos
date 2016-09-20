/*
 * Copyright (c) 2003-2009,2012,2016 Apple Inc. All Rights Reserved.
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
// codesigdb - code-hash equivalence database
//
#include "codesigdb.h"
#include "process.h"
#include "server.h"
#include "agentquery.h"
#include <security_utilities/memutils.h>
#include <security_utilities/logging.h>
#include <Security/SecRequirementPriv.h>


//
// Construct a CodeSignatures objects
//
CodeSignatures::CodeSignatures()
{
}

CodeSignatures::~CodeSignatures()
{
}

//
// (Re)open the equivalence database.
// This is useful to switch to database in another volume.
//
void CodeSignatures::open(const char *path)
{
}


//
// Basic Identity objects
//
CodeSignatures::Identity::Identity() : mState(untried)
{ }

CodeSignatures::Identity::~Identity()
{ }

//
// Verify signature matches.
// This ends up getting called when a CodeSignatureAclSubject is validated.
// The OSXVerifier describes what we require of the client code; the process represents
// the requesting client; and the context gives us access to the ACL and its environment
// in case we want to, well, creatively rewrite it for some reason.
//
bool CodeSignatures::verify(Process &process,
	const OSXVerifier &verifier, const AclValidationContext &context)
{
	secinfo("codesign", "start verify");

	StLock<Mutex> _(process);
	if (SecRequirementRef requirement = verifier.requirement()) {
		// If the ACL contains a code signature (requirement), we won't match against unsigned code at all.
		// The legacy hash is ignored (it's for use by pre-Leopard systems).
		secinfo("codesign", "CS requirement present; ignoring legacy hashes");
		Server::active().longTermActivity();
		switch (OSStatus rc = process.checkValidity(kSecCSDefaultFlags, requirement)) {
		case noErr:
			secinfo("codesign", "CS verify passed");
			return true;
		case errSecCSUnsigned:
			secinfo("codesign", "CS verify against unsigned binary failed");
			return false;
		default:
			secinfo("codesign", "CS verify failed OSStatus=%d", int32_t(rc));
			return false;
		}
	}
	switch (matchSignedClientToLegacyACL(process, verifier, context)) {
	case noErr:						// handled, allow access
		return true;
 	case errSecCSUnsigned: {		// unsigned client, complete legacy case
 		secinfo("codesign", "no CS requirement - using legacy hash");

		/*
		 * We should stop supporting this case for binaries
		 * built for modern OS/SDK, user should ad-hoc sign
		 * their binaries in that case.
		 *
		 * <rdar://problem/20546287>
		 */
		Identity &clientIdentity = process;
		try {
			if (clientIdentity.getHash() == CssmData::wrap(verifier.legacyHash(), SHA1::digestLength)) {
				secinfo("codesign", "direct match: pass");
				return true;
			}
		} catch (...) {
			secinfo("codesign", "exception getting client code hash: fail");
			return false;
		}
		return false;
	}
	default:						// client unsuitable, reject this match
		return false;
	}
}

//
// See if we can rewrite the ACL from legacy to Code Signing form without losing too much security.
// Returns true if the present validation should succeed (we probably rewrote the ACL).
// Returns false if the present validation shouldn't succeed based on what we did here (we may still
// have rewritten the ACL, in principle).
//
// Note that these checks add nontrivial overhead to ACL processing. We want to eventually phase
// this out, or at least make it an option that doesn't run all the time - perhaps an "extra legacy
// effort" per-client mode bit.
//
static string trim(string s, char delimiter)
{
	string::size_type p = s.rfind(delimiter);
	if (p != string::npos)
		s = s.substr(p + 1);
	return s;
}

static string trim(string s, char delimiter, string suffix)
{
	s = trim(s, delimiter);
	size_t preLength = s.length() - suffix.length();
	if (preLength > 0 && s.substr(preLength) == suffix)
		s = s.substr(0, preLength);
	return s;
}

OSStatus CodeSignatures::matchSignedClientToLegacyACL(Process &process,
	const OSXVerifier &verifier, const AclValidationContext &context)
{
	//
	// Check whether we seem to be matching a legacy .Mac ACL against a member of the .Mac group
	//
	if (SecurityServerAcl::looksLikeLegacyDotMac(context)) {
		Server::active().longTermActivity();
		CFRef<SecRequirementRef> dotmac;
		MacOSError::check(SecRequirementCreateGroup(CFSTR("dot-mac"), NULL, kSecCSDefaultFlags, &dotmac.aref()));
		if (process.checkValidity(kSecCSDefaultFlags, dotmac) == noErr) {
			secinfo("codesign", "client is a dot-mac application; update the ACL accordingly");

			// create a suitable AclSubject (this is the above-the-API-line way)
			CFRef<CFDataRef> reqdata;
			MacOSError::check(SecRequirementCopyData(dotmac, kSecCSDefaultFlags, &reqdata.aref()));
			RefPointer<CodeSignatureAclSubject> subject = new CodeSignatureAclSubject(NULL, "group://dot-mac");
			subject->add((const BlobCore *)CFDataGetBytePtr(reqdata));

			// add it to the ACL and pass the access check (we just quite literally did it above)
			SecurityServerAcl::addToStandardACL(context, subject);
			return noErr;
		}
	}

	//
	// Get best names for the ACL (legacy) subject and the (signed) client
	//
	CFRef<CFDictionaryRef> info;
	MacOSError::check(process.copySigningInfo(kSecCSSigningInformation, &info.aref()));
	CFStringRef signingIdentity = CFStringRef(CFDictionaryGetValue(info, kSecCodeInfoIdentifier));
	if (!signingIdentity)		// unsigned
		return errSecCSUnsigned;

	string bundleName;	// client
	if (CFDictionaryRef infoList = CFDictionaryRef(CFDictionaryGetValue(info, kSecCodeInfoPList)))
		if (CFStringRef name = CFStringRef(CFDictionaryGetValue(infoList, kCFBundleNameKey)))
			bundleName = trim(cfString(name), '.');
	if (bundleName.empty())	// fall back to signing identifier
		bundleName = trim(cfString(signingIdentity), '.');

	string aclName = trim(verifier.path(), '/', ".app");	// ACL

	secinfo("codesign", "matching signed client \"%s\" against legacy ACL \"%s\"",
		bundleName.c_str(), aclName.c_str());

	//
	// Check whether we're matching a signed APPLE application against a legacy ACL by the same name
	//
	if (bundleName == aclName) {
		const unsigned char reqData[] = {		// "anchor apple", version 1 blob, embedded here
			0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x10,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03
		};
		CFRef<SecRequirementRef> apple;
		MacOSError::check(SecRequirementCreateWithData(CFTempData(reqData, sizeof(reqData)),
			kSecCSDefaultFlags, &apple.aref()));
		Server::active().longTermActivity();
		switch (OSStatus rc = process.checkValidity(kSecCSDefaultFlags, apple)) {
		case noErr:
			{
				secinfo("codesign", "withstands strict scrutiny; quietly adding new ACL");
				RefPointer<AclSubject> subject = process.copyAclSubject();
				SecurityServerAcl::addToStandardACL(context, subject);
				return noErr;
			}
		default:
			secinfo("codesign", "validation fails with rc=%d, rejecting", int32_t(rc));
			return rc;
		}
	}

	// not close enough to even ask - this can't match
	return errSecCSReqFailed;
}
