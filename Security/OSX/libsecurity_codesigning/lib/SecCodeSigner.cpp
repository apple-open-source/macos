/*
 * Copyright (c) 2006-2012,2014 Apple Inc. All Rights Reserved.
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
#include "CodeSigner.h"
#include "cskernel.h"

using namespace CodeSigning;


//
// Parameter keys
//
const CFStringRef kSecCodeSignerApplicationData = CFSTR("application-specific");
const CFStringRef kSecCodeSignerDetached =		CFSTR("detached");
const CFStringRef kSecCodeSignerDigestAlgorithm = CFSTR("digest-algorithm");
const CFStringRef kSecCodeSignerDryRun =		CFSTR("dryrun");
const CFStringRef kSecCodeSignerEntitlements =	CFSTR("entitlements");
const CFStringRef kSecCodeSignerFlags =			CFSTR("flags");
const CFStringRef kSecCodeSignerForceLibraryEntitlements = CFSTR("force-library-entitlements");
const CFStringRef kSecCodeSignerIdentifier =	CFSTR("identifier");
const CFStringRef kSecCodeSignerIdentifierPrefix = CFSTR("identifier-prefix");
const CFStringRef kSecCodeSignerIdentity =		CFSTR("signer");
const CFStringRef kSecCodeSignerPageSize =		CFSTR("pagesize");
const CFStringRef kSecCodeSignerRequirements =	CFSTR("requirements");
const CFStringRef kSecCodeSignerResourceRules =	CFSTR("resource-rules");
const CFStringRef kSecCodeSignerSDKRoot =		CFSTR("sdkroot");
const CFStringRef kSecCodeSignerSigningTime =	CFSTR("signing-time");
const CFStringRef kSecCodeSignerRequireTimestamp = CFSTR("timestamp-required");
const CFStringRef kSecCodeSignerTimestampServer = CFSTR("timestamp-url");
const CFStringRef kSecCodeSignerTimestampAuthentication = CFSTR("timestamp-authentication");
const CFStringRef kSecCodeSignerTimestampOmitCertificates =	CFSTR("timestamp-omit-certificates");
const CFStringRef kSecCodeSignerPreserveMetadata = CFSTR("preserve-metadata");
const CFStringRef kSecCodeSignerTeamIdentifier =	CFSTR("teamidentifier");
const CFStringRef kSecCodeSignerPlatformIdentifier = CFSTR("platform-identifier");
const CFStringRef kSecCodeSignerRuntimeVersion = CFSTR("runtime-version");
const CFStringRef kSecCodeSignerPreserveAFSC = 	CFSTR("preserve-afsc");
const CFStringRef kSecCodeSignerOmitAdhocFlag =	CFSTR("omit-adhoc-flag");

const CFStringRef kSecCodeSignerLaunchConstraintSelf = CFSTR("lwcr-self");
const CFStringRef kSecCodeSignerLaunchConstraintParent = CFSTR("lwcr-parent");
const CFStringRef kSecCodeSignerLaunchConstraintResponsible = CFSTR("lwcr-responsible");
const CFStringRef kSecCodeSignerLibraryConstraint = CFSTR("lwcr-library");

// Keys for signature editing
const CFStringRef kSecCodeSignerEditCpuType = 	CFSTR("edit-cpu-type");
const CFStringRef kSecCodeSignerEditCpuSubtype = CFSTR("edit-cpu-subtype");
const CFStringRef kSecCodeSignerEditCMS = 		CFSTR("edit-cms");



//
// CF-standard type code functions
//
CFTypeID SecCodeSignerGetTypeID(void)
{
	BEGIN_CSAPI
	return gCFObjects().CodeSigner.typeID;
    END_CSAPI1(_kCFRuntimeNotATypeID)
}


//
// Create a signer object
//
OSStatus SecCodeSignerCreate(CFDictionaryRef parameters, SecCSFlags flags,
	SecCodeSignerRef *signerRef)
{
	BEGIN_CSAPI
		
	checkFlags(flags,
		  kSecCSEditSignature
		| kSecCSRemoveSignature
		| kSecCSSignPreserveSignature
		| kSecCSSignNestedCode
		| kSecCSSignOpaque
		| kSecCSSignV1
		| kSecCSSignNoV1
		| kSecCSSignBundleRoot
		| kSecCSSignStrictPreflight
		| kSecCSSignGeneratePEH
		| kSecCSSignGenerateEntitlementDER
		| kSecCSStripDisallowedXattrs
		| kSecCSSingleThreadedSigning);
	SecPointer<SecCodeSigner> signer = new SecCodeSigner(flags);
	signer->parameters(parameters);
	CodeSigning::Required(signerRef) = signer->handle();

    END_CSAPI
}


//
// Generate a signature
//
OSStatus SecCodeSignerAddSignature(SecCodeSignerRef signerRef,
	SecStaticCodeRef codeRef, SecCSFlags flags)
{
	return SecCodeSignerAddSignatureWithErrors(signerRef, codeRef, flags, NULL);
}

OSStatus SecCodeSignerAddSignatureWithErrors(SecCodeSignerRef signerRef,
	SecStaticCodeRef codeRef, SecCSFlags flags, CFErrorRef *errors)
{
	BEGIN_CSAPI
	checkFlags(flags,
		kSecCSReportProgress
	);
	SecCodeSigner::required(signerRef)->sign(SecStaticCode::required(codeRef), flags);
    END_CSAPI_ERRORS
}
