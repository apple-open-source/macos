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
// cs_sign - codesign signing operation
//
#include "codesign.h"
#include <Security/SecCodeSigner.h>
#include <cstdio>
#include <cmath>


using namespace CodeSigning;
using namespace UnixPlusPlus;


//
// One-time preparation
//
static SecCodeSignerRef signerRef;				// global signer object

void prepareToSign()
{
	CFRef<CFMutableDictionaryRef> parameters =
		makeCFMutableDictionary(1,
			kSecCodeSignerIdentity, signer
		);
	
	if (uniqueIdentifier)
		CFDictionaryAddValue(parameters,
			kSecCodeSignerIdentifier, CFTempString(uniqueIdentifier));
	if (identifierPrefix)
		CFDictionaryAddValue(parameters,
			kSecCodeSignerIdentifierPrefix, CFTempString(identifierPrefix));
	
	if (internalReq) {
		const Requirements *internalReqs = readRequirement<Requirements>(internalReq);
		CFDictionaryAddValue(parameters,
			kSecCodeSignerRequirements, CFTempData(*internalReqs));
		if (internalReqs->find(kSecGuestRequirementType)) {
			secdebug("codesign", "has guest requirements; setting host flag");
			cdFlags |= kSecCodeSignatureHost;
		}
	}
	
	if (signatureSize)
		CFDictionaryAddValue(parameters, CFSTR("cmssize"), CFTempNumber(signatureSize));
	if (pagesize != pagesizeUnspecified)
		CFDictionaryAddValue(parameters, kSecCodeSignerPageSize, CFTempNumber(pagesize));
	if (cdFlags)
		CFDictionaryAddValue(parameters, kSecCodeSignerFlags, CFTempNumber(cdFlags));
	if (detached)
		CFDictionaryAddValue(parameters, kSecCodeSignerDetached, CFTempURL(detached));
	if (signingTime)
		CFDictionaryAddValue(parameters, kSecCodeSignerSigningTime, signingTime);
	
	if (resourceRules) {
		if (CFRef<CFDataRef> data = cfLoadFile(resourceRules)) {
			CFDictionaryAddValue(parameters, kSecCodeSignerResourceRules,
				CFRef<CFDictionaryRef>(makeCFDictionaryFrom(data)));
		} else
			fail("%s: cannot read resources", resourceRules);
	}
	
	if (dryrun)
		CFDictionaryAddValue(parameters, kSecCodeSignerDryRun, kCFBooleanTrue);
	
	MacOSError::check(SecCodeSignerCreate(parameters, kSecCSDefaultFlags, &signerRef));
}


//
// Sign a code object.
//
void sign(const char *target)
{
	secdebug("codesign", "BEGIN SIGNING %s", target);
	
	CFRef<SecStaticCodeRef> code;
	MacOSError::check(SecStaticCodeCreateWithPath(CFTempURL(target), kSecCSDefaultFlags,
		&code.aref()));
	
	CFRef<CFDictionaryRef> dict;
	switch (OSStatus rc = SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &dict.aref())) {
	case noErr:
		if (CFDictionaryGetValue(dict, kSecCodeInfoIdentifier)) {	// binary is signed
			if (detached)
				note(0, "%s: not disturbing embedded signature", target);
			else if (force)
				note(0, "%s: replacing existing signature", target);
			else
				fail("%s: is already signed", target);
		}
		break;
	case errSecCSSignatureFailed:	// signed but signature invalid
		if (detached)
			note(0, "%s: ignoring invalid embedded signature", target);
		else if (force)
			note(0, "%s: replacing invalid existing signature", target);
		else
			fail("%s: is already signed", target);
		break;
	default:
		MacOSError::throwMe(rc);
	}
	
	ErrorCheck check;
	check(SecCodeSignerAddSignatureWithErrors(signerRef, code, kSecCSDefaultFlags, check));

	SecCSFlags flags = kSecCSDefaultFlags;
	if (modifiedFiles)
		flags |= kSecCSContentInformation;
	MacOSError::check(SecCodeCopySigningInformation(code, flags, &dict.aref()));
	note(1, "%s: signed %s [%s]", target,
		cfString(CFStringRef(CFDictionaryGetValue(dict, kSecCodeInfoFormat))).c_str(),
		cfString(CFStringRef(CFDictionaryGetValue(dict, kSecCodeInfoIdentifier))).c_str()
	);
	if (modifiedFiles)
		writeFileList(CFArrayRef(CFDictionaryGetValue(dict, kSecCodeInfoChangedFiles)), modifiedFiles, "a");
}
