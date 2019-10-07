//
//  HIDDeviceTester.h
//
//
//  Created by dekom on 2/22/19.
//  Copyright © 2019 apple. All rights reserved.
//

#ifndef HIDDeviceTester_h
#define HIDDeviceTester_h

#import <Foundation/Foundation.h>
#import <HID/HID.h>
#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDeviceTester : XCTestCase

/*!
 * @method setUp
 *
 * @discussion
 * Sets up HIDUserDevice, HIDDevice, and HIDEventSystemClient. If you wish to
 * provide your own, you should set it up before calling this method.
 */
- (void)setUp;

/*!
 * @method tearDown
 *
 * @discussion
 * Cancels the HIDUserDevice, HIDDevice, and HIDEventSystemClient.
 */
- (void)tearDown;

/*!
 * @method handleGetReport
 *
 * @discussion
 * Method invoked on a getReport call to the HIDUserDevice. If you provided your
 * own HIDUserDevice, this method will not be invoked.
 */
- (IOReturn)handleGetReport:(NSMutableData *)report
                       type:(HIDReportType)type
                   reportID:(NSInteger)reportID;

/*!
 * @method handleSetReport
 *
 * @discussion
 * Method invoked on a setReport call to the HIDUserDevice. If you provided your
 * own HIDUserDevice, this method will not be invoked.
 */
- (IOReturn)handleSetReport:(NSData *)report
                       type:(HIDReportType)type
                   reportID:(NSInteger)reportID;

/*!
 * @method handleInputReport
 *
 * @discussion
 * Method invoked when an input report is received by the HIDDevice. If you
 * provide your own HIDDevice, this method will not be invoked.
 */
- (void)handleInputReport:(NSData *)report
                timestamp:(uint64_t)timestamp
                     type:(HIDReportType)type
                 reportID:(NSInteger)reportID
                   device:(HIDDevice *)device;

/*!
 * @method handleEvent
 *
 * @discussion
 * Method invoked when a HIDEvent is received by the HIDEventSystemClient. If
 * you provide your own HIDEventSystemClient, this method will not be invoked.
 */
- (void)handleEvent:(HIDEvent *)event forService:(HIDServiceClient *)service;

/*!
 * @property userDevice
 *
 * @discussion
 * A HIDUserDevice used for testing. A report descriptor must be provided via
 * the "descriptor" property prior to the setUp call in order for a
 * HIDUserDevice to be created.
 *
 * If this object is not needed, you may set it to @NO before calling the setUp
 * method. You must set it back to nil before calling the tearDown method.
 */
@property (nullable) HIDUserDevice *userDevice;

/*!
 * @property device
 *
 * @discussion
 * A HIDDevice used for testing. The default behavior creates a HIDDevice based
 * off of the HIDUserDevice.
 *
 * If this object is not needed, you may set the useDevice property below to
 * false before calling the setUp method.
 */
@property (nullable) HIDDevice *device;

/*!
 * @property useDevice
 *
 * @discussion
 * Property used to determine if a HIDDevice should be created. Defaults to
 * true. Set to false prior to the setUp method if the device is not needed.
 */
@property BOOL useDevice;

/*!
 * @property client
 *
 * @discussion
 * A HIDEventSystemClient used for testing. The default behavior matches the
 * client to the service created by the HIDUserDevice.
 *
 * If this object is not needed, you may set the useClient property below to
 * false before calling the setUp method.
 */
@property (nullable) HIDEventSystemClient *client;

/*!
 * @property useClient
 *
 * @discussion
 * Property used to determine if a HIDEventSystemClient should be created.
 * Defaults to true. Set to false prior to the setUp method if the client is
 * not needed.
 */
@property BOOL useClient;

/*!
 * @property service
 *
 * @discussion
 * A HIDServiceClient used for testing. The default behavior matches the
 * client to the service created by the HIDUserDevice.
 */
@property (nullable) HIDServiceClient *service;

/*!
 * @property uniqueID
 *
 * @discussion
 * A UUID string that gets published to the HIDUserDevice. You may provide your
 * own if desired.
 *
 * If you choose to provide your own, you must set this property prior to
 * calling the setUp method.
 */
@property (nullable) NSString *uniqueID;

/*!
 * @property descriptor
 *
 * @discussion
 * The descriptor that is used for creating the HIDUserDevice. You must provide
 * this property prior to calling the setUp method.
 */
@property (nullable) NSData *descriptor;

/*!
 * @property properties
 *
 * @discussion
 * Properties that will be set on the HIDUserDevice upon creation. The default
 * properties will have the report descriptor and the uniqueID of the device.
 * You may provide any additional properties as needed.
 *
 * If you choose to provide your own, you must set this property prior to
 * calling the setUp method.
 */
@property (nullable) NSMutableDictionary *properties;

/*!
 * @property events
 *
 * @discussion
 * An array of HIDEvents received by the handleEvent method. Useful for keeping
 * track of events received by the HIDEventSystemClient. If you override the
 * handleEvent method, this array will be empty, unless you call the super
 * method. The array will also be empty if you provide your own client.
 */
@property (nullable) NSMutableArray *events;

/*!
 * @property eventExp
 *
 * @discussion
 * An expectation that is fulfilled when an event is received by the client.
 */
@property (nullable) XCTestExpectation *eventExp;


/*!
 * @property reports
 *
 * @discussion
 * An array of HID reports received by the handleInputReport method. Useful for
 * keeping track of reports received by the HIDDevice. If you override the
 * handleInputReport method, this array will be empty, unless you call the super
 * method. The array will also be empty if you provide your own HIDDevice.
 */
@property (nullable) NSMutableArray *reports;

/*!
 * @property reportExp
 *
 * @discussion
 * An expectation that is fulfilled when a report is received from the device.
 */
@property (nullable) XCTestExpectation *reportExp;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDeviceTester_h */
