//
//  HIDDisplayInterfacePrivate.h
//  IOHIDFamily
//
//  Created by AB on 1/16/19.
//

#ifndef HIDDisplayInterfacePrivate_h
#define HIDDisplayInterfacePrivate_h

#import <Foundation/Foundation.h>
#import <HIDDisplay/HIDDisplay.h>
#import <HIDDisplay/HIDDisplayInterface.h>
#import <IOKit/hid/IOHIDDevice.h>
#import "HIDElement.h"

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayInterface (HIDDisplayInterfacePrivate)

@property(nullable) IOHIDDeviceRef device;

-(nullable instancetype) initWithMatching:(NSDictionary*) matching;
-(BOOL) commit:(NSArray<HIDElement*>*) elements error:(NSError**) error;
-(BOOL) extract:(NSArray<HIDElement*>*) elements error:(NSError**) error;
-(NSArray* __nullable) getHIDDevices;
-(NSArray* __nullable) getHIDDevicesForMatching:(NSDictionary*) matching;
-(NSDictionary<NSNumber*,HIDElement*>* __nullable) getDeviceElements:(NSDictionary*) matching;
@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayInterfacePrivate_h */
