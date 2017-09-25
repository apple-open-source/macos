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

NSString* const CKKSAnalyticsAttributeRecoverableError = @"recoverableError";
NSString* const CKKSAnalyticsAttributeZoneName = @"zone";
NSString* const CKKSAnalyticsAttributeErrorDomain = @"errorDomain";
NSString* const CKKSAnalyticsAttributeErrorCode = @"errorCode";

NSString* const CKKSAnalyticsHasTLKs = @"TLKs";
NSString* const CKKSAnalyticsSyncedClassARecently = @"inSyncA";
NSString* const CKKSAnalyticsSyncedClassCRecently = @"inSyncC";
NSString* const CKKSAnalyticsIncomingQueueIsErrorFree = @"IQNOE";
NSString* const CKKSAnalyticsOutgoingQueueIsErrorFree = @"OQNOE";
NSString* const CKKSAnalyticsInSync = @"inSync";

CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassA = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassA";
CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassC = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassC";
CKKSAnalyticsFailableEvent* const CKKSEventUploadChanges = (CKKSAnalyticsFailableEvent*)@"CKKSEventUploadChanges";

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

- (void)logRecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view
{
    NSDictionary* attributes = @{ CKKSAnalyticsAttributeRecoverableError : @(YES),
                                  CKKSAnalyticsAttributeZoneName : view.zoneName,
                                  CKKSAnalyticsAttributeErrorDomain : error.domain,
                                  CKKSAnalyticsAttributeErrorCode : @(error.code) };

    [super logSoftFailureForEventNamed:event withAttributes:attributes];
}

- (void)logUnrecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view
{
    NSDictionary* attributes = @{ CKKSAnalyticsAttributeRecoverableError : @(NO),
                                  CKKSAnalyticsAttributeZoneName : view.zoneName,
                                  CKKSAnalyticsAttributeErrorDomain : error.domain,
                                  CKKSAnalyticsAttributeErrorCode : @(error.code) };

    [self logHardFailureForEventNamed:event withAttributes:attributes];
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
    for (NSString* viewName in [CKKSViewManager viewList]) {
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

        BOOL weThinkWeAreInSync = hasTLKs && syncedClassARecently && syncedClassCRecently && incomingQueueIsErrorFree && outgoingQueueIsErrorFree;
        NSString* inSyncKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsInSync];
        values[inSyncKey] = @(weThinkWeAreInSync);
    }

    return values;
}

@end

#endif // OCTAGON
