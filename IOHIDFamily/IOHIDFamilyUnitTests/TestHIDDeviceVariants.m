//
//  TestHIDDeviceElementUpdate.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/30/18.
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import <HID/HID.h>
#import "IOHIDHIDDeviceElementsDescriptor.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDServiceKeys.h>

#pragma clang diagnostic ignored "-Wunused-parameter"

static uint8_t descriptorOutputOnly[] = {
    0x06, 0x00, 0xFF,                         // Usage Page (65280)
    0x0A, 0x0B, 0x00,                         // Usage 11 (0xb)
    0xA1, 0x01,                               // Collection (Application)
    0x0A, 0x0B, 0x00,                         //   Usage 11 (0xb)
    0x91, 0x02,                               //   Output..................(Data, Variable, Absolute)
    0xC0,
};

static uint8_t descriptorFeatureOnly[] = {
    0x06, 0x00, 0xFF,                         // Usage Page (65280)
    0x0A, 0x0B, 0x00,                         // Usage 11 (0xb)
    0xA1, 0x01,                               // Collection (Application)
    0x0A, 0x0B, 0x00,                         //   Usage 11 (0xb)
    0xB1, 0x02,                               //   Feature.................(Data, Variable, Absolute)
    0xC0,
};


@interface TestHIDDeviceVariants : XCTestCase

@property HIDUserDevice             * userDeviceOutput;
@property HIDUserDevice             * userDeviceFeature;
@property HIDManager                * manager;
@property XCTestExpectation         * testDeviceMatchingExpectation;
@property XCTestExpectation         * testManagerCancelExpectation;

@end

@implementation TestHIDDeviceVariants

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
    [super setUp];
    
    NSDictionary * description;
 
    __weak TestHIDDeviceVariants * self_ = self;
    
    self.testDeviceMatchingExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: device matching"];
    self.testDeviceMatchingExpectation.expectedFulfillmentCount = 2;
    self.testManagerCancelExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: device cancel"];

    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    description = @{
                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptorOutputOnly length:sizeof(descriptorOutputOnly)],
                   @kIOHIDVendorIDKey   : @(555),
                   @kIOHIDProductIDKey  : @(555),
                   };


    self.userDeviceOutput = [[HIDUserDevice alloc] initWithProperties:description];
    if (!self.userDeviceOutput) {
        return;
    }
 
    description = @{
                    @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                    @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptorFeatureOnly length:sizeof(descriptorFeatureOnly)],
                    @kIOHIDVendorIDKey   : @(555),
                    @kIOHIDProductIDKey  : @(555),
                    };
    
    
    self.userDeviceFeature = [[HIDUserDevice alloc] initWithProperties:description];
    if (!self.userDeviceFeature) {
        return;
    }
    
    
    self.manager = [[HIDManager alloc] init];

    [self.manager setDeviceMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID }];
    [self.manager setDispatchQueue:dispatch_get_main_queue()];
    [self.manager setDeviceNotificationHandler:^(HIDDevice * device, BOOL added) {
                                                if (added) {
                                                    [self_.testDeviceMatchingExpectation fulfill];
                                                }
                                            }];

    [self.manager setCancelHandler:^{
        [self_.testManagerCancelExpectation fulfill];
    }];

    [self.manager activate];
}

- (void)tearDown {

    XCTWaiterResult     result;
    
    [self.manager cancel];
    
    result = [XCTWaiter waitForExpectations:@[self.testManagerCancelExpectation] timeout:5];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testManagerCancelExpectation);

    [super tearDown];
    
}

- (void)testDeviceVariants {
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDeviceFeature && self.userDeviceOutput, "userDeviceFeature:%@ userDeviceOutput:%@", self.userDeviceFeature, self.userDeviceOutput);

    result = [XCTWaiter waitForExpectations:@[self.testDeviceMatchingExpectation] timeout:5];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testDeviceMatchingExpectation);
    
    

}



@end
