#include "LocalKeychainAnalytics.h"

#if __OBJC2__

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

// Public consts
// rdar://problem/41745059 SFAnalytics: collect keychain upgrade outcome information
LKAnalyticsFailableEvent const LKAEventUpgrade = (LKAnalyticsFailableEvent)@"LKAEventUpgrade";

// Internal consts
NSString* const LKAOldSchemaKey = @"oldschema";
NSString* const LKANewSchemaKey = @"newschema";
NSString* const LKAUpgradeOutcomeKey = @"upgradeoutcome";

@implementation LocalKeychainAnalytics {
    BOOL _probablyInClassD;
    NSMutableArray<LKAUpgradeOutcomeReport*>* _pendingReports;
    dispatch_queue_t _queue;
    int _notificationToken;
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

@end

// MARK: C Bridging

void LKAReportKeychainUpgradeOutcome(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome) {
    [[LocalKeychainAnalytics logger] reportKeychainUpgradeFrom:fromversion to:toversion outcome:outcome error:NULL];
}

void LKAReportKeychainUpgradeOutcomeWithError(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome, CFErrorRef error) {
    [[LocalKeychainAnalytics logger] reportKeychainUpgradeFrom:fromversion to:toversion outcome:outcome error:(__bridge NSError*)error];
}

#else   // not __OBJC2__

void LKAReportKeychainUpgradeOutcome(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome) {
    // nothing to do on 32 bit
}

void LKAReportKeychainUpgradeOutcomeWithError(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome, CFErrorRef error) {
    // nothing to do on 32 bit
}

#endif  // __OBJC2__
