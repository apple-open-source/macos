/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#import "USBClass.h"

@implementation USBClass

- (id)init
{
    self = [super init];
    className = [[NSString alloc] init];
    subClassName = [[NSString alloc] init];
    return self;
}


- (void)setClassName:(NSString *)classname
{
    [className release];
    className = [[NSString alloc] initWithString:classname];
    return;
}

- (void)setSubClassName:(NSString *)subclassname
{
    [subClassName release];
    subClassName = [[NSString alloc] initWithString:subclassname];
    return;
}

- (NSString *)className
{
    return className;
}

- (NSString *)subClassName
{
    return subClassName;
}

- (void)dealloc
{
    [className release];
    [subClassName release];
    [super dealloc];
}
@end
