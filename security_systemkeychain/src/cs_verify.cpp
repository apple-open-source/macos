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
// cs_verify - codesign verification operations
//
#include "codesign.h"
#include <Security/SecRequirementPriv.h>
#include <Security/SecCodePriv.h>

using namespace UnixPlusPlus;

static void displayGuestChain(SecCodeRef code);


//
// One-time preparation
//
SecRequirementRef testReqs;				// external requirement (compiled)


void prepareToVerify()
{
	if (testReq)
		testReqs = readRequirement(testReq);
}


//
// Verify a code object's signature on disk.
// This also provides for live verification on processes (only).
//
void verify(const char *target)
{
	CFRef<SecCodeRef> code = dynamicCodePath(target);		// set if the target is dynamic
	CFRef<SecStaticCodeRef> staticCode;
	if (code)
		MacOSError::check(SecCodeCopyStaticCode(code, kSecCSDefaultFlags, &staticCode.aref()));
	else
		staticCode.take(staticCodePath(target, architecture, bundleVersion));
	if (detached)
		if (CFRef<CFDataRef> dsig = cfLoadFile(detached))
			MacOSError::check(SecCodeSetDetachedSignature(staticCode, dsig, kSecCSDefaultFlags));
		else
			fail("%s: cannot load detached signature", detached);
	if (code) {
		ErrorCheck check;
		check(SecCodeCheckValidityWithErrors(code, kSecCSDefaultFlags, NULL, check));
		note(1, "%s: dynamically valid", target);
	}
	if (!code || verbose > 0) {		// validate statically if static input or verbose dynamic
		ErrorCheck check;
		check(SecStaticCodeCheckValidityWithErrors(staticCode, staticVerifyOptions, NULL, check));
		if (staticVerifyOptions & kSecCSBasicValidateOnly)
			note(1, "%s: valid on disk (not all contents verified)", target);
		else
			note(1, "%s: valid on disk", target);
	}

	if (verbose > 0) {		// self-check designated requirement
		CFRef<SecRequirementRef> designated = NULL;
		if (OSStatus rc = SecCodeCopyDesignatedRequirement(staticCode, kSecCSDefaultFlags, &designated.aref())) {
			cssmPerror(target, rc);
			fail("%s: cannot retrieve designated requirement", target);
		} else if (rc = SecStaticCodeCheckValidity(staticCode, kSecCSBasicValidateOnly, designated)) {
			note(0, "%s: does not satisfy its designated Requirement", target);
			if (!exitcode)
				exitcode = exitNoverify;
		} else
			note(1, "%s: satisfies its Designated Requirement", target);
	}
	
    if (testReqs) {			// check explicit test requirement
        if (OSStatus rc = SecStaticCodeCheckValidity(staticCode, staticVerifyOptions, testReqs)) {
            cssmPerror("test-requirement", rc);
            if (!exitcode)
                exitcode = exitNoverify;
        } else {
            note(1, "%s: explicit requirement satisfied", target);
        }
	}
}


//
// Build and display the hosting chain for some running code.
// This won't work for static (file path) arguments.
//
void hostinginfo(const char *target)
{
	CFRef<SecCodeRef> code = dynamicCodePath(target);
	if (!code)
		fail("%s: not a dynamic code specification", target);

	do {
		CFDictionary info(noErr);
		MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDynamicInformation, &info.aref()));
		printf("%s", cfString(info.get<CFURLRef>(kSecCodeInfoMainExecutable)).c_str());
		if (verbose > 0) {
			printf("\t");
			if (info.get<CFStringRef>(kSecCodeInfoIdentifier))
				printf("(");
			else
				printf("UNSIGNED (");
			if (CFNumberRef state = info.get<CFNumberRef>(kSecCodeInfoStatus)) {
				uint32_t status = cfNumber(state);
				if (status & kSecCodeStatusValid)
					printf("valid");
				else
					printf("INVALID");
				if (status & kSecCodeStatusKill)
					printf(" kill");
				if (status & kSecCodeStatusHard)
					printf(" hard");
				if (status & ~(kSecCodeStatusValid | kSecCodeStatusKill | kSecCodeStatusHard))	// unrecognized flag
					printf(" 0x%x", status);
			} else
				printf("UNKNOWN");
			printf(")");
			}
		printf("\n");
		CFRef<SecCodeRef> host;
		MacOSError::check(SecCodeCopyHost(code, kSecCSDefaultFlags, &host.aref()));
		code = host;
	} while (code);
}
