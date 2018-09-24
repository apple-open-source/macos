//
//  ClientInfoByNotification.m
//  Security
//
//  Created by murf on 4/12/18.
//

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/Security.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>

#undef DOVIEWMACRO
#define DOVIEWMACRO(VIEWNAME, DEFSTRING, CMDSTRING, SYSTEM, DEFAULTSETTING, INITIALSYNCSETTING, ALWAYSONSETTING, BACKUPSETTING, V0SETTING) \
const CFStringRef k##SYSTEM##View##VIEWNAME          = CFSTR(DEFSTRING);
#include "Security/SecureObjectSync/ViewList.list"

#import "DeviceSimulatorProtocol.h"
#import "MultiDeviceNetworking.h"
#import <objc/runtime.h>


#if 1
@interface MDDevice2 : NSObject<DeviceSimulatorProtocol>
@property NSXPCConnection *connection;
@property NSString *name;
- (instancetype)initWithConnection:(NSXPCConnection *)connection;
@end

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wprotocol"
@implementation MDDevice2

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
#endif

@interface ClientInfoByNotification : XCTestCase
@property NSMutableDictionary<NSString *,MDDevice2 *> *connections;
@property MultiDeviceNetworking *network;
@property MDDevice2 *masterDevice;
@end

static NSString *testInstanceUUID;

@implementation ClientInfoByNotification


+ (void)setUp
{
    testInstanceUUID = [[NSUUID UUID] UUIDString];
}

- (void)setUp
{
    self.connections = [NSMutableDictionary dictionary];
    self.network = [[MultiDeviceNetworking alloc] init];
    [self runSigninWithAdditionalDevices:0];
}

- (void)tearDown
{
    __block uint64_t totalUserUsec = 0, totalSysUsec = 0;
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    
    for (NSString *name in self.connections) {
        MDDevice2 *device = self.connections[name];
        NSLog(@"device: %@", name);
        [device diagnosticsCPUUsage:^(bool success, uint64_t user_usec, uint64_t sys_usec, NSError *error) {
            NSLog(@"%@: %d: u:%llu s:%llu", device.name, success, (unsigned long long)user_usec, (unsigned long long)sys_usec);
            totalUserUsec += user_usec;
            totalSysUsec += sys_usec;
            result[[NSString stringWithFormat:@"cpu-%@", name]] = @{ @"user_usec" : @(user_usec), @"system_usec" : @(sys_usec)};
        }];
    }
    
    result[@"cpu-total"] = @{ @"user_usec" : @(totalUserUsec), @"system_usec" : @(totalSysUsec)};
    
    NSLog(@"Total: u:%llu s:%llu", (unsigned long long)totalUserUsec, (unsigned long long)totalSysUsec);
    
    /* XXX check for leaks in all devices */
    for (NSString *name in self.connections) {
        MDDevice2 *device = self.connections[name];
        [device.connection invalidate];
    }
    self.connections = NULL;
    [self.network dumpKVSState];
    [self.network dumpCounters];
    [self.network disconnectAll];
    self.network = NULL;
    
    NSData * jsonData = [NSJSONSerialization dataWithJSONObject:result options:0 error:NULL];
    
    [jsonData writeToFile:[NSString stringWithFormat:@"/tmp/test-result-%@", [self name]] atomically:NO];
    
}

//MARK: - Device logic

- (MDDevice2 *)device:(NSString *)name model:(NSString *)model version:(NSString *)version
{
    MDDevice2 *device = self.connections[name];
    if (device != NULL) {
        return NULL;
    }
    
    NSXPCConnection *conn = [[NSXPCConnection alloc] initWithServiceName:@"com.apple.Security.DeviceSimulator"];
    conn.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(DeviceSimulatorProtocol)];
    [conn _setUUID:[NSUUID UUID]]; // select a random instance
    [conn resume];
    
    device = [[MDDevice2 alloc] initWithConnection:conn];
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

- (void)runSigninWithAdditionalDevices:(unsigned)additionalDeviceCount {
    signal(SIGPIPE, SIG_IGN);
    _masterDevice = [self device:@"ipad" model:@"iPad" version:@"15E143a"];
    NSMutableArray<MDDevice2 *> *otherDevices = [NSMutableArray array];
    
    [_masterDevice setupSOSCircle:@"user" password:@"foo" complete:^void(bool success, NSError *error) {
        XCTAssert(success, "Expect success: %@", error);
    }];
    
    [_masterDevice sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
    }];
    
    sleep(4); // give "kvs" a chance to get the word out.
    for (unsigned n = 0; n < additionalDeviceCount; n++) {
        MDDevice2 *dev = [self device:[NSString stringWithFormat:@"mac-%u", n] model:@"Mac Pro" version:@"17E121"];
        
        [dev setupSOSCircle:@"user" password:@"foo" complete:^void(bool success, NSError *error) {
            XCTAssert(success, "%u: Expect success: %@", n, error);
        }];
        
        __block NSString *devPeerID = NULL;
        [dev sosRequestToJoin:^(bool success, NSString *peerID, NSError *error) {
            XCTAssert(success, "%u: Expect success: %@", n, error);
            XCTAssertNotEqual(peerID, NULL, @"%u: expected to find peerID for peer2", n);
            devPeerID = peerID;
        }];
        
        __block NSError *localErr = nil;
        __block bool done = false;
        for(int tries=0; tries < 5; tries++) {
            
            [_masterDevice sosApprovePeer:devPeerID complete:^(BOOL success, NSError *error) {
                localErr = [error copy];
                if(success) {
                    localErr = nil;
                    done = true;
                }
            }];
            if(done) break;
            sleep(10);
        }
        XCTAssert(done, "%u: Expect success (for approve of %@): %@", n, devPeerID, localErr);
        
        [otherDevices addObject:dev];
    }
}

- (void)testSOSIsThisDeviceInCircle
{
    [self measureBlock:^{
        for(int i=0; i<100; i++) {
            [self.masterDevice sosCircleStatus:^void(SOSCCStatus status, NSError *error) {
                XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
            }];
        }
    }];
}

- (void)testSOSIsThisDeviceInCircleNonCached
{
    [self measureBlock:^{
        for(int i=0; i<100; i++) {
            [self.masterDevice sosCircleStatusNonCached:^void(SOSCCStatus status, NSError *error) {
                XCTAssertEqual(status, kSOSCCInCircle, @"expected to be in circle: %@", error);
            }];
        }
    }];
}


#if TARGET_OS_IPHONE
- (void)testSOSViews2
{
    [self.masterDevice sosViewStatus: (__bridge NSString*)kSOSViewAutofillPasswords withCompletion: ^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCViewNotMember, @"expected to be not in view: %@", error);
    }];
    [self.masterDevice sosViewStatus: (__bridge NSString*)kSOSViewHomeKit withCompletion: ^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCViewMember, @"expected to be in view: %@", error);
    }];
    [self.masterDevice sosViewStatus: (__bridge NSString*)kSOSViewWiFi withCompletion: ^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCViewNotMember, @"expected to be not in view: %@", error);
    }];
    [self.masterDevice sosViewStatus: (__bridge NSString*)kSOSViewContinuityUnlock withCompletion: ^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCViewMember, @"expected to be in view: %@", error);
    }];
    
    [self.masterDevice sosEnableAllViews:^(BOOL success, NSError *error) {
        XCTAssert(success, "Expected to enable all views");
    }];

    //uint64_t bitmask = [self.masterDevice sosCachedCircleBitmask];
    //XCTAssertEqual(bitmask, 0, "Expected bitmask to be %llx", bitmask);


    [self.masterDevice sosViewStatus: (__bridge NSString*)kSOSViewWiFi withCompletion: ^void(SOSCCStatus status, NSError *error) {
        XCTAssertEqual(status, kSOSCCViewMember, @"expected to be in view: %@", error);
    }];

    [self.masterDevice sosCachedViewBitmask:^(uint64_t bitmask) {
        XCTAssertEqual(bitmask, 33554367, @"expected bitmask of %llx", bitmask);
    }];
    
    [self measureBlock:^{
        for(int i=0; i<100; i++) {
            [self.masterDevice sosViewStatus: (__bridge NSString*)kSOSViewHomeKit withCompletion: ^void(SOSCCStatus status, NSError *error) {
            }];
        }
    }];
}
#endif


- (void)testSOSMultiView
{
    [self.masterDevice sosEnableAllViews:^(BOOL success, NSError *error) {
        XCTAssert(success, "Expected to enable all views");
    }];
    
    [self.masterDevice sosICKStatus: ^void(bool status) {
        XCTAssert(status, "Expected to enable iCloud Keychain");
    }];

    [self measureBlock:^{
        for(int i=0; i<100; i++) {
            [self.masterDevice sosICKStatus: ^void(bool status) {
            }];
        }
    }];
}

- (void) testMaster2
{
    [self testSOSMultiView];
}

@end
