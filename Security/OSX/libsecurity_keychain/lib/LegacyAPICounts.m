/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#import <os/feature_private.h>
#import <sys/codesign.h>

#import "LegacyAPICounts.h"
#import "utilities/SecCoreAnalytics.h"
#import "SecEntitlements.h"
#import "debugging.h"
#import "SecInternalReleasePriv.h"

#pragma mark - File-Private

static NSString* applicationIdentifierForSelf() {
    NSString* identifier = nil;
    SecTaskRef task = SecTaskCreateFromSelf(kCFAllocatorDefault);

    if (task) {
        CFStringRef val = (CFStringRef)SecTaskCopyValueForEntitlement(task, kSecEntitlementApplicationIdentifier, NULL);
        if (val && CFGetTypeID(val) != CFStringGetTypeID()) {
            CFRelease(val);
        } else {
            identifier = CFBridgingRelease(val);
        }

        if (!identifier) {
            CFBundleRef mainbundle = CFBundleGetMainBundle();
            if (mainbundle != NULL) {
                CFStringRef tmp = CFBundleGetIdentifier(mainbundle);
                if (tmp != NULL) {
                    identifier = (__bridge NSString*)tmp;
                }
            }
        }

        if (!identifier) {
            identifier = CFBridgingRelease(SecTaskCopySigningIdentifier(task, NULL));
        }

        if (!identifier) {
            identifier = [NSString stringWithCString:getprogname() encoding:NSUTF8StringEncoding];
        }

        CFRelease(task);
    }

    return identifier;
}

static BOOL countLegacyAPIEnabledForThread() {
    NSNumber* value = [[NSThread currentThread] threadDictionary][@"countLegacyAPIEnabled"];

    // No value means not set at all, so not disabled by SecItem*
    if (!value || (value && [value isKindOfClass:[NSNumber class]] && [value boolValue])) {
        return YES;
    }
    return NO;
}

static NSString* identifier;
static BOOL shouldCount;

static void setup() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        identifier = applicationIdentifierForSelf() ?: @"unknown";
        shouldCount = os_feature_enabled(Security, LegacyAPICounts);
    });
}

#pragma mark - SPI

void setCountLegacyAPIEnabledForThread(bool value) {
    [[NSThread currentThread] threadDictionary][@"countLegacyAPIEnabled"] = value ? @YES : @NO;
}

void countLegacyAPI(dispatch_once_t* token, const char* api) {
    setup();

    if (api == nil) {
        secerror("LegacyAPICounts: Attempt to count API without name");
        return;
    }

    if (!shouldCount || !countLegacyAPIEnabledForThread()) {
        return;
    }

    dispatch_once(token, ^{
        NSString* apiStringObject = [NSString stringWithCString:api encoding:NSUTF8StringEncoding];
        if (!apiStringObject) {
            secerror("LegacyAPICounts: Surprisingly, char* for api name \"%s\" did not turn into NSString", api);
            return;
        }

        [SecCoreAnalytics sendEventLazy:@"com.apple.security.LegacyAPICounts" builder:^NSDictionary<NSString *,NSObject *> * _Nonnull{
            return @{
                @"app" : identifier,
                @"api" : apiStringObject,
            };
        }];
    });
}

void countLegacyMDSPlugin(const char* path, const char* guid) {
    setup();

    if (!shouldCount) {
        return;
    }

    NSString* pathString = [NSString stringWithCString:path encoding:NSUTF8StringEncoding];
    if (!pathString) {
        secerror("LegacyAPICounts: Unable to make NSString from path %s", path);
        return;
    }

    NSString* guidString = [NSString stringWithCString:guid encoding:NSUTF8StringEncoding];
    if (!guidString) {
        secerror("LegacyAPICounts: Unable to make NSString from guid %s", guid);
        return;
    }

    if(path && *path == '*') {
        // These are apparently 'built-in psuedopaths'. Don't log.
        secinfo("mds", "Ignoring the built-in MDS plugin: %@ %@", pathString, guidString);

    } else {
        secnotice("mds", "Recording an MDS plugin: %@ %@", pathString, guidString);

        [SecCoreAnalytics sendEventLazy:@"com.apple.security.LegacyMDSPluginCounts" builder:^NSDictionary<NSString *,NSObject *> * _Nonnull{
            return @{
                @"app" : identifier,
                @"mdsPath" : pathString,
                @"mdsGuid" : guidString,
            };
        }];
    }
}
