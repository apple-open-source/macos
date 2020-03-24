//
//  HIDDisplayUserAdjustmentInterface.h
//  IOHIDFamily
//
//  Created by abhishek on 1/15/20.
//

#import <Foundation/Foundation.h>
#import <HIDDisplay/HIDDisplayInterface.h>
#import <HIDDisplay/HIDDisplayUserAdjustmentCAPI.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayUserAdjustmentInterface : HIDDisplayInterface

@property (readonly) BOOL valid;

-(nullable instancetype) init NS_UNAVAILABLE;
-(BOOL) invalidate:(NSError**) error;
-(BOOL) set:(NSDictionary*) data error:(NSError**) error;
-(NSDictionary* _Nullable) get:(NSError**) error;
@end

NS_ASSUME_NONNULL_END

