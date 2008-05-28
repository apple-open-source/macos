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
// SecCode - API frame for SecCode objects.
//
// Note that some SecCode* functions take SecStaticCodeRef arguments in order to
// accept either static or dynamic code references, operating on the respective
// StaticCode. Those functions are in SecStaticCode.cpp, not here, despite their name.
//
#include "cs.h"
#include "Code.h"
#include "cskernel.h"
#include <security_codesigning/cfmunge.h>
#include <sys/codesign.h>

using namespace CodeSigning;


//
// CFError user info keys
//
const CFStringRef kSecCFErrorPattern =			CFSTR("SecCSPattern");
const CFStringRef kSecCFErrorResourceSeal =		CFSTR("SecCSResourceSeal");
const CFStringRef kSecCFErrorResourceAdded =	CFSTR("SecCSResourceAdded");
const CFStringRef kSecCFErrorResourceAltered =	CFSTR("SecCSResourceAltered");
const CFStringRef kSecCFErrorResourceMissing =	CFSTR("SecCSResourceMissing");
const CFStringRef kSecCFErrorInfoPlist =		CFSTR("SecCSInfoPlist");
const CFStringRef kSecCFErrorGuestAttributes =	CFSTR("SecCSGuestAttributes");
const CFStringRef kSecCFErrorRequirementSyntax = CFSTR("SecRequirementSyntax");

//
// CF-standard type code functions
//
CFTypeID SecCodeGetTypeID(void)
{
	BEGIN_CSAPI
	return gCFObjects().Code.typeID;
    END_CSAPI1(_kCFRuntimeNotATypeID)
}


//
// Get the root of trust Code
//
SecCodeRef SecGetRootCode(SecCSFlags flags)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	return KernelCode::active()->handle();
	
	END_CSAPI1(NULL)
}


//
// Get a reference to the calling code.
//
OSStatus SecCodeCopySelf(SecCSFlags flags, SecCodeRef *selfRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	CFRef<CFMutableDictionaryRef> attributes = makeCFMutableDictionary(1,
		kSecGuestAttributePid, CFTempNumber(getpid()).get());
	Required(selfRef) = SecCode::autoLocateGuest(attributes, flags)->handle(false);
	
	END_CSAPI
}


//
// Get the StaticCode for an Code
//
OSStatus SecCodeCopyStaticCode(SecCodeRef codeRef, SecCSFlags flags, SecStaticCodeRef *staticCodeRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	SecPointer<SecStaticCode> staticCode = SecCode::required(codeRef)->staticCode();
	Required(staticCodeRef) = staticCode ? staticCode->handle() : NULL;

	END_CSAPI
}


//
// Get the host for an Code
//
OSStatus SecCodeCopyHost(SecCodeRef guestRef, SecCSFlags flags, SecCodeRef *hostRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	SecPointer<SecCode> host = SecCode::required(guestRef)->host();
	Required(hostRef) = host ? host->handle() : NULL;

	END_CSAPI
}


//
// Find a guest by attribute(s)
//
const CFStringRef kSecGuestAttributePid =			CFSTR("pid");
const CFStringRef kSecGuestAttributeCanonical =		CFSTR("canonical");
const CFStringRef kSecGuestAttributeMachPort =		CFSTR("mach-port");

OSStatus SecCodeCopyGuestWithAttributes(SecCodeRef hostRef,
	CFDictionaryRef attributes,	SecCSFlags flags, SecCodeRef *guestRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	if (hostRef) {
		if (SecCode *guest = SecCode::required(hostRef)->locateGuest(attributes))
			Required(guestRef) = guest->handle(false);
		else
			return errSecCSNoSuchCode;
	} else
		Required(guestRef) = SecCode::autoLocateGuest(attributes, flags)->handle(false);
	
	END_CSAPI
}


//
// Shorthand for getting the SecCodeRef for a UNIX process
//
OSStatus SecCodeCreateWithPID(pid_t pid, SecCSFlags flags, SecCodeRef *processRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	if (SecCode *guest = KernelCode::active()->locateGuest(CFTemp<CFDictionaryRef>("{%O=%d}", kSecGuestAttributePid, pid)))
		Required(processRef) = guest->handle(false);
	else
		return errSecCSNoSuchCode;
	
	END_CSAPI
}


//
// Check validity of an Code
//
OSStatus SecCodeCheckValidity(SecCodeRef codeRef, SecCSFlags flags,
	SecRequirementRef requirementRef)
{
	return SecCodeCheckValidityWithErrors(codeRef, flags, requirementRef, NULL);
}

OSStatus SecCodeCheckValidityWithErrors(SecCodeRef codeRef, SecCSFlags flags,
	SecRequirementRef requirementRef, CFErrorRef *errors)
{
	BEGIN_CSAPI
	
	checkFlags(flags,
		kSecCSConsiderExpiration);
	SecPointer<SecCode> code = SecCode::required(codeRef);
	code->checkValidity(flags);
	if (const SecRequirement *req = SecRequirement::optional(requirementRef))
		code->staticCode()->validateRequirements(req->requirement(), errSecCSReqFailed);

	END_CSAPI_ERRORS
}


//
// Collect suitably laundered information about the code signature of a SecStaticCode
// and return it as a CFDictionary.
//
// This API contracts to return a few pieces of information even for unsigned
// code. This means that a SecStaticCodeRef is usable as a basic indentifier
// (i.e. handle) for any code out there.
//
const CFStringRef kSecCodeInfoCertificates =	CFSTR("certificates");
const CFStringRef kSecCodeInfoChangedFiles =	CFSTR("changed-files");
const CFStringRef kSecCodeInfoCMS =				CFSTR("cms");
const CFStringRef kSecCodeInfoDesignatedRequirement = CFSTR("designated-requirement");
const CFStringRef kSecCodeInfoEntitlements =	CFSTR("entitlements");
const CFStringRef kSecCodeInfoTime =			CFSTR("signing-time");
const CFStringRef kSecCodeInfoFormat =			CFSTR("format");
const CFStringRef kSecCodeInfoIdentifier =		CFSTR("identifier");
const CFStringRef kSecCodeInfoImplicitDesignatedRequirement = CFSTR("implicit-requirement");
const CFStringRef kSecCodeInfoMainExecutable =	CFSTR("main-executable");
const CFStringRef kSecCodeInfoPList =			CFSTR("info-plist");
const CFStringRef kSecCodeInfoRequirements =	CFSTR("requirements");
const CFStringRef kSecCodeInfoRequirementData =	CFSTR("requirement-data");
const CFStringRef kSecCodeInfoStatus =			CFSTR("status");
const CFStringRef kSecCodeInfoTrust =			CFSTR("trust");

OSStatus SecCodeCopySigningInformation(SecStaticCodeRef codeRef, SecCSFlags flags,
	CFDictionaryRef *infoRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags,
		  kSecCSInternalInformation
		| kSecCSSigningInformation
		| kSecCSRequirementInformation
		| kSecCSDynamicInformation
		| kSecCSContentInformation);
		
	SecPointer<SecStaticCode> code = SecStaticCode::requiredStatic(codeRef);
	CFRef<CFDictionaryRef> info = code->signingInformation(flags);
	
	if (flags & kSecCSDynamicInformation)
		if (SecPointer<SecCode> dcode = SecStaticCode::optionalDynamic(codeRef)) {
			uint32_t status;
			if (SecPointer<SecCode> host = dcode->host())
				status = host->getGuestStatus(dcode);
			else
				status = CS_VALID;		// root of trust, presumed valid
			info = cfmake<CFDictionaryRef>("{+%O,%O=%u}", info.get(),
				kSecCodeInfoStatus, status);
		}
	
	Required(infoRef) = info.yield();
	
	END_CSAPI
}

