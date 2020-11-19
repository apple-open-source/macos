#include "LocalKeychainAnalytics.h"

#import "Security/SFAnalyticsDefines.h"

#include <sys/stat.h>
#include <notify.h>

#include <utilities/SecFileLocations.h>
#include <utilities/SecAKSWrappers.h>

@interface LKAUpgradeOutcomeReport : NSObject
@property LKAKeychainUpgradeOutcome outcome;
@property NSDictionary* attributes;
- (instancetype) initWithOutcome:(LKAKeychainUpgradeOutcome)outcome attributes:(NSDictionary*)attributes;
@end

@implementation LKAUpgradeOutcomeReport
- (instancetype) initWithOutcome:(LKAKeychainUpgradeOutcome)outcome attributes:(NSDictionary*)attributes {
    if (self = [super init]) {
        self.outcome = outcome;
        self.attributes = attributes;
    }
    return self;
}
@end

// Approved event types
// rdar://problem/41745059 SFAnalytics: collect keychain upgrade outcome information
LKAnalyticsFailableEvent const LKAEventUpgrade = (LKAnalyticsFailableEvent)@"LKAEventUpgrade";

// <rdar://problem/52038208> SFAnalytics: collect keychain backup success rates and duration
LKAnalyticsFailableEvent const LKAEventBackup = (LKAnalyticsFailableEvent)@"LKAEventBackup";
LKAnalyticsMetric const LKAMetricBackupDuration = (LKAnalyticsMetric)@"LKAMetricBackupDuration";

// <rdar://problem/60767235> SFAnalytics: Collect keychain masterkey stash success/failure rates and failure codes on macOS SUs
LKAnalyticsFailableEvent const LKAEventStash = (LKAnalyticsFailableEvent)@"LKAEventStash";
LKAnalyticsFailableEvent const LKAEventStashLoad = (LKAnalyticsFailableEvent)@"LKAEventStashLoad";

// Internal consts
NSString* const LKAOldSchemaKey = @"oldschema";
NSString* const LKANewSchemaKey = @"newschema";
NSString* const LKAUpgradeOutcomeKey = @"upgradeoutcome";
NSString* const LKABackupLastSuccessDate = @"backupLastSuccess";

@implementation LocalKeychainAnalytics {
    BOOL _probablyInClassD;
    NSMutableArray<LKAUpgradeOutcomeReport*>* _pendingReports;
    dispatch_queue_t _queue;
    int _notificationToken;
    NSDate* _backupStartTime;
    LKAKeychainBackupType _backupType;
}

- (instancetype __nullable)init {
    if (self = [super init]) {
        _probablyInClassD = YES;
        _pendingReports = [NSMutableArray<LKAUpgradeOutcomeReport*> new];
        _queue = dispatch_queue_create("LKADataQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _notificationToken = NOTIFY_TOKEN_INVALID;
    }
    return self;
}

+ (NSString*)databasePath {
    return [self defaultAnalyticsDatabasePath:@"localkeychain"];
}

// MARK: Client-specific functionality

- (BOOL)canPersistMetrics {
    @synchronized(self) {
        if (!_probablyInClassD) {
            return YES;
        }
    }

    // If this gets busy we should start caching if AKS tells us no
    bool hasBeenUnlocked = false;
    if (!SecAKSGetHasBeenUnlocked(&hasBeenUnlocked, NULL) || !hasBeenUnlocked) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            notify_register_dispatch(kUserKeybagStateChangeNotification, &self->_notificationToken, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(int token) {
                // For side effect of processing pending messages if out of class D
                [self canPersistMetrics];
            });
        });
        return NO;
    }

    @synchronized(self) {
        _probablyInClassD = NO;
        if (_notificationToken != NOTIFY_TOKEN_INVALID) {
            notify_cancel(_notificationToken);
        }
    }

    [self processPendingMessages];
    return YES;
}

- (void)processPendingMessages {
    dispatch_async(_queue, ^{
        for (LKAUpgradeOutcomeReport* report in self->_pendingReports) {
            [self reportKeychainUpgradeOutcome:report.outcome attributes:report.attributes];
        }
    });
}

- (void)reportKeychainUpgradeFrom:(int)oldVersion to:(int)newVersion outcome:(LKAKeychainUpgradeOutcome)outcome error:(NSError*)error {

    NSMutableDictionary* attributes = [@{LKAOldSchemaKey : @(oldVersion),
                                         LKANewSchemaKey : @(newVersion),
                                         LKAUpgradeOutcomeKey : @(outcome),
                                         } mutableCopy];
    if (error) {
        [attributes addEntriesFromDictionary:@{SFAnalyticsAttributeErrorDomain : error.domain,
                                               SFAnalyticsAttributeErrorCode : @(error.code)}];
    }

    if (![self canPersistMetrics]) {
        dispatch_async(_queue, ^{
            [self->_pendingReports addObject:[[LKAUpgradeOutcomeReport alloc] initWithOutcome:outcome attributes:attributes]];
        });
    } else {
        [self reportKeychainUpgradeOutcome:outcome attributes:attributes];
    }
}

- (void)reportKeychainUpgradeOutcome:(LKAKeychainUpgradeOutcome)outcome attributes:(NSDictionary*)attributes {
    if (outcome == LKAKeychainUpgradeOutcomeSuccess) {
        [self logSuccessForEventNamed:LKAEventUpgrade];
    } else {
        // I could try and pick out the recoverable errors but I think we're good treating these all the same
        [self logHardFailureForEventNamed:LKAEventUpgrade withAttributes:attributes];
    }
}

- (void)reportKeychainBackupStartWithType:(LKAKeychainBackupType)type {
    _backupStartTime = [NSDate date];
    _backupType = type;
}

// Don't attempt to add to pending reports, this should not happen in Class D
- (void)reportKeychainBackupEnd:(bool)hasBackup error:(NSError*)error {
    NSDate* backupEndTime = [NSDate date];

    // Get duration in milliseconds rounded to 100ms.
    NSInteger backupDuration = (int)(([backupEndTime timeIntervalSinceDate:_backupStartTime] + 0.05) * 10) * 100;

    // Generate statistics on backup duration separately so we know what the situation is in the field even when succeeding
    [self logMetric:@(backupDuration) withName:LKAMetricBackupDuration];

    if (hasBackup) {
        [self setDateProperty:backupEndTime forKey:LKABackupLastSuccessDate];
        [self logSuccessForEventNamed:LKAEventBackup timestampBucket:SFAnalyticsTimestampBucketHour];
    } else {
        NSInteger daysSinceSuccess = [SFAnalytics fuzzyDaysSinceDate:[self datePropertyForKey:LKABackupLastSuccessDate]];
        [self logResultForEvent:LKAEventBackup
                    hardFailure:YES
                         result:error
                 withAttributes:@{@"daysSinceSuccess" : @(daysSinceSuccess),
                                  @"duration" : @(backupDuration),
                                  @"type" : @(_backupType),
                                  }
                timestampBucket:SFAnalyticsTimestampBucketHour];
    }
}

@end

// MARK: C Bridging

void LKAReportKeychainUpgradeOutcome(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome) {
    @autoreleasepool {
        [[LocalKeychainAnalytics logger] reportKeychainUpgradeFrom:fromversion to:toversion outcome:outcome error:NULL];
    }
}

void LKAReportKeychainUpgradeOutcomeWithError(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome, CFErrorRef error) {
    @autoreleasepool {
        [[LocalKeychainAnalytics logger] reportKeychainUpgradeFrom:fromversion to:toversion outcome:outcome error:(__bridge NSError*)error];
    }
}

void LKABackupReportStart(bool hasKeybag, bool hasPasscode, bool isEMCS) {
    LKAKeychainBackupType type;
    if (isEMCS) {
        type = LKAKeychainBackupTypeEMCS;
    } else if (hasKeybag && hasPasscode) {
        type = LKAKeychainBackupTypeBagAndCode;
    } else if (hasKeybag) {
        type = LKAKeychainBackupTypeBag;
    } else if (hasPasscode) {
        type = LKAKeychainBackupTypeCode;
    } else {
        type = LKAKeychainBackupTypeNeither;
    }

    // Keep track of backup type and start time
    @autoreleasepool {
        [[LocalKeychainAnalytics logger] reportKeychainBackupStartWithType:type];
    }
}

void LKABackupReportEnd(bool hasBackup, CFErrorRef error) {
    @autoreleasepool {
        [[LocalKeychainAnalytics logger] reportKeychainBackupEnd:hasBackup error:(__bridge NSError*)error];
    }
}
