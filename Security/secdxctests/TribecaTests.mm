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
#if AUTOERASE
#pragma push_macro("TESTING_TRIBECA")
#define TESTING_TRIBECA 1

#import <MobileGestalt.h>

#import <XCTest/XCTest.h>

#include "keychain/securityd/TribecaManager.h"

// Could replace these testing utilities with the ones in libspar
struct Unit {
	bool operator ==(Unit const &rhs) const{
		return true;
	}
};
constexpr Unit unit;

template <typename Arg1, typename RetT>
struct MockFunction{
    NSMutableArray<RetT (^)(Arg1)>* expectations_;
    MockFunction():expectations_([[NSMutableArray alloc] init]){};
    RetT operator()(Arg1 arg){
        if(expectations_.count == 0){
            XCTAssertFalse(true, "called when no expectation were set");
            panic("called when no expectation were set");
        }
        RetT (^expectation)(Arg1)  = [expectations_ lastObject];
        [expectations_ removeLastObject];
        return expectation(arg);
    }
    
    void expect(RetT (^fn)(Arg1)){
        [expectations_ addObject:fn];
    }
    
    bool done(){
        return expectations_.count == 0;
    }
};

@interface MockMSU : NSObject <MobileSoftwareUpdate>
@property MockFunction<Unit,NSTimeInterval> mockGetLastOTAUpdate;
@property MockFunction<Unit,NSTimeInterval> mockGetLastTetheredUpdate;
@end

@implementation MockMSU

- (NSTimeInterval)getLastOTAUpdate {
    return _mockGetLastOTAUpdate(unit);
}
- (NSTimeInterval)getLastTetheredUpdate {
    return _mockGetLastTetheredUpdate(unit);
}

@end

@interface NSDictTribecaPersistentData : NSObject <TribecaPersistentDataAdapter>
@property (nonatomic) NSDictionary *datastore;
@end
@implementation NSDictTribecaPersistentData
- (NSDictionary * _Nullable)getData {
    if (_datastore == NULL){
        return @{};
    }
    return _datastore;
}

- (bool)setData:(NSDictionary * _Nullable)data error:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    _datastore = data;
    return true;
}
@end

static MockFunction<Unit, NSString*> mockGetCurrentHardwareModel;
@interface TestTribecaManager : TribecaManager
// we should really swap these out these blocks with MockFunctions and use expectations
@property (nonatomic, copy) double (^currentTimeFn)();
@property (nonatomic, copy) ValidatedAETokenPolicy* __nullable (^checkForBypassFn)();
@property (nonatomic, copy) long (^uptimeFn)();
@property (nonatomic, copy) NSString* (^currentBuildFn)();
@property (nonatomic, copy) BOOL (^isPasscodeEnabledFn)();
@property (nonatomic, copy) NSDate* (^deadlineForOfflineFn)();
@property (nonatomic, copy) NSDate* (^deadlineForInactiveUseFn)();
@property (nonatomic, copy) BOOL (^tribecaDisabledFn)();
@property (nonatomic, copy) uint64_t (^getMinTTLFn)();
@property (nonatomic, copy) uint64_t (^getMaxTTLFn)();
@property (nonatomic, copy) bool (^isOnlineFn)();
@end
@implementation TestTribecaManager
- (NSString*) currentHardwareModel{
    return mockGetCurrentHardwareModel(unit);
}

- (double) currentTime{
    if(self.currentTimeFn == NULL){
        return [super currentTime];
    }
    return self.currentTimeFn();
}

- (ValidatedAETokenPolicy* __nullable) checkForBypass{
    if(self.checkForBypassFn == NULL){
        return [super checkForBypass];
    }
    return self.checkForBypassFn();
}

- (NSString *)currentBuild{
    if(self.currentBuildFn == NULL){
        return [super currentBuild];
    }
    return self.currentBuildFn();
}

- (long) uptime {
    if(self.uptimeFn == NULL){
        return [super uptime];
    }
    return self.uptimeFn();
}

- (bool) tribecaDisabled {
    if(self.tribecaDisabledFn == NULL){
        return [super tribecaDisabled];
    }
    return self.tribecaDisabledFn();
}

- (BOOL) isPasscodeEnabled{
    if(self.isPasscodeEnabledFn == NULL){
        return false;
    }
    return self.isPasscodeEnabledFn();
}
- (NSDate*) deadlineForOffline {
    if(self.deadlineForOfflineFn == NULL){
        return [super deadlineForOffline];
    }
    return self.deadlineForOfflineFn();
}

- (NSDate*) deadlineForInactiveUse{
    if(self.deadlineForInactiveUseFn == NULL){
         return [super deadlineForInactiveUse];
    }
    return self.deadlineForInactiveUseFn();
}

- (uint64_t) getMinTTL{
    if(self.getMinTTLFn == NULL){
         return [super getMinTTL];
    }
    return self.getMinTTLFn();
}

- (uint64_t) getMaxTTL{
    if(self.getMaxTTLFn == NULL){
         return [super getMaxTTL];
    }
    return self.getMaxTTLFn();
}

- (bool) isOnline{
    if(self.isOnlineFn == NULL){
         return [super isOnline];
    }
    return self.isOnlineFn();
}

@end

@interface TribecaTests : XCTestCase
@end

@implementation TribecaTests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testInvalidDataMinMaxTTL {
    bool passcodeEnable = false;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 0;
    double maxTTL = 0;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 0;
    double connectivityWindow = 0;
    double currentUptime = 0;
    // min and max are both 0. Should be invalid.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusInvalidData, testResult);
}

- (void)testInvalidDataMinMaxTTL2 {

    bool passcodeEnable = false;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 0;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 0;
    double connectivityWindow = 0;
    double currentUptime = 0;
    // happy path should be living within the regular bounds
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusInvalidData, testResult);
}

- (void)testObliterateStatusNone {

    bool passcodeEnable = false;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 10;
    // min and max are both 0. Should be invalid.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testObliterateStatusMinLife {

    bool passcodeEnable = false;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 50;
    // should trigger min case since it has no pass code
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testObliterateStatusMinLifeNotTriggered {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 50;
    // should trigger be happy now since it has pass code
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testObliterateStatusMaxLife {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 101;
   // should trigger max case since
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMaxLife, testResult);
}


- (void)testObliterateStatusMaxLifeBypassed {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 101;
   // should trigger max case since
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, true);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testObliterateStatusRdar108228354 {

    // This issue was caused by the status endpoint NOT being extended by the lastEscAuth
    bool passcodeEnable = false;
    double lastUnlock = 703548600; // Tuesday, April 18, 2023 3:10:00 PM GMT-07:00
    double lastCheckin = 703548000; // Tuesday, April 18, 2023 3:00:00 PM GMT-07:00
    NSNumber* lastEscAuth = @703541000; // Tuesday, April 18, 2023 1:03:20 PM GMT-07:00
    NSNumber* minTTL = @604800; // 7 days
    NSNumber* maxTTL = @2592000; // 30 days
    NSNumber* currentTime = @703548000; // Tuesday, April 18, 2023 3:00:00 PM GMT-07:00
    double gracePeriod = 3600; // 1 Hour
    NSNumber* lastRestore = @702936000; // Tuesday, April 11, 2023 1:00:00 PM GMT-07:00
    double inactivityWindow = 259200; // 3 days
    double connectivityWindow = 259200; // 3 days
    double currentUptime = 10; // 10 seconds after a reboot
    
    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestore currentTime:currentTime minTTL:minTTL maxTTL:maxTTL];
    manager.uptimeFn = ^long() {
        return currentUptime;
    };
    manager.currentState[kTribecaLastEscAuth] = lastEscAuth;
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];

    int testResult = tribecaShouldObliterate(
        passcodeEnable,
        lastUnlock,
        lastCheckin,
        minTTL.doubleValue,
        maxTTL.doubleValue,
        currentTime.doubleValue,
        gracePeriod,
        [manager getLastRestore].timeIntervalSinceReferenceDate,
        inactivityWindow,
        connectivityWindow,
        currentUptime,
        false
    );

    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestore);
    XCTAssertEqual([status.scheduledAutoerase timeIntervalSinceReferenceDate], lastEscAuth.doubleValue + minTTL.doubleValue);
}


- (void)testObliterateStatusMinLife2 {

    bool passcodeEnable = false;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 0;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 101;
   // should trigger min case since it has no pass code
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testShouldNotExpiredByLastRestoreTime {

    bool passcodeEnable = false;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 50;
    double gracePeriod = 0;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    // trigger that build should NOT be expired via the last restore time
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMinCase {

    bool passcodeEnable = false;
    double lastUnlock = 60;
    double lastCheckin = 60;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 60;
    double gracePeriod = 0;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMinCaseEscAuthExtend {

    bool passcodeEnable = false;
    double lastUnlock = 60;
    double lastCheckin = 60;
    NSNumber *lastEscAuth = @59;
    NSNumber *minTTL = @50;
    NSNumber *maxTTL = @100;
    double currentTime = 60;
    double gracePeriod = 0;
    NSNumber *lastRestore = @10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return currentTime;
    };
    manager.uptimeFn = ^long{
        return currentUptime;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestore,
        kTribecaLastEscAuth: lastEscAuth
    }];
        
    // trigger that build should be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL.doubleValue, maxTTL.doubleValue, currentTime, gracePeriod, [manager getLastRestore].timeIntervalSinceReferenceDate, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMinCaseEscAuthExtendExpired {

    bool passcodeEnable = false;
    double lastUnlock = 60;
    double lastCheckin = 60;
    NSNumber *lastEscAuth = @59;
    NSNumber *minTTL = @50;
    NSNumber *maxTTL = @100;
    double currentTime = 130;
    double gracePeriod = 0;
    NSNumber *lastRestore = @10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return currentTime;
    };
    manager.uptimeFn = ^long{
        return currentUptime;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestore,
        kTribecaLastEscAuth: lastEscAuth
    }];
        
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL.doubleValue, maxTTL.doubleValue, currentTime, gracePeriod, [manager getLastRestore].timeIntervalSinceReferenceDate, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testShouldNOTExpiredByLastRestoreTimeMin {

    bool passcodeEnable = true;
    double lastUnlock = 60;
    double lastCheckin = 60;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 60;
    double gracePeriod = 0;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    // trigger that build should NOT be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldNOTExpiredByLastRestoreTimeMinPreviousEscAuth {

    bool passcodeEnable = true;
    double lastUnlock = 60;
    double lastCheckin = 60;
    NSNumber *lastEscAuth = @1;
    NSNumber *minTTL = @50;
    NSNumber *maxTTL = @100;
    double currentTime = 60;
    double gracePeriod = 0;
    NSNumber *lastRestore = @10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return currentTime;
    };
    manager.uptimeFn = ^long{
        return currentUptime;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestore,
        kTribecaLastEscAuth: lastEscAuth
    }];
        
    // trigger that build should NOT be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL.doubleValue, maxTTL.doubleValue, currentTime, gracePeriod, [manager getLastRestore].timeIntervalSinceReferenceDate, inactivityWindow, connectivityWindow, currentUptime, false);
    
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMax {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 500;
    double gracePeriod = 0;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting max case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMaxLife, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMaxEscExtend {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    NSNumber *lastEscAuth = @499;
    NSNumber *minTTL = @50;
    NSNumber *maxTTL = @100;
    double currentTime = 500;
    double gracePeriod = 0;
    NSNumber *lastRestore = @10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return currentTime;
    };
    manager.uptimeFn = ^long{
        return currentUptime;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestore,
        kTribecaLastEscAuth: lastEscAuth
    }];
        
    // trigger that build should be expired via the last restore time hitting max case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL.doubleValue, maxTTL.doubleValue, currentTime, gracePeriod, [manager getLastRestore].timeIntervalSinceReferenceDate, inactivityWindow, connectivityWindow, currentUptime, false);

    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMaxEscExtendExpired {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    NSNumber *lastEscAuth = @499;
    NSNumber *minTTL = @50;
    NSNumber *maxTTL = @100;
    double currentTime = 650;
    double gracePeriod = 0;
    NSNumber *lastRestore = @10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    
    
    
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return currentTime;
    };
    manager.uptimeFn = ^long{
        return currentUptime;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestore,
        kTribecaLastEscAuth: lastEscAuth
    }];
        
    // trigger that build should be expired via the last restore time hitting max case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL.doubleValue, maxTTL.doubleValue, currentTime, gracePeriod, [manager getLastRestore].timeIntervalSinceReferenceDate, inactivityWindow, connectivityWindow, currentUptime, false);

    XCTAssertEqual(tribecaObliterateStatusMaxLife, testResult);
}

- (void)testShouldNOTExpiredByMinTTL {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 40;
    double gracePeriod = 0;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    // This test at first glance would appear like it would trigger the lastUnlock and Checkin condition
    // but since its under the minTTL nothing happenss.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByOfflineShortCircuit {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 20;
    double maxTTL = 100;
    double currentTime = 40;
    double gracePeriod = 0;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    // Should make it pass the minTTL short circuit condtion and land in inactive and offline
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusInactiveAndOffline, testResult);
}

- (void)testShouldNOTExpiredByOfflineShortCircuitByGracePeriod {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 20;
    double maxTTL = 100;
    double currentTime = 40;
    double gracePeriod = 1000;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;
    // should not evalute the condtion now since its in the grace period.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldNOTExpiredByOfflineShortCircuitByUnlock {

    bool passcodeEnable = true;
    
    double lastCheckin = 0;
    double minTTL = 20;
    double maxTTL = 100;
    double currentTime = 40;
    double gracePeriod = 0;
    double lastRestore = 10;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 0;

    uint64_t lastUnlock = currentTime;
    // even though its still offline, the device has been unlocked recently so should result in None
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testRestoreTimeAferCurrentTime {

    bool passcodeEnable = false;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 10000;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 10;
    // restore time is after current time
    int testResult = tribecaShouldObliterate(
        passcodeEnable,
        lastUnlock,
        lastCheckin,
        minTTL,
        maxTTL,
        currentTime,
        gracePeriod,
        lastRestore,
        inactivityWindow,
        connectivityWindow,
        currentUptime,
        false
    );
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testRestoreTimeLabIphone {

    bool passcodeEnable = true;
    double lastUnlock = 0;
    double lastCheckin = 0;
    double minTTL = 50;
    double maxTTL = 100;
    double currentTime = 0;
    double gracePeriod = 0;
    double lastRestore = 30;
    double inactivityWindow = 10;
    double connectivityWindow = 10;
    double currentUptime = 10;
    // there are lab iphone with no clock that the restore time might be in the future but currentTime always reboots to 0
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testGetStatusExpiredWhenNoData {
    TribecaManager* manager = [[TribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    XCTAssertTrue(status.timeLeftInSeconds == 0);
    XCTAssertEqual(status.usingAEToken, false);
}

- (void)testGetStatusValidCaseWithTimeLeftMinTTL {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return [now doubleValue];
    };
    
    manager.uptimeFn = ^long{
        return 0;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)false;
    };
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    XCTAssertEqual(
        status.timeLeftInSeconds,
        [minTTL unsignedLongLongValue] - ([now unsignedLongLongValue] - [lastRestoreTime unsignedLongLongValue])
    );
    XCTAssertEqual(status.usingAEToken, false);
}

- (void)testGetStatusValidCaseWithTimeLeftMinTTLUpTime {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return [now doubleValue];
    };
    const long uptime = minTTL.longValue - 10;
    manager.uptimeFn = ^long{
        return uptime;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)false;
    };
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    XCTAssertEqual(
        status.timeLeftInSeconds,
        (uint64_t)10
    );
    XCTAssertEqual(status.usingAEToken, false);
}


- (void)testGetStatusValidCaseWithTimeLeftMaxTTL {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return [now doubleValue];
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)true;
    };
    
    manager.uptimeFn = ^long {
        return 0;
    };
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    XCTAssertEqual(
        status.timeLeftInSeconds,
        [maxTTL unsignedLongLongValue] - ([now unsignedLongLongValue] - [lastRestoreTime unsignedLongLongValue])
    );
    XCTAssertEqual(status.usingAEToken, false);
}


- (void)testGetStatusValidCaseWithExpire{

    NSNumber* lastRestoreTime = @0;
    NSNumber* now = @tribecaSettingDefaultMaxTTL;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return [now doubleValue];
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)false;
    };
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    XCTAssertEqual(status.timeLeftInSeconds, (uint64_t)0);
    XCTAssertEqual(status.usingAEToken, false);
}

- (void)testGetStatusValidCaseWithExpire2{

    NSNumber* lastRestoreTime = @667868266; // Tuesday, March 1, 2022 2:57:46 PM 
    NSNumber* now = @667869562; //Tuesday, March 1, 2022 3:19:22 PM - so just restored couple of mins ago
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;
    bool hasPasscode = true;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return [now doubleValue];
    };
    
    manager.uptimeFn = ^long{
        return 0;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)hasPasscode;
    };
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    const uint64_t expectedTimeLeft = (uint64_t)(tribecaSettingDefaultMaxTTL -([now unsignedLongLongValue] - [lastRestoreTime unsignedLongLongValue]));
    XCTAssertEqual(status.timeLeftInSeconds, expectedTimeLeft);
    XCTAssertEqual(status.usingAEToken, false);
}

- (void)testGetStatusValidCaseWithTimeTravel {
    
    NSNumber* now = @704075256;
    NSNumber* lastRestoreTime = [[NSNumber alloc] initWithUnsignedLong:now.unsignedLongLongValue + 100000]; // In the future
    NSNumber* uptime = @100;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;
    
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentTimeFn = ^{
        return [now doubleValue];
    };
    
    manager.uptimeFn = ^long {
        return uptime.longValue;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)false;
    };
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    // System time in unreliable so NOW is the last restore. This means the uptime limits the build the most.
    XCTAssertEqual(status.timeLeftInSeconds,  minTTL.unsignedLongLongValue - uptime.unsignedLongLongValue);
    XCTAssertEqual(status.usingAEToken, false);
}

+ (void)testInitRestoreTimeWithLastOTA:(double)lastOTA
    lastTetherUpdate:(double)lastTetherUpdate
    currentTime:(double)now
    previousRestoreTime:(uint64_t*)maybePreviousRestoreTime
    previousBuildVersion:(NSString*)previousBuildVersion
    currentBuildVersion:(NSString*)currentBuildVersion
    expectedLastRestoreTime:(uint64_t)expectedLastRestoreTime
    
{
    MockMSU* msu = [[MockMSU alloc] init];
    NSDictTribecaPersistentData* datastore = [[NSDictTribecaPersistentData alloc] init];
    if(maybePreviousRestoreTime != NULL && previousBuildVersion != NULL){
        datastore.datastore = @{
            kTribecaLastRestoreTime: [NSNumber numberWithUnsignedLongLong: *maybePreviousRestoreTime],
            kTribecaLastRestoreTimeVersion: previousBuildVersion,
            kTribecaLastConnectivity: @10,
            kTribecaLastInteractivity: @10
        };
    }
    mockGetCurrentHardwareModel.expect(^(Unit u){
        return @"iphone13";
    });
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: @tribecaSettingDefaultMinTTL,
            kTribecaSettingMaxTTL: @tribecaSettingDefaultMaxTTL,
        }
        mobileSoftwareUpdate: msu
        datastore: datastore
    ];
    
    manager.currentBuildFn = ^NSString*{
        return currentBuildVersion;
    };
    
    manager.currentTimeFn = ^double{
        return now;
    };
    
    msu.mockGetLastOTAUpdate.expect(^double(Unit) {
       return lastOTA;
    });
    msu.mockGetLastTetheredUpdate.expect(^double(Unit) {
       return lastTetherUpdate;
    });
    
    [manager registerXPCActivities];
    
    // Should use current timestamp
    XCTAssertEqual([datastore.datastore[kTribecaLastRestoreTime] unsignedLongLongValue], expectedLastRestoreTime);
    XCTAssertTrue(msu.mockGetLastOTAUpdate.done());
    XCTAssertTrue(msu.mockGetLastTetheredUpdate.done());
    XCTAssertTrue(mockGetCurrentHardwareModel.done());
    
}

- (void)testInitRestoreTimeMSUTetherRestore {
    [TribecaTests
        testInitRestoreTimeWithLastOTA:10
        lastTetherUpdate:20
        currentTime:40000
        previousRestoreTime: NULL
        previousBuildVersion: NULL
        currentBuildVersion: @"A"
        expectedLastRestoreTime:20
    ];
}

- (void)testInitRestoreTimeMSUTetherRestoreWouldMakeBuildValid {
    uint64_t previousRestoreDate = 10;
    [TribecaTests
        testInitRestoreTimeWithLastOTA:0
        lastTetherUpdate:39999
        currentTime:40000
        previousRestoreTime: &previousRestoreDate
        previousBuildVersion: @"A"
        currentBuildVersion: @"A"
        expectedLastRestoreTime:39999
    ];
}

- (void)testInitRestoreTimeOTATetherRestore {
    [TribecaTests
        testInitRestoreTimeWithLastOTA:33
        lastTetherUpdate:11
        currentTime:40000
        previousRestoreTime: NULL
        previousBuildVersion: NULL
        currentBuildVersion: @"A"
        expectedLastRestoreTime:33
    ];
}

- (void)testInitRestoreTimeOTATetherRestoreWouldMakeBuildValid {
    uint64_t previousRestoreDate = 10;
    [TribecaTests
        testInitRestoreTimeWithLastOTA:39999
        lastTetherUpdate:0
        currentTime:40000
        previousRestoreTime: &previousRestoreDate
        previousBuildVersion: @"A"
        currentBuildVersion: @"A"
        expectedLastRestoreTime:39999
    ];
}

- (void)testInitRestoreTimeInvalidMSUData {
    // MSU is not in a reliable state, should trust current timestamp
    [TribecaTests
        testInitRestoreTimeWithLastOTA:10
        lastTetherUpdate:0
        currentTime:40000
        previousRestoreTime: NULL
        previousBuildVersion: NULL
        currentBuildVersion: @"A"
        expectedLastRestoreTime:40000
    ];
}

- (void)testInitRestoreTimeInvalidMSUData2 {
    // MSU is not in a reliable state, should trust current timestamp
    [TribecaTests
        testInitRestoreTimeWithLastOTA:0
        lastTetherUpdate:10
        currentTime:40000
        previousRestoreTime: NULL
        previousBuildVersion: NULL
        currentBuildVersion: @"A"
        expectedLastRestoreTime:40000
    ];
}

- (void)testInitRestoreTimeNoMSUData {
    // MSU is not in a reliable state, should trust current timestamp
    [TribecaTests
        testInitRestoreTimeWithLastOTA:0
        lastTetherUpdate:0
        currentTime:40000
        previousRestoreTime: NULL
        previousBuildVersion: NULL
        currentBuildVersion: @"A"
        expectedLastRestoreTime:40000
    ];
}

- (void)testInitRestoreTimeNoMSUDataWithPreviousRestoreDate {
    // MSU is not in a reliable state, but the build has not changed use last stored timestamp
    uint64_t previousRestoreDate = 1000;
    [TribecaTests
        testInitRestoreTimeWithLastOTA:0
        lastTetherUpdate:0
        currentTime:40000
        previousRestoreTime: &previousRestoreDate
        previousBuildVersion: @"A"
        currentBuildVersion: @"A"
        expectedLastRestoreTime: previousRestoreDate
    ];
}

- (void)testInitRestoreTimeNoMSUDataWithPreviousRestoreDateDifferentBuild {
    // MSU is not in a reliable state, but the build HAS changed use current timestamp as last restore.
    uint64_t previousRestoreDate = 1000;
    [TribecaTests
        testInitRestoreTimeWithLastOTA:0
        lastTetherUpdate:0
        currentTime:40000
        previousRestoreTime: &previousRestoreDate
        previousBuildVersion: @"OldVersion"
        currentBuildVersion: @"A"
        expectedLastRestoreTime: 40000
    ];
}

- (void)testGetStatusTimeLeft {
    TribecaManager* manager = [[TribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    [[manager persistentData]
        setData:@{}
        error:nil
    ];
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    XCTAssertTrue(status.timeLeftInSeconds == 0);
    XCTAssertEqual(status.usingAEToken, false);
}

- (void)testRunningOnDisabledHardware{
    mockGetCurrentHardwareModel.expect(^(Unit u){
        return @"E89AP";
    });
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];

    XCTAssertFalse([manager shouldRunTribeca], "Should not initialize on tribeca on unsupported hardware");
    XCTAssertTrue(mockGetCurrentHardwareModel.done());
}

- (void)testRunningOnDisabledHardware2{
    mockGetCurrentHardwareModel.expect(^(Unit u){
        return @"E89DEV";
    });
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    XCTAssertFalse([manager shouldRunTribeca], "Should not initialize on tribeca on unsupported hardware");
    XCTAssertTrue(mockGetCurrentHardwareModel.done());
}

- (void)testRunningOnDisabledHardwareReturnsNil{
    mockGetCurrentHardwareModel.expect(^(Unit u){
        return (NSString*)nullptr;
    });
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    XCTAssertTrue([manager shouldRunTribeca], "Should initialize tribeca");
    XCTAssertTrue(mockGetCurrentHardwareModel.done());
}

- (void)testRunningOnDisabledHardwareCaseInsensitive{
    mockGetCurrentHardwareModel.expect(^(Unit u){
        return @"E89ap";
    });
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    XCTAssertFalse([manager shouldRunTribeca], "Should not initialize on tribeca on unsupported hardware");
    XCTAssertTrue(mockGetCurrentHardwareModel.done());
}

- (void)testRunningOnEnabledHardware{
    mockGetCurrentHardwareModel.expect(^(Unit u){
        return @"randomDeviceNotOnDeviceList";
    });
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    XCTAssertTrue([manager shouldRunTribeca], "Should initialize on tribeca");
    XCTAssertTrue(mockGetCurrentHardwareModel.done());
}

- (void)testDefaultValues {
    TribecaManager* manager = [[TribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingCheckinInterval] != NULL &&
        [manager 
            getSetting:kTribecaSettingCheckinInterval
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultCheckinInterval);
    
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingMaxTTL] != NULL &&
        [manager 
            getSetting:kTribecaSettingMaxTTL
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultMaxTTL);
    
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingMinTTL] != NULL &&
        [manager 
            getSetting:kTribecaSettingMinTTL
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultMinTTL);
    
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingInteractivityTimeout] != NULL &&
        [manager 
            getSetting:kTribecaSettingInteractivityTimeout
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultInteractivityTimeout);
    
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingConnectivityTimeout] != NULL &&
        [manager 
            getSetting:kTribecaSettingConnectivityTimeout
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultInteractivityTimeout);
    
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingBootGracePeriod] != NULL &&
        [manager 
            getSetting:kTribecaSettingBootGracePeriod
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultBootGracePeriod);
    
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingShouldObliterate] != NULL &&
        [manager 
            getSetting:kTribecaSettingShouldObliterate
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultShouldObliterate);
    
    XCTAssertTrue(
        manager.tribecaSettings[kTribecaSettingModalSnoozeDuration] != NULL &&
        [manager 
            getSetting:kTribecaSettingModalSnoozeDuration
            defaultValue: INT_MAX
        ] == tribecaSettingDefaultSnoozeDuration
        );
}

- (void)testBypassPolicyValid {
    TribecaManager* manager = [[TribecaManager alloc] init];
    NSDictionary *goodTime = @{
        kTribecaBypassFieldValidAfter: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] - 100000],
        kTribecaBypassFieldBypassUntil: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] + 200000],
    };
    
    NSDictionary *badTime = @{
        kTribecaBypassFieldValidAfter: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] + 100000],
        kTribecaBypassFieldBypassUntil: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] + 200000],
    };
    
    XCTAssertNotNil([manager validateAETokenPolicy:goodTime]);
    XCTAssertNil([manager validateAETokenPolicy:badTime]);

    NSString* buildId = (__bridge_transfer NSString *)MGCopyAnswer(CFSTR("BuildVersion"), NULL);

    NSDictionary *goodBuild = @{
        kTribecaBypassFieldValidAfter: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] - 100000],
        kTribecaBypassFieldBypassUntil: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] + 200000],
        kTribecaBypassFieldBuildVersion: buildId,
    };

    NSDictionary *badBuild = @{
        kTribecaBypassFieldValidAfter: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] - 100000],
        kTribecaBypassFieldBypassUntil: [NSNumber numberWithLongLong:[[NSDate date] timeIntervalSince1970] + 200000],
        kTribecaBypassFieldBuildVersion: @"banana",
    };
    
    XCTAssertNotNil([manager validateAETokenPolicy:goodBuild]);
    XCTAssertNil([manager validateAETokenPolicy:badBuild]);
    
    NSDictionary *invalidTime = @{
        kTribecaBypassFieldValidAfter: @"banana",
        kTribecaBypassFieldBypassUntil: @"banana",
        kTribecaBypassFieldBuildVersion: @"banana",
    };
    XCTAssertNil([manager validateAETokenPolicy:invalidTime]);

}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

- (void)testGetStatusXPCTimeLeftZeroCase {
    TribecaManager* manager = [[TribecaManager alloc]
        initWithSettings: NULL
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];

    [[manager persistentData]
        setData:@{}
        error:nil
    ];

    TribecaServiceStatus status = [manager getStatus];

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSDictionary *statusDict = nil;
    [manager status:^(NSDictionary * s) {
        statusDict = s;
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    XCTAssertEqual(status.enabled, true);
    XCTAssertEqual(statusDict[kSecuritydTribecaServiceStatus], @"Enabled");
    XCTAssertTrue(status.timeLeftInSeconds == 0);
    XCTAssertTrue([statusDict[kSecuritydTribecaServiceTimeLeft] isEqual:@0]);
    XCTAssertTrue([statusDict[kSecuritydTribecaServiceTimeLeftFormatted] isEqual:@"0"]);
}

- (void)testGetStatusXPCWithTimeLeftMinTTL {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @604800;
    NSNumber* maxTTL = @2592000;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];

    manager.currentTimeFn = ^{
        return [now doubleValue];
    };
    
    manager.uptimeFn = ^long {
        return 0;
    };
    
    manager.getMinTTLFn = ^uint64_t {
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t {
        return maxTTL.unsignedLongLongValue;
    };
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];

    manager.isPasscodeEnabledFn = ^{
        return (BOOL)false;
    };
    
    uint64_t expectTimeLeft = [minTTL unsignedLongLongValue] - ([now unsignedLongLongValue] - [lastRestoreTime unsignedLongLongValue]);
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, true);
    XCTAssertEqual(
        status.timeLeftInSeconds,
        expectTimeLeft
    );
    XCTAssertEqual(status.usingAEToken, false);
    
     dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSDictionary *statusDict = nil;
    [manager status:^(NSDictionary * s) {
        statusDict = s;
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    XCTAssertEqual(status.enabled, true); //API_DEPRECATED("No longer supported", macos(10.3, 12.0));
    XCTAssertEqual(statusDict[kSecuritydTribecaServiceStatus], @"Enabled");
    XCTAssertTrue([statusDict[kSecuritydTribecaServiceTimeLeft] isEqual: [[NSNumber alloc] initWithUnsignedLongLong:expectTimeLeft]]);
    XCTAssertEqualObjects(statusDict[kSecuritydTribecaServiceTimeLeftFormatted], @"6d 23:59:40");

}

- (void)testGetStatusXPCDisabled {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];

	manager.tribecaDisabledFn = ^BOOL{
		return true;
	};

    manager.currentTimeFn = ^{
        return [now doubleValue];
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];

    manager.isPasscodeEnabledFn = ^{
        return (BOOL)false;
    };
    
    TribecaServiceStatus status = [manager getStatus];
    XCTAssertEqual(status.enabled, false);
    XCTAssertEqual(
        status.timeLeftInSeconds,
        (uint64_t)0
    );
    XCTAssertEqual(status.usingAEToken, false);
    
     dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSDictionary *statusDict = nil;
    [manager status:^(NSDictionary * s) {
        statusDict = s;
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    XCTAssertEqual(status.enabled, false);
    XCTAssertEqual(statusDict[kSecuritydTribecaServiceStatus], @"Disabled");
    XCTAssertTrue([statusDict[kSecuritydTribecaServiceTimeLeft] isEqual: [[NSNumber alloc] initWithUnsignedLongLong:0]]);
    XCTAssertTrue([statusDict[kSecuritydTribecaServiceTimeLeftFormatted] isEqual:@"0"]);
}

#pragma clang diagnostic pop

- (TestTribecaManager*) createTestManagerWithLastRestoreTime:(NSNumber*) lastRestoreTime
     currentTime:(NSNumber*)now
     minTTL:(NSNumber*)minTTL
     maxTTL:(NSNumber*)maxTTL
{
    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingMinTTL: minTTL,
            kTribecaSettingMaxTTL: maxTTL,
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [now doubleValue];
    };
    
    manager.uptimeFn = ^long{
        return 0;
    };
    
    manager.checkForBypassFn = ^ValidatedAETokenPolicy*{
        return NULL;
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.deadlineForInactiveUseFn = ^NSDate*{
        return [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:0];
    };
    
    manager.deadlineForOfflineFn = ^NSDate*{
        return [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:0];
    };

    manager.isPasscodeEnabledFn = ^{
        return (BOOL)false;
    };
    
    return manager;
}

- (void)testGetStatusV2MinTTL {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestore);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + minTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
    
}

- (void)testGetStatusV2MinTTL2 {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @19;
    NSNumber* minTTL = @20;
    NSNumber* maxTTL = @100000;

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestore);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + minTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
    
}

- (void)testGetStatusV2MinTTLTimePasses {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @19;
    NSNumber* minTTL = @20;
    NSNumber* maxTTL = @100000;

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
        
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestore);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + minTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
    
}

- (void)testGetStatusV2MaxTTL {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
    
    // Test: If Passcode is enabled MaxTTL is used
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)true;
    };
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];

    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestore);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + maxTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
}
    
- (void)testGetStatusV2UptimeCase {

    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
    
    // If uptime is closer than the system time, it priority
    manager.uptimeFn = ^long{
        // system has been up almost as much as TTL
        return minTTL.doubleValue - 10;
    };
    
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestore);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, now.doubleValue + 10);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
}

- (void)testGetStatusV2DisableForBoot {
    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
    
    // Revert the uptime to 0
    manager.uptimeFn = ^long{
        // system has been up almost as much as TTL
        return 0;
    };
    
    manager.tribecaDisabledFn = ^BOOL{
        return true;
    };
    
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    
    XCTAssertEqual(status.state, kSecuritydTribecaStateDisabledForCurrentBoot);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + minTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
        
    // Once again with the lockEnabled
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)true;
    };
    
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    XCTAssertEqual(status.state, kSecuritydTribecaStateDisabledForCurrentBoot);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + maxTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
    
}
    
- (void)testGetStatusV2AEToken {
    
    NSNumber* lastRestoreTime = @10;
    NSNumber* now = @30;
    NSNumber* minTTL = @tribecaSettingDefaultMinTTL;
    NSNumber* maxTTL = @tribecaSettingDefaultMaxTTL;

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
         
     //Test: Populate expired AE token
    manager.checkForBypassFn = ^ValidatedAETokenPolicy* {
        return NULL;
    };
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @10
    }];
    
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestore);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + minTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0);
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // Not reported since its an invalid token
    
    
    //Test: Populate valid AE token, but restore time is bigger....
    NSDate *appleIIReleaseDate = [[NSDate alloc] initWithTimeIntervalSince1970:234774000];
    manager.checkForBypassFn = ^ValidatedAETokenPolicy*{
        NSDate* heatDeathOfTheUniverse = [[NSDate alloc] initWithTimeIntervalSince1970: 2145945600]; // jan 1 2028
        return  [[ValidatedAETokenPolicy alloc]
            initWithIssuedAt:appleIIReleaseDate
            notValidBefore:heatDeathOfTheUniverse
            expires:heatDeathOfTheUniverse
            udid:@""
            buildVersion:@""
            HWModel:@""
            timeLeft:10000]
        ;
    };
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @10
    }];
    
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledAEToken);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + minTTL.doubleValue);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, now.doubleValue + 10000);
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, appleIIReleaseDate.timeIntervalSinceReferenceDate);
    
    
    //Test: Populate valid AE token, but restore time is smaller
    const double farInTheFuture = 10000000000;
    manager.currentTimeFn = ^{
        // far in the future
        return farInTheFuture;
    };
    
    const uint64_t aeTokenDuration = 10000;
    manager.checkForBypassFn = ^ValidatedAETokenPolicy*{
        NSDate* heatDeathOfTheUniverse = [[NSDate alloc] initWithTimeIntervalSince1970: 2145945600]; // jan 1 2028
        return  [[ValidatedAETokenPolicy alloc]
            initWithIssuedAt:appleIIReleaseDate
            notValidBefore:heatDeathOfTheUniverse
            expires:heatDeathOfTheUniverse
            udid:@""
            buildVersion:@""
            HWModel:@""
            timeLeft:aeTokenDuration]
        ;
    };
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @10
    }];
    
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledAEToken);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, farInTheFuture + aeTokenDuration);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, farInTheFuture + 10000);
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, appleIIReleaseDate.timeIntervalSinceReferenceDate);
}

- (void)testGetStatusV2ConditionalExtension {
    NSNumber* lastRestoreTime = @10;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @100000;
    NSNumber* now = [NSNumber numberWithDouble:[minTTL doubleValue] + [lastRestoreTime doubleValue] + 10]; // I am in the conditional zone.
    NSDate* nowDate = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:now.doubleValue];

    TestTribecaManager* manager = [self createTestManagerWithLastRestoreTime:lastRestoreTime currentTime:now minTTL:minTTL maxTTL:maxTTL];
    
    manager.deadlineForOfflineFn = ^NSDate*(){
        return [[NSDate alloc] initWithTimeInterval:10 sinceDate:nowDate];
    };
    
    manager.deadlineForInactiveUseFn = ^NSDate*(){
        return [[NSDate alloc] initWithTimeInterval:20 sinceDate:nowDate];
    };
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)true;
    };
    
    __block TribecaStatus *status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestoreInactiveUseChecking);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + maxTTL.doubleValue);
    XCTAssertEqual(status.inactiveUseExpiry.timeIntervalSinceReferenceDate, now.doubleValue + 20);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
    
     manager.deadlineForOfflineFn = ^NSDate*(){
        return [[NSDate alloc] initWithTimeInterval:30 sinceDate:nowDate];
    };
    
    manager.deadlineForInactiveUseFn = ^NSDate*(){
        return [[NSDate alloc] initWithTimeInterval:5 sinceDate:nowDate];
    };
    
    status = NULL;
    [manager statusV2:^(TribecaStatus * _Nonnull s) {
        status = s;
    }];
    
    XCTAssertEqual(status.state, kSecuritydTribecaStateEnabledLastRestoreInactiveUseChecking);
    XCTAssertEqual(status.scheduledAutoerase.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue + maxTTL.doubleValue);
    XCTAssertEqual(status.inactiveUseExpiry.timeIntervalSinceReferenceDate, now.doubleValue + 30);
    XCTAssertEqual(status.lastRestore.timeIntervalSinceReferenceDate, lastRestoreTime.doubleValue);
    XCTAssertEqual(status.aeTokenExpiry.timeIntervalSinceReferenceDate, 0); // these are null.
    XCTAssertEqual(status.aeTokenIssuedAt.timeIntervalSinceReferenceDate, 0); // these are null.
    
}

- (void)testDeadlineForInactiveUseGeneral{

    // Test Case one  passed
    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSNumber* interactivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:[NSNumber numberWithInteger:[minTTL integerValue] + [lastRestoreTime integerValue]].doubleValue];
    NSNumber* lastInteractivity = @45;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingInteractivityTimeout: interactivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastInteractivity:  lastInteractivity,
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    NSDate* deadline = [manager deadlineForInactiveUse];
    
    // Test Case general case where using the device extends it
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, lastInteractivity.doubleValue + interactivityTimeout.doubleValue);
}

- (void)testDeadlineForInactiveBeforeMinTTL{
    // Test Case one MAXTTL passed
    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSNumber* interactivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:0];
    NSNumber* lastInteractivity = @45;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingInteractivityTimeout: interactivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastInteractivity:  lastInteractivity,
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
  
    // Test Case MINTTL - deadlineForInactiveUse
    NSDate* deadline = [manager deadlineForInactiveUse];
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, 0);
}

- (void)testDeadlineForInactiveAfterMaxTTL{

    // This is when MAXTTL is passed
    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000;
    NSNumber* interactivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:maxTTL.doubleValue + 1];
    NSNumber* lastInteractivity = @45;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingInteractivityTimeout: interactivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastInteractivity:  lastInteractivity
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    // Test Case MAXTTL hit - we are above the maxTTL. This conditional logic SHOULD not extend build any more.
    NSDate* deadline = [manager deadlineForInactiveUse];
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, 0);

}

// In the conditional zone, and should be extended if device is or is NOT offline
- (void)testDeadlineForOfflineGeneral{

    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSNumber* connectivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:[NSNumber numberWithInteger:[minTTL integerValue] + [lastRestoreTime integerValue]].doubleValue];
    NSNumber* lastConnectivity = @45;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingConnectivityTimeout: connectivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastConnectivity: lastConnectivity,
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    manager.isOnlineFn = ^bool{
        return false;
    };
    
    NSDate* deadline = [manager deadlineForOffline];
    
    // Test Case general case where using the device extends it
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, lastConnectivity.doubleValue + connectivityTimeout.doubleValue);
}

- (void)testDeadlineForOfflineNoInformationInState{

    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSNumber* connectivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:[NSNumber numberWithInteger:[minTTL integerValue] + [lastRestoreTime integerValue]].doubleValue];

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingConnectivityTimeout: connectivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    manager.isOnlineFn = ^bool{
        return false;
    };
    
    NSDate* deadline = [manager deadlineForOffline];
    
    // Test Case general case where using the device extends it
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, 0);
}

- (void)testDeadlineForOfflineIsCurrentlyOnline{

    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSNumber* connectivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:[NSNumber numberWithInteger:[minTTL integerValue] + [lastRestoreTime integerValue]].doubleValue];

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingConnectivityTimeout: connectivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    manager.isOnlineFn = ^bool{
        return true;
    };
    
    NSDate* deadline = [manager deadlineForOffline];
    
    // Test Case general case where using the device extends it
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, now.timeIntervalSinceReferenceDate + connectivityTimeout.doubleValue);

}



- (void)testDeadlineForOfflineBeforeMinTTL{
    // Test Case one MAXTTL passed
    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSNumber* connectivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:0];
    NSNumber* lastConnectivity = @45;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingConnectivityTimeout: connectivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastConnectivity:  lastConnectivity,
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    manager.isOnlineFn = ^bool{
        return false;
    };
  
    // Test Case MINTTL - deadlineForInactiveUse does not care about MINTTL only that its under maxTTL, thus this should be NOOP
    NSDate* deadline = [manager deadlineForOffline];
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, 0);
}

- (void)testDeadlineForOfflineAfterMaxTTL{

    // This is when MAXTTL is passed
    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000;
    NSNumber* connectivityTimeout = @30;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:maxTTL.doubleValue + 1];
    NSNumber* lastConnectivity = @45;

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{
            kTribecaSettingConnectivityTimeout: connectivityTimeout
        }
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastConnectivity: lastRestoreTime,
        kTribecaLastInteractivity:  lastConnectivity
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    manager.isOnlineFn = ^bool{
        return false;
    };
    
    // Test Case MAXTTL hit - we are above the maxTTL. This conditional logic SHOULD not extend build any more.
    NSDate* deadline = [manager deadlineForOffline];
    XCTAssertEqual(deadline.timeIntervalSinceReferenceDate, 0);

}

- (void) testShouldCheckForInactiveUseUnderMinTTL {

    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate: [lastRestoreTime doubleValue]];

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{}
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    XCTAssertEqual([manager shouldCheckForInactiveUse], false);

}

- (void) testShouldCheckForInactiveUseOverMinTTL {

    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:[NSNumber numberWithDouble:[minTTL doubleValue] + [lastRestoreTime doubleValue]].doubleValue];

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{}
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    XCTAssertEqual([manager shouldCheckForInactiveUse], true);

}


- (void) testShouldCheckForInactiveUseOverMaxTTL {

    NSNumber* lastRestoreTime = @0;
    NSNumber* minTTL = @100;
    NSNumber* maxTTL = @10000000000;
    NSDate* now = [[NSDate alloc] initWithTimeIntervalSinceReferenceDate:[NSNumber numberWithDouble:[maxTTL doubleValue] + [lastRestoreTime doubleValue]].doubleValue + 6000];

    TestTribecaManager* manager = [[TestTribecaManager alloc]
        initWithSettings: @{}
        mobileSoftwareUpdate: [[MockMSU alloc] init]
        datastore: [[NSDictTribecaPersistentData alloc] init]
    ];
    
    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime
    }];
    
    manager.getMinTTLFn = ^uint64_t{
        return minTTL.unsignedLongLongValue;
    };
    
    manager.getMaxTTLFn = ^uint64_t{
        return maxTTL.unsignedLongLongValue;
    };

    manager.currentTimeFn = ^{
        return [[NSNumber alloc] initWithDouble:now.timeIntervalSinceReferenceDate].doubleValue;
    };
    
    XCTAssertEqual([manager shouldCheckForInactiveUse], false);

}

@end
#pragma pop_macro("TESTING_TRIBECA")
#endif // AUTOERASE
