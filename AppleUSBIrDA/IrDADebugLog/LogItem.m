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
#import "LogItem.h"

static NSString *defaultCategory = @"Some Incredibly Insightfull message";

@implementation LogItem

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject: [self time]];
    [coder encodeObject: [self first]];
    [coder encodeObject: [self second]];
	[coder encodeObject: [self message]];
}

- (id)initWithCoder:(NSCoder *)coder {
    // init variables in the same order as they were encoded    
    [self setTime: [coder decodeObject]];
    [self setFirst: [coder decodeObject]];
    [self setSecond: [coder decodeObject]];
    [self setMessage: [coder decodeObject]];
    return self;
}


+ (LogItem *)logItemWithValues:(UInt32)myTime:(UInt16)myFirst:(UInt16)mySecond:(NSString *)myMessage{
    // convenience method to create a new logItem with default
    // test data given an amount
    LogItem *newLogItem = [[LogItem alloc] init];
    [newLogItem autorelease];
    [newLogItem setTime: [NSNumber numberWithUnsignedLong:myTime]]; 
    [newLogItem setFirst: [NSNumber numberWithUnsignedShort:myFirst]]; 
    [newLogItem setSecond: [NSNumber numberWithUnsignedShort:mySecond]]; 
    [newLogItem setMessage: myMessage];
    return newLogItem;
}

+ (LogItem *)logItem {
    return [[[LogItem alloc] init] autorelease];
}

- (id)init {
    [super init];
    // ensure amount is never nil
    [self setTime: [NSNumber numberWithUnsignedLong:0]];
    [self setFirst: [NSNumber numberWithUnsignedShort:0]]; 
    [self setSecond: [NSNumber numberWithUnsignedShort:0]]; 
    [self setMessage: defaultCategory];
    return self;
}

- (NSNumber *)time {
	return [[time copy] autorelease];
}

- (void)setTime:(NSNumber *)value {
    [value retain];
    [time release];
    time = value;
}

- (NSString *)message {
	return [[message copy] autorelease];
}

- (void)setMessage:(NSString *)value {
    [value retain];
    [message release];
    message = value;
}

- (NSNumber *)first {
    return [[first copy] autorelease];
}
- (NSNumber *)second {
    return [[second copy] autorelease];
}

- (void)setFirst:(NSNumber *)value {
    NSNumber *copy = [value copy];
    [first release];
    first = copy;
}

- (void)setSecond:(NSNumber *)value {
    NSNumber *copy = [value copy];
    [second release];
    second = copy;
}

- (void)dealloc {
    [self setTime:nil];
    [self setFirst:nil];
    [self setSecond:nil];
    [self setMessage:nil];
    [super dealloc];
}

@end
