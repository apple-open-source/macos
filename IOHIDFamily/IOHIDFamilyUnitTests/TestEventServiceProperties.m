//
//  TestEventServiceProperties.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 3/12/19.
//

#import <Foundation/Foundation.h>
#import "HIDDeviceTester.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>

uint8_t absolutePointer[] = {
    HIDPointerAbsoluteDescriptor
};

@interface TestEventServiceProperties : HIDDeviceTester

@end

@implementation TestEventServiceProperties

- (void)setUp
{
    self.useDevice = false;
    
    self.properties[@kIOHIDVendorIDKey] = @(123);
    self.properties[@kIOHIDProductIDKey] = @(456);
    self.properties[@"TestProp1"] = @(1);
    self.properties[@"TestProp2"] = @(2);
    self.properties[@kIOHIDVersionNumberKey] = @(3);
    self.properties[@kIOHIDAppleVendorSupported] = @YES;
    
    self.descriptor = [NSData dataWithBytes:absolutePointer
                                     length:sizeof(absolutePointer)];
    
    [super setUp];
}

- (void)MAC_OS_ONLY_TEST_CASE(testEventServiceProperties)
{
    id prop = nil;
    NSDictionary *dict = nil;
    NSNumber *width = nil, *height = nil;
    NSInteger logicalMax = 32767; // defined in descriptor
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.userDevice,
                               "Failed to create HIDUserDevice");
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.service,
                               "Failed to create service");
    
    // Acceleration properties
    prop = [self.service propertyForKey:@kIOHIDPointerAccelerationTypeKey];
    NSLog(@"kIOHIDPointerAccelerationTypeKey: %@", prop);
    XCTAssert(prop, "Failed to find kIOHIDPointerAccelerationTypeKey");
    
    prop = [self.service propertyForKey:@kIOHIDScrollAccelerationTypeKey];
    NSLog(@"kIOHIDScrollAccelerationTypeKey: %@", prop);
    XCTAssert(prop, "Failed to find kIOHIDScrollAccelerationTypeKey");
    
    // Surface dimension properties
    prop = [self.service propertyForKey:@kIOHIDSurfaceDimensionsKey];
    NSLog(@"kIOHIDSurfaceDimensionsKey: %@", prop);
    XCTAssert(prop && [prop isKindOfClass:[NSDictionary class]],
              "Failed to find kIOHIDSurfaceDimensionsKey");
    
    dict = (NSDictionary *)prop;
    width = (NSNumber *)dict[@kIOHIDWidthKey];
    height = (NSNumber *)dict[@kIOHIDHeightKey];
    
    NSLog(@"kIOHIDWidthKey: %@ kIOHIDHeightKey: %@", width, height);
    XCTAssert(width && [width isEqualToNumber:@(logicalMax)],
              "Width incorrect expected: %ld actual: %@", logicalMax, width);
    XCTAssert(height && [height isEqualToNumber:@(logicalMax)],
              "Height incorrect expected: %ld actual: %@", logicalMax, height);
}

- (void)testGetProperties
{
    NSDictionary *props = nil;
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.userDevice,
                               "Failed to create HIDUserDevice");
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.service,
                               "Failed to create service");
    
    // test all property cache
    props = [self.service propertiesForKeys:@[@(kIOHIDPrimaryUsagePageKey),
                                              @(kIOHIDPrimaryUsageKey)]];
    NSLog(@"props: %@", props);
    XCTAssert(props && props.count == 2);
    XCTAssert([props[@kIOHIDPrimaryUsagePageKey] isEqualToNumber:@(1)]);
    XCTAssert([props[@kIOHIDPrimaryUsageKey] isEqualToNumber:@(2)]);
    
    // test some property cache
    props = [self.service propertiesForKeys:@[@(kIOHIDVendorIDKey),
                                              @(kIOHIDProductIDKey),
                                              @("TestProp1"),
                                              @("TestProp2")]];
    NSLog(@"props: %@", props);
    XCTAssert(props && props.count == 4);
    XCTAssert([props[@kIOHIDVendorIDKey] isEqualToNumber:@(123)]);
    XCTAssert([props[@kIOHIDProductIDKey] isEqualToNumber:@(456)]);
    XCTAssert([props[@"TestProp1"] isEqualToNumber:@(1)]);
    XCTAssert([props[@"TestProp2"] isEqualToNumber:@(2)]);
    
    // test no property cache
    props = [self.service propertiesForKeys:@[@(kIOHIDVirtualHIDevice),
                                              @(kIOHIDAppleVendorSupported)]];
    
    NSLog(@"props: %@", props);
    XCTAssert(props && props.count == 2);
    XCTAssert([props[@kIOHIDVirtualHIDevice] isEqual:@YES]);
    XCTAssert([props[@kIOHIDAppleVendorSupported] isEqual:@YES]);
    
    // test nonexistent property
    props = [self.service propertiesForKeys:@[@(kIOHIDBuiltInKey),
                                              @(kIOHIDVersionNumberKey)]];
    
    NSLog(@"props: %@", props);
    XCTAssert(props && props.count == 1);
    XCTAssert([props[@kIOHIDVersionNumberKey] isEqualToNumber:@(3)]);
}

@end
