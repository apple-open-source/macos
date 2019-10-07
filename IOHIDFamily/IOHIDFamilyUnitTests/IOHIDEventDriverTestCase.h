//
//  IOHIDEventDriverTestCase.h
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include <IOKit/hid/IOHIDUserDevice.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDEventDriverTestCase : XCTestCase

@property IOHIDEventSystemClientRef         eventSystem;
@property IOHIDServiceClientRef             eventService;
@property IOHIDUserDeviceRef                userDevice;
@property NSString                          * userDeviceUUID;
@property NSData                            * hidDeviceDescriptor;
@property NSDictionary                      * userDeviceDescription;
@property NSMutableArray                    * events;
@property XCTestExpectation                 * testServiceExpectation;


-(void) addService: (IOHIDServiceClientRef) service;
-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef) service;
-(IOReturn)userDeviceGetReportHandler: (IOHIDReportType)type :(uint32_t)reportID :(uint8_t *)report :(NSUInteger *) length;
-(IOReturn)userDeviceSetReportHandler: (IOHIDReportType)type :(uint32_t)reportID :(uint8_t *)report :(NSUInteger ) length;

@end

NS_ASSUME_NONNULL_END
