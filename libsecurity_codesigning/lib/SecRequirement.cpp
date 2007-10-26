/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// SecRequirement - API frame for SecRequirement objects
//
#include "cs.h"
#include "Requirements.h"
#include "reqparser.h"
#include "reqmaker.h"
#include "reqdumper.h"
#include <Security/SecCertificate.h>
#include <security_utilities/cfutilities.h>

using namespace CodeSigning;


//
// CF-standard type code function
//
CFTypeID SecRequirementGetTypeID(void)
{
	BEGIN_CSAPI
	return gCFObjects().Requirement.typeID;
    END_CSAPI1(_kCFRuntimeNotATypeID)
}


//
// Create a Requirement from data
//
OSStatus SecRequirementCreateWithData(CFDataRef data, SecCSFlags flags,
	SecRequirementRef *requirementRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	Required(requirementRef) = (new SecRequirement(CFDataGetBytePtr(data), CFDataGetLength(data)))->handle();

	END_CSAPI
}
	

//
// Create a Requirement from data in a file
//
OSStatus SecRequirementCreateWithResource(CFURLRef resource, SecCSFlags flags,
	SecRequirementRef *requirementRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	CFRef<CFDataRef> data = cfLoadFile(resource);
	Required(requirementRef) =
		(new SecRequirement(CFDataGetBytePtr(data), CFDataGetLength(data)))->handle();

	END_CSAPI
}


//
// Create a Requirement from source text (compiling it)
//
OSStatus SecRequirementCreateWithString(CFStringRef text, SecCSFlags flags,
	SecRequirementRef *requirementRef)
{
	return SecRequirementCreateWithStringAndErrors(text, flags, NULL, requirementRef);
}

OSStatus SecRequirementCreateWithStringAndErrors(CFStringRef text, SecCSFlags flags,
	CFErrorRef *errors, SecRequirementRef *requirementRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	Required(requirementRef) = (new SecRequirement(parseRequirement(cfString(text))))->handle();

	END_CSAPI_ERRORS
}


//
// Create a Requirement group.
// This is the canonical point where "application group" is defined.
//
OSStatus SecRequirementCreateGroup(CFStringRef groupName, SecCertificateRef anchorRef,
	SecCSFlags flags, SecRequirementRef *requirementRef)
{
	BEGIN_CSAPI
	
	checkFlags(flags);
	Requirement::Maker maker;
	maker.put(opAnd);		// both of...
	maker.infoKey("Application-Group", cfString(groupName));
	if (anchorRef) {
		CSSM_DATA certData;
		MacOSError::check(SecCertificateGetData(anchorRef, &certData));
		maker.anchor(0, certData.Data, certData.Length);
	} else {
		maker.anchor();			// canonical Apple anchor
	}
	Required(requirementRef) = (new SecRequirement(maker.make(), true))->handle();

	secdebug("codesign", "created group requirement for %s", cfString(groupName).c_str());

	END_CSAPI
}


//
// Extract the stable binary from from a SecRequirementRef
//
OSStatus SecRequirementCopyData(SecRequirementRef requirementRef, SecCSFlags flags,
	CFDataRef *data)
{
	BEGIN_CSAPI
	
	const Requirement *req = SecRequirement::required(requirementRef)->requirement();
	checkFlags(flags);
	Required(data);
	*data = makeCFData(*req);

	END_CSAPI
}


//
// Generate source form for a SecRequirement (decompile/disassemble)
//
OSStatus SecRequirementCopyString(SecRequirementRef requirementRef, SecCSFlags flags,
	CFStringRef *text)
{
	BEGIN_CSAPI
	
	const Requirement *req = SecRequirement::required(requirementRef)->requirement();
	checkFlags(flags);
	Required(text);
	*text = makeCFString(Dumper::dump(req));

	END_CSAPI
}

