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

#import "CKKSAnalyticsLogger.h"
#import "debugging.h"
#import "CKKS.h"
#import "CKKSViewManager.h"
#import "CKKSKeychainView.h"
#include <utilities/SecFileLocations.h>
#import "Analytics/SFAnalyticsLogger.h"
#import <os/log.h>

static NSString* const CKKSAnalyticsAttributeRecoverableError = @"recoverableError";
static NSString* const CKKSAnalyticsAttributeZoneName = @"zone";
static NSString* const CKKSAnalyticsAttributeErrorDomain = @"errorDomain";
static NSString* const CKKSAnalyticsAttributeErrorCode = @"errorCode";

static NSString* const CKKSAnalyticsInCircle = @"inCircle";
static NSString* const CKKSAnalyticsDeviceID = @"ckdeviceID";
static NSString* const CKKSAnalyticsHasTLKs = @"TLKs";
static NSString* const CKKSAnalyticsSyncedClassARecently = @"inSyncA";
static NSString* const CKKSAnalyticsSyncedClassCRecently = @"inSyncC";
static NSString* const CKKSAnalyticsIncomingQueueIsErrorFree = @"IQNOE";
static NSString* const CKKSAnalyticsOutgoingQueueIsErrorFree = @"OQNOE";
static NSString* const CKKSAnalyticsInSync = @"inSync";

CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassA = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassA";
CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassC = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassC";
CKKSAnalyticsFailableEvent* const CKKSEventUploadChanges = (CKKSAnalyticsFailableEvent*)@"CKKSEventUploadChanges";
CKKSAnalyticsFailableEvent* const CKKSEventStateError = (CKKSAnalyticsFailableEvent*)@"CKKSEventStateError";

CKKSAnalyticsSignpostEvent* const CKKSEventPushNotificationReceived = (CKKSAnalyticsSignpostEvent*)@"CKKSEventPushNotificationReceived";
CKKSAnalyticsSignpostEvent* const CKKSEventItemAddedToOutgoingQueue = (CKKSAnalyticsSignpostEvent*)@"CKKSEventItemAddedToOutgoingQueue";

@implementation CKKSAnalyticsLogger

+ (NSString*)databasePath
{
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"ckks_analytics_v2.db") path];
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

- (void)logRecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view withAttributes:(NSDictionary *)attributes
{
    NSDictionary* eventAttributes = @{ CKKSAnalyticsAttributeRecoverableError : @(YES),
                                       CKKSAnalyticsAttributeZoneName : view.zoneName,
                                       CKKSAnalyticsAttributeErrorDomain : error.domain,
                                       CKKSAnalyticsAttributeErrorCode : @(error.code) };

    if (attributes) {
        /* Don't allow caller to overwrite our attributes */
        NSMutableDictionary *mergedAttributes = [attributes mutableCopy];
        [mergedAttributes setValuesForKeysWithDictionary:eventAttributes];
        eventAttributes = mergedAttributes;
    }

    [super logSoftFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)logUnrecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view withAttributes:(NSDictionary *)attributes
{
    if (error == nil)
        return;
    NSDictionary* eventAttributes = @{ CKKSAnalyticsAttributeRecoverableError : @(NO),
                                       CKKSAnalyticsAttributeZoneName : view.zoneName,
                                       CKKSAnalyticsAttributeErrorDomain : error.domain,
                                       CKKSAnalyticsAttributeErrorCode : @(error.code) };

    if (attributes) {
        /* Don't allow caller to overwrite our attributes */
        NSMutableDictionary *mergedAttributes = [attributes mutableCopy];
        [mergedAttributes setValuesForKeysWithDictionary:eventAttributes];
        eventAttributes = mergedAttributes;
    }

    [self logHardFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event inView:(CKKSKeychainView*)view
{
    [self noteEventNamed:[NSString stringWithFormat:@"%@-%@", view.zoneName, event]];
}

- (NSDate*)dateOfLastSuccessForEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view
{
    return [self datePropertyForKey:[NSString stringWithFormat:@"last_success_%@-%@", view.zoneName, event]];
}

- (NSDictionary*)extraValuesToUploadToServer
{
    NSMutableDictionary* values = [NSMutableDictionary dictionary];
    CKKSCKAccountStateTracker* accountTracker = [[CKKSViewManager manager] accountTracker];
    BOOL inCircle = accountTracker && accountTracker.currentCircleStatus == kSOSCCInCircle;
    values[CKKSAnalyticsInCircle] = @(inCircle);

    NSString *ckdeviceID = accountTracker.ckdeviceID;
    if (ckdeviceID)
        values[CKKSAnalyticsDeviceID] = ckdeviceID;
    for (NSString* viewName in [[CKKSViewManager manager] viewList]) {
        CKKSKeychainView* view = [CKKSViewManager findOrCreateView:viewName];
        NSDate* dateOfLastSyncClassA = [self dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassA inView:view];
        NSDate* dateOfLastSyncClassC = [self dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassC inView:view];

        NSInteger fuzzyDaysSinceClassASync = [CKKSAnalyticsLogger fuzzyDaysSinceDate:dateOfLastSyncClassA];
        NSInteger fuzzyDaysSinceClassCSync = [CKKSAnalyticsLogger fuzzyDaysSinceDate:dateOfLastSyncClassC];
        [values setValue:@(fuzzyDaysSinceClassASync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassASync", viewName]];
        [values setValue:@(fuzzyDaysSinceClassCSync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassCSync", viewName]];

        BOOL hasTLKs = [view.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady];
        BOOL syncedClassARecently = fuzzyDaysSinceClassASync < 7;
        BOOL syncedClassCRecently = fuzzyDaysSinceClassCSync < 7;
        BOOL incomingQueueIsErrorFree = view.lastIncomingQueueOperation.error == nil;
        BOOL outgoingQueueIsErrorFree = view.lastOutgoingQueueOperation.error == nil;

        NSString* hasTLKsKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsHasTLKs];
        NSString* syncedClassARecentlyKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsSyncedClassARecently];
        NSString* syncedClassCRecentlyKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsSyncedClassCRecently];
        NSString* incomingQueueIsErrorFreeKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsIncomingQueueIsErrorFree];
        NSString* outgoingQueueIsErrorFreeKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsOutgoingQueueIsErrorFree];

        values[hasTLKsKey] = @(hasTLKs);
        values[syncedClassARecentlyKey] = @(syncedClassARecently);
        values[syncedClassCRecentlyKey] = @(syncedClassCRecently);
        values[incomingQueueIsErrorFreeKey] = @(incomingQueueIsErrorFree);
        values[outgoingQueueIsErrorFreeKey] = @(outgoingQueueIsErrorFree);

        BOOL weThinkWeAreInSync = inCircle && hasTLKs && syncedClassARecently && syncedClassCRecently && incomingQueueIsErrorFree && outgoingQueueIsErrorFree;
        NSString* inSyncKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsInSync];
        values[inSyncKey] = @(weThinkWeAreInSync);
    }

    return values;
}

@end

#endif // OCTAGON
