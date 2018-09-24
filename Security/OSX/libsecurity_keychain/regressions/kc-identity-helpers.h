/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include "kc-helpers.h"
#include <Security/Security.h>

#ifndef kc_identity_helpers_h
#define kc_identity_helpers_h

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

static SecIdentityRef
copyFirstIdentity(SecKeychainRef kc)
{
    // Returns the first SecIdentityRef we can find.
    // This should always succeed since we can fall back on the system identity.
    // Caller must release the reference.

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0,
                                                             &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);

    /* set up the query */
    CFDictionaryAddValue( query, kSecClass, kSecClassIdentity );
    CFDictionaryAddValue( query, kSecMatchLimit, kSecMatchLimitAll );
    CFDictionaryAddValue( query, kSecReturnRef, kCFBooleanTrue );

    CFMutableArrayRef searchList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)searchList, kc);
    CFDictionarySetValue(query, kSecMatchSearchList, searchList);

    CFTypeRef results = NULL;
    OSStatus status = SecItemCopyMatching(query, &results);
    ok_status(status, "%s: SecItemCopyMatching", testName);
    CFRelease(query);

    if (status) {
        return NULL;
    }
    if (results) {
        CFArrayRef resultArray = (CFArrayRef)results;
        SecIdentityRef identity = (SecIdentityRef)CFArrayGetValueAtIndex(resultArray, 0);
        CFRetain(identity); // since we will return it
        CFRelease(results);
        return identity;
    }
    return NULL;
}
#define copyFirstIdentityTests 1

// findIdentity
// - returns a SecIdentityRef for the first identity in the given keychain
// which matches the provided certificate.
//
static SecIdentityRef
findIdentity(SecKeychainRef keychain, SecCertificateRef cert)
{
    OSStatus status = noErr;
    SecIdentitySearchRef searchRef = NULL;
    CSSM_DATA certData = { 0, NULL };

    SecIdentityRef outIdentity = NULL;

    if (!keychain || !cert) {
        return NULL;
    }

    // note: we should be using CFEqual on certificate references instead of
    // comparing the certificate data, but that is currently broken
    status = SecCertificateGetData(cert, &certData);
    ok_status(status, "%s: findIdentity: SecCertificateGetData", testName);
    if (status) {
        return NULL;
    }

    status = SecIdentitySearchCreate(keychain, (CSSM_KEYUSE)0, &searchRef);
    while (!status) {
        SecIdentityRef identityRef = NULL;
        status = SecIdentitySearchCopyNext(searchRef, &identityRef);
        if (!status) {
            SecCertificateRef aCert = NULL;
            status = SecIdentityCopyCertificate(identityRef, &aCert);
            if (!status) {
                CSSM_DATA aCertData = { 0, NULL };
                status = SecCertificateGetData(aCert, &aCertData);
                if (!status) {
                    if (aCertData.Length == certData.Length &&
                        !memcmp(aCertData.Data, certData.Data, certData.Length)) {
                        // we found the identity
                        CFRelease(aCert);
                        outIdentity = identityRef;
                        break;
                    }
                }
            }
            if (aCert) {
                CFRelease(aCert);
            }
        }
        if (identityRef) {
            CFRelease(identityRef);
        }
    }

    ok(outIdentity, "%s: findIdentity: found an identity", testName);

    if (searchRef) {
        CFRelease(searchRef);
    }

    return outIdentity;
}
#define findIdentityTests 2

#pragma clang diagnostic pop

#endif /* kc_identity_helpers_h */
