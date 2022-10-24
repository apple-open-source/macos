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
@property (nonatomic, copy) uint64_t (^currentTimeFn)();
@property (nonatomic, copy) NSString* (^currentBuildFn)();
@property (nonatomic, copy) BOOL (^isPasscodeEnabledFn)();
@end
@implementation TestTribecaManager

- (NSString*) currentHardwareModel{
    return mockGetCurrentHardwareModel(unit);
}

- (uint64_t) currentTime{
    if(self.currentTimeFn == NULL){
        return [super currentTime];
    }
    return self.currentTimeFn();
}

- (NSString *)currentBuild{
    if(self.currentBuildFn == NULL){
        return [super currentBuild];
    }
    return self.currentBuildFn();
}

- (BOOL) isPasscodeEnabled{
    if(self.isPasscodeEnabledFn == NULL){
        return false;
    }
    return self.isPasscodeEnabledFn();
    
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
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 0;
    uint64_t maxTTL = 0;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 0;
    uint64_t connectivityWindow = 0;
    uint64_t currentUptime = 0;
    // min and max are both 0. Should be invalid.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusInvalidData, testResult);
}

- (void)testInvalidDataMinMaxTTL2 {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t minTTL = 0;
    uint64_t maxTTL = 100;
    uint64_t lastEscAuth = 0;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 0;
    uint64_t connectivityWindow = 0;
    uint64_t currentUptime = 0;
    // happy path should be living within the regular bounds
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusInvalidData, testResult);
}

- (void)testObliterateStatusNone {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t lastEscAuth = 0;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 10;
    // min and max are both 0. Should be invalid.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testObliterateStatusMinLife {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 50;
    // should trigger min case since it has no pass code
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testObliterateStatusMinLifeNotTriggered {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 50;
    // should trigger be happy now since it has pass code
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testObliterateStatusMaxLife {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 101;
   // should trigger max case since
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMaxLife, testResult);
}


- (void)testObliterateStatusMaxLifeBypassed {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 101;
   // should trigger max case since
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, true);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testObliterateStatusMinLife2 {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 0;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 101;
   // should trigger min case since it has no pass code
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testShouldNotExpiredByLastRestoreTime {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 50;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should NOT be expired via the last restore time
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMinCase {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 60;
    uint64_t lastCheckin = 60;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 60;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMinCaseEscAuthExtend {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 60;
    uint64_t lastCheckin = 60;
    uint64_t lastEscAuth = 59;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 60;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMinCaseEscAuthExtendExpired {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 60;
    uint64_t lastCheckin = 60;
    uint64_t lastEscAuth = 59;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 130;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMinLife, testResult);
}

- (void)testShouldNOTExpiredByLastRestoreTimeMin {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 60;
    uint64_t lastCheckin = 60;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 60;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should NOT be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldNOTExpiredByLastRestoreTimeMinPreviousEscAuth {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 60;
    uint64_t lastCheckin = 60;
    uint64_t lastEscAuth = 1;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 60;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should NOT be expired via the last restore time hitting min case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMax {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 500;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting max case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMaxLife, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMaxEscExtend {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 499;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 500;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting max case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByLastRestoreTimeMaxEscExtendExpired {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 499;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 650;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // trigger that build should be expired via the last restore time hitting max case
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusMaxLife, testResult);
}

- (void)testShouldNOTExpiredByMinTTL {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 40;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // This test at first glance would appear like it would trigger the lastUnlock and Checkin condition
    // but since its under the minTTL nothing happenss.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldExpiredByOfflineShortCircuit {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 20;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 40;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // Should make it pass the minTTL short circuit condtion and land in inactive and offline
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusInactiveAndOffline, testResult);
}

- (void)testShouldNOTExpiredByOfflineShortCircuitByGracePeriod {

    bool passcodeEnable = true;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 20;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 40;
    uint64_t gracePeriod = 1000;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;
    // should not evalute the condtion now since its in the grace period.
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testShouldNOTExpiredByOfflineShortCircuitByUnlock {

    bool passcodeEnable = true;
    
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 20;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 40;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 0;


    uint64_t lastUnlock = currentTime;
    // even though its still offline, the device has been unlocked recently so should result in None
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
    XCTAssertEqual(tribecaObliterateStatusNone, testResult);
}

- (void)testRestoreTimeAferCurrentTime {

    bool passcodeEnable = false;
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 10000;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 10;
    // restore time is after current time
    int testResult = tribecaShouldObliterate(
        passcodeEnable,
        lastUnlock,
        lastCheckin,
        lastEscAuth,
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
    uint64_t lastUnlock = 0;
    uint64_t lastCheckin = 0;
    uint64_t lastEscAuth = 0;
    uint64_t minTTL = 50;
    uint64_t maxTTL = 100;
    uint64_t currentTime = 0;
    uint64_t gracePeriod = 0;
    uint64_t lastRestore = 30;
    uint64_t inactivityWindow = 10;
    uint64_t connectivityWindow = 10;
    uint64_t currentUptime = 10;
    // there are lab iphone with no clock that the restore time might be in the future but currentTime always reboots to 0
    int testResult = tribecaShouldObliterate(passcodeEnable, lastUnlock, lastCheckin, lastEscAuth, minTTL, maxTTL, currentTime, gracePeriod, lastRestore, inactivityWindow, connectivityWindow, currentUptime, false);
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
        return [now unsignedLongLongValue];
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
        return [now unsignedLongLongValue];
    };

    manager.currentState = [NSMutableDictionary dictionaryWithDictionary:@{
        kTribecaLastRestoreTime: lastRestoreTime,
        kTribecaLastEscAuth: @0
    }];
    
    manager.isPasscodeEnabledFn = ^{
        return (BOOL)true;
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
        return [now unsignedLongLongValue];
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

    NSNumber* lastRestoreTime = @667868266;
    NSNumber* now = @667869562;
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
        return [now unsignedLongLongValue];
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
    XCTAssertEqual(status.timeLeftInSeconds, (uint64_t)2590704);
    XCTAssertEqual(status.usingAEToken, false);
}

- (void)testGetStatusValidCaseWithTimeTravel {

    NSNumber* lastRestoreTime = @40;
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
        return [now unsignedLongLongValue];
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
    // System time in unreliable so we default to assuming the max.
    XCTAssertEqual(status.timeLeftInSeconds, (uint64_t)tribecaSettingDefaultMinTTL);
    XCTAssertEqual(status.usingAEToken, false);
}

+ (void)testInitRestoreTimeWithLastOTA:(double)lastOTA
    lastTetherUpdate:(double)lastTetherUpdate
    currentTime:(uint64_t)now
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
    
    manager.currentTimeFn = ^uint64_t{
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

    XCTAssertFalse([manager shouldRunTribeca], "Should not initilize on tribeca on unsupported hardware");
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
    XCTAssertFalse([manager shouldRunTribeca], "Should not initilize on tribeca on unsupported hardware");
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
    XCTAssertTrue([manager shouldRunTribeca], "Should initilize tribeca");
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
    XCTAssertFalse([manager shouldRunTribeca], "Should not initilize on tribeca on unsupported hardware");
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
    XCTAssertTrue([manager shouldRunTribeca], "Should initilize on tribeca");
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
    
    XCTAssertEqual([manager validateAETokenPolicy:goodTime]>0, true);
    XCTAssertEqual([manager validateAETokenPolicy:badTime], false);

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
    
    XCTAssertEqual([manager validateAETokenPolicy:goodBuild]>0, true);
    XCTAssertEqual([manager validateAETokenPolicy:badBuild], false);
    
    NSDictionary *invalidTime = @{
        kTribecaBypassFieldValidAfter: @"banana",
        kTribecaBypassFieldBypassUntil: @"banana",
        kTribecaBypassFieldBuildVersion: @"banana",
    };
    XCTAssertEqual([manager validateAETokenPolicy:invalidTime], false);

    

}

@end

#endif // AUTOERASE
