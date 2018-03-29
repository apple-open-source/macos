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

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <os/log.h>

#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "Analytics/SFAnalytics.h"
#include <utilities/SecFileLocations.h>
#include <sys/stat.h>

NSString* const CKKSAnalyticsInCircle = @"inCircle";
NSString* const CKKSAnalyticsHasTLKs = @"TLKs";
NSString* const CKKSAnalyticsSyncedClassARecently = @"inSyncA";
NSString* const CKKSAnalyticsSyncedClassCRecently = @"inSyncC";
NSString* const CKKSAnalyticsIncomingQueueIsErrorFree = @"IQNOE";
NSString* const CKKSAnalyticsOutgoingQueueIsErrorFree = @"OQNOE";
NSString* const CKKSAnalyticsInSync = @"inSync";
NSString* const CKKSAnalyticsValidCredentials = @"validCredentials";
NSString* const CKKSAnalyticsLastUnlock = @"lastUnlock";
NSString* const CKKSAnalyticsLastKeystateReady = @"lastKSR";
NSString* const CKKSAnalyticsLastInCircle = @"lastInCircle";

static NSString* const CKKSAnalyticsAttributeRecoverableError = @"recoverableError";
static NSString* const CKKSAnalyticsAttributeZoneName = @"zone";
static NSString* const CKKSAnalyticsAttributeErrorDomain = @"errorDomain";
static NSString* const CKKSAnalyticsAttributeErrorCode = @"errorCode";
static NSString* const CKKSAnalyticsAttributeErrorChain = @"errorChain";

CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassA = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassA";
CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassC = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassC";
CKKSAnalyticsFailableEvent* const CKKSEventProcessOutgoingQueue = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessOutgoingQueue";
CKKSAnalyticsFailableEvent* const CKKSEventUploadChanges = (CKKSAnalyticsFailableEvent*)@"CKKSEventUploadChanges";
CKKSAnalyticsFailableEvent* const CKKSEventStateError = (CKKSAnalyticsFailableEvent*)@"CKKSEventStateError";
CKKSAnalyticsFailableEvent* const CKKSEventProcessHealKeyHierarchy = (CKKSAnalyticsFailableEvent *)@"CKKSEventProcessHealKeyHierarchy";

NSString* const OctagonEventFailureReason = @"FailureReason";

CKKSAnalyticsFailableEvent* const OctagonEventPreflightBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightBottle";
CKKSAnalyticsFailableEvent* const OctagonEventLaunchBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventLaunchBottle";
CKKSAnalyticsFailableEvent* const OctagonEventRestoreBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventRestoreBottle";
CKKSAnalyticsFailableEvent* const OctagonEventScrubBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventScrubBottle";
CKKSAnalyticsFailableEvent* const OctagonEventSignIn = (CKKSAnalyticsFailableEvent *)@"OctagonEventSignIn";
CKKSAnalyticsFailableEvent* const OctagonEventSignOut = (CKKSAnalyticsFailableEvent *)@"OctagonEventSignIn";
CKKSAnalyticsFailableEvent* const OctagonEventRamp = (CKKSAnalyticsFailableEvent *)@"OctagonEventRamp";
CKKSAnalyticsFailableEvent* const OctagonEventBottleCheck = (CKKSAnalyticsFailableEvent *)@"OctagonEventBottleCheck";
CKKSAnalyticsFailableEvent* const OctagonEventCoreFollowUp = (CKKSAnalyticsFailableEvent *)@"OctagonEventCoreFollowUp";

CKKSAnalyticsSignpostEvent* const CKKSEventPushNotificationReceived = (CKKSAnalyticsSignpostEvent*)@"CKKSEventPushNotificationReceived";
CKKSAnalyticsSignpostEvent* const CKKSEventItemAddedToOutgoingQueue = (CKKSAnalyticsSignpostEvent*)@"CKKSEventItemAddedToOutgoingQueue";
CKKSAnalyticsSignpostEvent* const CKKSEventMissingLocalItemsFound = (CKKSAnalyticsSignpostEvent*)@"CKKSEventMissingLocalItemsFound";
CKKSAnalyticsSignpostEvent* const CKKSEventReachabilityTimerExpired = (CKKSAnalyticsSignpostEvent *)@"CKKSEventReachabilityTimerExpired";

CKKSAnalyticsActivity* const CKKSActivityOTFetchRampState = (CKKSAnalyticsActivity *)@"CKKSActivityOTFetchRampState";
CKKSAnalyticsActivity* const CKKSActivityOctagonSignIn = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonSignIn";
CKKSAnalyticsActivity* const CKKSActivityOctagonPreflightBottle = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonPreflightBottle";
CKKSAnalyticsActivity* const CKKSActivityOctagonLaunchBottle = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonLaunchBottle";
CKKSAnalyticsActivity* const CKKSActivityOctagonRestore = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonRestore";
CKKSAnalyticsActivity* const CKKSActivityScrubBottle = (CKKSAnalyticsActivity *)@"CKKSActivityScrubBottle";
CKKSAnalyticsActivity* const CKKSActivityBottleCheck = (CKKSAnalyticsActivity *)@"CKKSActivityBottleCheck";

@implementation CKKSAnalytics

+ (NSString*)databasePath
{
    // This block exists because we moved database locations in 11.3 for easier sandboxing of securityuploadd, so we're cleaning up.
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        WithPathInKeychainDirectory(CFSTR("ckks_analytics_v2.db"), ^(const char *filename) {
            remove(filename);
        });
        WithPathInKeychainDirectory(CFSTR("ckks_analytics_v2.db-wal"), ^(const char *filename) {
            remove(filename);
        });
        WithPathInKeychainDirectory(CFSTR("ckks_analytics_v2.db-shm"), ^(const char *filename) {
            remove(filename);
        });
    });

    WithPathInKeychainDirectory(CFSTR("Analytics"), ^(const char *path) {
#if TARGET_OS_IPHONE
        mode_t permissions = 0775;
#else
        mode_t permissions = 0700;
#endif // TARGET_OS_IPHONE
        int ret = mkpath_np(path, permissions);
        if (!(ret == 0 || ret ==  EEXIST)) {
            secerror("could not create path: %s (%s)", path, strerror(ret));
        }
        chmod(path, permissions);
    });
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/ckks_analytics.db") path];
}

+ (instancetype)logger
{
    // just here because I want it in the header for discoverability
    return [super logger];
}

- (void)logSuccessForEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view
{
    [self logSuccessForEventNamed:[NSString stringWithFormat:@"%@-%@", view.zoneName, event]];
    [self setDateProperty:[NSDate date] forKey:[NSString stringWithFormat:@"last_success_%@-%@", view.zoneName, event]];
}

- (bool)isCKPartialError:(NSError *)error
{
    return [error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorPartialFailure;
}

- (void)addCKPartialError:(NSMutableDictionary *)errorDictionary error:(NSError *)error depth:(NSUInteger)depth
{
    // capture one random underlaying error
    if ([self isCKPartialError:error]) {
        NSDictionary<NSString *,NSError *> *partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];
        if ([partialErrors isKindOfClass:[NSDictionary class]]) {
            for (NSString *key in partialErrors) {
                NSError* ckError = partialErrors[key];
                if (![ckError isKindOfClass:[NSError class]])
                    continue;
                if ([ckError.domain isEqualToString:CKErrorDomain] && ckError.code == CKErrorBatchRequestFailed) {
                    continue;
                }
                NSDictionary *res =  [self errorChain:ckError depth:(depth + 1)];
                if (res) {
                    errorDictionary[@"oneCloudKitPartialFailure"] = res;
                    break;
                }
            }
        }
    }
}

// if we have underlying errors, capture the chain below the top-most error
- (NSDictionary *)errorChain:(NSError *)error depth:(NSUInteger)depth
{
    NSMutableDictionary *errorDictionary = nil;

    if (depth > 5 || ![error isKindOfClass:[NSError class]])
        return nil;

    errorDictionary = [@{
       @"domain" : error.domain,
       @"code" : @(error.code),
    } mutableCopy];

    errorDictionary[@"child"] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:(depth + 1)];
    [self addCKPartialError:errorDictionary error:error depth:(depth + 1)];

    return errorDictionary;
}
- (void)logRecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event zoneName:(NSString*)zoneName withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];
    
    /* Don't allow caller to overwrite our attributes, lets merge them first */
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }
    
    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      CKKSAnalyticsAttributeRecoverableError : @(YES),
                                                      CKKSAnalyticsAttributeZoneName : zoneName,
                                                      CKKSAnalyticsAttributeErrorDomain : error.domain,
                                                      CKKSAnalyticsAttributeErrorCode : @(error.code)
                                                      }];
    
    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];
    
    [super logSoftFailureForEventNamed:event withAttributes:eventAttributes];
}
- (void)logRecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];

    /* Don't allow caller to overwrite our attributes, lets merge them first */
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }

    [eventAttributes setValuesForKeysWithDictionary:@{
        CKKSAnalyticsAttributeRecoverableError : @(YES),
        CKKSAnalyticsAttributeZoneName : view.zoneName,
        CKKSAnalyticsAttributeErrorDomain : error.domain,
        CKKSAnalyticsAttributeErrorCode : @(error.code)
    }];

    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];

    [super logSoftFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)logUnrecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }

    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];

    [eventAttributes setValuesForKeysWithDictionary:@{
        CKKSAnalyticsAttributeRecoverableError : @(NO),
        CKKSAnalyticsAttributeZoneName : view.zoneName,
        CKKSAnalyticsAttributeErrorDomain : error.domain,
        CKKSAnalyticsAttributeErrorCode : @(error.code)
    }];

    [self logHardFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)logUnrecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];

    /* Don't allow caller to overwrite our attributes, lets merge them first */
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }

    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];

    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      CKKSAnalyticsAttributeRecoverableError : @(NO),
                                                      CKKSAnalyticsAttributeZoneName : OctagonEventAttributeZoneName,
                                                      CKKSAnalyticsAttributeErrorDomain : error.domain,
                                                      CKKSAnalyticsAttributeErrorCode : @(error.code)
                                                      }];

    [self logHardFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event
{
    [self noteEventNamed:event];
}
- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event inView:(CKKSKeychainView*)view
{
    [self noteEventNamed:[NSString stringWithFormat:@"%@-%@", view.zoneName, event]];
}

- (NSDate*)dateOfLastSuccessForEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view
{
    return [self datePropertyForKey:[NSString stringWithFormat:@"last_success_%@-%@", view.zoneName, event]];
}

- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key inView:(CKKSKeychainView *)view
{
    [self setDateProperty:date forKey:[NSString stringWithFormat:@"%@-%@", key, view.zoneName]];
}
- (NSDate *)datePropertyForKey:(NSString *)key inView:(CKKSKeychainView *)view
{
    return [self datePropertyForKey:[NSString stringWithFormat:@"%@-%@", key, view.zoneName]];
}

@end

#endif // OCTAGON
