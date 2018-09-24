//
//  IOHIDTestController+IOHIDDeviceTestController.h
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#import  <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDDevice.h>


@interface IOHIDDeviceTestController : NSObject

@property (readonly, nonnull)  IOHIDManagerRef           deviceManager;
@property (          nullable) IOHIDDeviceRef            device;
@property (readonly, nonnull)  dispatch_queue_t          deviceQueue;
@property (          nonnull)  NSMutableArray*           values;
@property (          nonnull)  NSMutableArray*           reports;

/*!
 * @method -initWithDeviceUniqueID:AndQueue:
 *
 * Init HID Device  test controller with configuration of IOHIDUserDevice
 *
 * @param deviceID Unique Device ID.
 *
 * @param runLoop Runnlop to shedule HID manager with.
 *
 * @result instance of the IOHIDEventSystemTestController or nil.
 *
 */
-(nullable instancetype) initWithDeviceUniqueID: (nonnull id) deviceID :(nonnull CFRunLoopRef) runLoop;

/*!
 * @method -initWithMatching:AndQueue:
 *
 * Init HID Device  test controller with configuration of IOHIDUserDevice
 *
 * @param matching Matching criteria for device.
 *
 * @param runLoop Runnlop to shedule HID manager with.
 *
 * @result instance of the IOHIDEventSystemTestController or nil.
 *
 */
-(nullable instancetype) initWithMatching: (nonnull NSDictionary *) matching :(nonnull CFRunLoopRef) runLoop;

/*!
 * @method invalidate
 *
 * invalidate object.
 *
 */
-(void)invalidate;

@end
