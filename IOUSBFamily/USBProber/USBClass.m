/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
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
