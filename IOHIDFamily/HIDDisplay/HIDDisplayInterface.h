//
//  HIDDisplayInterface.h
//  IOHIDFamily
//
//  Created by AB on 1/10/19.
//

#ifndef HIDDisplayInterface_h
#define HIDDisplayInterface_h


#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayInterface : NSObject

@property(readonly) NSString* containerID;

@property(readonly,nullable) NSArray<NSString*> *capabilities;

-(nullable instancetype) initWithContainerID:(NSString*) containerID;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayInterface_h */
