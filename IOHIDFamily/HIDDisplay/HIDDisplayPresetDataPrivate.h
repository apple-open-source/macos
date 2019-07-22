//
//  HIDDisplayPresetDataPrivate.h
//  IOHIDFamily
//
//  Created by AB on 1/16/19.
//

#ifndef HIDDisplayPresetDataPrivate_h
#define HIDDisplayPresetDataPrivate_h

#import <HIDDisplay/HIDDisplayPresetData.h>
#import <HIDDisplay/HIDDisplayInterface.h>
#import <HIDDisplay/HIDDisplayPresetInterface.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayPresetData (HIDDisplayPresetDataPrivate)

@property HIDDisplayPresetInterface* hidDisplay;

-(nullable instancetype) initWithDisplayDevice:(HIDDisplayPresetInterface*) hidDisplay index:(NSInteger) index;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayPresetDataPrivate_h */
