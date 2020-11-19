/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecRequirement.h>
#include <Security/SecRequirementPriv.h>

#include <utilities/SecCFRelease.h>
#include "security_tool.h"
#include "trusted_cert_utils.h"
#include "requirement.h"

int requirement_evaluate(int argc, char * const *argv)
{
    int err = 0;
    CFErrorRef error = NULL;
    CFStringRef reqStr = NULL;
    SecRequirementRef req = NULL;
    CFMutableArrayRef certs = NULL;

    if (argc < 3) {
        return SHOW_USAGE_MESSAGE;
    }

    // Create Requirement
    
    reqStr = CFStringCreateWithCString(NULL, argv[1], kCFStringEncodingUTF8);
    
    OSStatus status = SecRequirementCreateWithStringAndErrors(reqStr,
                                                              kSecCSDefaultFlags, &error, &req);
    
    if (status != errSecSuccess) {
        CFStringRef errorDesc = CFErrorCopyDescription(error);
        CFIndex errorLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(errorDesc),
                                                                kCFStringEncodingUTF8);
        char *errorStr = malloc(errorLength+1);
        
        CFStringGetCString(errorDesc, errorStr, errorLength+1, kCFStringEncodingUTF8);
        
        fprintf(stderr, "parsing requirement failed (%d): %s\n", status, errorStr);
        
        free(errorStr);
        CFReleaseSafe(errorDesc);
        
        err = 1;
    }

    // Create cert chain
    
    const int num_certs = argc - 2;
    
    certs = CFArrayCreateMutable(NULL, num_certs, &kCFTypeArrayCallBacks);
    
    for (int i = 0; i < num_certs; ++i) {
        SecCertificateRef cert = NULL;
        
        if (readCertFile(argv[2 + i], &cert) != 0) {
            fprintf(stderr, "Error reading certificate at '%s'\n", argv[2 + i]);
            err = 2;
            goto out;
        }
        
        CFArrayAppendValue(certs, cert);
        CFReleaseSafe(cert);
    }
    
    // Evaluate!
    
    if (req != NULL) {
        status = SecRequirementEvaluate(req, certs, NULL, kSecCSDefaultFlags);
        printf("%d\n", status);
        err = status == 0 ? 0 : 3;
    }
    
out:
    CFReleaseSafe(certs);
    CFReleaseSafe(req);
    CFReleaseSafe(reqStr);
    CFReleaseSafe(error);

    return err;
}
