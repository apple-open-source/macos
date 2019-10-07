//
//  HIDDisplayInterface.h
//  IOHIDFamily
//
//  Created by AB on 1/10/19.
//

#ifndef HIDDisplayInterface_h
#define HIDDisplayInterface_h


#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayInterface : NSObject

@property(readonly) NSString* containerID;
@property(readonly) uint64_t  registryID;

@property(readonly,nullable) NSArray<NSString*> *capabilities;

-(nullable instancetype) initWithContainerID:(NSString*) containerID;
-(nullable instancetype) initWithService:(io_service_t) service;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayInterface_h */
