/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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

#include <Security/SecCertificateBundle.h>

#include "SecBridge.h"

#if defined(__cplusplus)
extern "C" {
#endif
// misspelled function name is declared here so symbol won't be stripped
OSStatus SecCertifcateBundleExport(
        CFArrayRef itemList,
        CSSM_CERT_BUNDLE_TYPE type,
        CSSM_CERT_BUNDLE_ENCODING encodingType,
        CSSM_DATA* data);
#if defined(__cplusplus)
}
#endif


OSStatus
SecCertificateBundleImport(
        SecKeychainRef keychain,
        const CSSM_CERT_BUNDLE* bundle,
        CSSM_CERT_BUNDLE_TYPE type,
        CSSM_CERT_BUNDLE_ENCODING encodingType,
        CFArrayRef keychainListToSkipDuplicates)
{
    BEGIN_SECAPI

	MacOSError::throwMe(unimpErr);//%%%for now

    END_SECAPI
}


OSStatus
SecCertificateBundleExport(
        CFArrayRef certificates,
        CSSM_CERT_BUNDLE_TYPE type,
        CSSM_CERT_BUNDLE_ENCODING encodingType,
        CSSM_DATA* data)
{
    BEGIN_SECAPI

	MacOSError::throwMe(unimpErr);//%%%for now

    END_SECAPI
}

// note: misspelled function name is still exported as a precaution;
// can remove this after deprecation
OSStatus
SecCertifcateBundleExport(
        CFArrayRef itemList,
        CSSM_CERT_BUNDLE_TYPE type,
        CSSM_CERT_BUNDLE_ENCODING encodingType,
        CSSM_DATA* data)
{
	return SecCertificateBundleExport(itemList, type, encodingType, data);
}
