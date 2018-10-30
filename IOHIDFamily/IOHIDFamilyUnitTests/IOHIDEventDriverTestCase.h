//
//  IOHIDEventDriverTestCase.h
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include <IOKit/hid/IOHIDUserDevice.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>

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

@end

NS_ASSUME_NONNULL_END
