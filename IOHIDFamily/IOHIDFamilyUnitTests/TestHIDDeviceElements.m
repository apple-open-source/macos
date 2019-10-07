//
//  TestHIDDeviceElementUpdate.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/30/18.
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "HID.h"
#import "IOHIDHIDDeviceElementsDescriptor.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDServiceKeys.h>

#pragma clang diagnostic ignored "-Wunused-parameter"

static uint8_t descriptor[] = {
    HIDDeviceElements
};

@interface TestHIDDeviceElements : XCTestCase

@property HIDUserDevice             * userDevice;
@property HIDDevice                 * device;
@property XCTestExpectation         * testDeviceCancelExpectation;
@property XCTestExpectation         * testDeviceGetReportExpectation;
@property NSInteger                 setReportCount;
@property NSInteger                 getReportCount;

@end

@implementation TestHIDDeviceElements

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
    [super setUp];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDVendorIDKey   : @(555),
                                   @kIOHIDProductIDKey  : @(555),
                                   };

    
    self.testDeviceCancelExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: device cancel"];
    self.testDeviceCancelExpectation.expectedFulfillmentCount = 2;

    self.testDeviceGetReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: device get report"];
    self.testDeviceGetReportExpectation.expectedFulfillmentCount = 2;

    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties:description];
    if (!self.userDevice) {
        return;
    }
    
    [self.userDevice setDispatchQueue:dispatch_get_main_queue()];

    __weak TestHIDDeviceElements * self_ = self;
    
    [self.userDevice setCancelHandler:^{
        [self_.testDeviceCancelExpectation fulfill];
    }];

    [self.userDevice setSetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, const void *report __unused, NSInteger reportLength __unused) {
        IOReturn status = kIOReturnSuccess;
        NSLog(@"setSetReportHandler: type:%d reportID:%d",(int)type, (int)reportID);
        self_.setReportCount++;
        return  status;
    }];

    [self.userDevice setGetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, void *report __unused, NSInteger *reportLength __unused) {
        IOReturn status = kIOReturnSuccess;
        NSLog(@"setGetReportHandler: type:%d reportID:%d",(int)type, (int)reportID);
        self_.getReportCount++;
        return  status;
    }];

    
    [self.userDevice activate];
    
    mach_timespec_t waitTime = {30, 0};
    kern_return_t kr = IOServiceWaitQuiet (self.userDevice.service, &waitTime);
    if (kr) {
        NSLog(@"IOServiceWaitQuiet:0x%x", kr);
        return;
    }

    self.device = [[HIDDevice alloc] initWithService:self.userDevice.service];
    if (!self.device) {
        return;
    }
 
    [self.device setCancelHandler:^{
        [self_.testDeviceCancelExpectation fulfill];
    }];

    [self.device setDispatchQueue:dispatch_get_main_queue()];
    
    [self.device activate];
    
    [self.device open];
}

- (void)tearDown {

    [self.userDevice cancel];

    [self.device close];

    [self.device cancel];
    
    XCTWaiterResult result  = [XCTWaiter waitForExpectations:@[self.testDeviceCancelExpectation] timeout:2];
    HIDXCTAssertWithParameters (COLLECT_LOGARCHIVE, result == XCTWaiterResultCompleted, "expectation: %@", self.testDeviceCancelExpectation);
    
    [super tearDown];
    
}

- (void)testFeatureArrayElementPost {
    __block NSError     * error;
    __block BOOL        ret;
 
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);

    NSArray<HIDElement *> * arrayElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(1)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, arrayElements && arrayElements.count, "elements: %@", arrayElements);

    for (HIDElement * element in arrayElements ) {
        element.integerValue = 0;
    }

    arrayElements[1].integerValue = 1;
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];

    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:arrayElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@", error);
    
    for (HIDElement * element in arrayElements ) {
        element.integerValue = 0;
    }
    arrayElements[5].integerValue = 1;

    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];


    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:arrayElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@", error);

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.setReportCount == 2, "report count: %ld", self.setReportCount);

}

- (void)testOutputArrayElementPost {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * arrayElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(2)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, arrayElements && arrayElements.count, "elements: %@", arrayElements);
    
    for (HIDElement * element in arrayElements ) {
        element.integerValue = 0;
    }
    
    arrayElements[3].integerValue = 1;
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:arrayElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@ arrayElements:%@", error, arrayElements);
    
    for (HIDElement * element in arrayElements ) {
        element.integerValue = 0;
    }
    arrayElements[6].integerValue = 1;
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:arrayElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@ arrayElements:%@", error, arrayElements);

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.setReportCount == 2, "report count: %ld", self.setReportCount);
    
}



- (void)testFeatureDataElementPost {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * dataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(4)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, dataElements && dataElements.count, "elements: %@", dataElements);
    
    for (HIDElement * element in dataElements ) {
        element.integerValue = 0;
    }
    
    dataElements[1].integerValue = 1;
    dataElements[2].integerValue = 1;
    dataElements[3].integerValue = 1;

    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:dataElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ dataElements:%@", error, dataElements);
    
    for (HIDElement * element in dataElements ) {
        element.integerValue = 0;
    }
    
    dataElements[5].integerValue = 1;
    dataElements[6].integerValue = 1;

    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:dataElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ dataElements:%@", error, dataElements);

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.setReportCount == 2, "report count: %ld", self.setReportCount);
    
}


- (void)testOutputDataElementPost {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * dataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(5)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, dataElements && dataElements.count, "elements: %@", dataElements);
    
    for (HIDElement * element in dataElements ) {
        element.integerValue = 0;
    }
    
    dataElements[1].integerValue = 1;
    dataElements[2].integerValue = 1;
    dataElements[3].integerValue = 1;
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:dataElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ dataElements:%@", error, dataElements);

    for (HIDElement * element in dataElements ) {
        element.integerValue = 0;
    }
    
    dataElements[5].integerValue = 1;
    dataElements[6].integerValue = 1;
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:dataElements direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ dataElements:%@", error, dataElements);

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.setReportCount == 2, "report count: %ld", self.setReportCount);
    
}


- (void)testPostDataAndArrayElements {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * dataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(4)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, dataElements && dataElements.count, "elements: %@", dataElements);

    NSArray<HIDElement *> * arrayElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(1)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, arrayElements && arrayElements.count, "elements: %@", arrayElements);
    
    for (HIDElement * element in dataElements ) {
        element.integerValue = 0;
    }

    for (HIDElement * element in arrayElements ) {
        element.integerValue = 0;
    }

    dataElements[0].integerValue    = 1;
    dataElements[1].integerValue    = 1;
    arrayElements[4].integerValue   = 1;

    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];

    NSMutableArray * transaction = [[NSMutableArray alloc] initWithArray:arrayElements];
    [transaction addObjectsFromArray:[dataElements subarrayWithRange:NSMakeRange(0, 2)]];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:transaction direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@", error);
    
    
    for (HIDElement * element in arrayElements ) {
        element.integerValue = 0;
    }

    arrayElements[2].integerValue = 1;
    dataElements[3].integerValue = 1;
    dataElements[4].integerValue = 1;

    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    transaction = [[NSMutableArray alloc] initWithArray:arrayElements];
    [transaction addObjectsFromArray:[dataElements subarrayWithRange:NSMakeRange(3, 2)]];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:transaction direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ transaction:%@", error, transaction);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.setReportCount == 4, "report count: %ld", self.setReportCount);
    
}

- (void)testFeatureAndOutputDataElementPost {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * fdataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(4)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, fdataElements && fdataElements.count, "elements: %@", fdataElements);
    
    NSArray<HIDElement *> * odataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(5)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, odataElements && odataElements.count, "elements: %@", odataElements);
    
    for (HIDElement * element in fdataElements ) {
        element.integerValue = 0;
    }
    
    for (HIDElement * element in odataElements ) {
        element.integerValue = 0;
    }
    
    fdataElements[0].integerValue    = 1;
    odataElements[0].integerValue    = 1;

    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:@[fdataElements[0], odataElements[0]] direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@", error);
    
    
    
    fdataElements[1].integerValue    = 1;
    odataElements[1].integerValue    = 1;

    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:@[fdataElements[1], odataElements[1]] direction:HIDDeviceCommitDirectionOut error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@", error);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.setReportCount == 4, "report count: %ld", self.setReportCount);
    
}


- (void)testFeatureArrayElementUpdate {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * arrayElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(1)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, arrayElements && arrayElements.count, "elements: %@", arrayElements);
    
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:arrayElements direction:HIDDeviceCommitDirectionIn error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@", error);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.getReportCount == 1, "report count: %ld", self.getReportCount);
}

- (void)testInputArrayElementUpdate {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * arrayElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(3)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, arrayElements && arrayElements.count, "elements: %@", arrayElements);
    
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:arrayElements direction:HIDDeviceCommitDirectionIn error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error: %@", error);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.getReportCount == 1, "report count: %ld", self.getReportCount);
}


- (void)testFeatureDataElementUpdate {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * dataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(4)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, dataElements && dataElements.count, "elements: %@", dataElements);
    
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:dataElements direction:HIDDeviceCommitDirectionIn error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ dataElements:%@", error, dataElements);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.getReportCount == 1, "report count: %ld", self.getReportCount);
}

- (void)testInputDataElementUpdate {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * dataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(6)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, dataElements && dataElements.count, "elements: %@", dataElements);
    
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:dataElements direction:HIDDeviceCommitDirectionIn error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ dataElements:%@", error, dataElements);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.getReportCount == 1, "report count: %ld", self.getReportCount);
}


- (void)testUpdateDataAndArrayElements {
    __block NSError     * error;
    __block BOOL        ret;
    
    XCTestExpectation   * expectation;
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);
    
    NSArray<HIDElement *> * dataElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(4)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, dataElements && dataElements.count, "elements: %@", dataElements);
    
    NSArray<HIDElement *> * arrayElements  =  [self.device elementsMatching: @{ @kIOHIDElementReportIDKey: @(1)}];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, arrayElements && arrayElements.count, "elements: %@", arrayElements);
    
    for (HIDElement * element in dataElements ) {
        element.integerValue = 0;
    }
    
    for (HIDElement * element in arrayElements ) {
        element.integerValue = 0;
    }
    
    expectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: block completion"];
    
    NSMutableArray * transaction = [[NSMutableArray alloc] initWithArray:arrayElements];
    [transaction addObjectsFromArray: @[dataElements[0],dataElements[1]]];
    
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ret = [self.device commitElements:transaction direction:HIDDeviceCommitDirectionIn error:&error];
        [expectation fulfill];
    });
    
    result = [XCTWaiter waitForExpectations:@[expectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                expectation);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@ transaction:%@", error, transaction);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.getReportCount == 2, "report count: %ld", self.getReportCount);
    
}


@end
