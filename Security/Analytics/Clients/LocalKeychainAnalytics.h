#ifndef LocalKeychainAnalytics_h
#define LocalKeychainAnalytics_h

#include <CoreFoundation/CoreFoundation.h>

typedef enum {
    LKAKeychainUpgradeOutcomeSuccess,
    LKAKeychainUpgradeOutcomeUnknownFailure,
    LKAKeychainUpgradeOutcomeLocked,
    LKAKeychainUpgradeOutcomeInternal,
    LKAKeychainUpgradeOutcomeNewDb,
    LKAKeychainUpgradeOutcomeObsoleteDb,
    LKAKeychainUpgradeOutcomeNoSchema,
    LKAKeychainUpgradeOutcomeIndices,
    LKAKeychainUpgradeOutcomePhase1AlterTables,
    LKAKeychainUpgradeOutcomePhase1DropIndices,
    LKAKeychainUpgradeOutcomePhase1CreateSchema,
    LKAKeychainUpgradeOutcomePhase1Items,
    LKAKeychainUpgradeOutcomePhase1NonItems,
    LKAKeychainUpgradeOutcomePhase1DropOld,
    LKAKeychainUpgradeOutcomePhase2,
} LKAKeychainUpgradeOutcome;

void LKAReportKeychainUpgradeOutcome(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome);
void LKAReportKeychainUpgradeOutcomeWithError(int fromversion, int toversion, LKAKeychainUpgradeOutcome outcome, CFErrorRef error);

#if __OBJC2__

#import <Foundation/Foundation.h>
#import <Security/SFAnalytics.h>

typedef NSString* LKAnalyticsFailableEvent NS_STRING_ENUM;
extern LKAnalyticsFailableEvent const LKAEventUpgrade;

@interface LocalKeychainAnalytics : SFAnalytics

- (void)reportKeychainUpgradeFrom:(int)oldVersion to:(int)newVersion outcome:(LKAKeychainUpgradeOutcome)result error:(NSError*)error;

@end

#endif  // OBJC2
#endif  // LocalKeychainAnalytics_h
