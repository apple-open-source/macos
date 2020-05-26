//
//  HIDDisplayDeviceManagementInterface.h
//  IOHIDFamily
//
//  Created by abhishek on 1/6/20.
//

#import <Foundation/Foundation.h>
#import <HIDDisplay/HIDDisplayInterface.h>
#import <HIDDisplay/HIDDisplayDeviceManagementCAPI.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayDeviceManagementInterface : HIDDisplayInterface

-(nullable instancetype) init NS_UNAVAILABLE;
-(BOOL) factoryReset:(uint8_t) type securityToken:(uint64_t) securityToken error:(NSError**) error;
-(BOOL) getSecurityToken:(uint64_t*) securityToken error:(NSError**) error;

@end

NS_ASSUME_NONNULL_END
