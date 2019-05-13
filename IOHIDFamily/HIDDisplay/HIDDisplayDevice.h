//
//  HIDDisplayDevice.h
//  IOHIDFamily
//
//  Created by AB on 1/10/19.
//

#ifndef HIDDisplayDevice_h
#define HIDDisplayDevice_h


#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDDisplayDevicePreset;

@interface HIDDisplayDevice : NSObject

@property(readonly) NSArray<HIDDisplayDevicePreset*> *presets;

@property(readonly) NSString* containerID;

@property(readonly,nullable) NSArray<NSString*> *capabilities; // return preset field keys supported by display

-(instancetype) init NS_UNAVAILABLE;

-(nullable instancetype) initWithContainerID:(NSString*) containerID;
-(BOOL) setActivePresetIndex:(NSInteger) index error:(NSError**) error;
-(NSInteger) getActivePresetIndex:(NSError**) error;
-(NSInteger) getFactoryDefaultPresetIndex:(NSError**) error;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayDevice_h */
