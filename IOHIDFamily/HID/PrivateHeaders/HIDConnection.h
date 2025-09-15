/*!
 * HIDConnection.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDConnection_h
#define HIDConnection_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDConnectionBase.h>
#import <IOKit/hidobjc/HIDConnectionBase.h>
#import <IOKit/hid/IOHIDEventSystemConnection.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @category HIDConnection
 *
 * @abstract
 * Direct interaction with a HID event system connection.
 *
 * @discussion
 * Should only be used by system code.
 */
@interface HIDConnection (HIDFramework)

@property (readonly) NSString *uuid;

@property (readonly) IOHIDEventSystemConnectionType type;

-(void)getAuditToken:(audit_token_t *)token;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDConnection_h */
