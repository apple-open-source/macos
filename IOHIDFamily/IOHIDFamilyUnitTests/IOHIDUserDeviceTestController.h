//
//  NSObject+IOHIDTestController.h
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#import  <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDUserDevice.h>


@protocol  IOHIDUserDeviceObserver <NSObject>

@optional

-(IOReturn) GetReportCallback: (IOHIDReportType) type : (uint32_t) reportID : (uint8_t *_Nonnull ) report : (CFIndex *_Nonnull) reportLength;

-(IOReturn) SetReportCallback: (IOHIDReportType) type : (uint32_t) reportID : (uint8_t *_Nonnull) report : (CFIndex) reportLength;

@end


@interface IOHIDUserDeviceTestController : NSObject

@property (readonly, nonnull)   IOHIDUserDeviceRef          userDevice;
@property (readonly, nonnull)   NSString*                   userDeviceUniqueID;
@property (readonly, nullable)  dispatch_queue_t            userDeviceQueue;
@property (nonnull)             NSMutableArray*             userDeviceSetReports;
@property (nullable)            id<IOHIDUserDeviceObserver> userDeviceObserver;
/*!
 * @method -initWithDescriptor:DeviceUniqueID:andQueue
 *
 * Init controller with descriptor and uniqueID. Other configuration parameter will be synthesized
 *
 * @param descriptor HID descriptor for IOHIDUserDevice.
 *
 * @param uniqueID unique ID that will be used to match device
 *
 * @param queue Optional queue to shedule IOHIDUserDevice if not provide device will not be scheduled with dispatch queue.
 *
 * @result instance of the IOHIDUserDeviceTestController or nil.
 *
 */
-(nullable instancetype) initWithDescriptor: (nonnull NSData *) descriptor DeviceUniqueID: (nonnull NSString*) uniqueID andQueue:(nullable dispatch_queue_t) queue;

/*!
 * @method initWithDeviceConfiguration:andQueue:
 *
 * Init controller with configuration and queue.
 *
 * @param deviceConfig Configuration of  IOHIDUserDevice.
 *
 * @param queue Optional queue to shedule IOHIDUserDevice if not provide device will not be scheduled with dispatch queue.
 *
 * @result instance of the IOHIDUserDeviceTestController or nil.
 *
 */
-(nullable instancetype) initWithDeviceConfiguration: (nonnull NSDictionary *) deviceConfig andQueue:(nullable dispatch_queue_t) queue;

/*!
 * @method handleReport:withInterval:
 *
 * Send single report to userDevice.
 *
 * @param report The report bytes.
 *
 * @param interval The interval in us prior issuing report.
 *
 * @result Return kIOReturnSuccess if success.
 *
 */
-(IOReturn) handleReport: (nonnull NSData *) report withInterval: (NSInteger) interval;

/*!
 * @method handleReports: withInterval:
 *
 * Send series of reports with fixe interval to userDevice.
 *
 * @param interval Interval in us.
 *
 * @param reports Array of NSData reports.
 *
 * @result Return kIOReturnSuccess if success.
 *
 */
-(IOReturn) handleReports: (nonnull NSArray *) reports withInterval: (NSInteger) interval;

/*!
 * @method handleReports:Length:andInterval:
 *
 * Send series of reports with fixed interval to userDevice.
 *
 * @param report Report buffer
 *
 * @param length Report buffer length
 *
 * @param interval Interval in us
 *
 * @result Return kIOReturnSuccess if success.
 *
 */
-(IOReturn) handleReport: (nonnull uint8_t*) report Length: (NSUInteger) length  andInterval: (NSInteger) interval;

/*!
 * @method handleReportsWithIntervals:
 *
 * Send series of reports with corresponding interval to userDevice.
 *
 * @param reports Array of NSDictionary which contain key @"report" of NSData and @"interval" of NSNumber (in us)
 *
 * @result Return kIOReturnSuccess is success.
 *
 */
-(IOReturn) handleReportsWithIntervals: (nonnull NSArray *) reports;


/*!
 * @method handleReports:withTimestamp:
 *
 * Send series of reports with corresponding interval to userDevice.
 *
 * @param report Report buffer
 *
 * @param timestamp Report timestamp
 *
 * @result Return kIOReturnSuccess is success.
 *
 */
-(IOReturn) handleReport: (nonnull NSData*) report withTimestamp: (uint64_t) timestamp;


/*!
 * @method handleReportAsync:withTimestamp:Callback:Context
 *
 * Send series of reports with corresponding interval to userDevice.
 *
 * @param report Report buffer
 *
 * @param timestamp Report timestamp
 *
 * @param callback completion callback
 *
 * @param context completion context
 *
 * @result Return kIOReturnSuccess is success.
 *
 */

-(IOReturn) handleReportAsync: (nonnull NSData*) report withTimestamp: (uint64_t) timestamp Callback: (IOHIDUserDeviceHandleReportAsyncCallback _Nullable ) callback Context: (void * _Nullable) context;

/*!
 * @method handleReportAsync:Callback:Context
 *
 * Send series of reports with corresponding interval to userDevice.
 *
 * @param report Report buffer
 *
 *
 * @param callback completion callback
 *
 * @param context completion context
 *
 * @result Return kIOReturnSuccess is success.
 *
 */
-(IOReturn) handleReportAsync: (nonnull NSData*) report  Callback: (IOHIDUserDeviceHandleReportAsyncCallback _Nullable) callback Context: (void * _Nullable) context;

/*!
 * @method invalidate
 *
 * invalidate object.
 *
 */
-(void)invalidate;

@end
