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
// SecStaticCode - API frame for SecStaticCode objects
//
#include "cs.h"
#include "StaticCode.h"
#include <security_utilities/cfmunge.h>
#include <fcntl.h>
#include <dirent.h>

using namespace CodeSigning;


//
// CF-standard type code function
//
CFTypeID SecStaticCodeGetTypeID(void)
{
	BEGIN_CSAPI
	return gCFObjects().StaticCode.typeID;
    END_CSAPI1(_kCFRuntimeNotATypeID)
}


//
// Create an StaticCode directly from disk path.
//
OSStatus SecStaticCodeCreateWithPath(CFURLRef path, SecCSFlags flags, SecStaticCodeRef *staticCodeRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	CodeSigning::Required(staticCodeRef) = (new SecStaticCode(DiskRep::bestGuess(cfString(path).c_str())))->handle();

	END_CSAPI
}

const CFStringRef kSecCodeAttributeArchitecture =	CFSTR("architecture");
const CFStringRef kSecCodeAttributeSubarchitecture =CFSTR("subarchitecture");
const CFStringRef kSecCodeAttributeBundleVersion =	CFSTR("bundleversion");
const CFStringRef kSecCodeAttributeUniversalFileOffset =	CFSTR("UniversalFileOffset");

OSStatus SecStaticCodeCreateWithPathAndAttributes(CFURLRef path, SecCSFlags flags, CFDictionaryRef attributes,
	SecStaticCodeRef *staticCodeRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	DiskRep::Context ctx;
	std::string version; // holds memory placed into ctx
	if (attributes) {
		std::string archName;
		int archNumber, subarchNumber, offset;
		if (cfscan(attributes, "{%O=%d}", kSecCodeAttributeUniversalFileOffset, &offset)) {
			ctx.offset = offset;
		} else if (cfscan(attributes, "{%O=%s}", kSecCodeAttributeArchitecture, &archName)) {
			ctx.arch = Architecture(archName.c_str());
		} else if (cfscan(attributes, "{%O=%d,%O=%d}",
				kSecCodeAttributeArchitecture, &archNumber, kSecCodeAttributeSubarchitecture, &subarchNumber))
			ctx.arch = Architecture(archNumber, subarchNumber);
		else if (cfscan(attributes, "{%O=%d}", kSecCodeAttributeArchitecture, &archNumber))
			ctx.arch = Architecture(archNumber);
		if (cfscan(attributes, "{%O=%s}", kSecCodeAttributeBundleVersion, &version))
			ctx.version = version.c_str();
	}
	
	CodeSigning::Required(staticCodeRef) = (new SecStaticCode(DiskRep::bestGuess(cfString(path).c_str(), &ctx)))->handle();

	END_CSAPI
}


//
// Check static validity of a StaticCode
//
OSStatus SecStaticCodeCheckValidity(SecStaticCodeRef staticCodeRef, SecCSFlags flags,
	SecRequirementRef requirementRef)
{
	return SecStaticCodeCheckValidityWithErrors(staticCodeRef, flags, requirementRef, NULL);
}

OSStatus SecStaticCodeCheckValidityWithErrors(SecStaticCodeRef staticCodeRef, SecCSFlags flags,
	SecRequirementRef requirementRef, CFErrorRef *errors)
{
	BEGIN_CSAPI
	
	checkFlags(flags,
		  kSecCSCheckAllArchitectures
		| kSecCSDoNotValidateExecutable
		| kSecCSDoNotValidateResources
		| kSecCSConsiderExpiration
		| kSecCSEnforceRevocationChecks
		| kSecCSCheckNestedCode);

	SecPointer<SecStaticCode> code = SecStaticCode::requiredStatic(staticCodeRef);
	const SecRequirement *req = SecRequirement::optional(requirementRef);
	DTRACK(CODESIGN_EVAL_STATIC, code, (char*)code->mainExecutablePath().c_str());
	code->staticValidate(flags, req);

	END_CSAPI_ERRORS
}


//
// ====================================================================================
//
// The following API functions are called SecCode* but accept both SecCodeRef and
// SecStaticCodeRef arguments, operating on the implied SecStaticCodeRef as appropriate.
// Hence they're here, rather than in SecCode.cpp.
//


//
// Retrieve location information for an StaticCode.
//
OSStatus SecCodeCopyPath(SecStaticCodeRef staticCodeRef, SecCSFlags flags, CFURLRef *path)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	SecPointer<SecStaticCode> staticCode = SecStaticCode::requiredStatic(staticCodeRef);
	CodeSigning::Required(path) = staticCode->canonicalPath();

	END_CSAPI
}


//
// Fetch or make up a designated requirement
//
OSStatus SecCodeCopyDesignatedRequirement(SecStaticCodeRef staticCodeRef, SecCSFlags flags,
	SecRequirementRef *requirementRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	const Requirement *req =
		SecStaticCode::requiredStatic(staticCodeRef)->designatedRequirement();
	CodeSigning::Required(requirementRef) = (new SecRequirement(req))->handle();

	END_CSAPI
}


//
// Fetch a particular internal requirement, if present
//
OSStatus SecCodeCopyInternalRequirement(SecStaticCodeRef staticCodeRef, SecRequirementType type,
	SecCSFlags flags, SecRequirementRef *requirementRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	const Requirement *req =
		SecStaticCode::requiredStatic(staticCodeRef)->internalRequirement(type);
	CodeSigning::Required(requirementRef) = req ? (new SecRequirement(req))->handle() : NULL;

	END_CSAPI
}


//
// Record for future use a detached code signature.
//
OSStatus SecCodeSetDetachedSignature(SecStaticCodeRef codeRef, CFDataRef signature,
	SecCSFlags flags)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	SecPointer<SecStaticCode> code = SecStaticCode::requiredStatic(codeRef);

	code->detachedSignature(signature); // ... and pass it to the code
	code->resetValidity();

	END_CSAPI
}


//
// Attach a code signature to a kernel memory mapping for page-in validation.
//
OSStatus SecCodeMapMemory(SecStaticCodeRef codeRef, SecCSFlags flags)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	SecPointer<SecStaticCode> code = SecStaticCode::requiredStatic(codeRef);
	if (const CodeDirectory *cd = code->codeDirectory(false)) {
		fsignatures args = { code->diskRep()->signingBase(), (void *)cd, cd->length() };
		UnixError::check(::fcntl(code->diskRep()->fd(), F_ADDSIGS, &args));
	} else
		MacOSError::throwMe(errSecCSUnsigned);

	END_CSAPI
}


//
// Attach a callback block to a code object
//
OSStatus SecStaticCodeSetCallback(SecStaticCodeRef codeRef, SecCSFlags flags, SecCodeCallback *old, SecCodeCallback monitor)
{
	BEGIN_CSAPI

	checkFlags(flags);
	SecStaticCode *code = SecStaticCode::requiredStatic(codeRef);
	if (old)
		*old = code->monitor();
	code->setMonitor(monitor);

	END_CSAPI
}
