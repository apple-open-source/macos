//
//  HIDConnection.h
//  HID
//
//  Created by dekom on 9/16/18.
//

#ifndef HIDConnection_h
#define HIDConnection_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDConnectionBase.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDConnection (HIDFramework)

@property (readonly) NSString *uuid;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDConnection_h */
