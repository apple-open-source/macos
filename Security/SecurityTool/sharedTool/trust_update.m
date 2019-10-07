/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
 * trust_update.m
 */

#import <Foundation/Foundation.h>

#import <utilities/SecCFWrappers.h>
#import <Security/SecTrustPriv.h>

#include "SecurityCommands.h"

static int check_OTA_Supplementals_asset(void) {
    CFErrorRef error = NULL;
    uint64_t version = SecTrustOTAPKIGetUpdatedAsset(&error);
    if (error) {
        CFStringRef errorDescription = CFErrorCopyDescription(error);
        if (errorDescription) {
            char *errMsg = CFStringToCString(errorDescription);
            fprintf(stdout, "Update failed: %s\n", errMsg);
            if (errMsg) { free(errMsg); }
            CFRelease(errorDescription);
        } else {
            fprintf(stdout, "Update failed: no description\n");
        }
        CFRelease(error);
    } else {
        fprintf(stdout, "Updated succeeded\n");
    }
    if (version != 0) {
        fprintf(stdout, "Asset Content Version: %llu\n", version);
    } else {
        return 1;
    }
    return 0;
}

static int check_OTA_sec_experiment_asset(void) {
    CFErrorRef error = NULL;
    uint64_t version = SecTrustOTASecExperimentGetUpdatedAsset(&error);
    if (error) {
        CFStringRef errorDescription = CFErrorCopyDescription(error);
        if (errorDescription) {
            char *errMsg = CFStringToCString(errorDescription);
            fprintf(stdout, "Update failed: %s\n", errMsg);
            if (errMsg) { free(errMsg); }
            CFRelease(errorDescription);
        } else {
            fprintf(stdout, "Update failed: no description\n");
        }
        CFRelease(error);
    } else {
        fprintf(stdout, "Updated succeeded\n");
    }
    if (version != 0) {
        fprintf(stdout, "Asset Content Version: %llu\n", version);
    } else {
        return 1;
    }
    return 0;
}

int check_trust_update(int argc, char * const *argv) {
    int arg;

    if (argc == 1) {
        return SHOW_USAGE_MESSAGE;
    }

    while ((arg = getopt(argc, argv, "se")) != -1) {
        switch(arg) {
            case 's':
                return check_OTA_Supplementals_asset();
            case 'e':
                return check_OTA_sec_experiment_asset();
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    return 0;
}
