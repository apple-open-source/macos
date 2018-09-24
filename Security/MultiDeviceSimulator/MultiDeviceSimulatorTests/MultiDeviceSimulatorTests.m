//
//  MultiDeviceSimulatorTests.m
//  MultiDeviceSimulatorTests
//
//

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/Security.h>

#import "DeviceSimulatorProtocol.h"
#import "MultiDeviceNetworking.h"
#import <objc/runtime.h>

@interface MDDevice : NSObject<DeviceSimulatorProtocol>
@property NSXPCConnection *connection;
@property NSString *name;
- (instancetype)initWithConnection:(NSXPCConnection *)connection;
@end

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wprotocol"
@implementation MDDevice

- (instancetype)initWithConnection:(NSXPCConnection *)connection
{
    self = [super init];
    if (self) {
        self.connection = connection;
    }
    return self;
}

/* Oh, ObjC, you are my friend */
- (void)forwardInvocation:(NSInvocation *)invocation
{
    struct objc_method_description desc = protocol_getMethodDescription(@protocol(DeviceSimulatorProtocol), [invocation selector], true, true);
    if (desc.name == NULL) {
        [super forwardInvocation:invocation];
    } else {
        __block bool dooooooEeeeetExclamationPoint = true;
        NSLog(@"forwarding to [%@]: %s", self.name, sel_getName(desc.name));
        id object = [self.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            NSLog(@"peer failed with: %@", error);
            dooooooEeeeetExclamationPoint = false;
            //abort();
        }];
        if(dooooooEeeeetExclamationPoint) {
            [invocation invokeWithTarget:object];
        }
    }
}
@end
#pragma clang diagnostic pop


@interface MultiDeviceSimulatorTests : XCTestCase <NSXPCListenerDelegate>
@property NSMutableDictionary<NSString *,MDDevice *> *connections;
@property MultiDeviceNetworking *network;
@property MDDevice *masterDevice;
@property NSMutableArray <MDDevice *> *minionDevices;
@end

static NSString *testInstanceUUID;

@implementation MultiDeviceSimulatorTests

+ (void)setUp
{
    testInstanceUUID = [[NSUUID UUID] UUIDString];
}

- (void)setUp
{
    signal(SIGPIPE, SIG_IGN);
    self.connections = [NSMutableDictionary dictionary];
    self.network = [[MultiDeviceNetworking alloc] init];

    self.minionDevices = [NSMutableArray array];
}

- (void)tearDown
{
    __block uint64_t totalUserUsec = 0, totalSysUsec = 0, totalDiskUsage = 0;
    NSMutableDictionary *result = [NSMutableDictionary dictionary];

    for (NSString *name in self.connections) {
        MDDevice *device = self.connections[name];
        NSLog(@"device: %@", name);
        [device diagnosticsCPUUsage:^(bool success, uint64_t user_usec, uint64_t sys_usec, NSError *error) {
            NSLog(@"cpu %@: %d: u:%llu s:%llu", device.name, success, (unsigned long long)user_usec, (unsigned long long)sys_usec);
            totalUserUsec += user_usec;
            totalSysUsec += sys_usec;
            result[[NSString stringWithFormat:@"cpu-%@", name]] = @{ @"user_usec" : @(user_usec), @"system_usec" : @(sys_usec)};
        }];
        [device diagnosticsDiskUsage:^(bool success, uint64_t usage, NSError *error) {
            NSLog(@"disk %@: %d: %llu", device.name, success, (unsigned long long)usage);
            totalDiskUsage += usage;
            result[[NSString stringWithFormat:@"disk-%@", name]] = @{ @"usage" : @(usage) };
        }];
    }

    for(MDDevice *dev in self.minionDevices) {
        [dev sosLeaveCircle:^(bool success, NSError *error) {
            ;
        }];
    }
    [self.masterDevice sosLeaveCircle:^(bool success, NSError *error) {
        ;
    }];

    self.minionDevices = NULL;
    self.masterDevice = NULL;

    result[@"cpu-total"] = @{ @"user_usec" : @(totalUserUsec), @"system_usec" : @(totalSysUsec)};
    result[@"disk-total"] = @{ @"disk" : @(totalDiskUsage) };

    NSLog(@"Total cpu: u:%llu s:%llu", (unsigned long long)totalUserUsec, (unsigned long long)totalSysUsec);
    NSLog(@"Total disk: %llu", (unsigned long long)totalDiskUsage);

    /* XXX check for leaks in all devices */
    for (NSString *name in self.connections) {
        MDDevice *device = self.connections[name];
        [device.connection invalidate];
    }
    self.connections = NULL;
    [self.network dumpKVSState];
    [self.network dumpCounters];
    [self.network disconnectAll];
    [self.network clearTestExpectations];
    self.network = NULL;

    NSData * jsonData = [NSJSONSerialization dataWithJSONObject:result options:0 error:NULL];

    [jsonData writeToFile:[NSString stringWithFormat:@"/tmp/test-result-%@", [self name]] atomically:NO];

}

- (void)setupMasterDevice
{
    self.masterDevice = [self device:@"ipad" model:@"iPad" version:@"15E143a"];

    [self.masterDevice setupSOSCircle:@"user" password:@"foo" complete:^void(bool success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
    }];

    [self.masterDevice sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
    }];
}

//MARK: - Device logic

- (MDDevice *)device:(NSString *)name model:(NSString *)model version:(NSString *)version {
    MDDevice *device = self.connections[name];
    if (device != NULL) {
        return NULL;
    }

    NSXPCConnection *conn = [[NSXPCConnection alloc] initWithServiceName:@"com.apple.Security.DeviceSimulator"];
    conn.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(DeviceSimulatorProtocol)];
    [conn _setUUID:[NSUUID UUID]]; // select a random instance
    [conn resume];

    device = [[MDDevice alloc] initWithConnection:conn];
    device.name = name;

    self.connections[name] = device;

    [device setDevice:name
              version:version
                model:model
         testInstance:testInstanceUUID
              network:[self.network endpoint]
             complete:^(BOOL success) {
        if (!success) {
            abort();
        }
    }];
    return device;
}

- (bool)addDeviceToCircle: (MDDevice *) dev {
    __block bool added = false;
    __block NSString *devPeerID = NULL;
    __block NSError *localErr = nil;

    [dev setupSOSCircle:@"user" password:@"foo" complete:^void(bool success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
        added = success;
    }];

    if(added) {
        [dev sosRequestToJoin:^(bool success, NSString *peerID, NSError *error) {
            XCTAssert(success, "Expect success: %@", error);
            XCTAssertNotEqual(peerID, NULL, "Expected to find peerID for peer2");
            devPeerID = peerID;
            added &= success;
        }];
    }

    if(added) {
        __block bool done = false;
        for(int tries=0; tries < 5; tries++) {
            sleep(2);

            [self.masterDevice sosApprovePeer:devPeerID complete:^(BOOL success, NSError *error) {
                localErr = [error copy];
                if(success) {
                    localErr = nil;
                    done = true;
                }
            }];
            if(done) break;
        }
        added &= done;
    }
    XCTAssert(added, "Expect success (for approve of %@): %@", devPeerID, localErr);
    return added;
}

- (void)addKeychainItems:(unsigned long)items toDevice:(MDDevice *)device
{
    NSDictionary *addItem = @{
                              (__bridge id)kSecClass :(__bridge id)kSecClassInternetPassword,
                              (__bridge id)kSecValueData : [@"foo" dataUsingEncoding:NSUTF8StringEncoding],
                              (__bridge id)kSecAttrAccessGroup: @"com.apple.cfnetwork",
                              (__bridge id)kSecAttrSyncViewHint: @"PCS-MasterKey",
                              (__bridge id)kSecAttrDescription: @"delete me if found",
                              (__bridge id)kSecAttrServer: @"server",
                              (__bridge id)kSecAttrAccount: @"account",
                              (__bridge id)kSecAttrSynchronizable: @YES,
                              (__bridge id)kSecAttrPath: @"/path",
                              (__bridge id)kSecAttrIsInvisible: @YES,
                              };

    [device secItemAdd:addItem complete:^void(OSStatus status, NSDictionary *result) {
        NSLog(@"Result string was dev1: %d %@", (int)status, result);
        XCTAssertEqual(status, 0, "Expect success");
    }];

}

- (void)runSigninWithAdditionalDevices:(unsigned)additionalDeviceCount keychainItems:(unsigned long)items {

    for (unsigned n = 0; n < additionalDeviceCount; n++) {
        MDDevice *dev = [self device:[NSString stringWithFormat:@"mac-%u", n] model:@"Mac Pro" version:@"17E121"];
        if(dev) {
            [self addDeviceToCircle: dev];
            [self.minionDevices addObject:dev];
        }
    }

    if (items) {
        [self addKeychainItems:items toDevice:self.masterDevice];
    }
}


- (void)testPref3Devices {
    [self setupMasterDevice];

    [self measureBlock:^{
        [self runSigninWithAdditionalDevices:2 keychainItems:0];
    }];
}

#if 0 /* disabled because of 10min time limit in bats (for now) */

- (void)testPref3Devices1 {
    [self setupMasterDevice];
    [self runSigninWithAdditionalDevices:2 keychainItems:1];
    sleep(60);
}

- (void)testPref3Devices10 {
    [self setupMasterDevice];
    [self runSigninWithAdditionalDevices:2 keychainItems:10];
    sleep(60);
}

- (void)testPref3Devices100 {
    [self setupMasterDevice];
    [self runSigninWithAdditionalDevices:2 keychainItems:100];
    sleep(60);
}

- (void)testPref3Devices1000 {
    [self setupMasterDevice];
    [self runSigninWithAdditionalDevices:2 keychainItems:1000];
    sleep(60);
    sleep(1);
}

- (void)testPref6Devices {
    [self setupMasterDevice];

    [self measureBlock:^{
        [self runSigninWithAdditionalDevices:5 keychainItems:0];
    }];
}

- (void) testDevices6Retired {
    [self setupMasterDevice];

    [self measureBlock:^{
        [self runSigninWithAdditionalDevices:5 keychainItems:0];
    }];

}
#endif

- (void)test2Device {

    NSLog(@"create devices");
    MDDevice *dev1 = [self device:@"ipad" model:@"iPad" version:@"15E143a"];
    MDDevice *dev2 = [self device:@"mac" model:@"Mac Pro" version:@"17E121"];

    /*
     * using PCS-MasterKey for direct syncing during inital sync
     */

    NSDictionary *addItem = @{
                              (__bridge id)kSecClass :(__bridge id)kSecClassInternetPassword,
                              (__bridge id)kSecValueData : [@"foo" dataUsingEncoding:NSUTF8StringEncoding],
                              (__bridge id)kSecAttrAccessGroup: @"com.apple.cfnetwork",
                              (__bridge id)kSecAttrSyncViewHint: @"PCS-MasterKey",
                              (__bridge id)kSecAttrDescription: @"delete me if found",
                              (__bridge id)kSecAttrServer: @"server",
                              (__bridge id)kSecAttrAccount: @"account",
                              (__bridge id)kSecAttrSynchronizable: @YES,
                              (__bridge id)kSecAttrPath: @"/path",
                              (__bridge id)kSecAttrIsInvisible: @YES,
                              };

    NSDictionary *findItem = @{
                               (__bridge id)kSecClass :(__bridge id)kSecClassInternetPassword,
                               (__bridge id)kSecAttrAccessGroup: @"com.apple.cfnetwork",
                               (__bridge id)kSecAttrServer: @"server",
                               (__bridge id)kSecAttrAccount: @"account",
                               (__bridge id)kSecAttrSynchronizable: @YES,
                               };

    [dev1 secItemAdd:addItem complete:^void(OSStatus status, NSDictionary *result) {
        NSLog(@"Result string was dev1: %d %@", (int)status, result);
        XCTAssertEqual(status, 0, "Expect success");
    }];
    [dev1 secItemCopyMatching:findItem complete:^(OSStatus status, NSArray<NSDictionary *> *result) {
        NSLog(@"Result string was dev1: %d %@", (int)status, result);
        XCTAssertEqual(status, 0, "Expect success");
    }];

    /*
     * Setup and validate device 1
     */

    XCTestExpectation *expection = [self expectationWithDescription:@"expect to create circle"];
    [dev1 setupSOSCircle:@"user" password:@"foo" complete:^void(bool success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
        [expection fulfill];
    }];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];

    [dev1 sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
    }];
    [dev2 sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCError, @"expected to be in error: %@", error);
    }];

    /*
     * Setup and validate device 2
     */

    expection = [self expectationWithDescription:@"expect to create circle"];
    [dev2 setupSOSCircle:@"user" password:@"foo" complete:^void(bool success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
        [expection fulfill];
    }];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];

    [dev1 sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
    }];
    [dev2 sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCNotInCircle, @"expected to be NOT in circle: %@", error);
    }];

    NSLog(@"Update all views (dev1)");
    expection = [self expectationWithDescription:@"expect circle update"];
    expection.assertForOverFulfill = false;
    [self.network setTestExpectation:expection forKey:@"oak"];

    expection.assertForOverFulfill = false;
    [dev1 sosEnableAllViews:^(BOOL success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
    }];

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
    [self.network clearTestExpectations];

    __block NSString *peerID1 = NULL;
    [dev1 sosPeerID:^(NSString *peerID) {
        XCTAssertNotEqual(peerID, NULL, @"expected to find a peerID for peer1");
        peerID1 = peerID;
    }];

    [dev2 sosPeerID:^(NSString *peerID) {
        XCTAssertEqual(peerID, NULL, @"expected to NOT find peerID for peer2");
    }];

    [dev1 sosPeerID:^(NSString *peerID) {
        XCTAssertNotEqual(peerID, NULL, @"expected to find a peerID for peer1");
        XCTAssertEqualObjects(peerID1, peerID, "dev1 changed ?");
    }];

    /*
     * Validate second device can request to join
     */

    expection = [self expectationWithDescription:@"expect circle update"];
    expection.assertForOverFulfill = false;
    [self.network setTestExpectation:expection forKey:@"oak"];

    __block NSString *peerID2 = NULL;
    [dev2 sosRequestToJoin:^(bool success, NSString *peerID, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
        XCTAssertNotEqual(peerID, NULL, @"expected to find peerID for peer2");
        peerID2 = peerID;
    }];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];
    [self.network clearTestExpectations];

    /*
     * Check that device 2 can't self join
     */

    expection = [self expectationWithDescription:@"expect device 2 can't self join"];
    [dev2 sosApprovePeer:peerID2 complete:^(BOOL success, NSError *error) {
        XCTAssert(!success, "Expect failure: %@", error);
        [expection fulfill];
    }];
    [self waitForExpectationsWithTimeout:5.0 handler:nil];

    [dev2 sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCRequestPending, @"expected to be pending request: %@", error);
    }];

    [dev1 sosPeerID:^(NSString *peerID) {
        XCTAssertNotEqual(peerID, NULL, @"expected to find a peerID for peer1");
        XCTAssertEqualObjects(peerID1, peerID, "dev1 changed ?");
    }];

    /*
     * Approve device 2 and enable all views
     */

    NSLog(@"approve");
    expection = [self expectationWithDescription:@"expect circle update"];
    expection.assertForOverFulfill = false;
    [self.network setTestExpectation:expection forKey:@"oak"];

    [dev1 sosApprovePeer:peerID2 complete:^(BOOL success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
    }];
    [self waitForExpectationsWithTimeout:60.0 handler:nil];
    [self.network clearTestExpectations];


    /*
     * Validate device 2 made it into circle and have a peerID
     */

    [dev1 sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
    }];
    [dev2 sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
    }];

    [dev2 sosPeerID:^(NSString *peerID) {
        XCTAssertNotEqual(peerID, NULL, @"expected to find a peerID for peer2");
        peerID2 = peerID;
        //XCTAssertEqualObject(peerID1, peerID2, "expect peerID to be different");
    }];

    /*
     * Enable view for syncing
     */

    NSString *netID1toID2 = [NSString stringWithFormat:@"ak|%@:%@", peerID1, peerID2];
    NSString *netID2toID1 = [NSString stringWithFormat:@"ak|%@:%@", peerID2, peerID1];

    expection = [self expectationWithDescription:@"expect traffic from 1->2"];
    expection.assertForOverFulfill = false;
    [self.network setTestExpectation:expection forKey:netID1toID2];

    expection = [self expectationWithDescription:@"expect traffic from 2->1"];
    expection.assertForOverFulfill = false;
    [self.network setTestExpectation:expection forKey:netID2toID1];

    expection = [self expectationWithDescription:@"expect circle update"];
    expection.assertForOverFulfill = false;
    [self.network setTestExpectation:expection forKey:@"oak"];

    /*
     * Perform initial sync
     */

    NSLog(@"initial sync");
    expection = [self expectationWithDescription:@"perform initial sync"];
    [dev2 sosWaitForInitialSync:^(bool success, NSError *error) {
        XCTAssert(success, "Expect success for syncing: %@", error);
        [expection fulfill];
    }];

    /*
     *
     */

    NSLog(@"Update all views (dev2)");
    [dev2 sosEnableAllViews:^(BOOL success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
    }];

    [self waitForExpectationsWithTimeout:60.0 handler:nil];
    [self.network clearTestExpectations];

    /*
     * check syncing did its thing
     */
    NSLog(@"SecItemCopyMatching");
    [dev1 secItemCopyMatching:findItem complete:^(OSStatus status, NSArray<NSDictionary *> *result) {
        NSLog(@"Result string was dev1: %d %@", (int)status, result);
        XCTAssertEqual(status, 0, "Expect success");
    }];

    [dev2 secItemCopyMatching:findItem complete:^(OSStatus status, NSArray<NSDictionary *> *result) {
        NSLog(@"Result string was dev2: %d %@", (int)status, result);
        XCTAssertEqual(status, 0, "Expect success");
    }];

    NSLog(@"done");
}

@end
