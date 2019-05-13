//
//  HIDDisplayDevicePresetPrivate.h
//  IOHIDFamily
//
//  Created by AB on 1/16/19.
//

#ifndef HIDDisplayDevicePresetPrivate_h
#define HIDDisplayDevicePresetPrivate_h

#import <HIDDisplay/HIDDisplayDevicePreset.h>
#import <HIDDisplay/HIDDisplayDevice.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayDevicePreset (HIDDisplayDevicePresetPrivate)

@property HIDDisplayDevice* hidDisplay;

-(nullable instancetype) initWithDisplayDevice:(HIDDisplayDevice*) hidDisplay index:(NSInteger) index;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayDevicePresetPrivate_h */
