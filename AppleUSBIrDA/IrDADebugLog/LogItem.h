/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <Foundation/Foundation.h>

@interface LogItem : NSObject <NSCoding> {
    NSNumber *time;				// Used as a UInt32 from IrDA
    NSNumber *first;			// Used as a UInt16 from IrDA
    NSNumber *second;			// Used as a UInt16 from IrDA
    NSString *message;
}

- (id)initWithCoder:(NSCoder *)coder;
- (void)encodeWithCoder:(NSCoder *)coder;

+ (LogItem *)logItem;
+ (LogItem *)logItemWithValues:(UInt32)myTime:(UInt16)myFirst:(UInt16)mySecond:(NSString *)myMessages;

- (NSNumber *)time;
- (void)setTime:(NSNumber *)value;
- (NSNumber *)first;
- (void)setFirst:(NSNumber *)value;
- (NSNumber *)second;
- (void)setSecond:(NSNumber *)value;
- (NSString *)message;
- (void)setMessage:(NSString *)value;

@end
