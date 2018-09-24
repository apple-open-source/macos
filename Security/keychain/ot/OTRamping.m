/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CKContainer_Private.h>
#import <utilities/debugging.h>
#import "OTRamping.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ot/OTDefines.h"

static NSString* kFeatureAllowedKey =       @"FeatureAllowed";
static NSString* kFeaturePromotedKey =      @"FeaturePromoted";
static NSString* kFeatureVisibleKey =       @"FeatureVisible";
static NSString* kRetryAfterKey =           @"RetryAfter";
static NSString* kRampPriorityKey =         @"RampPriority";

#define kCKRampManagerDefaultRetryTimeInSeconds 86400

#if OCTAGON
@interface OTRamp (lockstateTracker) <CKKSLockStateNotification>
@end
#endif

@interface OTRamp ()
@property (nonatomic, strong) CKContainer   *container;
@property (nonatomic, strong) CKDatabase    *database;

@property (nonatomic, strong) CKRecordZone      *zone;
@property (nonatomic, strong) CKRecordZoneID    *zoneID;

@property (nonatomic, strong) NSString      *recordName;
@property (nonatomic, strong) NSString      *featureName;
@property (nonatomic, strong) CKRecordID    *recordID;

@property (nonatomic, strong) CKKSCKAccountStateTracker *accountTracker;
@property (nonatomic, strong) CKKSLockStateTracker      *lockStateTracker;
@property (nonatomic, strong) CKKSReachabilityTracker   *reachabilityTracker;

@property CKKSAccountStatus accountStatus;

@property (readonly) Class<CKKSFetchRecordsOperation> fetchRecordRecordsOperationClass;

@end

@implementation OTRamp

-(instancetype) initWithRecordName:(NSString *) recordName
                       featureName:(NSString*) featureName
                         container:(CKContainer*) container
                          database:(CKDatabase*) database
                            zoneID:(CKRecordZoneID*) zoneID
                    accountTracker:(CKKSCKAccountStateTracker*) accountTracker
                  lockStateTracker:(CKKSLockStateTracker*) lockStateTracker
               reachabilityTracker:(CKKSReachabilityTracker*) reachabilityTracker
fetchRecordRecordsOperationClass:(Class<CKKSFetchRecordsOperation>) fetchRecordRecordsOperationClass

{
    self = [super init];
    if(self){
        _container = container;
        _recordName = [recordName copy];
        _featureName = [featureName copy];
        _database = database;
        _zoneID = zoneID;
        _accountTracker = accountTracker;
        _lockStateTracker = lockStateTracker;
        _reachabilityTracker = reachabilityTracker;
        _fetchRecordRecordsOperationClass = fetchRecordRecordsOperationClass;
    }
    return self;
}

-(void) fetchRampRecord:(CKOperationDiscretionaryNetworkBehavior)networkBehavior reply:(void (^)(BOOL featureAllowed, BOOL featurePromoted, BOOL featureVisible, NSInteger retryAfter, NSError *rampStateFetchError))recordRampStateFetchCompletionBlock
{
    __weak __typeof(self) weakSelf = self;

    CKOperationConfiguration *opConfig = [[CKOperationConfiguration alloc] init];
    opConfig.allowsCellularAccess = YES;
    opConfig.discretionaryNetworkBehavior = networkBehavior;

    _recordID = [[CKRecordID alloc] initWithRecordName:_recordName zoneID:_zoneID];
    CKFetchRecordsOperation *operation = [[[self.fetchRecordRecordsOperationClass class] alloc] initWithRecordIDs:@[ _recordID]];

    operation.desiredKeys = @[kFeatureAllowedKey, kFeaturePromotedKey, kFeatureVisibleKey, kRetryAfterKey];
    
    operation.configuration = opConfig;
    operation.fetchRecordsCompletionBlock = ^(NSDictionary<CKRecordID *,CKRecord *> * _Nullable recordsByRecordID, NSError * _Nullable operationError) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            secnotice("octagon", "received callback for released object");
            operationError = [NSError errorWithDomain:octagonErrorDomain code:OTErrorCKCallback userInfo:@{NSLocalizedDescriptionKey: @"Received callback for released object"}];
            recordRampStateFetchCompletionBlock(NO, NO, NO, kCKRampManagerDefaultRetryTimeInSeconds , operationError);
            return;
        }

        BOOL featureAllowed = NO;
        BOOL featurePromoted = NO;
        BOOL featureVisible = NO;
        NSInteger retryAfter = kCKRampManagerDefaultRetryTimeInSeconds;

        secnotice("octagon", "Fetch operation records %@ fetchError %@", recordsByRecordID, operationError);
        // There should only be only one record.
        CKRecord *rampRecord = recordsByRecordID[strongSelf.recordID];

        if (rampRecord) {
            featureAllowed = [rampRecord[kFeatureAllowedKey] boolValue];
            featurePromoted = [rampRecord[kFeaturePromotedKey] boolValue];
            featureVisible = [rampRecord[kFeatureVisibleKey] boolValue];
            retryAfter = [rampRecord[kRetryAfterKey] integerValue];

            secnotice("octagon", "Fetch ramp state - featureAllowed %@, featurePromoted: %@, featureVisible: %@, retryAfter: %ld", (featureAllowed ? @YES : @NO), (featurePromoted ? @YES : @NO), (featureVisible ? @YES : @NO), (long)retryAfter);
        } else {
            secerror("octagon: Couldn't find CKRecord for ramp. Defaulting to not ramped in");
            operationError = [NSError errorWithDomain:octagonErrorDomain code:OTErrorRecordNotFound userInfo:@{NSLocalizedDescriptionKey: @" Couldn't find CKRecord for ramp. Defaulting to not ramped in"}];
        }
        recordRampStateFetchCompletionBlock(featureAllowed, featurePromoted, featureVisible, retryAfter, operationError);
    };

    [self.database addOperation: operation];
    secnotice("octagon", "Attempting to fetch ramp state from CloudKit");
}

-(BOOL) checkRampState:(NSInteger*)retryAfter networkBehavior:(CKOperationDiscretionaryNetworkBehavior)networkBehavior error:(NSError**)error
{
    __block BOOL isFeatureEnabled = NO;
    __block NSError* localError = nil;
    __block NSInteger localRetryAfter = 0;

    if(self.lockStateTracker.isLocked){
        secnotice("octagon","device is locked! can't check ramp state");
        localError = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                                         code:errSecInteractionNotAllowed
                                     userInfo:@{NSLocalizedDescriptionKey: @"device is locked"}];
        if(error){
            *error = localError;
        }
        return NO;
    }
    if(self.accountTracker.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable){
        secnotice("octagon","not signed in! can't check ramp state");
        localError = [NSError errorWithDomain:octagonErrorDomain
                                         code:OTErrorNotSignedIn
                                     userInfo:@{NSLocalizedDescriptionKey: @"not signed in"}];
        if(error){
            *error = localError;
        }
        return NO;
    }
    if(!self.reachabilityTracker.currentReachability){
        secnotice("octagon","no network! can't check ramp state");
        localError = [NSError errorWithDomain:octagonErrorDomain
                                         code:OTErrorNoNetwork
                                     userInfo:@{NSLocalizedDescriptionKey: @"no network"}];
        if(error){
            *error = localError;
        }
        return NO;
    }

    //defaults write to for whether or not a ramp record returns "enabled or disabled"
    CFBooleanRef enabled = (CFBooleanRef)CFPreferencesCopyValue((__bridge CFStringRef)self.recordName,
                                                                CFSTR("com.apple.security"),
                                                                kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    if(enabled && CFGetTypeID(enabled) == CFBooleanGetTypeID()){
        BOOL localConfigEnable = (enabled == kCFBooleanTrue);
        secnotice("octagon", "feature is %@: %@ (local config)", localConfigEnable ? @"enabled" : @"disabled", self.recordName);
        CFReleaseNull(enabled);
        return localConfigEnable;
    }
    CFReleaseNull(enabled);
    
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityOTFetchRampState withAction:nil];

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [tracker start];

    [self fetchRampRecord:networkBehavior reply:^(BOOL featureAllowed, BOOL featurePromoted, BOOL featureVisible, NSInteger retryAfter, NSError *rampStateFetchError) {
        secnotice("octagon", "fetch ramp records returned with featureAllowed: %d,\n featurePromoted: %d,\n featureVisible: %d,\n", featureAllowed, featurePromoted, featureVisible);

        isFeatureEnabled = featureAllowed;
        localRetryAfter = retryAfter;
        if(rampStateFetchError){
            localError = rampStateFetchError;
        }
        dispatch_semaphore_signal(sema);
    }];

    long timeout = (SecCKKSTestsEnabled() ? 2*NSEC_PER_SEC : NSEC_PER_SEC * 65);
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, timeout)) != 0) {
        secnotice("octagon", "timed out waiting for response from CloudKit\n");
        localError = [NSError errorWithDomain:octagonErrorDomain code:OTErrorCKTimeOut userInfo:@{NSLocalizedDescriptionKey: @"Failed to deserialize bottle peer"}];

        [logger logUnrecoverableError:localError forEvent:OctagonEventRamp withAttributes:@{
                                                                                            OctagonEventAttributeFailureReason : @"cloud kit timed out"}
         ];
    }

    [tracker stop];

    if(localRetryAfter > 0){
        secnotice("octagon", "cloud kit asked security to retry: %lu", (unsigned long)localRetryAfter);
        *retryAfter = localRetryAfter;
    }

    if(localError){
        secerror("octagon: had an error fetching ramp state: %@", localError);
        [logger logUnrecoverableError:localError forEvent:OctagonEventRamp withAttributes:@{
                                                                                            OctagonEventAttributeFailureReason : @"fetching ramp state"}
         ];
        if(error){
            *error = localError;
        }
    }
    if(isFeatureEnabled){
        [logger logSuccessForEventNamed:OctagonEventRamp];
    }

    return isFeatureEnabled;

}

- (void)ckAccountStatusChange:(CKKSAccountStatus)oldStatus to:(CKKSAccountStatus)currentStatus {
    secnotice("octagon", "%@ Received notification of CloudKit account status change, moving from %@ to %@",
              self.zoneID.zoneName,
              [CKKSCKAccountStateTracker stringFromAccountStatus: oldStatus],
              [CKKSCKAccountStateTracker stringFromAccountStatus: currentStatus]);

    switch(currentStatus) {
        case CKKSAccountStatusAvailable: {
            secnotice("octagon", "Logged into iCloud.");
            self.accountStatus = CKKSAccountStatusAvailable;
        }
            break;

        case CKKSAccountStatusNoAccount: {
            secnotice("octagon", "Logging out of iCloud. Shutting down.");
            self.accountStatus = CKKSAccountStatusNoAccount;
        }
            break;

        case CKKSAccountStatusUnknown: {
            // We really don't expect to receive this as a notification, but, okay!
            secnotice("octagon", "Account status has become undetermined. Pausing for %@", self.zoneID.zoneName);
            self.accountStatus = CKKSAccountStatusNoAccount;

        }
            break;
    }
}

@end
#endif


