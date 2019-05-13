//
//  HIDDisplayDevicePrivate.h
//  IOHIDFamily
//
//  Created by AB on 1/16/19.
//

#ifndef HIDDisplayDevicePrivate_h
#define HIDDisplayDevicePrivate_h

#import <Foundation/Foundation.h>
#import <HIDDisplay/HIDDisplay.h>
#import <HIDDisplay/HIDDisplayDevice.h>
#import <IOKit/hid/IOHIDDevice.h>
#import "HIDElement.h"

NS_ASSUME_NONNULL_BEGIN


@interface HIDDisplayDevice (HIDDisplayDevicePrivate)

@property(nullable) IOHIDDeviceRef device;

-(nullable instancetype) initWithMatching:(NSDictionary*) matching;

-(BOOL) commit:(NSArray<HIDElement*>*) elements error:(NSError**) error;
-(BOOL) extract:(NSArray<HIDElement*>*) elements error:(NSError**) error;
-(NSInteger) getCurrentPresetIndex:(NSError**) error;
-(BOOL) setCurrentPresetIndex:(NSInteger) index error:(NSError**) error;
-(HIDElement* __nullable) getHIDElementForUsage:(NSInteger) usage;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayDevicePrivate_h */
