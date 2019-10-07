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
 */

#import "SecC2DeviceInfo.h"

#import <os/variant_private.h>
#import <CoreFoundation/CFPriv.h>
#import <os/log.h>
#if TARGET_OS_IPHONE
#import <MobileGestalt.h>
#else
#import <sys/sysctl.h>
#endif

static NSString* C2MetricBuildVersion(void);
static NSString* C2MetricProductName(void);
static NSString* C2MetricProductType(void);
static NSString* C2MetricProductVersion(void);
static NSString* C2MetricProcessName(void);
static NSString* C2MetricProcessVersion(void);
static NSString* C2MetricProcessUUID(void);

@implementation SecC2DeviceInfo

+ (BOOL) isAppleInternal {
    return os_variant_has_internal_content("com.apple.security.analytics");
}

+ (NSString*) buildVersion {
    return C2MetricBuildVersion();
}

+ (NSString*) productName {
    return C2MetricProductName();
}

+ (NSString*) productType {
    return C2MetricProductType();
}

+ (NSString*) productVersion {
    return C2MetricProductVersion();
}

+ (NSString*) processName {
    return C2MetricProcessName();
}

+ (NSString*) processVersion {
    return C2MetricProcessVersion();
}

+ (NSString*) processUUID {
    return C2MetricProcessUUID();
}

@end

/* Stolen without remorse from CloudKit. */

#pragma mark - NSBundleInfoDictionary Constants

static NSDictionary *processInfoDict() {
    static NSDictionary *processInfoDict = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSBundle *processBundle = [NSBundle mainBundle];
        processInfoDict = [processBundle infoDictionary];
    });
    return processInfoDict;
}

static NSString* C2MetricProcessName() {
    return processInfoDict()[(NSString *)kCFBundleIdentifierKey];
}

static NSString* C2MetricProcessVersion() {
    return processInfoDict()[(NSString *)_kCFBundleShortVersionStringKey];
}

static NSString* C2MetricProcessUUID() {
    static NSString* processUUIDString;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        processUUIDString = [[NSUUID UUID] UUIDString];
    });
    return processUUIDString;
}

#pragma mark - MobileGestalt Constants

#if TARGET_OS_IPHONE

static NSMutableDictionary* _CKCachedGestaltValues = nil;

static NSArray* _CKCachedLockdownKeys() {
    return @[(NSString *)kMGQUniqueDeviceID,
             (NSString *)kMGQBuildVersion,
             (NSString *)kMGQProductName,
             (NSString *)kMGQProductType,
             (NSString *)kMGQProductVersion];
}

static NSDictionary* _CKGetCachedGestaltValues() {
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        _CKCachedGestaltValues = [[NSMutableDictionary alloc] initWithCapacity:0];

        for (NSString *key in _CKCachedLockdownKeys()) {
            NSString *value = CFBridgingRelease(MGCopyAnswer((__bridge CFStringRef)key, NULL));
            if (value) {
                _CKCachedGestaltValues[key] = value;
            } else {
                os_log(OS_LOG_DEFAULT, "Error getting %@ from MobileGestalt", key);
            }
        }
    });
    return _CKCachedGestaltValues;
}

static NSString* _CKGetCachedGestaltValue(NSString *key) {
    return _CKGetCachedGestaltValues()[key];
}

static NSString* C2MetricBuildVersion() {
    return _CKGetCachedGestaltValue((NSString *)kMGQBuildVersion);
}

static NSString* C2MetricProductName() {
    return _CKGetCachedGestaltValue((NSString *)kMGQProductName);
}

static NSString* C2MetricProductType() {
    return _CKGetCachedGestaltValue((NSString *)kMGQProductType);
}

static NSString* C2MetricProductVersion() {
    return _CKGetCachedGestaltValue((NSString *)kMGQProductVersion);
}

#else

static CFStringRef CKCopySysctl(int mib[2]) {
    char sysctlString[128];
    size_t len = sizeof(sysctlString);

    // add the system product
    if (sysctl(mib, 2, sysctlString, &len, 0, 0) >= 0) {
        return CFStringCreateWithCString(kCFAllocatorDefault, sysctlString, kCFStringEncodingUTF8);
    }

    return NULL;
}

static NSDictionary* systemVersionDict() {
    static NSDictionary *sysVers = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sysVers = (__bridge NSDictionary *)_CFCopySystemVersionDictionary();
    });
    return sysVers;
}

static NSString* C2MetricBuildVersion() {
    return systemVersionDict()[(NSString *)_kCFSystemVersionBuildVersionKey];
}

static NSString* C2MetricProductName() {
    return systemVersionDict()[(NSString *)_kCFSystemVersionProductNameKey];
}

static NSString* C2MetricProductType() {
    static dispatch_once_t onceToken;
    static NSString *productType = nil;
    dispatch_once(&onceToken, ^{
        productType = (__bridge NSString *)CKCopySysctl((int[2]) { CTL_HW, HW_MODEL });
    });
    return productType;
}

static NSString* C2MetricProductVersion() {
    return systemVersionDict()[(NSString *)_kCFSystemVersionProductVersionKey];
}

#endif
