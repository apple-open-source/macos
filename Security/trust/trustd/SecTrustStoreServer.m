/*
 * Copyright (c) 2018-2020 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#include <stdatomic.h>
#include <notify.h>
#include <Security/Security.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecPolicyPriv.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>
#import "OTATrustUtilities.h"
#include "SecTrustStoreServer.h"

//
// MARK : CT Exceptions
//
typedef bool(*exceptionsArrayValueChecker)(id _Nonnull obj);

static bool checkDomainsValuesCompliance(id _Nonnull obj) {
    if (![obj isKindOfClass:[NSString class]]) {
        return false;
    }
    if (SecDNSIsTLD((__bridge CFStringRef)obj)) {
        return false;
    }
    return true;
}

static bool checkCAsValuesCompliance(id _Nonnull obj) {
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return false;
    }
    if (2 != [(NSDictionary*)obj count]) {
        return false;
    }
    if (nil == ((NSDictionary*)obj)[(__bridge NSString*)kSecCTExceptionsHashAlgorithmKey] ||
        nil == ((NSDictionary*)obj)[(__bridge NSString*)kSecCTExceptionsSPKIHashKey]) {
        return false;
    }
    if (![((NSDictionary*)obj)[(__bridge NSString*)kSecCTExceptionsHashAlgorithmKey] isKindOfClass:[NSString class]] ||
        ![((NSDictionary*)obj)[(__bridge NSString*)kSecCTExceptionsSPKIHashKey] isKindOfClass:[NSData class]]) {
        return false;
    }
    if (![((NSDictionary*)obj)[(__bridge NSString*)kSecCTExceptionsHashAlgorithmKey] isEqualToString:@"sha256"]) {
        return false;
    }
    return true;
}

static bool checkExceptionsValues(NSString *key, id value, exceptionsArrayValueChecker checker, CFErrorRef *error) {
    if (![value isKindOfClass:[NSArray class]]) {
        return SecError(errSecParam, error, CFSTR("value for %@ is not an array in exceptions dictionary"), key);
    }

    __block bool result = true;
    [(NSArray*)value enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if (!checker(obj)) {
            result = SecError(errSecParam, error, CFSTR("value %lu for %@ is not the expected type"), (unsigned long)idx, key);
            *stop = true;
        }
    }];
    return result;
}

static bool checkInputExceptionsAndSetAppExceptions(NSDictionary *inExceptions, NSMutableDictionary *appExceptions, CFErrorRef *error) {
    __block bool result = true;
    [inExceptions enumerateKeysAndObjectsUsingBlock:^(NSString *_Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        if ([key isEqualToString:(__bridge NSString*)kSecCTExceptionsDomainsKey]) {
            if (!checkExceptionsValues(key, obj, checkDomainsValuesCompliance, error)) {
                *stop = YES;
                result = false;
                return;
            }
        } else if ([key isEqualToString:(__bridge NSString*)kSecCTExceptionsCAsKey]) {
            if (!checkExceptionsValues(key, obj, checkCAsValuesCompliance, error)) {
                *stop = YES;
                result = false;
                return;
            }
        } else {
            result = SecError(errSecParam, error, CFSTR("unknown key (%@) in exceptions dictionary"), key);
            *stop = YES;
            result = false;
            return;
        }
        if ([(NSArray*)obj count] == 0) {
            [appExceptions removeObjectForKey:key];
        } else {
            appExceptions[key] = obj;
        }
    }];
    return result;
}

static _Atomic bool gHasCTExceptions = false;
#define kSecCTExceptionsChanged "com.apple.trustd.ct.exceptions-changed"

static NSURL *CTExceptionsFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("CTExceptions.plist")));
}

static NSDictionary <NSString*,NSDictionary*> *readExceptionsFromDisk(NSError **error) {
    secdebug("ct", "reading CT exceptions from disk");
    NSDictionary <NSString*,NSDictionary*> *allExceptions = [NSDictionary dictionaryWithContentsOfURL:CTExceptionsFileURL()
                                                                                                error:error];
    return allExceptions;
}

bool _SecTrustStoreSetCTExceptions(CFStringRef appID, CFDictionaryRef exceptions, CFErrorRef *error)  {
    if (!SecOTAPKIIsSystemTrustd()) {
        secerror("Unable to write CT exceptions from user agent");
        return SecError(errSecWrPerm, error, CFSTR("Unable to write CT exceptions from user agent"));
    }

    if (!appID) {
        secerror("application-identifier required to set exceptions");
        return SecError(errSecParam, error, CFSTR("application-identifier required to set exceptions"));
    }

    @autoreleasepool {
        NSError *nserror = nil;
        NSMutableDictionary *allExceptions = [readExceptionsFromDisk(&nserror) mutableCopy];
        NSMutableDictionary *appExceptions = NULL;
        if (allExceptions && allExceptions[(__bridge NSString*)appID]) {
            appExceptions = [allExceptions[(__bridge NSString*)appID] mutableCopy];
        } else {
            appExceptions = [NSMutableDictionary dictionary];
            if (!allExceptions) {
                allExceptions =  [NSMutableDictionary dictionary];
            }
        }

        if (exceptions && (CFDictionaryGetCount(exceptions) > 0)) {
            NSDictionary *inExceptions = (__bridge NSDictionary*)exceptions;
            if (!checkInputExceptionsAndSetAppExceptions(inExceptions, appExceptions, error)) {
                secerror("input exceptions have error: %@", error ? *error : nil);
                return false;
            }
        }

        if (!exceptions || [appExceptions count] == 0) {
            [allExceptions removeObjectForKey:(__bridge NSString*)appID];
        } else {
            allExceptions[(__bridge NSString*)appID] = appExceptions;
        }

        if (![allExceptions writeToURL:CTExceptionsFileURL() error:&nserror]) {
            secerror("failed to write CT exceptions: %@", nserror);
            if (error) {
                *error = CFRetainSafe((__bridge CFErrorRef)nserror);
            }
            return false;
        }
        secnotice("ct", "wrote %lu CT exceptions", (unsigned long)[allExceptions count]);
        atomic_store(&gHasCTExceptions, [allExceptions count] != 0);
        notify_post(kSecCTExceptionsChanged);
        return true;
    }
}

CFDictionaryRef _SecTrustStoreCopyCTExceptions(CFStringRef appID, CFErrorRef *error) {
    @autoreleasepool {
        /* Set us up for not reading the disk when there are never exceptions */
        static int notify_token = 0;
        int check = 0;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            /* initialize gHasCTExceptions cache */
            NSError *read_error = nil;
            NSDictionary <NSString*,NSDictionary*> *allExceptions = readExceptionsFromDisk(&read_error);
            if (!allExceptions || [allExceptions count] == 0) {
                secnotice("ct", "skipping further reads. no CT exceptions found: %@", read_error);
                atomic_store(&gHasCTExceptions, false);
            } else {
                secnotice("ct", "have CT exceptions. will need to read.");
                atomic_store(&gHasCTExceptions, true);
            }

            /* read-only trustds register for notfications from the read-write trustd */
            if (!SecOTAPKIIsSystemTrustd()) {
                uint32_t status = notify_register_check(kSecCTExceptionsChanged, &notify_token);
                if (status == NOTIFY_STATUS_OK) {
                    status = notify_check(notify_token, NULL);
                }
                if (status != NOTIFY_STATUS_OK) {
                    secerror("failed to establish notification for CT exceptions: %u", status);
                    notify_cancel(notify_token);
                    notify_token = 0;
                }
            }
        });

        /* Read the negative cached value as to whether there are any exceptions to read */
        if (!SecOTAPKIIsSystemTrustd()) {
            /* Check whether we got a notification. If we didn't, and there are no exceptions set, return NULL.
             * Otherwise, we need to read from disk */
            uint32_t check_status = notify_check(notify_token, &check);
            if (check_status == NOTIFY_STATUS_OK && check == 0 && !atomic_load(&gHasCTExceptions)) {
                return NULL;
            }
        } else if (!atomic_load(&gHasCTExceptions)) {
            return NULL;
        }

        /* We need to read the exceptions from disk */
        NSError *read_error = nil;
        NSDictionary <NSString*,NSDictionary*> *allExceptions = readExceptionsFromDisk(&read_error);
        if (!allExceptions || [allExceptions count] == 0) {
            secnotice("ct", "skipping further reads. no CT exceptions found: %@", read_error);
            atomic_store(&gHasCTExceptions, false);
            return NULL;
        }

        /* If the caller specified an appID, return only the exceptions for that appID */
        if (appID) {
            return CFBridgingRetain(allExceptions[(__bridge NSString*)appID]);
        }

        /* Otherwise, combine all the exceptions into one array */
        NSMutableArray *domainExceptions = [NSMutableArray array];
        NSMutableArray *caExceptions = [NSMutableArray array];
        [allExceptions enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull __unused key, NSDictionary * _Nonnull appExceptions,
                                                           BOOL * _Nonnull __unused stop) {
            if (appExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey] &&
                checkExceptionsValues((__bridge NSString*)kSecCTExceptionsDomainsKey, appExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey],
                                      checkDomainsValuesCompliance, error)) {
                [domainExceptions addObjectsFromArray:appExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey]];
            }
            if (appExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey] &&
                checkExceptionsValues((__bridge NSString*)kSecCTExceptionsCAsKey, appExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey],
                                      checkCAsValuesCompliance, error)) {
                [caExceptions addObjectsFromArray:appExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey]];
            }
        }];
        NSMutableDictionary *exceptions = [NSMutableDictionary dictionaryWithCapacity:2];
        if ([domainExceptions count] > 0) {
            exceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey] = domainExceptions;
        }
        if ([caExceptions count] > 0) {
            exceptions[(__bridge NSString*)kSecCTExceptionsCAsKey] = caExceptions;
        }
        if ([exceptions count] > 0) {
            secdebug("ct", "found %lu CT exceptions on disk", (unsigned long)[exceptions count]);
            atomic_store(&gHasCTExceptions, true);
            return CFBridgingRetain(exceptions);
        }
        return NULL;
    }
}

//
// MARK: CA Revocation Additions
//
typedef bool(*additionsArrayValueChecker)(id _Nonnull obj);

static bool checkCARevocationValuesCompliance(id _Nonnull obj) {
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return false;
    }
    if (2 != [(NSDictionary*)obj count]) {
        return false;
    }
    if (nil == ((NSDictionary*)obj)[(__bridge NSString*)kSecCARevocationHashAlgorithmKey] ||
        nil == ((NSDictionary*)obj)[(__bridge NSString*)kSecCARevocationSPKIHashKey]) {
        return false;
    }
    if (![((NSDictionary*)obj)[(__bridge NSString*)kSecCARevocationHashAlgorithmKey] isKindOfClass:[NSString class]] ||
        ![((NSDictionary*)obj)[(__bridge NSString*)kSecCARevocationSPKIHashKey] isKindOfClass:[NSData class]]) {
        return false;
    }
    if (![((NSDictionary*)obj)[(__bridge NSString*)kSecCARevocationHashAlgorithmKey] isEqualToString:@"sha256"]) {
        return false;
    }
    return true;
}

static bool checkCARevocationValues(NSString *key, id value, additionsArrayValueChecker checker, CFErrorRef *error) {
    if (![value isKindOfClass:[NSArray class]]) {
        return SecError(errSecParam, error, CFSTR("value for %@ is not an array in revocation additions dictionary"), key);
    }

    __block bool result = true;
    [(NSArray*)value enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if (!checker(obj)) {
            result = SecError(errSecParam, error, CFSTR("value %lu for %@ is not the expected type"), (unsigned long)idx, key);
            *stop = true;
        }
    }];
    return result;
}

static bool checkInputAdditionsAndSetAppAdditions(NSDictionary *inAdditions, NSMutableDictionary *appAdditions, CFErrorRef *error) {
    __block bool result = true;
    [inAdditions enumerateKeysAndObjectsUsingBlock:^(NSString *_Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        if ([key isEqualToString:(__bridge NSString*)kSecCARevocationAdditionsKey]) {
            if (!checkCARevocationValues(key, obj, checkCARevocationValuesCompliance, error)) {
                *stop = YES;
                result = false;
                return;
            }
        } else {
            result = SecError(errSecParam, error, CFSTR("unknown key (%@) in additions dictionary"), key);
            *stop = YES;
            result = false;
            return;
        }
        if ([(NSArray*)obj count] == 0) {
            [appAdditions removeObjectForKey:key];
        } else {
            appAdditions[key] = obj;
        }
    }];
    return result;
}

static _Atomic bool gHasCARevocationAdditions = false;
#define kSecCARevocationChanged "com.apple.trustd.ca.revocation-changed"

static NSURL *CARevocationFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("CARevocation.plist")));
}

static NSDictionary <NSString*,NSDictionary*> *readRevocationAdditionsFromDisk(NSError **error) {
    secdebug("ocsp", "reading CA revocation additions from disk");
    NSDictionary <NSString*,NSDictionary*> *allAdditions = [NSDictionary dictionaryWithContentsOfURL:CARevocationFileURL()
                                                                                                error:error];
    return allAdditions;
}

bool _SecTrustStoreSetCARevocationAdditions(CFStringRef appID, CFDictionaryRef additions, CFErrorRef *error)  {
    if (!SecOTAPKIIsSystemTrustd()) {
        secerror("Unable to write CA revocation additions from user agent");
        return SecError(errSecWrPerm, error, CFSTR("Unable to write CA revocation additions from user agent"));
    }

    if (!appID) {
        secerror("application-identifier required to set CA revocation additions");
        return SecError(errSecParam, error, CFSTR("application-identifier required to set CA revocation additions"));
    }

    @autoreleasepool {
        NSError *nserror = nil;
        NSMutableDictionary *allAdditions = [readRevocationAdditionsFromDisk(&nserror) mutableCopy];
        NSMutableDictionary *appAdditions = NULL;
        if (allAdditions && allAdditions[(__bridge NSString*)appID]) {
            appAdditions = [allAdditions[(__bridge NSString*)appID] mutableCopy];
        } else {
            appAdditions = [NSMutableDictionary dictionary];
            if (!allAdditions) {
                allAdditions =  [NSMutableDictionary dictionary];
            }
        }

        if (additions && (CFDictionaryGetCount(additions) > 0)) {
            NSDictionary *inAdditions = (__bridge NSDictionary*)additions;
            if (!checkInputAdditionsAndSetAppAdditions(inAdditions, appAdditions, error)) {
                secerror("input additions have error: %@", error ? *error : nil);
                return false;
            }
        }

        if (!additions || [appAdditions count] == 0) {
            [allAdditions removeObjectForKey:(__bridge NSString*)appID];
        } else {
            allAdditions[(__bridge NSString*)appID] = appAdditions;
        }

        if (![allAdditions writeToURL:CARevocationFileURL() error:&nserror]) {
            secerror("failed to write CA revocation additions: %@", nserror);
            if (error) {
                *error = CFRetainSafe((__bridge CFErrorRef)nserror);
            }
            return false;
        }
        secnotice("ocsp", "wrote %lu CA revocation additions", (unsigned long)[allAdditions count]);
        atomic_store(&gHasCARevocationAdditions, [allAdditions count] != 0);
        notify_post(kSecCARevocationChanged);
        return true;
    }
}

CFDictionaryRef _SecTrustStoreCopyCARevocationAdditions(CFStringRef appID, CFErrorRef *error) {
    @autoreleasepool {
        /* Set us up for not reading the disk when there are never exceptions */
        static int notify_token = 0;
        int check = 0;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            /* initialize gHasCARevocation cache */
            NSError *read_error = nil;
            NSDictionary <NSString*,NSDictionary*> *allAdditions = readRevocationAdditionsFromDisk(&read_error);
            if (!allAdditions || [allAdditions count] == 0) {
                secnotice("ocsp", "skipping further reads. no CA revocation additions found: %@", read_error);
                atomic_store(&gHasCARevocationAdditions, false);
            } else {
                secnotice("ocsp", "have CA revocation additions. will need to read.");
                atomic_store(&gHasCARevocationAdditions, true);
            }

            /* read-only trustds register for notfications from the read-write trustd */
            if (!SecOTAPKIIsSystemTrustd()) {
                uint32_t status = notify_register_check(kSecCARevocationChanged, &notify_token);
                if (status == NOTIFY_STATUS_OK) {
                    status = notify_check(notify_token, NULL);
                }
                if (status != NOTIFY_STATUS_OK) {
                    secerror("failed to establish notification for CA revocation additions: %u", status);
                    notify_cancel(notify_token);
                    notify_token = 0;
                }
            }
        });

        /* Read the negative cached value as to whether there are any revocation additions to read */
        if (!SecOTAPKIIsSystemTrustd()) {
            /* Check whether we got a notification. If we didn't, and there are no additions set, return NULL.
             * Otherwise, we need to read from disk */
            uint32_t check_status = notify_check(notify_token, &check);
            if (check_status == NOTIFY_STATUS_OK && check == 0 && !atomic_load(&gHasCARevocationAdditions)) {
                return NULL;
            }
        } else if (!atomic_load(&gHasCARevocationAdditions)) {
            return NULL;
        }

        /* We need to read the exceptions from disk */
        NSError *read_error = nil;
        NSDictionary <NSString*,NSDictionary*> *allAdditions = readRevocationAdditionsFromDisk(&read_error);
        if (!allAdditions || [allAdditions count] == 0) {
            secnotice("ocsp", "skipping further reads. no CA revocation additions found: %@", read_error);
            atomic_store(&gHasCARevocationAdditions, false);
            return NULL;
        }

        /* If the caller specified an appID, return only the exceptions for that appID */
        if (appID) {
            return CFBridgingRetain(allAdditions[(__bridge NSString*)appID]);
        }

        /* Otherwise, combine all the revocation additions into one array */
        NSMutableArray *caAdditions = [NSMutableArray array];
        [allAdditions enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull __unused key, NSDictionary * _Nonnull appAdditions,
                                                           BOOL * _Nonnull __unused stop) {
            if (appAdditions[(__bridge NSString*)kSecCARevocationAdditionsKey] &&
                checkCARevocationValues((__bridge NSString*)kSecCARevocationAdditionsKey,
                                        appAdditions[(__bridge NSString*)kSecCARevocationAdditionsKey],
                                        checkCARevocationValuesCompliance, error)) {
                [caAdditions addObjectsFromArray:appAdditions[(__bridge NSString*)kSecCARevocationAdditionsKey]];
            }
        }];
        NSMutableDictionary *additions = [NSMutableDictionary dictionaryWithCapacity:2];
        if ([caAdditions count] > 0) {
            additions[(__bridge NSString*)kSecCARevocationAdditionsKey] = caAdditions;
        }
        if ([additions count] > 0) {
            secdebug("ocsp", "found %lu CA revocation additions on disk", (unsigned long)[additions count]);
            atomic_store(&gHasCARevocationAdditions, true);
            return CFBridgingRetain(additions);
        }
        return NULL;
    }
}
