
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestServer.h"
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestServerHelpers.h"
#import "keychain/ot/OTConstants.h"
#include "keychain/ckks/CKKS.h"
#include "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/tests/CKKSMockLockStateProvider.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/spi.h"
#include "utilities/SecFileLocations.h"
#import "keychain/ot/tests/TestsObjcTranslation.h"
#include "tests/secdmockaks/mockaks.h"

@interface SecKeychainItemUpgradeRequestTests : XCTestCase
@property KeychainItemUpgradeRequestServer* server;
@property CKKSMockLockStateProvider* lockStateProvider;
@property CKKSLockStateTracker* lockStateTracker;
@property NSOperationQueue* operationQueue;

@end

@implementation SecKeychainItemUpgradeRequestTests

+ (void)setUp {
    securityd_init_local_spi();
    KeychainItemUpgradeRequestServerSetEnabled(true);
}

- (void)setUp {
    NSString* testName = [self.name componentsSeparatedByString:@" "][1];
    testName = [testName stringByReplacingOccurrencesOfString:@"]" withString:@""];
    secnotice("seckeychainitemupgrade", "Beginning test %@", testName);

    [SecMockAKS unlockAllClasses];

    [super setUp];

    // Make a new fake keychain
    NSString* tmp_dir = [NSString stringWithFormat: @"/tmp/%@.%X", testName, arc4random()];
    [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithFormat: @"%@/Library/Keychains", tmp_dir] withIntermediateDirectories:YES attributes:nil error:NULL];

    SecCKKSTestsEnable();

    self.lockStateProvider = [[CKKSMockLockStateProvider alloc] initWithCurrentLockStatus:NO];
    self.lockStateTracker = [[CKKSLockStateTracker alloc] initWithProvider:self.lockStateProvider];
    self.operationQueue = [[NSOperationQueue alloc] init];

    self.server = [[KeychainItemUpgradeRequestServer alloc] initWithLockStateTracker:self.lockStateTracker];

    SecSetCustomHomeURLString((__bridge CFStringRef) tmp_dir);
    SecKeychainDbReset(NULL);

    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });
}

- (void)tearDown {
    [self.server.controller.persistentReferenceUpgrader cancel];
    [self.server.controller.stateMachine haltOperation];
    
    clearRowIDAndErrorDictionary();
    clearTestError();
    clearLastRowIDHandledForTests();
    
    [super tearDown];

    self.server = nil;
    self.lockStateTracker = nil;
    self.lockStateProvider = nil;
    self.operationQueue = nil;
    
}

- (void)testStateMachineEntersNothingToDo
{
    [self.server.controller.stateMachine startOperation];
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
}

- (void)testTriggerUpdate {

    [self.server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable error) {
        XCTAssertNil(error, @"Should be no error triggering a keychain item update");
    }];

    XCTAssertEqual(0, [self.server.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
    
    XCTAssertNil(self.server.controller.persistentReferenceUpgrader.nextFireTime, @"nextFireTime should be nil");
}

- (void)testTriggerUpdateWhileLocked {

    self.lockStateProvider.aksCurrentlyLocked = false;
    [self.lockStateTracker recheck];

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);

    // perform a bunch of SecItemAdds without the feature flag enabled so that the keychain has items with a NULL persist ref
    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"testKeychainItemUpgrade",
                                (id)kSecAttrService : @"TestUUIDPersistentRefService",
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES),
                                (id)kSecReturnPersistentRef : @(YES)
    };

    CFTypeRef result = NULL;

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    XCTAssertTrue(isDictionary(result), "result should be a dictionary");
    CFDataRef oldStylePersistRef = CFDictionaryGetValue(result, kSecValuePersistentRef);
    XCTAssertNotNil((__bridge id)oldStylePersistRef, @"oldStylePersistRef should not be nil");
    XCTAssertTrue(CFDataGetLength(oldStylePersistRef) == 12, "oldStylePersistRef should be 12 bytes long");
    CFReleaseNull(result);

    NSString* descriptionString = [NSString stringWithFormat:@"Fake error %d for testing", (int)errSecInteractionNotAllowed];
    CFErrorRef error = (__bridge CFErrorRef)[NSError errorWithDomain:(id)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:@{NSLocalizedDescriptionKey : descriptionString}];
    setExpectedErrorForTests(error);
    
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    [SecMockAKS lockClassA];
    self.lockStateProvider.aksCurrentlyLocked = true;
    [self.lockStateTracker recheck];

    [self.server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable triggerError) {
        XCTAssertNil(triggerError, @"Should be no error triggering a keychain item update");
    }];
        
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateWaitForUnlock] wait:10*NSEC_PER_SEC], "State machine enters WaitForUnlock");

    [SecMockAKS unlockAllClasses];
    self.lockStateProvider.aksCurrentlyLocked = false;
    [self.lockStateTracker recheck];
    clearTestError();

    [self.server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable triggerError) {
        XCTAssertNil(triggerError, @"Should be no error triggering a keychain item update");
    }];


    XCTAssertEqual(0, [self.server.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
    XCTAssertTrue([TestsObjectiveC checkAllPersistentRefBeenUpgraded], @"all items should be upgraded");
    XCTAssertNil(self.server.controller.persistentReferenceUpgrader.nextFireTime, @"nextFireTime should be nil");
}

- (void)testTriggerUpdate200Items {

    [TestsObjectiveC addNRandomKeychainItemsWithoutUpgradedPersistentRefs:200];
    
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    XCTAssertTrue([TestsObjectiveC expectXNumberOfItemsUpgraded:0], @"expect 0 items to be upgraded");

    [self.server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable triggerError) {
        XCTAssertNil(triggerError, @"Should be no error triggering a keychain item update");
    }];
    // first state
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateUpgradePersistentRef] wait:10*NSEC_PER_SEC], "State machine enters persistent ref");
        
    XCTAssertEqual(0, [self.server.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
    XCTAssertTrue([TestsObjectiveC checkAllPersistentRefBeenUpgraded], @"should all be upgraded");
}

- (void)triggerUpdate200ItemsErrorOnRandomRowIDN:(int)rowID {
    [TestsObjectiveC addNRandomKeychainItemsWithoutUpgradedPersistentRefs: 200];

    // set up an error so we can test upgrade and encounters an error at a particular row
    CFMutableDictionaryRef rowIDToErrorDictionary = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFErrorRef error = (__bridge CFErrorRef)[NSError errorWithDomain:(id)kSecErrorDomain code:kAKSReturnNotReady userInfo:@{NSLocalizedDescriptionKey : @"Fake error kAKSReturnNotReady keychain item for testing"}];
    CFNumberRef rowIDNumberRef = CFBridgingRetain([[NSNumber alloc]initWithInt:rowID]);
    CFDictionaryAddValue(rowIDToErrorDictionary, rowIDNumberRef, error);
    
    setRowIDToErrorDictionary(rowIDToErrorDictionary);
    
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callback expectation"];
    CKKSResultOperation *callback = [CKKSResultOperation named:@"callback" withBlock:^{
        [callbackExpectation fulfill];
    }];
    [callback timeout:(10 * NSEC_PER_SEC)];
    [callback addDependency:self.server.controller.persistentReferenceUpgrader.operationDependency];
    [self.operationQueue addOperation:callback];

    [self.server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable triggerError) {
        XCTAssertNil(triggerError, @"Should be no error triggering a keychain item update");
    }];

    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateWaitForTrigger] wait:10*NSEC_PER_SEC], "State machine enters wait for trigger");

    [self waitForExpectations:@[callbackExpectation] timeout:10];
    
    //trigger occurred
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateUpgradePersistentRef] wait:10*NSEC_PER_SEC], "State machine enters upgrade persistent ref");

    // should see 199 items upgraded
    XCTAssertTrue([TestsObjectiveC expectXNumberOfItemsUpgraded:(rowID-1)], @"%d items should be upgraded", rowID-1);
    XCTAssertNotNil(self.server.controller.persistentReferenceUpgrader.nextFireTime, @"next fire time should NOT be nil");

    // set up the next trigger expectation
    callbackExpectation = [self expectationWithDescription:@"callback expectation"];
    callback = [CKKSResultOperation named:@"callback" withBlock:^{
        [callbackExpectation fulfill];
    }];
    [callback timeout:(10 * NSEC_PER_SEC)];
    [callback addDependency:self.server.controller.persistentReferenceUpgrader.operationDependency];
    [self.operationQueue addOperation:callback];
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateWaitForTrigger] wait:10*NSEC_PER_SEC], "State machine enters wait for trigger");

    [self waitForExpectations:@[callbackExpectation] timeout:10];

    // now the error is gone! the next trigger should fire and upgrade all the items successfully
    clearRowIDAndErrorDictionary();

    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateUpgradePersistentRef] wait:10*NSEC_PER_SEC], "State machine enters upgrade persistent ref");

    XCTAssertEqual(0, [self.server.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
    
    XCTAssertTrue([TestsObjectiveC checkAllPersistentRefBeenUpgraded], @"all items should be upgrade");
    XCTAssertNil(self.server.controller.persistentReferenceUpgrader.nextFireTime, @"next fire time should be nil");
    
    NSNumber *expectedRowID = [[NSNumber alloc] initWithInt:200];
    XCTAssertEqualObjects((__bridge NSNumber*)lastRowIDHandledForTests(), expectedRowID, @"should be 200");
}

- (void)testErrorRowID50
{
    [self triggerUpdate200ItemsErrorOnRandomRowIDN:50];
}

- (void)testErrorRowID100
{
    [self triggerUpdate200ItemsErrorOnRandomRowIDN:100];
}

- (void)testErrorRowID150
{
    [self triggerUpdate200ItemsErrorOnRandomRowIDN:150];
}

- (void)testErrorRowID200
{
    [self triggerUpdate200ItemsErrorOnRandomRowIDN:200];
}

- (void)triggerUpdate200ItemsErrorAuthNeededOnRandomRowIDN:(int)rowID {
    [TestsObjectiveC addNRandomKeychainItemsWithoutUpgradedPersistentRefs: 200];

    // set up an error so we can test upgrade and encounters an error at a particular row
    CFMutableDictionaryRef rowIDToErrorDictionary = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFErrorRef error = (__bridge CFErrorRef)[NSError errorWithDomain:(id)kSecErrorDomain code:errSecAuthNeeded userInfo:@{NSLocalizedDescriptionKey : @"Fake error errSecAuthNeeded keychain item for testing"}];
    CFNumberRef rowIDNumberRef = CFBridgingRetain([[NSNumber alloc]initWithInt:rowID]);
    CFDictionaryAddValue(rowIDToErrorDictionary, rowIDNumberRef, error);
    
    setRowIDToErrorDictionary(rowIDToErrorDictionary);
    
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    [self.server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable triggerError) {
        XCTAssertNil(triggerError, @"Should be no error triggering a keychain item update");
    }];

    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateUpgradePersistentRef] wait:10*NSEC_PER_SEC], "State machine enters upgrade persistent ref");

    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");

    XCTAssertNil(self.server.controller.persistentReferenceUpgrader.nextFireTime, @"next fire time should be nil");
   
    XCTAssertTrue([TestsObjectiveC expectXNumberOfItemsUpgraded:199], @"expect 199 items to be upgraded");

    NSNumber *expectedRowID = [[NSNumber alloc] initWithInt:200];
    XCTAssertEqualObjects((__bridge NSNumber*)lastRowIDHandledForTests(), expectedRowID, @"should be 200");
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
    XCTAssertNil(self.server.controller.persistentReferenceUpgrader.nextFireTime, @"next fire time should be nil");
    
    clearRowIDAndErrorDictionary();
    clearLastRowIDHandledForTests();
    clearTestError();
    
    [self.server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable triggerError) {
        XCTAssertNil(triggerError, @"Should be no error triggering a keychain item update");
    }];
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateUpgradePersistentRef] wait:10*NSEC_PER_SEC], "State machine enters upgrade persistent ref");

    XCTAssertEqual(0, [self.server.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");
    
    XCTAssertEqual(0, [self.server.controller.stateMachine.stateConditions[KeychainItemUpgradeRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
    XCTAssertTrue([TestsObjectiveC checkAllPersistentRefBeenUpgraded], @"all items should be upgraded");
    XCTAssertNil(self.server.controller.persistentReferenceUpgrader.nextFireTime, @"next fire time should be nil");
    expectedRowID = [[NSNumber alloc] initWithLongLong:rowID];
    XCTAssertEqualObjects((__bridge NSNumber*)lastRowIDHandledForTests(), expectedRowID, @"should be %d", rowID);
}
- (void)testErrorAuthNeededRowID50
{
    [self triggerUpdate200ItemsErrorAuthNeededOnRandomRowIDN:50];
}

- (void)testErrorAuthNeededRowID100
{
    [self triggerUpdate200ItemsErrorAuthNeededOnRandomRowIDN:100];
}

- (void)testErrorAuthNeededRowID150
{
    [self triggerUpdate200ItemsErrorAuthNeededOnRandomRowIDN:150];
}

- (void)testErrorAuthNeededRowID200
{
    [self triggerUpdate200ItemsErrorAuthNeededOnRandomRowIDN:200];
}

@end
