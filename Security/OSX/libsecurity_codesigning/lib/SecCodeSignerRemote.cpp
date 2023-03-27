/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#include "cs.h"
#include "CodeSignerRemote.h"
#include "cskernel.h"

using namespace CodeSigning;

CFTypeID
SecCodeSignerRemoteGetTypeID(void)
{
	BEGIN_CSAPI
	return gCFObjects().CodeSignerRemote.typeID;
	END_CSAPI1(_kCFRuntimeNotATypeID)
}

OSStatus
SecCodeSignerRemoteCreate(CFDictionaryRef parameters,
						  CFArrayRef signingCertificateChain,
						  SecCSFlags flags,
						  SecCodeSignerRemoteRef * __nonnull CF_RETURNS_RETAINED signerRef,
						  CFErrorRef *errors)
{
	BEGIN_CSAPI
	checkFlags(flags,
			   kSecCSSignPreserveSignature
			   | kSecCSSignV1
			   | kSecCSSignNoV1
			   | kSecCSSignBundleRoot
			   | kSecCSSignStrictPreflight
			   | kSecCSSignGeneratePEH
			   | kSecCSSignGenerateEntitlementDER);

	SecPointer<SecCodeSignerRemote> signer = new SecCodeSignerRemote(flags, signingCertificateChain);
	signer->parameters(parameters);
	CodeSigning::Required(signerRef) = signer->handle();
	END_CSAPI_ERRORS
}

OSStatus
SecCodeSignerRemoteAddSignature(SecCodeSignerRemoteRef signerRef,
								SecStaticCodeRef codeRef,
								SecCSFlags flags,
								SecCodeRemoteSignHandler signHandler,
								CFErrorRef *errors)
{
	BEGIN_CSAPI
	checkFlags(flags, 0);
	SecPointer<SecCodeSignerRemote> signer = SecCodeSignerRemote::required(signerRef);
	signer->sign(SecStaticCode::required(codeRef), flags, signHandler);
	END_CSAPI_ERRORS
}
