//
//  HIDDisplayPresetInterface.h
//  IOHIDFamily
//
//  Created by AB on 3/26/19.
//

#ifndef HIDDisplayPresetInterface_h
#define HIDDisplayPresetInterface_h

#import <Foundation/Foundation.h>
#import <HIDDisplay/HIDDisplayInterface.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDDisplayPresetData;

@interface HIDDisplayPresetInterface : HIDDisplayInterface

@property(readonly) NSArray<HIDDisplayPresetData*> *presets;

-(nullable instancetype) init NS_UNAVAILABLE;
-(BOOL) setActivePresetIndex:(NSInteger) index error:(NSError**) error;
-(NSInteger) getActivePresetIndex:(NSError**) error;
-(NSInteger) getFactoryDefaultPresetIndex:(NSError**) error;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayPresetInterface_h */
