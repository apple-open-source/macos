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
#include <sys/codesign.h>


using namespace CodeSigning;
using namespace UnixPlusPlus;

static void displayGuestChain(SecCodeRef code);


//
// One-time preparation
//
static const Requirement *testReqs;		// external requirement (compiled)


void prepareToVerify()
{
	if (testReq)
		testReqs = readRequirement<Requirement>(testReq);
}


//
// Verify a code object's signature on disk.
// This also provides for live verification on processes (only).
//
void verify(const char *target)
{
	CFRef<SecStaticCodeRef> ondiskRef;
	if (CFRef<SecCodeRef> code = codePath(target)) {	// dynamic specification (running code)
		ErrorCheck check;
		check(SecCodeCheckValidityWithErrors(code, kSecCSDefaultFlags, NULL, check));
		MacOSError::check(SecCodeCopyStaticCode(code, kSecCSDefaultFlags, &ondiskRef.aref()));
		note(1, "%s: dynamically valid", target);
	} else {
		// verify program on disk
		MacOSError::check(SecStaticCodeCreateWithPath(CFTempURL(target), kSecCSDefaultFlags, &ondiskRef.aref()));
		if (detached) {
			CFRef<CFDataRef> dsig = cfLoadFile(detached);
			MacOSError::check(SecCodeSetDetachedSignature(ondiskRef, dsig, kSecCSDefaultFlags));
		}
		ErrorCheck check;
		check(SecStaticCodeCheckValidityWithErrors(ondiskRef, verifyOptions, NULL, check));
		if (verifyOptions & kSecCSBasicValidateOnly)
			note(1, "%s: valid on disk (not all contents verified)", target);
		else
			note(1, "%s: valid on disk", target);
	}

	CFRef<SecRequirementRef> designated = NULL;
	if (OSStatus rc = SecCodeCopyDesignatedRequirement(ondiskRef, kSecCSDefaultFlags, &designated.aref())) {
		note(0, "%s: cannot retrieve designated requirement\n", target);
		cssmPerror(target, rc);
	} else if (rc = SecStaticCodeCheckValidity(ondiskRef, kSecCSBasicValidateOnly, designated)) {
		note(0, "%s: does not satisfy its designated Requirement", target);
		if (!exitcode)
			exitcode = exitNoverify;
	} else
		note(3, "%s: satisfies its Designated Requirement", target);
	
    if (testReqs) {
        CFRef<SecRequirementRef> req;
        MacOSError::check(SecRequirementCreateWithData(CFTempData(*testReqs), kSecCSDefaultFlags, &req.aref()));
        if (OSStatus rc = SecStaticCodeCheckValidity(ondiskRef, verifyOptions, req)) {
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
	CFRef<SecCodeRef> code = codePath(target);
	if (!code)
		fail("%s: not a dynamic code specification", target);

	do {
		CFRef<CFDictionaryRef> info;
		MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDynamicInformation, &info.aref()));
		printf("%s", cfString(CFURLRef(CFDictionaryGetValue(info,
			kSecCodeInfoMainExecutable))).c_str());
		if (verbose > 0) {
			printf("\t");
			CFStringRef identifier = CFStringRef(CFDictionaryGetValue(info, kSecCodeInfoIdentifier));
			if (!identifier)
				printf("\tUNSIGNED (");
			else
				printf("(");
			uint32_t status = cfNumber(CFNumberRef(CFDictionaryGetValue(info, kSecCodeInfoStatus)));
			if (status & CS_VALID)
				printf("valid");
			else
				printf("INVALID");
			if (status & CS_KILL)
				printf(" kill");
			if (status & CS_HARD)
				printf(" hard");
			if (status & ~(CS_VALID | CS_KILL | CS_HARD))	// unrecognized flag
				printf(" 0x%x", status);
			printf(")");
			}
		printf("\n");
		CFRef<SecCodeRef> host;
		MacOSError::check(SecCodeCopyHost(code, kSecCSDefaultFlags, &host.aref()));
		code = host;
	} while (code);
}
