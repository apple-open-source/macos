/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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
 *
 * reset_trust_settings.m
 */

#import <Foundation/Foundation.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecTrustSettingsPriv.h>
#include <utilities/SecCFWrappers.h>

#include "SecurityCommands.h"

static int returnCFError(CFErrorRef CF_CONSUMED error) {
    CFStringRef errorString = CFErrorCopyDescription(error);
    CFStringPerformWithCString(errorString, ^(const char *utf8Str) {
        fprintf(stderr, "Failed to reset trust settings: %s\n", utf8Str);
    });
    CFIndex errCode = CFErrorGetCode(error);
    CFReleaseNull(error);
    return (int)errCode;
}

int reset_trust_settings(int argc, char * const *argv) {
    int arg;
    SecTrustResetFlags flags = 0;
    /* parse args */
    while ((arg = getopt(argc, argv, "AUXOIVC")) != -1) {
        switch(arg) {
            case 'A':
                flags = kSecTrustResetAllSettings;
                break;
            case 'U':
                flags |= kSecTrustResetUserTrustSettings;
                break;
            case 'X':
                flags |= kSecTrustResetExceptions;
                break;
            case 'O':
                flags |= kSecTrustResetOCSPCache;
                break;
            case 'I':
                flags |= kSecTrustResetIssuersCache;
                break;
            case 'V':
                flags |= kSecTrustResetValidDB;
                break;
            case 'C':
                flags |= kSecTrustResetAllCaches;
                break;
            default:
                flags = 0;
                break;
        }
    }
    if (flags == 0) {
        return SHOW_USAGE_MESSAGE; // no flags were specified
    }
    CFErrorRef error = NULL;
    bool result = SecTrustResetSettings(flags, &error);
    if (!result && error) {
        return returnCFError(error);
    }
    return 0;
}
