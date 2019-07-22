//
//  HIDDisplayPresetInterfacePrivate.h
//  IOHIDFamily
//
//  Created by AB on 3/26/19.
//

#ifndef HIDDisplayPresetInterfacePrivate_h
#define HIDDisplayPresetInterfacePrivate_h


#import <Foundation/Foundation.h>
#import <HIDDisplay/HIDDisplay.h>
#import <HIDDisplay/HIDDisplayInterface.h>
#import "HIDElement.h"
#import "HIDDisplayPresetInterface.h"
#import "HIDDisplayInterfacePrivate.h"

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayPresetInterface (HIDDisplayPresetInterfacePrivate)

-(NSInteger) getCurrentPresetIndex:(NSError**) error;
-(BOOL) setCurrentPresetIndex:(NSInteger) index error:(NSError**) error;
-(HIDElement* __nullable) getHIDElementForUsage:(NSInteger) usage;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayPresetInterfacePrivate_h */
