
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <CoreCDP/CDPError.h>
#import <CoreCDP/CDPStateController.h>
#import <CloudServices/CloudServices.h>

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"

#import "keychain/escrowrequest/EscrowRequestServerHelpers.h"
#import "keychain/escrowrequest/operations/EscrowRequestInformCloudServicesOperation.h"
#import "keychain/escrowrequest/operations/EscrowRequestPerformEscrowEnrollOperation.h"
#import "keychain/escrowrequest/EscrowRequestServerHelpers.h"

#import "keychain/escrowrequest/tests/MockSynchronousEscrowServer.h"

#include "keychain/ckks/CKKS.h"
#include "keychain/ckks/CKKSLockStateTracker.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/spi.h"
#include "utilities/SecFileLocations.h"

#include "tests/secdmockaks/mockaks.h"

@interface SecEscrowRequestTests : XCTestCase
@property SecEscrowRequest* escrowRequest;
@property EscrowRequestServer* escrowServer;
@property CKKSLockStateTracker* lockStateTracker;

@property id escrowRequestServerClassMock;
@property id escrowRequestInformCloudServicesOperationMock;
@property id escrowRequestPerformEscrowEnrollOperationMock;
@end

@implementation SecEscrowRequestTests

+ (void)setUp {
    securityd_init_local_spi();
    EscrowRequestServerSetEnabled(true);
}

- (void)setUp {
    NSString* testName = [self.name componentsSeparatedByString:@" "][1];
    testName = [testName stringByReplacingOccurrencesOfString:@"]" withString:@""];
    secnotice("secescrowtest", "Beginning test %@", testName);

    [SecMockAKS unlockAllClasses];

    // Make a new fake keychain
    NSString* tmp_dir = [NSString stringWithFormat: @"/tmp/%@.%X", testName, arc4random()];
    [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithFormat: @"%@/Library/Keychains", tmp_dir] withIntermediateDirectories:YES attributes:nil error:NULL];

    SecCKKSDisable();
    SecCKKSTestDisableSOS();

    // Mock out the SBD layer
    self.escrowRequestServerClassMock = OCMClassMock([EscrowRequestServer class]);
    self.escrowRequestInformCloudServicesOperationMock = OCMClassMock([EscrowRequestInformCloudServicesOperation class]);
    self.escrowRequestPerformEscrowEnrollOperationMock = OCMClassMock([EscrowRequestPerformEscrowEnrollOperation class]);

    self.lockStateTracker = [[CKKSLockStateTracker alloc] init];
    self.escrowServer = [[EscrowRequestServer alloc] initWithLockStateTracker:self.lockStateTracker];

    id mockConnection = OCMPartialMock([[NSXPCConnection alloc] init]);
    OCMStub([mockConnection remoteObjectProxyWithErrorHandler:[OCMArg any]]).andCall(self, @selector(escrowServer));
    OCMStub([mockConnection synchronousRemoteObjectProxyWithErrorHandler:[OCMArg any]]).andCall(self, @selector(synchronousEscrowServer));
    self.escrowRequest = [[SecEscrowRequest alloc] initWithConnection:mockConnection];

    SecSetCustomHomeURLString((__bridge CFStringRef) tmp_dir);
    SecKeychainDbReset(NULL);

    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });
}


- (NSData* _Nullable)mockTriggerCloudServicesPasscodeRequest:(NSString*)uuid error:(NSError**)error
{
    return [@"certdata" dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSData* _Nullable)mockTriggerCloudServicesPasscodeRequestAndLockClassA:(NSString*)uuid error:(NSError**)error
{
    [SecMockAKS lockClassA];
    [self.lockStateTracker recheck];
    return [@"certdata" dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSData* _Nullable)mockFailTriggerCloudServicesPasscodeRequest:(NSString*)uuid error:(NSError**)error
{
    if(error) {
        *error = [NSError errorWithDomain:@"NSURLErrorDomain"
                                     code:-1009
                              description:@"The internet connection appears to be offline (mock)"];
    }
    return nil;
}

- (void)mockcdpUploadPrerecord:(SecEscrowPendingRecord*)recordToSend
                    secretType:(CDPDeviceSecretType)secretType
                         reply:(void (^)(bool didUpdate, NSError* _Nullable error))reply
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^ {
        reply(YES, nil);
    });
}

- (void)mockFailcdpUploadPrerecord:(SecEscrowPendingRecord*)recordToSend
                        secretType:(CDPDeviceSecretType)secretType
                             reply:(void (^)(bool didUpdate, NSError* _Nullable error))reply
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^ {
        reply(NO, [NSError errorWithDomain:@"EscrowServiceErrorDomain" code:-6570 description:@"mock CLUBH error"]);
    });
}

- (void)mockcdpFailBadPeerUploadPrerecord:(SecEscrowPendingRecord*)recordToSend
                           secretType:(CDPDeviceSecretType)secretType
                                reply:(void (^)(bool didUpdate, NSError* _Nullable error))reply
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^ {
        reply(NO, [NSError errorWithDomain:kSecureBackupErrorDomain code:kSecureBackupInternalError description:@"SOS peer ID mismatch"]);
    });
}

- (void)mockcdpFailNoSOSPeerUploadPrerecord:(SecEscrowPendingRecord*)recordToSend
                                 secretType:(CDPDeviceSecretType)secretType
                                      reply:(void (^)(bool didUpdate, NSError* _Nullable error))reply
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^ {
        reply(NO, [NSError errorWithDomain:kSecureBackupErrorDomain code:kSecureBackupNotInSyncCircleError description:@"SOS peer not present"]);
    });
}

- (void)mockcdpFailNoCDPPeerUploadPrerecord:(SecEscrowPendingRecord*)recordToSend
                              secretType:(CDPDeviceSecretType)secretType
                                   reply:(void (^)(bool didUpdate, NSError* _Nullable error))reply
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^ {
        reply(NO, [NSError errorWithDomain:CDPStateErrorDomain code:CDPStateErrorNoPeerIdFound description:@"SOS peer not present"]);
    });
}


- (id<EscrowRequestXPCProtocol>)synchronousEscrowServer
{
    return [[MockSynchronousEscrowServer alloc] initWithServer:self.escrowServer];
}

- (void)tearDown {
    [self.escrowServer.controller.stateMachine haltOperation];
}

- (void)allCloudServicesCallsSucceed {
    OCMStub([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));
}

- (void)testStateMachineEntersNothingToDo
{
    [self.escrowServer.controller.stateMachine startOperation];
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.stateConditions[EscrowRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo");
}

- (void)testTriggerUpdate {
    [self allCloudServicesCallsSucceed];

    NSError* error = nil;

    XCTAssertNil([self.escrowRequest fetchRequestWaitingOnPasscode:&error], @"Should be no pending updates");
    XCTAssertNil(error, @"Should be no error fetching pending updates");

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");
}

- (void)testTriggerUpdateWhileLocked {
    [self allCloudServicesCallsSucceed];

    [SecMockAKS lockClassA];

    NSError* error = nil;

    XCTAssertNil([self.escrowRequest fetchRequestWaitingOnPasscode:&error], @"Should be no pending updates");
    XCTAssertNil(error, @"Should be no error fetching pending updates");

    XCTAssertFalse([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should not be able to trigger an update when locked");
    XCTAssertNotNil(error, @"Should be an error triggering an escrow update (while locked)");
    error = nil;

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNil(pendingUUID, "Should be no pending request UUID after a failed trigger");
}

- (void)testMultipleTriggers {
    [self allCloudServicesCallsSucceed];

    NSError* error = nil;

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 1, @"Should be one in-flight status");
}

- (void)testFetchUpdateWhileLocked {
    [self allCloudServicesCallsSucceed];

    NSError* error = nil;

    XCTAssertNil([self.escrowRequest fetchRequestWaitingOnPasscode:&error], @"Should be no pending updates");
    XCTAssertNil(error, @"Should be no error fetching pending updates");

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should churn through");
    error = nil;

    [SecMockAKS lockClassA];

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNotNil(error, @"Should be an error fetching a pending UUID while the device is locked");
    XCTAssertNil(pendingUUID, "Should be no pending request UUID while device is locked");
}

- (void)testReaskCloudServicesWhenLocked {
    // In this test, CloudServices is able to succeed (and cache a certificate), but we can't write down that the operation succeeded.
    NSError* error = nil;

    // No rate limiting for us!
    self.escrowServer.controller.forceIgnoreCloudServicesRateLimiting = true;

    // Pause the state machine for a bit...
    [self.escrowServer.controller.stateMachine startOperation];
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause");
    [self.escrowServer.controller.stateMachine haltOperation];

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    // First, let's ensure that we properly handle the case where the device is locked when we go to ask CloudServices about things

    [SecMockAKS lockClassA];
    [self.lockStateTracker recheck];
    [self.escrowServer.controller.stateMachine startOperation];
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.stateConditions[EscrowRequestStateWaitForUnlock] wait:10*NSEC_PER_SEC], "State machine enters waitforunlock");

    // Now, we should call CloudServices, but the device locks between CS success and us writing it down
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequestAndLockClassA:error:));
    [SecMockAKS unlockAllClasses];
    [self.lockStateTracker recheck];

    OCMVerifyAllWithDelay(self.escrowRequestInformCloudServicesOperationMock, 10);
    // and we should be back in wait for unlock:
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.stateConditions[EscrowRequestStateWaitForUnlock] wait:10*NSEC_PER_SEC], "State machine enters waitforunlock");

    // Then, unlock one last time, and let everything succeed
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));
    [SecMockAKS unlockAllClasses];
    [self.lockStateTracker recheck];

    OCMVerifyAllWithDelay(self.escrowRequestInformCloudServicesOperationMock, 10);
}

- (void)testRateLimitSBDTriggerWhenFailing {

    // Trigger an escrow update, which should call CloudServices and fail
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockFailTriggerCloudServicesPasscodeRequest:error:));

    NSError* error = nil;
    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    // And, the state machine should pause
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    // But, there should be no pending requests (as we need to retry CloudServices)
    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNil(pendingUUID, "Should be no pending request UUID after a failed trigger");


    // But, the controller will recover at some point (for now, disable rate limiting)
    [self allCloudServicesCallsSucceed];
    self.escrowServer.controller.forceIgnoreCloudServicesRateLimiting = true;
    [self.escrowServer.controller.stateMachine pokeStateMachine];

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after the state machine retries certificate caching");
}

- (void)testRateLimitSBDUploadWhenFailing {

    // Trigger an escrow update, which should call CloudServices
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    NSError* error = nil;
    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAllWithDelay(self.escrowRequestInformCloudServicesOperationMock, 5);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");


    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockFailcdpUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];


    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");

    // Wait for the upload to fail...
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    //////////////////////////
    // Trigger another update, to check coalescing
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test2" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");
    OCMVerifyAllWithDelay(self.escrowRequestInformCloudServicesOperationMock, 5);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    NSString* pendingUUIDTheSecond = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNotNil(pendingUUIDTheSecond, @"Should have a request waiting on passcode");
    XCTAssertEqualObjects(pendingUUID, pendingUUIDTheSecond, @"In-flight request should have been restarted");
    //////////////

    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockFailcdpUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUIDTheSecond
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 1, @"Should be one in-flight status");

    // But if escrow plays nice again, kick state machine, and make sure we don't touch escrow proxy
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock stub] andCall:@selector(mockcdpUploadPrerecord:secretType:reply:)
                                                                onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];


    [self.escrowServer.controller.stateMachine startOperation];

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    /* item should still be in flight */
    statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 1, @"Should be one in-flight status");
}


- (void)testPrerecordCaching {
    NSError* error = nil;

    // Trigger an escrow update, which should call CloudServices
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    // Now, there should be a pending request

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    // Caching a prerecord will attempt an upload via the state machine. But, since we're testing prerecord fetching, we don't want that to happen.
    // So, let the upload fail.
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockFailcdpUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];


    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");

    // Wait for the upload to fail...
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    NSString* nowPendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNil(nowPendingUUID, "Should be no pending request UUID after a prerecord cache");

    NSData* d2 = [self.escrowRequest fetchPrerecord:pendingUUID
                                              error:&error];
    XCTAssertNotNil(d2, @"Should be able to retrieve a cached prerecord");
    XCTAssertNil(error, @"Should be no error fetching a cached prerecord");

    XCTAssertEqualObjects(d,d2, @"Cached prerecord should be equal to original");
}

- (void)testPrerecordCachingWhileLocked {
    NSError* error = nil;

    // Trigger an escrow update, which should call CloudServices
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");

    // Now, there should be a pending request
    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    [SecMockAKS lockClassA];
    // Caching a prerecord will fail, and should not invoke CDP.
    // So, let the upload fail.
    [[[self.escrowRequestPerformEscrowEnrollOperationMock reject] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertFalse([self.escrowRequest cachePrerecord:pendingUUID
                                  serializedPrerecord:d
                                                error:&error], @"Should be not able to cache a prerecord while the device is locked");
    XCTAssertNotNil(error, @"Should be an error caching a prerecord while the device is locked");

    // Ensure we don't invoke CDP
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should rest");
    OCMVerifyAll(self.escrowRequestPerformEscrowEnrollOperationMock);

    // And now, when we unlock, there's still a pending request
    error = nil;
    [SecMockAKS unlockAllClasses];
    pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");
}

- (void)testEscrowUploadViaStateMachine {
    // This test should call CloudServices, which will succeed. It should then tell CoreCDP to upload the record.
    [self allCloudServicesCallsSucceed];
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockcdpUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    [self.escrowServer.controller.stateMachine startOperation];
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.stateConditions[EscrowRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo when started");

    NSError* error = nil;
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));
    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");
    OCMVerifyAll(self.escrowRequestServerClassMock);


    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");

    // Don't call the "do it" RPC, but it should happen anyway
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);
}

- (void)testEscrowUploadViaRPCAfterFailure {
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockFailcdpUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
        cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    NSError* error = nil;

    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");


    // Now, the state machine should try and fail the upload
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    // And pause for a while
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    // But if escrow plays nice again, a 'try again' RPC should succeed
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock stub] andCall:@selector(mockcdpUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    uint64_t records = [self.escrowRequest storePrerecordsInEscrow:&error];
    XCTAssertNil(error, @"Should be no error storing prerecords in escrow");
    XCTAssertEqual(records, 1, @"Should have stored one record in escrow");

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 1, @"Should be one in-flight status");
    for(id key in statuses.keyEnumerator) {
        XCTAssertEqualObjects(statuses[key], @"complete", "Record should be in 'complete' state");
    }
}

- (void)testEscrowUploadViaStateMachineAfterFailureDueToLockState {
    // This test should call CloudServices, which will succeed. It should then tell CDP to upload the record, which will fail due to lock state.
    [self allCloudServicesCallsSucceed];
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockcdpUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    [self.escrowServer.controller.stateMachine startOperation];
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.stateConditions[EscrowRequestStateNothingToDo] wait:10*NSEC_PER_SEC], "State machine enters NothingToDo when started");

    NSError* error = nil;
    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));
    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");
    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    // We need to lock directly after the prerecord is cached, but before the CDP attempt is attempted
    [self.escrowServer.controller.stateMachine haltOperation];

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");

    [SecMockAKS lockClassA];
    [self.escrowServer.controller.stateMachine startOperation];

    // The state machine should notice the unlock, and try again
    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.stateConditions[EscrowRequestStateWaitForUnlock] wait:10*NSEC_PER_SEC], "State machine enters waitforunlock");

    // The upload should be tried after an unlock
    [SecMockAKS unlockAllClasses];
    [self.lockStateTracker recheck];

    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);
}

- (void)testEscrowUploadBadPeerFailure {
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockcdpFailBadPeerUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    NSError* error = nil;

    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");


    // Now, the state machine should try and fail the upload
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 0, @"Should be zero in-flight status");
}

- (void)testEscrowUploadNoSOSPeerFailure {
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockcdpFailNoSOSPeerUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    NSError* error = nil;

    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");


    // Now, the state machine should try and fail the upload
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 0, @"Should be zero in-flight status");
}

- (void)testEscrowUploadNoCDPSOSPeerFailure {
    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockcdpFailNoCDPPeerUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
     cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    NSError* error = nil;

    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");


    // Now, the state machine should try and fail the upload
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 0, @"Should be zero in-flight status");
}

- (void)testPendingPreRecordsCheck {
    [self allCloudServicesCallsSucceed];

    NSError* error = nil;

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 1, @"Should be one in-flight status");

    BOOL result = [self.escrowRequest pendingEscrowUpload:&error];
    XCTAssertNil(error, @"Should be no error checking for pending uploads");
    XCTAssertEqual(result, true, @"pendingEscrowUpload should return true");

}

- (void)testClearedPreRecordsCheck {
    [self allCloudServicesCallsSucceed];

    NSError* error = nil;

    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockcdpFailNoCDPPeerUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
    cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");


    // Now, the state machine should try and fail the upload
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowRequest fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 0, @"Should be zero in-flight status");

    BOOL result = [self.escrowRequest pendingEscrowUpload:&error];
    XCTAssertNil(error, @"Should be no error checking for pending uploads");
    XCTAssertEqual(result, false, @"pendingEscrowUpload should return true");
}

- (void)testServerPendingPreRecordsCheck {
    [self allCloudServicesCallsSucceed];

    NSError* error = nil;

    XCTAssertTrue([self.escrowServer triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertTrue([self.escrowServer triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    XCTAssertTrue([self.escrowServer triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowServer fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 1, @"Should be one in-flight status");

    BOOL result = [self.escrowServer pendingEscrowUpload:&error];
    XCTAssertNil(error, @"Should be no error checking for pending uploads");
    XCTAssertEqual(result, true, @"pendingEscrowUpload should return true");

}

- (void)testServerClearedPreRecordsCheck {
    [self allCloudServicesCallsSucceed];

    NSError* error = nil;

    [[[[self.escrowRequestPerformEscrowEnrollOperationMock expect] andCall:@selector(mockcdpFailNoCDPPeerUploadPrerecord:secretType:reply:)
                                                                  onObject:self] ignoringNonObjectArgs]
    cdpUploadPrerecord:[OCMArg any] secretType:0 reply:[OCMArg any]];

    OCMExpect([self.escrowRequestInformCloudServicesOperationMock triggerCloudServicesPasscodeRequest:[OCMArg any] error:[OCMArg anyObjectRef]]).andCall(self, @selector(mockTriggerCloudServicesPasscodeRequest:error:));

    XCTAssertTrue([self.escrowRequest triggerEscrowUpdate:@"test" error:&error], @"Should be able to trigger an update");
    XCTAssertNil(error, @"Should be no error triggering an escrow update");

    OCMVerifyAll(self.escrowRequestServerClassMock);

    XCTAssertEqual(0, [self.escrowServer.controller.stateMachine.paused wait:10*NSEC_PER_SEC], @"State machine should pause within some time");

    NSString* pendingUUID = [self.escrowRequest fetchRequestWaitingOnPasscode:&error];
    XCTAssertNil(error, @"Should be no error fetching a pending UUID");
    XCTAssertNotNil(pendingUUID, "Should be a pending request UUID after a trigger");

    NSData* d = [[NSData alloc]initWithBase64EncodedString:@"YXNkZgo=" options:0];
    XCTAssertTrue([self.escrowRequest cachePrerecord:pendingUUID
                                 serializedPrerecord:d
                                               error:&error], @"Should be able to cache a prerecord");
    XCTAssertNil(error, @"Should be no error caching a prerecord");


    // Now, the state machine should try and fail the upload
    OCMVerifyAllWithDelay(self.escrowRequestPerformEscrowEnrollOperationMock, 10);

    NSDictionary<NSString*, NSString*>* statuses = [self.escrowServer fetchStatuses:&error];
    XCTAssertNotNil(statuses, @"Should be able to fetch statuses");
    XCTAssertNil(error, @"Should be no error fetching statuses");

    XCTAssertEqual(statuses.count, 0, @"Should be zero in-flight status");

    BOOL result = [self.escrowServer pendingEscrowUpload:&error];
    XCTAssertNil(error, @"Should be no error checking for pending uploads");
    XCTAssertEqual(result, false, @"pendingEscrowUpload should return true");
}

@end
