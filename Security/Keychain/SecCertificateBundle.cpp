/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

#include <Security/SecCertificateBundle.h>

#include "SecBridge.h"


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
SecCertifcateBundleExport(
        CFArrayRef itemList,
        CSSM_CERT_BUNDLE_TYPE type,
        CSSM_CERT_BUNDLE_ENCODING encodingType,
        CSSM_DATA* data)
{
    BEGIN_SECAPI

	MacOSError::throwMe(unimpErr);//%%%for now

    END_SECAPI
}
